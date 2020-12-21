#include "filesys/directory.h"
#include <stdio.h>
#include <string.h>
#include <list.h>
#include "filesys/filesys.h"
#include "filesys/inode.h"
#include "threads/malloc.h"
#include "threads/thread.h"
#include "threads/vaddr.h"

/* A directory. */
struct dir 
  {
    struct inode *inode;                /* Backing store. */
    off_t pos;                          /* Current position. */
  };

/* A single directory entry. */
struct dir_entry 
  {
    block_sector_t inode_sector;        /* Sector number of header. */
    char name[NAME_MAX + 1];            /* Null terminated file name. */
    bool in_use;                        /* In use or free? */
  };

/* Creates a directory with space for ENTRY_CNT entries in the
   given SECTOR.  Returns true if successful, false on failure. */
bool
dir_create (block_sector_t sector, size_t entry_cnt)
{
  // return inode_create (sector, entry_cnt * sizeof (struct dir_entry));
  /************************ NEW CODE ***************************/
  bool success = inode_create (sector, (entry_cnt+2) 
                  * sizeof (struct dir_entry));
  if (success)
    {
      struct inode* inode = inode_open (sector);
      ASSERT (inode != NULL);
      inode_set_dir (inode);

      struct dir* dir = dir_open (inode);
      ASSERT (dir != NULL);
      // add . to directory
      ASSERT (dir_add(dir, ".", sector));
      // add .. to directory
      ASSERT (dir_add(dir, "..", sector));
      dir_close(dir);
    }
  return success;
  /********************** END NEW CODE *************************/
}

/* Opens and returns the directory for the given INODE, of which
   it takes ownership.  Returns a null pointer on failure. */
struct dir *
dir_open (struct inode *inode) 
{
  struct dir *dir = calloc (1, sizeof *dir);
  if (inode != NULL && dir != NULL)
    {
      /************************ NEW CODE ***************************/
      dir->inode = inode;
      dir->pos = 2 * sizeof(struct dir_entry);
      /********************** END NEW CODE *************************/
      return dir;
    }
  else
    {
      inode_close (inode);
      free (dir);
      return NULL; 
    }
}

/* Opens the root directory and returns a directory for it.
   Return true if successful, false on failure. */
struct dir *
dir_open_root (void)
{
  return dir_open (inode_open (ROOT_DIR_SECTOR));
}

/* Opens and returns a new directory for the same inode as DIR.
   Returns a null pointer on failure. */
struct dir *
dir_reopen (struct dir *dir) 
{
  return dir_open (inode_reopen (dir->inode));
}

/* Destroys DIR and frees associated resources. */
void
dir_close (struct dir *dir) 
{
  if (dir != NULL)
    {
      inode_close (dir->inode);
      free (dir);
    }
}

/* Returns the inode encapsulated by DIR. */
struct inode *
dir_get_inode (struct dir *dir) 
{
  return dir->inode;
}

/* Searches DIR for a file with the given NAME.
   If successful, returns true, sets *EP to the directory entry
   if EP is non-null, and sets *OFSP to the byte offset of the
   directory entry if OFSP is non-null.
   otherwise, returns false and ignores EP and OFSP. */
static bool
lookup (const struct dir *dir, const char *name,
        struct dir_entry *ep, off_t *ofsp) 
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use && !strcmp (name, e.name)) 
      {
        if (ep != NULL)
          *ep = e;
        if (ofsp != NULL)
          *ofsp = ofs;
        return true;
      }
  return false;
}

/* Searches DIR for a file with the given NAME
   and returns true if one exists, false otherwise.
   On success, sets *INODE to an inode for the file, otherwise to
   a null pointer.  The caller must close *INODE. */
bool
dir_lookup (const struct dir *dir, const char *name,
            struct inode **inode) 
{
  struct dir_entry e;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  if (lookup (dir, name, &e, NULL))
    *inode = inode_open (e.inode_sector);
  else
    *inode = NULL;

  return *inode != NULL;
}

/* Adds a file named NAME to DIR, which must not already contain a
   file by that name.  The file's inode is in sector
   INODE_SECTOR.
   Returns true if successful, false on failure.
   Fails if NAME is invalid (i.e. too long) or a disk or memory
   error occurs. */
bool
dir_add (struct dir *dir, const char *name, block_sector_t inode_sector)
{
  struct dir_entry e;
  off_t ofs;
  bool success = false;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Check NAME for validity. */
  if (*name == '\0' || strlen (name) > NAME_MAX)
    return false;

  /* Check that NAME is not in use. */
  if (lookup (dir, name, NULL, NULL))
    goto done;

  /* Set OFS to offset of free slot.
     If there are no free slots, then it will be set to the
     current end-of-file.
     
     inode_read_at() will only return a short read at end of file.
     Otherwise, we'd need to verify that we didn't get a short
     read due to something intermittent such as low memory. */
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (!e.in_use)
      break;

  /* Write slot. */
  e.in_use = true;
  strlcpy (e.name, name, sizeof e.name);
  e.inode_sector = inode_sector;
  success = inode_write_at (dir->inode, &e, sizeof e, ofs) == sizeof e;

  /************************ NEW CODE ***************************/
  if (name[0] == '.')
    goto done;
  if (success)
    {
      struct inode* inode = inode_open (inode_sector);
      ASSERT (inode != NULL);

      if (inode_is_dir (inode))
        {
          // whether file added is a directory
          struct dir* d = dir_open (inode);
          ASSERT (d != NULL);

          struct dir_entry ep;
          off_t ofsp;
          ASSERT (lookup (d, "..", &ep, &ofsp));
          ep.inode_sector = inode_get_inumber (dir->inode);
          ASSERT (inode_write_at (d->inode, &ep, sizeof (ep), ofsp)
                   == sizeof (ep));
          dir_close (d);
        }
      else
        {
          inode_close (inode);
        }
    }
  /********************** END NEW CODE *************************/
 done:
  return success;
}

/* Removes any entry for NAME in DIR.
   Returns true if successful, false on failure,
   which occurs only if there is no file with the given NAME. */
bool
dir_remove (struct dir *dir, const char *name) 
{
  struct dir_entry e;
  struct inode *inode = NULL;
  bool success = false;
  off_t ofs;

  ASSERT (dir != NULL);
  ASSERT (name != NULL);

  /* Find directory entry. */
  if (!lookup (dir, name, &e, &ofs))
    goto done;

  /* Open inode. */
  inode = inode_open (e.inode_sector);
  if (inode == NULL)
    goto done;

  /* Erase directory entry. */
  e.in_use = false;
  if (inode_write_at (dir->inode, &e, sizeof e, ofs) != sizeof e) 
    goto done;

  /* Remove inode. */
  inode_remove (inode);
  success = true;

 done:
  inode_close (inode);
  return success;
}

/* Reads the next directory entry in DIR and stores the name in
   NAME.  Returns true if successful, false if the directory
   contains no more entries. */
bool
dir_readdir (struct dir *dir, char name[NAME_MAX + 1])
{
  struct dir_entry e;

  while (inode_read_at (dir->inode, &e, sizeof e, dir->pos) == sizeof e) 
    {
      dir->pos += sizeof e;
      if (e.in_use)
        {
          strlcpy (name, e.name, NAME_MAX + 1);
          return true;
        } 
    }
  return false;
}

/************************ NEW CODE ***************************/
// divide the path into directory and file name
bool 
dir_divide (char *name, struct dir *cur_dir, struct dir **ret_dir,
            char **ret_name)
{
  ASSERT (cur_dir != NULL);
  if (*name == NULL){
    return false;
  }
  
  char *path = malloc(strlen (name) + 1);
  strlcpy (path, name, PGSIZE);
  char *token, *save_ptr;
  struct inode *inode = NULL;
  if (name[0] == '/')
    {
      // under root directory
      dir_close (cur_dir);
      cur_dir = dir_open_root ();
    }
  // count for directories
  int count = 0;
  // divide the path by '/'
  for (token = strtok_r (path, "/", &save_ptr); token != NULL;
      token = strtok_r (NULL, "/", &save_ptr))
    {
      count++;
    }
  int i = 0;
  strlcpy (path, name, PGSIZE);
  for (token = strtok_r (path, "/", &save_ptr); token != NULL;
      token = strtok_r (NULL, "/", &save_ptr))
    {
      if (i==count-1)
        break;
      // search every token in the current directory
      if (!dir_lookup (cur_dir, token, &inode))
        {
          dir_close (cur_dir);
          free (path);
          return false;
        }
      dir_close (cur_dir);
      cur_dir = dir_open (inode);
      i++;
    }
  *ret_dir = dir_reopen(cur_dir);
  if (token == NULL)
    {
      (*ret_name)[0] = '.';
      (*ret_name)[1] = '\0';
    }
  else
    strlcpy(*ret_name, token, 15);
  dir_close (cur_dir);
  free (path);
  return true;
}

// whether DIR is empty
bool 
dir_empty (struct dir *dir)
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  int count = 0;
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    if (e.in_use) 
      {
        count++;
      }
  return count == 2;
}

// clear DIR
bool 
dir_clear (struct dir *dir)
{
  struct dir_entry e;
  size_t ofs;
  
  ASSERT (dir != NULL);
  int count = 0;
  for (ofs = 0; inode_read_at (dir->inode, &e, sizeof e, ofs) == sizeof e;
       ofs += sizeof e) 
    {
      if (e.in_use)
        {
          count++;
          e.in_use = false;
        }
    }
  ASSERT (count==2);
  return true;
}
/********************** END NEW CODE *************************/
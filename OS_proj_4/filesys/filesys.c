#include "filesys/filesys.h"
#include <debug.h>
#include <stdio.h>
#include <string.h>
#include "filesys/file.h"
#include "filesys/free-map.h"
#include "filesys/inode.h"
#include "filesys/directory.h"
#include "threads/thread.h"
#include "filesys/cache.h"

/* Partition that contains the file system. */
struct block *fs_device;

static void do_format (void);

/* Initializes the file system module.
   If FORMAT is true, reformats the file system. */
void
filesys_init (bool format) 
{
  /************************ NEW CODE ***************************/
  cache_init();
  /********************** END NEW CODE *************************/
  fs_device = block_get_role (BLOCK_FILESYS);
  if (fs_device == NULL)
    PANIC ("No file system device found, can't initialize file system.");

  inode_init ();
  free_map_init ();

  if (format) 
    do_format ();

  free_map_open ();
}

/* Shuts down the file system module, writing any unwritten data
   to disk. */
void
filesys_done (void) 
{
  /************************ NEW CODE ***************************/
  cache_back_to_disk();
  /********************** END NEW CODE *************************/
  free_map_close ();
}

/* Creates a file named NAME with the given INITIAL_SIZE.
   Returns true if successful, false otherwise.
   Fails if a file named NAME already exists,
   or if internal memory allocation fails. */
bool
filesys_create (const char *name, off_t initial_size) 
{
  block_sector_t inode_sector = 0;
  
  /************************ NEW CODE ***************************/
  ASSERT (name != NULL);
  struct dir *cur_dir = thread_current()->pwd;
  if (*name == NULL)
    return false;
  if (cur_dir == NULL)
    cur_dir = dir_open_root ();
  else
    cur_dir = dir_reopen (cur_dir);
  ASSERT (cur_dir != NULL);
  struct dir *ret_dir = NULL;
  char *ret_name = malloc(15);
  struct inode *inode = NULL;
  if (strlen(name) > 14)
    return false;
  bool success = dir_divide (name, cur_dir, &ret_dir, &ret_name);
  if (!success || dir_lookup (ret_dir, ret_name, &inode))
    {
      dir_close (ret_dir);
      inode_close (inode);
      free (ret_name);
      return NULL;
    }
  success = (ret_dir != NULL
                  && free_map_allocate (1, &inode_sector)
                  && inode_create (inode_sector, initial_size)
                  && dir_add (ret_dir, ret_name, inode_sector));
  dir_close (ret_dir);
  free (ret_name);
  /********************** END NEW CODE *************************/

  return success;
}

/* Opens the file with the given NAME.
   Returns the new file if successful or a null pointer
   otherwise.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
struct file *
filesys_open (const char *name)
{
  
  struct inode *inode = NULL;
  /************************ NEW CODE ***************************/
  ASSERT (name != NULL);
  if (*name == NULL)
    return NULL;
  struct dir *cur_dir = thread_current()->pwd;
  if (cur_dir == NULL)
    cur_dir = dir_open_root ();
  else
    cur_dir = dir_reopen (cur_dir);
  ASSERT (cur_dir != NULL);
  struct dir *ret_dir = NULL;
  char *ret_name = malloc(15);
  bool success = dir_divide (name, cur_dir, &ret_dir, &ret_name);
  if (!success || !dir_lookup (ret_dir, ret_name, &inode))
    {
      dir_close (ret_dir);
      free (ret_name);
      return NULL;
    }
  
  dir_close (ret_dir);
  free (ret_name);
  /********************** END NEW CODE *************************/

  return file_open (inode);
}

/* Deletes the file named NAME.
   Returns true if successful, false on failure.
   Fails if no file named NAME exists,
   or if an internal memory allocation fails. */
bool
filesys_remove (const char *name) 
{
  /************************ NEW CODE ***************************/
  ASSERT (name != NULL);
  if (*name == NULL)
    return false;
  struct dir *cur_dir = thread_current()->pwd;
  if (cur_dir == NULL)
    cur_dir = dir_open_root ();
  else
    cur_dir = dir_reopen (cur_dir);
  ASSERT (cur_dir != NULL);
  struct dir *ret_dir = NULL;
  char *ret_name = malloc(15);
  struct inode *inode = NULL;

  bool success = dir_divide (name, cur_dir, &ret_dir, &ret_name);
  if (!success || !dir_lookup (ret_dir, ret_name, &inode))
    {
      dir_close (ret_dir);
      free (ret_name);
      return false;
    }
  if (!inode_is_dir (inode))
    {
      success = success && dir_remove (ret_dir, ret_name);
    }
  else
    {
      struct dir* dir_to_remove = dir_open (inode);
      success = success && dir_empty (dir_to_remove) 
                        && (inode_open_count (inode) <= 1)
                        && dir_clear (dir_to_remove)
                        && dir_remove (ret_dir, ret_name);
    }
  inode_close (inode);
  dir_close (ret_dir);
  free (ret_name);
  /********************** END NEW CODE *************************/
  return success;
}

/* Formats the file system. */
static void
do_format (void)
{
  printf ("Formatting file system...");
  free_map_create ();
  if (!dir_create (ROOT_DIR_SECTOR, 16))
    PANIC ("root directory creation failed");
  free_map_close ();
  printf ("done.\n");
}

/************************ NEW CODE ***************************/
/* Changes the current working directory of the process to dir,
   which may be relative or absolute. Returns true if successful, 
   false on failure. */
bool 
filesys_chdir (const char *name)
{
  ASSERT (name != NULL);
  if (*name == NULL)
    return false;
  struct thread *cur = thread_current();
  struct dir *cur_dir = cur->pwd;
  if (cur_dir == NULL)
    cur_dir = dir_open_root ();
  else
    cur_dir = dir_reopen (cur_dir);
  ASSERT (cur_dir != NULL);
  ASSERT (cur_dir != NULL);

  struct dir *ret_dir = NULL;
  char *ret_name = malloc(15);
  struct inode *inode = NULL;

  bool success = dir_divide (name, cur_dir, &ret_dir, &ret_name);

  if (!success ||
      !dir_lookup (ret_dir, ret_name, &inode) || 
      !inode_is_dir (inode))
    {
      dir_close (ret_dir);
      free (ret_name);
      return false;
    }
  if (cur->pwd != NULL)
    dir_close (cur->pwd);
  cur->pwd = dir_open (inode);
  dir_close (ret_dir);
  free (ret_name);
  return true;
}

/* Creates the directory named dir, which may be relative or absolute.
   Returns true if successful, false on failure. Fails if dir already 
   exists or if any directory name in dir, besides the last, does not 
   already exist. That is, mkdir("/a/b/c") succeeds only if /a/b already 
   exists and /a/b/c does not. */
bool 
filesys_mkdir (const char *name)
{
  ASSERT (name != NULL);

  if (*name == NULL){
    return false;
  }

  struct dir *cur_dir = thread_current()->pwd;
  if (cur_dir == NULL)
    cur_dir = dir_open_root ();
  else
    cur_dir = dir_reopen (cur_dir);
  ASSERT (cur_dir != NULL);
  if (name == NULL)
    return false;
  struct dir *ret_dir = NULL;
  char *ret_name = malloc(15);
  struct inode *inode = NULL;

  bool success = dir_divide (name, cur_dir, &ret_dir, &ret_name);
  if (!success ||
      dir_lookup (ret_dir, ret_name, &inode))
    {
      dir_close (ret_dir);
      inode_close (inode);
      free (ret_name);
      return false;
    }

  block_sector_t sector;
  if (!free_map_allocate (1, &sector) ||
      !dir_create (sector, 0))
    {
      dir_close (ret_dir);
      free (ret_name);
      return false;
    }
  
  success = dir_add (ret_dir, ret_name, sector);
  dir_close (ret_dir);
  free (ret_name);
  return success;
}

/********************** END NEW CODE *************************/
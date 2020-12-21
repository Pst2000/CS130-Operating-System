#include "filesys/inode.h"
#include <stdio.h>
#include <list.h>
#include <debug.h>
#include <round.h>
#include <string.h>
#include "filesys/filesys.h"
#include "filesys/free-map.h"
#include "threads/malloc.h"
#include "filesys/cache.h"

/* Identifies an inode. */
#define INODE_MAGIC 0x494e4f44
/************************ NEW CODE ***************************/
#define INODE_TABLE_LENGTH 128
#define INODE_DIRECT_N 8
#define INODE_INDIRECT_N 32

static char zeros[BLOCK_SECTOR_SIZE];
/********************** END NEW CODE *************************/

/* On-disk inode.
   Must be exactly BLOCK_SECTOR_SIZE bytes long. */
struct inode_disk
  {
    // block_sector_t start;               /* First data sector. */
    /************************ NEW CODE ***************************/
    // direct blocks
    block_sector_t direct_blocks[INODE_DIRECT_N];
    // indirect blocks
    block_sector_t indirect_blocks[INODE_INDIRECT_N];
    // whether it's a dir
    bool is_dir;
    /********************** END NEW CODE *************************/
    off_t length;                       /* File size in bytes. */
    unsigned magic;                     /* Magic number. */
    uint32_t unused[85];               /* Not used. */
  };

/* Returns the number of sectors to allocate for an inode SIZE
   bytes long. */
static inline size_t
bytes_to_sectors (off_t size)
{
  return DIV_ROUND_UP (size, BLOCK_SECTOR_SIZE);
}

/* In-memory inode. */
struct inode 
  {
    struct list_elem elem;              /* Element in inode list. */
    block_sector_t sector;              /* Sector number of disk location. */
    int open_cnt;                       /* Number of openers. */
    bool removed;                       /* True if deleted, false otherwise.*/
    int deny_write_cnt;                 /* 0: writes ok, >0: deny writes. */
    struct inode_disk data;             /* Inode content. */
  };

/************************ NEW CODE ***************************/
int
get_direct_i(off_t pos)
{
  int direct_block_i = pos / BLOCK_SECTOR_SIZE;
  direct_block_i = direct_block_i < 8 ? direct_block_i : -1;
  return direct_block_i;
}
/********************** END NEW CODE *************************/

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector (const struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  if (pos < inode->data.length)
    {
      /************************ NEW CODE ***************************/
      if (pos < INODE_DIRECT_N * BLOCK_SECTOR_SIZE)
        {
          // pos is in direct block
          int direct_block_i = pos / BLOCK_SECTOR_SIZE;
          return inode->data.direct_blocks[direct_block_i];
        }
      else if (pos < INODE_DIRECT_N * BLOCK_SECTOR_SIZE + 
        INODE_INDIRECT_N * INODE_TABLE_LENGTH * BLOCK_SECTOR_SIZE)
        {
          // pos is in indirect block
          int indirect_table_i = (pos - INODE_DIRECT_N * BLOCK_SECTOR_SIZE)
                                / (INODE_TABLE_LENGTH * BLOCK_SECTOR_SIZE);
          int table_entry_i = (pos - INODE_DIRECT_N * BLOCK_SECTOR_SIZE - 
                                indirect_table_i * INODE_TABLE_LENGTH 
                                * BLOCK_SECTOR_SIZE) 
                                / BLOCK_SECTOR_SIZE;

          block_sector_t *table = calloc(INODE_TABLE_LENGTH, 
                                          sizeof (block_sector_t*));
          ASSERT (table != NULL);
          // read file data into cache buffer
          cache_read (inode->data.indirect_blocks[indirect_table_i], table);
          block_sector_t result = table[table_entry_i];
          free (table);
          return result;
        }
      else
      {
        ASSERT (false);
      }
      
      /********************** END NEW CODE *************************/
      // return inode->data.start + pos / BLOCK_SECTOR_SIZE;
    }
  else
    return -1;
}

/* Returns the block device sector that contains byte offset POS
   within INODE.
   Returns -1 if INODE does not contain data for a byte at offset
   POS. */
static block_sector_t
byte_to_sector_write (struct inode *inode, off_t pos) 
{
  ASSERT (inode != NULL);
  block_sector_t result;
  // current position is already smaller than length, 
  // no need to extend into direct/indirect blocks
  if (pos < inode->data.length)
    {
      result = byte_to_sector (inode, pos);
      return result;
    }
  else
    {
      size_t sectors_cur = bytes_to_sectors (inode->data.length);
      size_t sectors_new = bytes_to_sectors (pos+1);
      // within the reach of direct blocks
      if (sectors_cur < INODE_DIRECT_N)
        {
          size_t sectors_direct_new = sectors_new < INODE_DIRECT_N ?
                                      sectors_new : INODE_DIRECT_N;
          for (size_t i=sectors_cur; i<sectors_direct_new; i++)
            {
              // allocate the bitmap and write into cache buffer
              if (!free_map_allocate (1, &inode->data.direct_blocks[i]))
                return -1;
              cache_write (inode->data.direct_blocks[i], zeros);
            }
        }

      if (sectors_new <= INODE_DIRECT_N)
        {
          inode->data.length = pos+1;
          cache_write (inode->sector, &inode->data);
          return inode->data.direct_blocks[sectors_new-1];
        }
      // cases when sector_new is even larger than direct block's range
      // get into indirect blocks
      size_t sectors_indirect_cur = sectors_cur > INODE_DIRECT_N ?
                                    sectors_cur-INODE_DIRECT_N : 0;
      size_t table_n_old = (sectors_indirect_cur+INODE_TABLE_LENGTH-1) 
                            / INODE_TABLE_LENGTH;  // how many tables
      size_t sectors_indirect_new = sectors_new - INODE_DIRECT_N;
      block_sector_t *table = calloc(INODE_TABLE_LENGTH, 
                                      sizeof (block_sector_t*));
      ASSERT (table != NULL);
      for (size_t i= (sectors_indirect_cur/ INODE_TABLE_LENGTH)*
                  INODE_TABLE_LENGTH; 
            i<sectors_indirect_new; i+=INODE_TABLE_LENGTH)
        {
          size_t off_i;
          size_t indirect_i = i / INODE_TABLE_LENGTH;
          // check if we need a new table
          if (indirect_i+1 > table_n_old) 
            {
              if (!free_map_allocate (1, 
                    &inode->data.indirect_blocks[indirect_i])) 
                {
                  free (table);
                  return -1;
                }
              memset (table, 0, BLOCK_SECTOR_SIZE);
              off_i = 0;
            }
          else
            {
              // read from cache to memory the original indirect blocks
              cache_read (inode->data.indirect_blocks[indirect_i], table);
              off_i = sectors_indirect_cur % INODE_TABLE_LENGTH;
            }
          
          size_t n_table_entry = 
            (sectors_indirect_new-i) < INODE_TABLE_LENGTH ?
            (sectors_indirect_new-i) : INODE_TABLE_LENGTH;
          
          for (size_t j=off_i; j<n_table_entry; j+=1)
            {
              if (!free_map_allocate (1, &table[j]))
                {
                  free (table);
                  return -1;
                }
              cache_write (table[j], zeros);
            }
          // write table back into cache
          // given the indirect index
          cache_write (inode->data.indirect_blocks[indirect_i], table);
        }
      result = table[(sectors_indirect_new-1)%INODE_TABLE_LENGTH];

      free (table);
    }
    inode->data.length = pos+1;
    // finally write all the data part of the inode into cache sector
    cache_write (inode->sector, &inode->data);
    return result;
}
/* List of open inodes, so that opening a single inode twice
   returns the same `struct inode'. */
static struct list open_inodes;

/* Initializes the inode module. */
void
inode_init (void) 
{
  list_init (&open_inodes);
}

/* Initializes an inode with LENGTH bytes of data and
   writes the new inode to sector SECTOR on the file system
   device.
   Returns true if successful.
   Returns false if memory or disk allocation fails. */
bool
inode_create (block_sector_t sector, off_t length)
{
  struct inode_disk *disk_inode = NULL;
  bool success = false;

  ASSERT (length >= 0);

  /* If this assertion fails, the inode structure is not exactly
     one sector in size, and you should fix that. */
  ASSERT (sizeof *disk_inode == BLOCK_SECTOR_SIZE);

  disk_inode = calloc (1, sizeof *disk_inode);
  if (disk_inode != NULL)
    {
      size_t sectors = bytes_to_sectors (length);
      disk_inode->length = length;
      disk_inode->magic = INODE_MAGIC;
      // if (free_map_allocate (sectors, &disk_inode->start)) 
      // if (free_map_allocate (1, &disk_inode->direct_blocks[0])) 
      //   {
        /************************ NEW CODE ***************************/
      // similar as above 
      disk_inode->is_dir = false;
      size_t n_direct_blocks = sectors < INODE_DIRECT_N ? 
                                sectors : INODE_DIRECT_N;
      for (size_t i=0; i<n_direct_blocks; i++)
        {
          if (!free_map_allocate (1, &disk_inode->direct_blocks[i]))
            {
              // if it's not allocated yet
              free (disk_inode);
              return false;
            }
          // write zeros
          cache_write (disk_inode->direct_blocks[i], zeros);
        }

      if (sectors <= INODE_DIRECT_N)
        {
          // within direct blocks
          cache_write (sector, disk_inode);
          free (disk_inode);
          return true;
        }
      size_t n_indirect_blocks = sectors - INODE_DIRECT_N;
      block_sector_t *table = calloc(INODE_TABLE_LENGTH, 
                                      sizeof (block_sector_t*));
      ASSERT (table != NULL);

      for (size_t i=0; i<n_indirect_blocks; i+=INODE_TABLE_LENGTH)
        {
          size_t indirect_i = i / INODE_TABLE_LENGTH;
          if (!free_map_allocate (1, 
                &disk_inode->indirect_blocks[indirect_i])) 
            {
              free (disk_inode);
              free (table);
              return false;
            }

          memset (table, 0, BLOCK_SECTOR_SIZE);
          size_t n_table_entry = 
            (n_indirect_blocks-i) < INODE_TABLE_LENGTH ?
            (n_indirect_blocks-i) : INODE_TABLE_LENGTH;
          for (size_t j=0; j<n_table_entry; j+=1)
            {
              if (!free_map_allocate (1, &table[j]))
                {
                  free (disk_inode);
                  free (table);
                  return false;
                }
              cache_write (table[j], zeros);
            }
          // write table back into cache
          // given the indirect index
          cache_write (disk_inode->indirect_blocks[indirect_i], table);
        }
      success = true;
      free (table);
      // finally write all the data part of the inode into cache sector
      cache_write (sector, disk_inode);
      /********************** END NEW CODE *************************/
      free (disk_inode);
    }
  return success;
}

/* Reads an inode from SECTOR
   and returns a `struct inode' that contains it.
   Returns a null pointer if memory allocation fails. */
struct inode *
inode_open (block_sector_t sector)
{
  struct list_elem *e;
  struct inode *inode;

  /* Check whether this inode is already open. */
  for (e = list_begin (&open_inodes); e != list_end (&open_inodes);
       e = list_next (e)) 
    {
      inode = list_entry (e, struct inode, elem);
      if (inode->sector == sector) 
        {
          inode_reopen (inode);
          return inode; 
        }
    }

  /* Allocate memory. */
  inode = malloc (sizeof *inode);
  if (inode == NULL)
    return NULL;

  /* Initialize. */
  list_push_front (&open_inodes, &inode->elem);
  inode->sector = sector;
  inode->open_cnt = 1;
  inode->deny_write_cnt = 0;
  inode->removed = false;
  /************************ NEW CODE ***************************/
  cache_read (inode->sector, &inode->data);
  /********************** END NEW CODE *************************/
  // block_read (fs_device, inode->sector, &inode->data);
  return inode;
}

/* Reopens and returns INODE. */
struct inode *
inode_reopen (struct inode *inode)
{
  if (inode != NULL)
    inode->open_cnt++;
  return inode;
}

/* Returns INODE's inode number. */
block_sector_t
inode_get_inumber (const struct inode *inode)
{
  return inode->sector;
}

/* Closes INODE and writes it to disk.
   If this was the last reference to INODE, frees its memory.
   If INODE was also a removed inode, frees its blocks. */
void
inode_close (struct inode *inode) 
{
  /* Ignore null pointer. */
  if (inode == NULL)
    return;
  
  /* Release resources if this was the last opener. */
  if (--inode->open_cnt == 0)
    {
      /* Remove from inode list and release lock. */
      list_remove (&inode->elem);
 
      /* Deallocate blocks if removed. */
      if (inode->removed) 
        {
          /************************ NEW CODE ***************************/
          size_t sectors = bytes_to_sectors (inode->data.length);
          size_t n_direct_blocks = sectors <= INODE_DIRECT_N ? 
                                    sectors : INODE_DIRECT_N;
          for (size_t i=0; i<n_direct_blocks; i++)
            {
              // make the direct blocks entries available to use
              free_map_release (inode->data.direct_blocks[i], 1);
            }

          if (sectors <= INODE_DIRECT_N)
            {
              // make inode sectors available to use
              free_map_release (inode->sector, 1);
              free (inode);
              return;
            }

          size_t n_indirect_blocks = sectors - INODE_DIRECT_N;
          block_sector_t *table = calloc(INODE_TABLE_LENGTH, 
                                          sizeof (block_sector_t*));
          ASSERT (table != NULL);

          for (size_t i=0; i<n_indirect_blocks; i+=INODE_TABLE_LENGTH)
            {
              size_t indirect_i = i / INODE_TABLE_LENGTH;
              // read indirect blocks back into table
              cache_read (inode->data.indirect_blocks[indirect_i], table);
              size_t n_table_entry = 
                (n_indirect_blocks-i) < INODE_TABLE_LENGTH ?
                (n_indirect_blocks-i) : INODE_TABLE_LENGTH;
              for (size_t j=0; j<n_table_entry; j+=1)
                {
                  free_map_release (table[j], 1);
                }
              // make indirect blocks available to use
              free_map_release (inode->data.indirect_blocks[indirect_i], 1);
            }
          free (table);
          free_map_release (inode->sector, 1);
          /*********************** END NEW CODE *************************/
          // free_map_release (inode->sector, 1);
          // free_map_release (inode->data.start,
          //                   bytes_to_sectors (inode->data.length)); 
        }
      free (inode); 
    }
}

/* Marks INODE to be deleted when it is closed by the last caller who
   has it open. */
void
inode_remove (struct inode *inode) 
{
  ASSERT (inode != NULL);
  inode->removed = true;
}

/* Reads SIZE bytes from INODE into BUFFER, starting at position OFFSET.
   Returns the number of bytes actually read, which may be less
   than SIZE if an error occurs or end of file is reached. */
off_t
inode_read_at (struct inode *inode, void *buffer_, off_t size, off_t offset) 
{
  uint8_t *buffer = buffer_;
  off_t bytes_read = 0;
  uint8_t *bounce = NULL;

  while (size > 0) 
    {
      /* Disk sector to read, starting byte offset within sector. */
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually copy out of this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Read full sector directly into caller's buffer. */
          /************************ NEW CODE ***************************/
          cache_read (sector_idx, buffer + bytes_read);
          /********************** END NEW CODE *************************/
        }
      else 
        {
          /* Read sector into bounce buffer, then partially copy
             into caller's buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }
      /************************ NEW CODE ***************************/
          cache_read (sector_idx, bounce);
      /********************** END NEW CODE *************************/
          memcpy (buffer + bytes_read, bounce + sector_ofs, chunk_size);
        }
      
      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_read += chunk_size;
    }
  free (bounce);

  return bytes_read;
}

/* Writes SIZE bytes from BUFFER into INODE, starting at OFFSET.
   Returns the number of bytes actually written, which may be
   less than SIZE if end of file is reached or an error occurs.
   (Normally a write at end of file would extend the inode, but
   growth is not yet implemented.) */
off_t
inode_write_at (struct inode *inode, const void *buffer_, off_t size,
                off_t offset) 
{
  const uint8_t *buffer = buffer_;
  off_t bytes_written = 0;
  uint8_t *bounce = NULL;

  if (inode->deny_write_cnt)
    return 0;
  byte_to_sector_write (inode, offset+size-1);
  while (size > 0) 
    {
      /* Sector to write, starting byte offset within sector. */
      // block_sector_t sector_idx = byte_to_sector_write (inode, offset);
      block_sector_t sector_idx = byte_to_sector (inode, offset);
      int sector_ofs = offset % BLOCK_SECTOR_SIZE;

      /* Bytes left in inode, bytes left in sector, lesser of the two. */
      off_t inode_left = inode_length (inode) - offset;
      int sector_left = BLOCK_SECTOR_SIZE - sector_ofs;
      int min_left = inode_left < sector_left ? inode_left : sector_left;

      /* Number of bytes to actually write into this sector. */
      int chunk_size = size < min_left ? size : min_left;
      if (chunk_size <= 0)
        break;

      if (sector_ofs == 0 && chunk_size == BLOCK_SECTOR_SIZE)
        {
          /* Write full sector directly to disk. */
          /************************ NEW CODE ***************************/
          cache_write (sector_idx, buffer + bytes_written);
          /********************** END NEW CODE **************************/
        }
      else 
        {
          /* We need a bounce buffer. */
          if (bounce == NULL) 
            {
              bounce = malloc (BLOCK_SECTOR_SIZE);
              if (bounce == NULL)
                break;
            }

          /* If the sector contains data before or after the chunk
             we're writing, then we need to read in the sector
             first.  Otherwise we start with a sector of all zeros. */
          if (sector_ofs > 0 || chunk_size < sector_left) 
            cache_read (sector_idx, bounce);
          else
            memset (bounce, 0, BLOCK_SECTOR_SIZE);
          memcpy (bounce + sector_ofs, buffer + bytes_written, chunk_size);
          cache_write (sector_idx, bounce);
        }

      /* Advance. */
      size -= chunk_size;
      offset += chunk_size;
      bytes_written += chunk_size;
    }
  free (bounce);

  return bytes_written;
}

/* Disables writes to INODE.
   May be called at most once per inode opener. */
void
inode_deny_write (struct inode *inode) 
{
  inode->deny_write_cnt++;
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
}

/* Re-enables writes to INODE.
   Must be called once by each inode opener who has called
   inode_deny_write() on the inode, before closing the inode. */
void
inode_allow_write (struct inode *inode) 
{
  ASSERT (inode->deny_write_cnt > 0);
  ASSERT (inode->deny_write_cnt <= inode->open_cnt);
  inode->deny_write_cnt--;
}

/* Returns the length, in bytes, of INODE's data. */
off_t
inode_length (const struct inode *inode)
{
  return inode->data.length;
}

/************************ NEW CODE ***************************/
/* Returns whether the inode is a diretory. */
bool 
inode_is_dir (const struct inode *inode)
{
  return inode->data.is_dir;
}

/* set the file associated with the inode a dir */
void 
inode_set_dir (struct inode * inode)
{
  inode->data.is_dir = true;
  cache_write (inode->sector, &inode->data);
}

/* count for the number of this inode currently being open */
int 
inode_open_count (struct inode * inode)
{
  return inode->open_cnt;
}
/********************** END NEW CODE *************************/
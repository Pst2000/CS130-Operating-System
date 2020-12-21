#ifndef FILESYS_FILESYS_H
#define FILESYS_FILESYS_H

#include <stdbool.h>
#include "filesys/off_t.h"

/* Sectors of system file inodes. */
#define FREE_MAP_SECTOR 0       /* Free map file inode sector. */
#define ROOT_DIR_SECTOR 1       /* Root directory file inode sector. */

/* Block device that contains the file system. */
extern struct block *fs_device;

void filesys_init (bool format);
void filesys_done (void);
bool filesys_create (const char *name, off_t initial_size);
struct file *filesys_open (const char *name);
bool filesys_remove (const char *name);

/************************ NEW CODE ***************************/
/* Changes the current working directory of the process to dir,
   which may be relative or absolute. Returns true if successful, 
   false on failure. */
bool filesys_chdir (const char *name);

/* Creates the directory named dir, which may be relative or absolute.
   Returns true if successful, false on failure. Fails if dir already 
   exists or if any directory name in dir, besides the last, does not 
   already exist. That is, mkdir("/a/b/c") succeeds only if /a/b already 
   exists and /a/b/c does not. */
bool filesys_mkdir (const char *name);
/********************** END NEW CODE *************************/

#endif /* filesys/filesys.h */

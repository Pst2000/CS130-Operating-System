#ifndef FILESYS_FILE_H
#define FILESYS_FILE_H

#include "filesys/off_t.h"
#include "threads/thread.h"

struct inode;

/* Opening and closing files. */
struct file *file_open (struct inode *);
struct file *file_reopen (struct file *);
void file_close (struct file *);
struct inode *file_get_inode (struct file *);

/* Reading and writing. */
off_t file_read (struct file *, void *, off_t);
off_t file_read_at (struct file *, void *, off_t size, off_t start);
off_t file_write (struct file *, const void *, off_t);
off_t file_write_at (struct file *, const void *, off_t size, off_t start);

/* Preventing writes. */
void file_deny_write (struct file *);
void file_allow_write (struct file *);

/* File position. */
void file_seek (struct file *, off_t);
off_t file_tell (struct file *);
off_t file_length (struct file *);

/************************ NEW CODE ***************************/
/* Reads a directory entry from file descriptor fd, which must represent 
  a directory. If successful, stores the null-terminated file name in name, 
  which must have room for READDIR_MAX_LEN + 1 bytes, and returns true. 
  If no entries are left in the directory, returns false. */
bool filesys_readdir (int fd, char *name);

/* Returns true if fd represents a directory, 
   false if it represents an ordinary file. */
bool filesys_isdir (int fd);

/* Returns the inode number of the inode associated with fd, 
   which may represent an ordinary file or a directory.
   An inode number persistently identifies a file or directory. 
   It is unique during the file's existence. In Pintos, the 
   sector number of the inode is suitable for use as an inode number. */
int filesys_inumber (int fd);
/********************** END NEW CODE *************************/
#endif /* filesys/file.h */

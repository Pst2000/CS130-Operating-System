#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "list.h"
#include "filesys/filesys.h"
#include "filesys/file.h"
#include "filesys/inode.h"

 /************************ NEW CODE ***************************/
#include "threads/vaddr.h"
#include "userprog/process.h"
// #include "file.h"
// #include "filesys.h"
 /********************* END NEW CODE **************************/

static void syscall_handler (struct intr_frame *);

/************************ NEW CODE ***************************/
// syscall_functions
void halt1 (void);
void exit1(int status);
tid_t exec1 (const char *cmd_line);
int wait1 (tid_t pid);
bool create1 (const char *file, unsigned initial_size);
bool remove1 (const char *file);
int open1 (const char *file);
int filesize1 (int fd);
int read1 (int fd, void *buffer, unsigned size);
int write1 (int fd, const void *buffer, unsigned size);
void seek1 (int fd, unsigned position);
unsigned tell1 (int fd);
void close1 (int fd);

#ifdef FILESYS
bool chdir1 (const char *dir);
bool mkdir1 (const char *dir);
bool readdir1 (int fd, char *name);
bool isdir1 (int fd);
int inumber1 (int fd);
#endif

// when we do operations on the file, acquire the lock to 
// ensure synchronization
struct lock file_lock;

 /********************* END NEW CODE *************************/

void
syscall_init (void) 
{
  lock_init (&file_lock);
  intr_register_int (0x30, 3, INTR_ON, syscall_handler, "syscall");
}

static void
syscall_handler (struct intr_frame *f UNUSED) 
{
  /************************ NEW CODE ***************************/
  struct thread * cur = thread_current();

  // need to check for whether each arguments of the interrupt frame
  // and make sure they are in the user memory as well as they are 
  // mapped correctly to the physical memory
  if (!is_user_vaddr(f->esp) || !pagedir_get_page(cur->pagedir, f->esp)
      || !is_user_vaddr((int*)f->esp+1) 
      || !pagedir_get_page(cur->pagedir, (int*)f->esp+1)
      || !is_user_vaddr((int*)f->esp+2) 
      || !pagedir_get_page(cur->pagedir, (int*)f->esp+2)
      || !is_user_vaddr((int*)f->esp+3) 
      || !pagedir_get_page(cur->pagedir, (int*)f->esp+3)){
    exit_wrong(-1);
  }

  int sys_code = *(int*)f->esp;
  switch (sys_code)
  {
    /* Terminates Pintos by calling shutdown_power_off() */
    case SYS_HALT:
    {
      halt1();
      break;
    }

    /* Terminates the current user program, returning status to the kernel */
    case SYS_EXIT:
    {
      int status = *((int*)f->esp + 1);
      exit1(status);
      break;
    }
    
    /* Runs the executable whose name is given in cmd_line, 
       passing any given arguments, and returns the new 
       process's program id (pid) */
    case SYS_EXEC:
    {
      char *cmd_line = (char *)(*((int*)f->esp + 1));
      for (char * p = cmd_line; ; p++)
      {
        if(!is_user_vaddr (p) || !pagedir_get_page (cur->pagedir, p))
          exit_wrong(-1);
        if (*p == '\0')
          break;
      }
      f->eax = exec1(cmd_line);
      break;
    }

    /* Waits for a child process pid and retrieves the child's exit status */
    case SYS_WAIT:
    {
      tid_t pid = (*((int*)f->esp + 1));
      f->eax = wait1(pid);
      break;
    }

    /* Creates a new file called file initially initial_size bytes in size.
       Returns true if successful, false otherwise.  */
    case SYS_CREATE:
    {
      char *file = (char *)(*((int*)f->esp + 1));
      unsigned initial_size = *((unsigned*)f->esp + 2);
      for (char * p = file; ; p++)
      {
        if(!is_user_vaddr (p) || !pagedir_get_page (cur->pagedir, p))
          exit_wrong(-1);
        if (*p == '\0')
          break;
      }
      lock_acquire (&file_lock);
      f->eax = create1(file, initial_size);
      lock_release (&file_lock);
      break;
    }

    /* Deletes the file called file. 
       Returns true if successful, false otherwise */
    case SYS_REMOVE:
    {
      char *file = (char *)(*((int*)f->esp + 1));
      for (char * p = file; ; p++)
      {
        if(!is_user_vaddr (p) || !pagedir_get_page (cur->pagedir, p))
          exit_wrong(-1);
        if (*p == '\0')
          break;
      }
      lock_acquire (&file_lock);
      f->eax = remove1(file);
      lock_release (&file_lock);
      break;
    }

    /* Opens the file called file. Returns file descriptor, 
       or -1 if the file could not be opened. */
    case SYS_OPEN:
    {
      char *file = (char *)(*((int*)f->esp + 1));

      for (char * p = file; ; p++)
      {
        if(!is_user_vaddr (p) || !pagedir_get_page (cur->pagedir, p))
          exit_wrong(-1);
        if (*p == '\0')
          break;
      }
      f->eax = open1(file);
      break;
    }

    /* Returns the size, in bytes, of the file open as fd. */
    case SYS_FILESIZE:
    {
      int fd = *((int*)f->esp + 1);
      f->eax = filesize1(fd);
      break;
    }

    /* Reads size bytes from the file open as fd into buffer. Returns the 
       number of bytes actually read (0 at end of file), or -1 if the 
       file could not be read (due to a condition other than end of file).
       Fd 0 reads from the keyboard using input_getc(). */
    case SYS_READ:
    {
      int fd = *((int*)f->esp + 1);
      void* buffer = (void*)(*((int*)f->esp + 2));
      unsigned size = *((unsigned*)f->esp + 3);

      // check for the validity of each address of buffer
      for (unsigned i=0; i<size; i++)
      {
        if(!is_user_vaddr (buffer+i) 
          || !pagedir_get_page (cur->pagedir, buffer+i))
          exit_wrong(-1);
      }
      f->eax = read1(fd, buffer, size);
      break;
    }

    /* Writes size bytes from buffer to the open file fd. Returns the 
       number of bytes actually written, which may be less than size 
       if some bytes could not be written. */
    case SYS_WRITE:
    {
      int fd = *((int*)f->esp + 1);
      void* buffer = (void*)(*((int*)f->esp + 2));
      unsigned size = *((unsigned*)f->esp + 3);

      // check for the validity of each address of buffer
      for (unsigned i=0; i<size; i++)
      {
        if(!is_user_vaddr (buffer+i) 
          || !pagedir_get_page (cur->pagedir, buffer+i))
          exit_wrong(-1);
      }
      lock_acquire (&file_lock);
      f->eax = write1(fd, buffer, size);
      lock_release (&file_lock);
      break;
    }

    /* Changes the next byte to be read or written in open file fd to 
       position, expressed in bytes from the beginning of the file. 
       (Thus, a position of 0 is the file's start.) */
    case SYS_SEEK:
    {
      int fd = *((int*)f->esp + 1);
      unsigned position = *((unsigned*)f->esp + 2);
      lock_acquire (&file_lock);
      seek1(fd, position);
      lock_release (&file_lock);
      break;
    }
    
    /* Returns the position of the next byte to be read or written 
       in open file fd, expressed in bytes from the beginning of the file. */
    case SYS_TELL:
    {
      int fd = *((int*)f->esp + 1);
      lock_acquire (&file_lock);
      f->eax = tell1(fd);
      lock_release (&file_lock);
      break;
    }

    /* Closes file descriptor fd. Exiting or terminating a process implicitly 
       closes all its open file descriptors, as if by calling this function 
       for each one. */
    case SYS_CLOSE:
    {
      int fd = *((int*)f->esp + 1);
      lock_acquire (&file_lock);
      close1(fd);
      lock_release (&file_lock);
      break;
    }

    #ifdef FILESYS
    /* Changes the current working directory of the process to dir, 
       which may be relative or absolute. Returns true if successful, 
       false on failure. */
    case SYS_CHDIR: 
    {
      char *dir = (char *)(*((int*)f->esp + 1));

      for (char * p = dir; ; p++)
      {
        if(!is_user_vaddr (p) || !pagedir_get_page (cur->pagedir, p))
          exit_wrong(-1);
        if (*p == '\0')
          break;
      }
      f->eax = chdir1(dir);
      break;
      }

    /* Creates the directory named dir, which may be relative or absolute. 
       Returns true if successful, false on failure. Fails if dir already 
       exists or if any directory name in dir, besides the last, 
       does not already exist. */
    case SYS_MKDIR:
    {
      char *dir = (char *)(*((int*)f->esp + 1));

      for (char * p = dir; ; p++)
      {
        if(!is_user_vaddr (p) || !pagedir_get_page (cur->pagedir, p))
          exit_wrong(-1);
        if (*p == '\0')
          break;
      }
      f->eax = mkdir1(dir);
      break;
      }

    /* Reads a directory entry from file descriptor fd, which must represent 
       a directory. If successful, stores the null-terminated file name in 
      name, which must have room for READDIR_MAX_LEN + 1 bytes, and returns 
      true. If no entries are left in the directory, returns false. */
    case SYS_READDIR: 
    {
      int fd = *((int*)f->esp + 1);
      char *name = (void*)(*((int*)f->esp + 2));

      for (char * p = name; ; p++)
      {
        if(!is_user_vaddr (p) || !pagedir_get_page (cur->pagedir, p))
          exit_wrong(-1);
        if (*p == '\0')
          break;
      }
      f->eax = readdir1(fd, name);
      break;
    }

    /* Returns true if fd represents a directory, 
       false if it represents an ordinary file. */
    case SYS_ISDIR:
    {
      int fd = *((int*)f->esp + 1);
      f->eax = isdir1(fd);
      break;
    }

    /* Returns the inode number of the inode associated with fd, 
       which may represent an ordinary file or a directory. */
    case SYS_INUMBER:
    {
      int fd = *((int*)f->esp + 1);
      f->eax = inumber1(fd);
      break;
    }

#endif

    default:
      exit_wrong(-1);
      break;
  }
}
  /************************ NEW CODE ***************************/

void
exit_wrong(int status){
  thread_current()->exit_code = status;
  thread_exit();
}

void halt1 (void){
  shutdown_power_off();
}

void exit1(int status){
  thread_current()->exit_code = status;
  thread_exit();
}

tid_t exec1 (const char *cmd_line){
  const char *file1 = (char*)*cmd_line; 
  if (file1 == NULL)
    return -1;
  else
  {
    char * fn_cp = malloc (strlen(cmd_line)+1);
   strlcpy(fn_cp, cmd_line, strlen(cmd_line)+1);
   char * save_ptr;
   fn_cp = strtok_r(fn_cp," ",&save_ptr);

    // check for whether 'com_line' is actually an exist file 
    struct dir *dir = dir_open_root ();
    struct inode *inode = NULL;
    if (dir != NULL)
    {
      if (dir_lookup (dir, fn_cp, &inode))
      {
        dir_close (dir);
        free (fn_cp);
        return process_execute(cmd_line);
      }
      else
      {
        dir_close (dir);
        free (fn_cp);
        return -1;
      }
    }
  }
    
}

int wait1 (tid_t pid){
  return process_wait(pid);
}

bool create1 (const char *file, unsigned initial_size){
  return filesys_create(file, initial_size);
}

bool remove1 (const char *file){
  return filesys_remove(file);
}

int open1 (const char *file){
  lock_acquire (&file_lock);
  struct file* fptr = filesys_open(file);
  lock_release (&file_lock);
  if (fptr == NULL)
    return -1;
  else
  // allocate the opened file node memory
  {
    struct file_node * f_node = 
        (struct file_node *) malloc (sizeof (struct file_node));
    int result = f_node -> fd = thread_current()->fd_num++;
    f_node -> file_ptr = fptr;
    if (inode_is_dir (file_get_inode (fptr)))
      f_node->dir_ptr = dir_open (file_get_inode (fptr));
    else
      f_node->dir_ptr = NULL;
    list_push_back(&thread_current()->files, &f_node -> elem);
    return (result);
  }
}

int filesize1 (int fd){
  struct file_node* f_node = search_fd(&thread_current()->files, fd, false);
  if(f_node != NULL)
  {
    lock_acquire (&file_lock);
    int result = file_length(f_node->file_ptr);
    lock_release (&file_lock);
    return result;
  }
  else
    return -1;
}

int read1 (int fd, void *buffer, unsigned size){
  // read the every single character into buffer
  if(fd == STDIN_FILENO)
  {
    for (int i = 0; i < size; i ++)
    {
      ((char*)buffer)[i] = input_getc();
    }
    return size;
  }
  else if(fd != STDOUT_FILENO)
  {
    struct file_node* f_node = search_fd(&thread_current()->files, fd, false);
    if(f_node != NULL)
      {
        lock_acquire (&file_lock);
        int result = file_read (f_node->file_ptr, buffer, size);
        lock_release (&file_lock);
        return result;
      }
  }
  return -1;
}

int write1 (int fd, const void *buffer, unsigned size){
  // print the content in buffer into console
  if(fd == STDOUT_FILENO)
  {
    putbuf(buffer, size);
    return size;
  }
  else if(fd != STDIN_FILENO)
  {
    struct file_node* f_node = 
      search_fd (&thread_current ()->files, fd, false);
    if(f_node != NULL)
      return file_write (f_node->file_ptr, buffer, size);
  }
  return -1;
}

void seek1 (int fd, unsigned position){
  struct file_node* f_node = search_fd(&thread_current()->files, fd, false);
  if (f_node == NULL)
    exit_wrong(-1);
  file_seek(f_node->file_ptr, position);
}

unsigned tell1 (int fd){
  struct file_node* f_node = search_fd(&thread_current()->files, fd, false);
  if (f_node == NULL)
    return -1;
  return file_tell(f_node->file_ptr);
}

void close1 (int fd){
  struct file_node* f_node = search_fd (&thread_current ()->files, fd, true);
  if (f_node == NULL)
    return -1;
  file_close (f_node -> file_ptr);
  list_remove (&f_node->elem);
  free (f_node);
}

#ifdef FILESYS
bool chdir1 (const char *dir){
  lock_acquire (&file_lock);
  bool ret = filesys_chdir (dir);
  lock_release (&file_lock);
  return ret;
}

bool mkdir1 (const char *dir){
  lock_acquire (&file_lock);
  bool ret = filesys_mkdir (dir);
  lock_release (&file_lock);
  return ret;
}

bool readdir1 (int fd, char *name){
  lock_acquire (&file_lock);
  bool ret = filesys_readdir (fd, name);
  lock_release (&file_lock);
  return ret;
}

bool isdir1 (int fd){
  lock_acquire (&file_lock);
  bool ret = filesys_isdir (fd);
  lock_release (&file_lock);
  return ret;
}

int inumber1 (int fd){
  lock_acquire (&file_lock);
  int ret = filesys_inumber (fd);
  lock_release (&file_lock);
  return ret;
}
#endif
#include "userprog/syscall.h"
#include <stdio.h>
#include <syscall-nr.h>
#include "threads/interrupt.h"
#include "threads/thread.h"
#include "list.h"

 /************************ NEW CODE ***************************/
#include "threads/vaddr.h"
#include "threads/pte.h"
#include "userprog/process.h"
#include "vm/frame.h"
#include "vm/page.h"
// #include "userprog/process.c"
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
mmapid_t mmap (int fd, void *addr);
void munmap (mmapid_t mid);
void mmf_free (struct mmf_node* mmf_ptr);

bool is_user_vaddr_valid (void* addr, struct intr_frame *f);

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
  // printf("syscall!!\n");
  // need to check for whether each arguments of the interrupt frame
  // and make sure they are in the user memory as well as they are 
  // mapped correctly to the physical memory
  // if (!is_user_vaddr(f->esp) || !pagedir_get_page(cur->pagedir, f->esp)
  //     || !is_user_vaddr((int*)f->esp+1) 
  //     || !pagedir_get_page(cur->pagedir, (int*)f->esp+1)
  //     || !is_user_vaddr((int*)f->esp+2) 
  //     || !pagedir_get_page(cur->pagedir, (int*)f->esp+2)
  //     || !is_user_vaddr((int*)f->esp+3) 
  //     || !pagedir_get_page(cur->pagedir, (int*)f->esp+3)){
  //   printf("enter here!!!\n");
  //   exit_wrong(-1);
  // }
  cur->cur_esp = f->esp;
  if (!is_user_vaddr_valid(f->esp, f) 
      || !is_user_vaddr_valid((int*)f->esp+1, f)
      || !is_user_vaddr_valid((int*)f->esp+2, f) 
      || !is_user_vaddr_valid((int*)f->esp+3, f)){
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
        if(is_user_vaddr_valid (buffer+i, f))
        {
          uint32_t * pte = lookup_page (cur->pagedir, buffer+i, false);
          if (!(*pte & PTE_W))
            exit_wrong(-1);
        }
        else
        {
          exit_wrong(-1);
        }
      }
      // In sys_read, ready to read
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

    /* Maps the file open as fd into the process's virtual 
    address space. The entire file is mapped into consecutive 
    virtual pages starting at addr.*/
    case SYS_MMAP:
    {
      int fd = *((int*)f->esp + 1);
      void* addr = (void*)(*((int*)f->esp + 2));
      lock_acquire (&file_lock);
      f->eax = mmap1(fd, addr);
      lock_release (&file_lock);
      break;
    }

    /* Unmaps the mapping designated by mapping, which must 
    be a mapping ID returned by a previous call to mmap by 
    the same process that has not yet been unmapped. */
    case SYS_MUNMAP:
    {
      int mid = *((int*)f->esp + 1);
      lock_acquire (&file_lock);
      munmap1(mid);
      lock_release (&file_lock);
      break;
    }

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
  if (fptr == NULL){
    lock_release (&file_lock);
    return -1;
  }
  else
  // allocate the opened file node memory
  {
    struct file_node * f_node = 
        (struct file_node *) malloc (sizeof (struct file_node));
    int result = f_node -> fd = thread_current()->fd_num++;
    f_node -> file_ptr = fptr;
    list_push_back(&thread_current()->files, &f_node -> elem);
    lock_release (&file_lock);
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
    {
      int result = file_write (f_node->file_ptr, buffer, size);
      return result;
    }
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

bool is_user_vaddr_valid (void* addr, struct intr_frame *f){
  if (addr == NULL || !is_user_vaddr(addr) || addr < 0x8048000)
    return false;
  struct thread * cur = thread_current ();
  bool success = pagedir_get_page (cur->pagedir, addr);
  if (!success){
    if (addr > f->esp || addr == f->esp-4 || addr == f->esp-32){
      // grow stack!
      void *upage = pg_round_down (addr);
      if (sup_pte_lookup (&cur->sup_pt, upage) != NULL)
        return true;
      void *kpage = get_free_frame (upage);
      success = pagedir_set_page (cur->pagedir, upage, kpage, true);
    }
    else{
      struct sup_page_table_entry *spte;
      void *fault_page = pg_round_down (addr);
	    spte = sup_pte_lookup (&cur->sup_pt, fault_page);
      if (spte != NULL)
        return sup_load_page (&cur->sup_pt, cur->pagedir, fault_page);
      else
        return false;
    }
  }
  return success;
}

mmapid_t
mmap1 (int fd, void *addr)
{
  struct file_node *f_node;
  struct thread *curr = thread_current ();
  struct file *f = NULL;

  if (addr == NULL || (pg_ofs (addr) != 0) || fd == 0 || fd == 1)
    return -1;

  f_node = search_fd (&curr->files, fd, false);
  if (f_node == NULL)
    return -1;
  else
  {
    f = file_reopen(f_node->file_ptr);
  }

  if (file_length (f_node->file_ptr) <= 0)
    return -1;

  int ofs;
  int f_len = file_length (f_node->file_ptr);
  for (ofs = 0; ofs < f_len; ofs += PGSIZE){
    void *upage = pg_round_down(addr + ofs);
    if (sup_pte_lookup 
      (&curr->sup_pt, upage) || pagedir_get_page(curr->pagedir, upage))
    {
      return -1;
    }
    // !!! we can't combine the following part here!
  }
  // traverse the file pages and set them to supt sequentially
  for (ofs = 0; ofs < f_len; ofs += PGSIZE){
    void *page_num = addr + ofs;
    size_t read_bytes = (ofs + PGSIZE < f_len ? PGSIZE : f_len - ofs);
    size_t zero_bytes = PGSIZE - read_bytes;

    supt_set_page_from_MMF 
      (&curr->sup_pt, page_num, f, ofs, read_bytes, zero_bytes);
  }
  int f_len_new = file_length (f_node->file_ptr);
  // create the corresponding memory mapped file
  mmapid_t result = mmf_create (addr, f, f_len_new);
  return (f == NULL) ? -1 : result;
}

void
munmap1 (mmapid_t mid)
{
  struct thread *curr = thread_current ();
  struct list_elem *e;
  struct list_elem *el;
  struct list *mmfiles = &curr->mmf_list;
  if (list_empty (mmfiles)){
    exit_wrong(-1);
  }

  // find the mmfile in mmf_list according to the mid!
  for (e = list_begin (mmfiles); e != list_end (mmfiles); e = list_next (e))
  {
    struct mmf_node *mmfile = list_entry (e, struct mmf_node, elem);
    if(mmfile->mid == mid)
    {
      // delete the file from mmf_list
      el = list_remove (&mmfile->elem);
      if (el != NULL)
      {
        struct mmf_node *mmf_ptr = list_entry (e, struct mmf_node, elem);
        mmf_free (mmf_ptr);
      }
    }
  }
}

void
mmf_free (struct mmf_node* mmf_ptr)
{
  struct thread *curr = thread_current ();
  struct hash_elem *he;
  struct sup_page_table_entry spte;
  struct sup_page_table_entry *spte_ptr;

  // traverse the pages, delete each entry iteratively
  for (int ofs = 0; ofs != mmf_ptr->page_num; ofs ++)
  {
    spte.upage = mmf_ptr->addr + ofs*PGSIZE;
    he = hash_delete (&curr->sup_pt, &spte.hash_elem);
    if (he != NULL)
    {
      spte_ptr = hash_entry (he, struct sup_page_table_entry, hash_elem);
      // if dirty: the file has been writen
      if (spte_ptr->status == ON_FRAME && 
          pagedir_is_dirty (curr->pagedir, spte_ptr->upage))
      {
        // write it back from upage to the original file
        file_seek (spte_ptr->file, spte_ptr->file_offset);
        file_write (spte_ptr->file, spte_ptr->upage, spte_ptr->read_bytes);
      }
      free (spte_ptr);
    }
  }
  // close the file !!!
  file_close (mmf_ptr->file_ptr);
}

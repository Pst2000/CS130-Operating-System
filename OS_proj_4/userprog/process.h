#ifndef USERPROG_PROCESS_H
#define USERPROG_PROCESS_H

#include "threads/thread.h"

/************************ NEW CODE ***************************/
#include "threads/synch.h"
// /* Thread identifier type.
//    You can redefine this to whatever type you like. */
// typedef int tid_t;

// the current executing thread's information
struct exce_file_info
{
    void * file_name;
    struct semaphore load_sema;
    bool success;
};

/********************** END NEW CODE *************************/

tid_t process_execute (const char *file_name);
int process_wait (tid_t);
void process_exit (void);
void process_activate (void);

#endif /* userprog/process.h */

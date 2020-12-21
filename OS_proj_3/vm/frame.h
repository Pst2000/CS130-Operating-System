#ifndef VM_FRAME_H
#define VM_FRAME_H

#include <debug.h>
#include <list.h>
#include <stdint.h>
#include "threads/thread.h"

struct frame_table_entry{
    // physical user address
    void * kpage;
    // virtual user address
    void * upage;
    // which thread this frame belongs to 
    struct thread * owner;
    struct list_elem elem;
};

// similar to pagedir_set_page()
struct frame_table_entry *frame_table_set_page 
    (void *kpage, void *upage, struct frame_table_entry *frame);

// If full, choose a frame to evict
struct frame_table_entry *frame_to_evict (void *upage);

// get a free frame to store UPAGE. If full, call frame_to_evict() to evict 
void * get_free_frame (void *upage);

// initialization
void init_frame_table ();

#endif
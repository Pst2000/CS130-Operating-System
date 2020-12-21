#include "vm/frame.h"
#include "vm/swap.h"
#include "vm/page.h"
#include "userprog/pagedir.h"
#include "threads/thread.h"
#include "threads/palloc.h"
#include "threads/malloc.h"

static struct list frame_table;
static struct lock frame_lock;

/*  Set up a frame. If parameter frame is NULL, malloc a new frame  */
struct frame_table_entry * 
frame_table_set_page (void *kpage, void *upage, 
    struct frame_table_entry *frame)
{
    lock_acquire (&frame_lock);
    if (frame == NULL){
        frame = malloc (sizeof (struct frame_table_entry));
        ASSERT (frame != NULL);
    }
    frame->kpage = kpage;
    frame->upage = upage;
    frame->owner = thread_current();
    list_push_front (&frame_table, &frame->elem);
    lock_release (&frame_lock);

    return frame;
}


/*  Use clock to find the proper frame to be evict. */
struct frame_table_entry *
frame_to_evict (void *upage)
{
    // naively delete the last entry
    struct frame_table_entry *frame = 
        list_entry(list_back (&frame_table), struct frame_table_entry, elem);
    return frame;
}


/*  Get a new user frame from user pool. 
    If there is no free frame, evict an old frame. */
void *
get_free_frame (void *upage)
{
    // lock_acquire (&frame_lock);
    
    bool success = true;
    void *kpage = (void*)palloc_get_page (PAL_ZERO | PAL_USER);
    struct frame_table_entry *frame = NULL;
    struct thread *cur = thread_current ();
    // if current frame is full, choose one to evict
    if (kpage == NULL)
    {
        
        // palloc() returns NULL, means we need to evict some frame
        frame = frame_to_evict (upage);
        kpage = frame->kpage;
        ASSERT (frame != NULL);
        
        struct sup_page_table_entry *spte;
        spte = sup_pte_lookup(&frame->owner->sup_pt, frame->upage);
        ASSERT (spte != NULL); 
        ASSERT (spte->status == ON_FRAME);

        // originally, where does this page come from?
        switch (spte->origin_status)
        {
            case IN_SWAP:
                // size_t swap_index = swap_out (frame->kpage);
                set_sup_swap (spte, swap_out (frame->kpage));
                spte->status = IN_SWAP;
                break;
            case IN_FILESYS:
                set_sup_swap (spte, swap_out (frame->kpage));
                spte->status = IN_SWAP;
                spte->origin_status = IN_SWAP;
                break;
            case IN_MMF:
                // dirty if the file in page has been writen
                if (pagedir_is_dirty (frame->owner->pagedir, spte->upage))
                {
                    // write it back
                    file_seek (spte->file, spte->file_offset);
                    file_write (spte->file, spte->upage, spte->read_bytes);
                }
                spte->status = IN_MMF;
                break;
            default:
                break;
        }
        // store the info about the frame into sup_pt
        
        // clear the kpage
        pagedir_clear_page (frame->owner->pagedir, frame->upage);
        palloc_free_page (kpage);
        list_remove (&frame->elem);

        // get this new page
        kpage = palloc_get_page (PAL_ZERO | PAL_USER);
        ASSERT (kpage != NULL);
    }
    // if we can't find the upage in sup_pt: set it!
    if (sup_pte_lookup(&cur->sup_pt, upage) == NULL){
        supt_set_page (&cur->sup_pt, upage);
    }
    // lock_release (&frame_lock);
    frame = frame_table_set_page (kpage, upage, frame);
    
    return kpage;
}

void init_frame_table ()
{
    lock_init (&frame_lock);
    list_init (&frame_table);
}

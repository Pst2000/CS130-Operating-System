#include <stdint.h>
#include <hash.h>
#include <stdbool.h>
#include "filesys/off_t.h"

enum page_status {
  ALL_ZERO,
  ON_FRAME,
  IN_SWAP,
  IN_FILESYS,
  IN_MMF
};

struct sup_page_table_entry
{
    // user virtual address
    void * upage;
    // physical virtual address
    void * kpage;
    // if the page entry is swaped into disk, we need to store the index
    size_t swap_index;
    // which thread keeps this supplementary page table
    struct thread * owner;
    // where the page is currently
    enum page_status status;
    enum page_status origin_status;
    struct hash_elem hash_elem;

    // for filesys
    struct file *file;
    // file page offset
    off_t file_offset;
    // bytes within one page
    uint32_t read_bytes, zero_bytes;
    bool writable;
};

// hash function for sup_pt
unsigned sup_pte_hash (const struct hash_elem *e, void *aux);

// hash compare
bool sup_pte_less 
    (const struct hash_elem *a_, const struct hash_elem *b_, void *aux);

// return the spte at ADDRESS in SUP_TABLE, NULL if not found
struct sup_page_table_entry *sup_pte_lookup 
    (struct hash *sup_table, const void *address);

// set for UPAGE in SUP_TABLE, return the new spte
struct sup_page_table_entry *supt_set_page 
    (struct hash *sup_table, void * upage);

// If the page going to set is in file system. Used for lazy load
struct sup_page_table_entry *supt_set_page_from_filesys 
    (struct hash *sup_table, void *upage, struct file * file,
     off_t offset, uint32_t read_bytes, uint32_t zero_bytes, bool writable);

// If the page going to set is an memory mapped file. Used for mmap
struct sup_page_table_entry *supt_set_page_from_MMF 
    (struct hash *sup_table, void *upage, struct file * file, 
     off_t offset, uint32_t read_bytes, uint32_t zero_bytes);

// when we have to evict a frame into swap, store its data!
void set_sup_swap (struct sup_page_table_entry *spte, size_t swap_index);

// remove SPTE from SUP_TABLE
void remove_spte (struct hash *sup_table, struct sup_page_table_entry *spte);

// load page from SUP_TABLE
bool sup_load_page (struct hash *sup_table, uint32_t *pd, void *uaddr);

// If the page is in file system, used for case IN FILESYS in sup_load_page
bool sup_load_page_from_filesys (struct hash *sup_table, 
    struct sup_page_table_entry *spte, void *kpage);

// If the page is a memory mapped file, used for case IN MMF in sup_load_page
bool sup_load_page_from_mmf (struct hash *sup_table, 
    struct sup_page_table_entry *spte, void *kpage);
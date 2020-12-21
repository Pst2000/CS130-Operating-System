#include "vm/page.h"
#include "threads/thread.h"
#include "vm/swap.h"
#include "threads/malloc.h"

unsigned
sup_pte_hash (const struct hash_elem *e, void *aux UNUSED)
{
  const struct sup_page_table_entry *p = 
    hash_entry (e, struct sup_page_table_entry, hash_elem);
  return hash_bytes (&p->upage, sizeof(p->upage));
}

/* Returns true if page a precedes page b. */
bool
sup_pte_less (const struct hash_elem *a_, const struct hash_elem *b_,
           void *aux UNUSED)
{
  const struct sup_page_table_entry *a = 
    hash_entry (a_, struct sup_page_table_entry, hash_elem);
  const struct sup_page_table_entry *b = 
    hash_entry (b_, struct sup_page_table_entry, hash_elem);

  return a->upage < b->upage;
}

/* Returns the page containing the given virtual address,
   or a null pointer if no such page exists. */
struct sup_page_table_entry *
sup_pte_lookup (struct hash *sup_table, const void *address)
{
  struct sup_page_table_entry p;
  struct hash_elem *e;

  p.upage = address;
  e = hash_find (sup_table, &p.hash_elem);
  // printf ("in lookup: %d\n", e);
  return e != NULL ? 
    hash_entry (e, struct sup_page_table_entry, hash_elem) : NULL;
}

/* Create and set up a new supplementary page table entry */
struct sup_page_table_entry *
supt_set_page (struct hash *sup_table, void * upage)
{
  struct sup_page_table_entry * spte = 
    malloc (sizeof (struct sup_page_table_entry));
  struct hash_elem *result;
  ASSERT (spte != NULL);
  
  spte->status = ON_FRAME;
  spte->origin_status = IN_SWAP;
  spte->upage = upage;
  result = hash_insert (sup_table, &spte->hash_elem);
  if (result != NULL){
    return false;
  }
  size_t hashsize = hash_size(sup_table);

  return spte;
}

/* Create and set up a new supplementary page table entry */
struct sup_page_table_entry *
supt_set_page_from_filesys (struct hash *sup_table, void *upage, 
  struct file * file, off_t offset, uint32_t read_bytes, 
  uint32_t zero_bytes, bool writable)
{
  struct sup_page_table_entry * spte = 
    malloc (sizeof (struct sup_page_table_entry));
  struct hash_elem *result;
  ASSERT (spte != NULL);

  spte->upage = upage;
  spte->kpage = NULL;
  spte->status = IN_FILESYS;
  spte->origin_status = IN_FILESYS;
  spte->file = file;
  spte->file_offset = offset;
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  spte->writable = writable;

  // insert this spte into sup_table
  result = hash_insert (sup_table, &spte->hash_elem);
  if (result != NULL){
    // In supt_set_page_from_filesys, result != NULL
    return false;
  }
  size_t hashsize = hash_size(sup_table);

  return spte;
}

/* Create and set up a new supplementary page table entry */
struct sup_page_table_entry *
supt_set_page_from_MMF (struct hash *sup_table, void *upage,
    struct file * file, off_t offset, uint32_t read_bytes, 
    uint32_t zero_bytes)
{
  struct sup_page_table_entry * spte = 
    malloc (sizeof (struct sup_page_table_entry));
  struct hash_elem *result;
  ASSERT (spte != NULL);

  spte->upage = upage;
  spte->kpage = NULL;
  spte->status = IN_MMF;
  spte->origin_status = IN_MMF;
  spte->file = file;
  spte->file_offset = offset;
  spte->read_bytes = read_bytes;
  spte->zero_bytes = zero_bytes;
  spte->writable = true;

  // insert this spte into sup_table
  result = hash_insert (sup_table, &spte->hash_elem);
  if (result != NULL){
    // In supt_set_page_from_filesys, result != NULL
    return false;
  }
  size_t hashsize = hash_size(sup_table);

  return spte;
}

bool 
sup_load_page (struct hash *sup_table, uint32_t *pd, void *upage)
{
  struct sup_page_table_entry * spte = sup_pte_lookup (sup_table, upage);
  if (spte == NULL){
    // In sup_load_page: sup_pte_lookup return NULL
    return false;
  }
  void * kpage = NULL;
  kpage = get_free_frame (upage); // must return true!
  bool writable = true;
  switch (spte->status)
  {
    case ALL_ZERO:
      memset (kpage, 0, PGSIZE);
      break;
    case ON_FRAME:
      // Already in frame!
      break;
    case IN_SWAP:
      // Loading page from swap!
      swap_in (spte->swap_index, kpage);
      break;
    case IN_FILESYS:
      // Loading page from filesys!
      if (!sup_load_page_from_filesys (sup_table, spte, kpage))
      {
        // sup_load_page_from_filesys return false!
      }
      writable = spte->writable;
      break;
    case IN_MMF:
      if (!sup_load_page_from_mmf (sup_table, spte, kpage))
      {
        // sup_load_page_from_mmf return false!
      }
      break;
    default:
      break;
  }

  if(!pagedir_set_page (pd, upage, kpage, writable)) {
    return false;
  }
  spte->kpage = kpage;
  spte->status = ON_FRAME;

  return true;
}

bool
sup_load_page_from_filesys (struct hash *sup_table, 
  struct sup_page_table_entry *spte, void *kpage)
{
  file_seek (spte->file, spte->file_offset);

  uint32_t bytes_read = file_read (spte->file, kpage, spte->read_bytes);
  ASSERT (bytes_read == spte->read_bytes);
  ASSERT (spte->read_bytes + spte->zero_bytes == PGSIZE);

  memset (kpage + bytes_read, 0, spte->zero_bytes);
  // In sup_load_page_from_filesys, return True!
  return true;
}

bool
sup_load_page_from_mmf (struct hash *sup_table, 
  struct sup_page_table_entry *spte, void *kpage)
{
  // similar as filesys
  file_seek (spte->file, spte->file_offset);

  uint32_t bytes_read = file_read (spte->file, kpage, spte->read_bytes);
  ASSERT (bytes_read == spte->read_bytes);
  ASSERT (spte->read_bytes + spte->zero_bytes == PGSIZE);

  memset (kpage + bytes_read, 0, spte->zero_bytes);
  // In sup_load_page_from_filesys, return True!
  return true;
}

void 
set_sup_swap (struct sup_page_table_entry *spte, size_t swap_index)
{
  spte->swap_index = swap_index;
  spte->status = IN_SWAP;
  spte->kpage = NULL;
}

void 
remove_spte (struct hash *sup_table, struct sup_page_table_entry *spte)
{
  hash_delete (sup_table, &spte->hash_elem);
  free (spte);
}
#include <stdbool.h>
#include <stdio.h>
#include "devices/block.h"
#include "threads/vaddr.h"
#include "lib/kernel/bitmap.h"

// initialization swap
void swap_init (void);

// get an entry back to KPAGE from swap according to SWAP_INDEX
void swap_in (size_t swap_index, void *kpage);

// return the page at KPAGE back to disk
size_t swap_out (void *kpage);
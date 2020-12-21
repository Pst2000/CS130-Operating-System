#include "swap.h"

struct block *swap_device;
struct bitmap *swap_bitmap;

/* 4096 / 512 = 8 */
const size_t BLOCKS_PER_PAGE = PGSIZE / BLOCK_SECTOR_SIZE;

void
swap_init (void)
{
    // get the block used as swap
    swap_device = block_get_role (BLOCK_SWAP);
    ASSERT (swap_device != NULL);

    static size_t swap_size;
    // calculate the size of swap, and create the corresponding bitmap
    swap_size = block_size(swap_device) / BLOCKS_PER_PAGE;
    swap_bitmap = bitmap_create(swap_size);
    ASSERT (swap_bitmap != NULL);

    // set all entries to true: all empty
    bitmap_set_all(swap_bitmap, true);
}

void
swap_in (size_t swap_index, void *kpage)
{
    // iteratively read in blocks in a page
    for (int i = 0; i < BLOCKS_PER_PAGE; ++i)
    {
        block_read (swap_device, swap_index*BLOCKS_PER_PAGE + i, 
                    kpage + i*BLOCK_SECTOR_SIZE);
    }
    bitmap_set (swap_bitmap, swap_index, true);
}

size_t
swap_out (void *kpage)
{
    // search for empty swaps
    size_t swap_index = bitmap_scan (swap_bitmap, 0, 1, true);
    ASSERT (swap_index != BITMAP_ERROR);

    // iteratively write KPAGE into swap
    for (int i = 0; i < BLOCKS_PER_PAGE; ++i)
    {
        block_write (swap_device, swap_index*BLOCKS_PER_PAGE + i,
                     kpage + i*BLOCK_SECTOR_SIZE);
    }
    // set to false: these swaps are not empty
    bitmap_set (swap_bitmap, swap_index, false);
    return swap_index;
}
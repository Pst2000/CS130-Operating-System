#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/synch.h"
#include "string.h"
#include <stdio.h>
#include "threads/thread.h"
#include "devices/timer.h"
#include "threads/malloc.h"

/* the struct of cache entry */
struct cache_sector
{
    // cache buffer
    unsigned char buffer[BLOCK_SECTOR_SIZE];
    // lock for EACH cache sector
    struct lock cache_lock;
    // current cache's sector index
    block_sector_t sector_id;
    // how many time this cache is accessed
    int accessed;
    // dirty bit
    bool dirty;
    // whether it's used
    bool used;
};

struct read_ahead_sector
{
    block_sector_t sector_id;
    struct list_elem read_ahead_elem;
};

/* the cache, which is an array of struct cache_sector */
static struct cache_sector cache[64]; 

// lock used for the whole cache operations
static struct lock cache_big_lock;

// list used for storing the next block of data
static struct list read_ahead_list;

// lock used for read ahead
static struct lock read_ahead_lock;

// condition variable used for waiting read_ahead_list
static struct condition read_ahead_condition;

// current cache pointed. Used for clock algorithm
int cache_cur;

/*  The function used to initialize the whole 
    cache at the beginning of the system.   */
void 
cache_init ()
{
    lock_init(&cache_big_lock);
    cache_cur = -1;
    for (int i = 0; i < 64; i ++){
        memset (cache[i].buffer, 0, BLOCK_SECTOR_SIZE);
        lock_init (&cache[i].cache_lock);
        cache[i].sector_id = 0;
        cache[i].accessed = 0;
        cache[i].dirty = false;
        cache[i].used = false;
    }

    // lock_init (&read_ahead_lock);
    // cond_init (&read_ahead_condition);
    // list_init (&read_ahead_list);
    // read_ahead();

    write_behind();
}

void cache_read (block_sector_t sector_id, void *buffer)
{
    // read_ahead, remain to be fixed
    /*
    lock_acquire (&read_ahead_lock);
    struct read_ahead_sector *ras = 
             malloc (sizeof (struct read_ahead_sector));
    ras->sector_id = sector_id + 1;
    // if (ras->sector_id < 16) {
        list_push_back (&read_ahead_list, &ras->read_ahead_elem);
    // }
    // else {
        // free (ras);
    // }
    cond_signal (&read_ahead_condition, &read_ahead_lock);
    lock_release (&read_ahead_lock);
    */
    
    lock_acquire(&cache_big_lock);
    int cache_id = find_sector (sector_id);
    if (cache_id != -1){
        // sector_id is in cache currently!
        // read block from cache
        memcpy (buffer, cache[cache_id].buffer, BLOCK_SECTOR_SIZE);
    }
    else {
        // not found this sector in cache: fetch it from disk
        int cache_id = fetch_free_cache ();
        cache[cache_id].sector_id = sector_id;
        cache[cache_id].dirty == false;
        cache[cache_id].used = true;
        cache[cache_id].accessed = 1;
        // read block into cache
        block_read (fs_device, sector_id, cache[cache_id].buffer);
        // read block from cache
        memcpy (buffer, cache[cache_id].buffer, BLOCK_SECTOR_SIZE);
    }
    lock_release(&cache_big_lock);
    // block_read (fs_device, sector_id, buffer);
}

void cache_write (block_sector_t sector_id, void *buffer)
{
    lock_acquire(&cache_big_lock);
    int cache_id = find_sector (sector_id);
    if (cache_id != -1){
        // sector_id is in cache currently!
        increase_accessed(cache_id);
        // write buffer to cache
        memcpy (cache[cache_id].buffer, buffer, BLOCK_SECTOR_SIZE);
        cache[cache_id].dirty = true;
    }
    else {
        // not found this sector in cache: fetch it from disk
        int cache_id = fetch_free_cache ();
        // question: why can't I acquire that lock ?!
        cache[cache_id].sector_id = sector_id;
        cache[cache_id].used = true;
        cache[cache_id].dirty = true;
        cache[cache_id].accessed = 1;
        // write buffer to cache
        memcpy (cache[cache_id].buffer, buffer, BLOCK_SECTOR_SIZE);
    }
    lock_release(&cache_big_lock);
    block_write (fs_device, sector_id, buffer);
}

void cache_back_to_disk ()
{
    lock_acquire(&cache_big_lock);
    for (int i = 0; i < 64; ++i) {
        if (cache[i].used == true && cache[i].dirty == true){
            // write back all the dirty caches
            block_write (fs_device, cache[i].sector_id, cache[i].buffer);
            cache[i].used = false;
            cache[i].dirty = false;
        }
    }
    lock_release(&cache_big_lock);
}

int find_sector (block_sector_t sector_id)
{
    for (int i = 0; i < 64; i ++){
        if (cache[i].used == true && cache[i].sector_id == sector_id){
            return i;
        }
    }
    // not found
    return -1;
}

int fetch_free_cache ()
{
    // use clock to find a cache entry to evict
    while (true)
    {
        if (cache[cache_cur].used == false) {
            int temp = cache_cur;
            cache_cur = (cache_cur + 1) % 64;
            return temp;
        }
        if (cache[cache_cur].accessed == 0){
            if (cache[cache_cur].dirty == true)
                block_write (fs_device, cache[cache_cur].sector_id, 
                                cache[cache_cur].buffer);
            cache[cache_cur].used = false;
            int temp = cache_cur;
            cache_cur = (cache_cur + 1) % 64;
            return temp;
        }
        cache[cache_cur].accessed = 0;
        cache_cur = (cache_cur + 1) % 64;
    }
}

void increase_accessed (int cache_operating)
{
    for (int i = 0; i < 64; i ++){
        if (cache[i].used == true)
            cache[i].accessed += 1;
    }
    // refresh the cache we are currently reading or writing
    cache[cache_operating].accessed = 0;
}

/* advanced */

void write_behind ()
{
    thread_create ("write_behind_t", PRI_DEFAULT, write_behind_func, NULL);
}

void write_behind_func ()
{
    while (true){
        // flush back every 0.5s
        timer_msleep (500);
        cache_back_to_disk ();
    }
}

void read_ahead ()
{
    // thread_create ("read_ahead_t", PRI_MIN, read_ahead_func, NULL);
}

/*
void read_ahead_func (){
    while (true){
        lock_acquire (&read_ahead_lock);
        while (list_empty (&read_ahead_list)){
            cond_wait (&read_ahead_condition, &read_ahead_lock);
        }
        struct read_ahead_sector *ra_block = 
            list_entry (list_pop_front (&read_ahead_list),
             struct read_ahead_sector, read_ahead_elem);
        block_sector_t sector_id = ra_block->sector_id;
        lock_release (&read_ahead_lock);

        // lock_acquire (&cache_big_lock);
        int cache_id = find_sector (sector_id);
        if (cache_id != -1){
            // the next sector is already in the cache
            // we don't need to do anything
        }
        else
        {
            int cache_id = fetch_free_cache ();
            // lock_acquire(&cache[cache_id].cache_lock);
            cache[cache_id].sector_id = sector_id;
            cache[cache_id].dirty == false;
            cache[cache_id].used = true;
            cache[cache_id].accessed = 1;
            // read block into cache
            block_read (fs_device, sector_id, cache[cache_id].buffer);
            // no need to read block from cache !
            // lock_release(&cache[cache_id].cache_lock);
        }
        // lock_release (&cache_big_lock);
        free (ra_block);
    }
}
*/
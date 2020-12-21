#include "devices/block.h"

// init cache
void cache_init ();

// find and read the cache corresponding to SECTOR_ID to BUFFER
void cache_read (block_sector_t sector_id, void *buffer);

// find and write the BUFFER to cache corresponding to SECTOR_ID
void cache_write (block_sector_t sector_id, void *buffer);

// flush all cache back to disk
void cache_back_to_disk ();

// find the cache index corresponding to SECTOR_ID
int find_sector (block_sector_t sector_id);

// get free cache. Evict if necessary
int fetch_free_cache ();

// increase accessed number of all cache except the cache being operating.
// Used for LRU.
void increase_accessed (int cache_operating);

// thread function for write-behind
void write_behind_func ();

// periodically write back all the cache into disk
void write_behind ();

// thread function used for read_ahead
void read_ahead_func ();

/* automatically fetch the next block of a file into the cache when 
   one block of a file is read, in case that block is about to be read */
void read_ahead ();
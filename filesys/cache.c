#include <string.h>
#include "filesys/cache.h"
#include "filesys/filesys.h"
#include "threads/synch.h"

#define CACHE_SIZE 64

struct cache_entry
  {
    block_sector_t disk_sector;
    uint8_t buffer[BLOCK_SECTOR_SIZE];
    bool filled;        /* If cache entry is filled. */
    bool is_dirty;
    bool flag;          /* Clock algorithm. */
  };

/* Global lock for cache. */
static struct lock cache_lock;
static struct cache_entry cache[CACHE_SIZE];

static struct cache_entry *cache_acquare (void);
static void cache_flush (struct cache_entry *entry);
static struct cache_entry *get_cache (block_sector_t sector);

void
cache_init (void)
{
  lock_init (&cache_lock);
  for (size_t i = 0; i < CACHE_SIZE; i++)
    cache[i].filled = false;
}


/* Return the cache entry */
static struct cache_entry *
get_cache (block_sector_t sector)
{
  for (size_t i = 0; i < CACHE_SIZE; i++)
    {
      if (!cache[i].filled)
        continue;
      if (cache[i].disk_sector == sector)
        return &cache[i];
    }
  return NULL;
}

/* Return a free cache entry. An empty one or evict one */

static struct cache_entry *
cache_acquare (void)
{
  static size_t clock = 0;
  while (true)
    {
      if (!cache[clock].filled)
        return &cache[clock];
      if (cache[clock].flag)
        cache[clock].flag = false;
      else
        break;
      clock++;
      clock %= CACHE_SIZE;
    }

  struct cache_entry *flush = &cache[clock];
  if (flush->is_dirty)
    cache_flush (flush);
  flush->filled = false;
  return flush;
}

/* Flushing back the cache entry into disk. */
static void
cache_flush (struct cache_entry *entry)
{
  if (entry->is_dirty)
    {
      block_write (fs_device, entry->disk_sector, entry->buffer);
      entry->is_dirty = false;
    }
}

void
cache_read (block_sector_t sector, void *receiver)
{
  lock_acquire (&cache_lock);

  struct cache_entry *cache = get_cache (sector);
  if (cache == NULL)
    {
      cache = cache_acquare ();
      cache->is_dirty = false;
      cache->filled = true;
      cache->disk_sector = sector;
      block_read (fs_device, sector, cache->buffer);
    }
  cache->flag = true;
  memcpy (receiver, cache->buffer, BLOCK_SECTOR_SIZE);

  lock_release (&cache_lock);
}

void
cache_write (block_sector_t sector, const void *giver)
{
  lock_acquire (&cache_lock);

  struct cache_entry *cache = get_cache (sector);
  if (cache == NULL)
    {
      cache = cache_acquare ();
      cache->is_dirty = false;
      cache->filled = true;
      cache->disk_sector = sector;
      block_read (fs_device, sector, cache->buffer);
    }
  cache->flag = true;
  cache->is_dirty = true;
  memcpy (cache->buffer, giver, BLOCK_SECTOR_SIZE);

  lock_release (&cache_lock);
}

void
cache_close (void)
{
  lock_acquire (&cache_lock);
  for (size_t i = 0; i < CACHE_SIZE; i++)
    {
      if (!cache[i].filled)
        continue;
      cache_flush (&cache[i]);
    }
  lock_release (&cache_lock);
}

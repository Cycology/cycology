// Joseph Oh
// Based on https://gist.github.com/tonious/1377667

/* Enable certain library functions (strdup) on linux.  See feature_test_macros(7) */
#define _XOPEN_SOURCE 500 

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>

#include "loggingDiskFormats.h"
#include "fuseLogging.h"
#include "lruFileList.c"

#define HASH_SIZE 1000;

typedef struct pageKey {
  struct openFile file;
  int dataOffset;
  int levelsAbove;
  
} *pageKey;

typedef struct cacheEntry {
  // Content fields
  pageKey key;
  writeablePage wp;
  cacheEntry next;
  bool dirty;

  // Global LRU data pages list
  cacheEntry lruDataNext;
  cacheEntry lruDataPrev;

  // OpenFile's data pages list
  cacheEntry fileDataNext;
  cacheEntry fileDataPrev; // TODO: Possibly remove when removing just data (global lru)
  
  // OpenFile's dirty metadata pages list
  cacheEntry fileMetadataNext;
  cacheEntry fileMetadataPrev;
  
} *cacheEntry;

typedef struct addressCache {
  int MAX_SIZE;               
  int size;
  
  struct cacheEntry **table;  /* Actual data table of the cacheEntries */

  // Global LRU data pages list
  cacheEntry lruDataHead;     
  cacheEntry lruDataTail;

  // Global LRU openFiles list
  openFile lruFileHead;       
  openFile lruFileTail;
  
} *addressCache;
 

/* Create a new hashtable. */
addressCache *cache_create( int size ) {

  addressCache cache = NULL;
  int i;

  if (size < 1) { return NULL; }

  /* Allocate the table itself. */
  if ( ( cache = malloc( sizeof( struct addressCache ) ) ) == NULL ) {
    return NULL;
  }

  /* Allocate pointers to the head nodes. */
  if( ( cache->table = malloc( sizeof( struct cacheEntry ) * size ) ) == NULL ) {
    return NULL;
  }
  for( i = 0; i < size; i++ ) {
    cache->table[i] = NULL;
  }

  /* Initialize data fields */
  cache->size = 0;
  cache->MAX_SIZE = size;
  lruDataHead = NULL;
  lruDataTail = NULL;
  lruFileHead = NULL;
  lruFileTail = NULL;

  return cache;	
}

/* Hash a string for a particular hash table. */
int cache_hash( addressCache cache, pageKey page_key ) {
  char* key = page_key->file->name + page_key->offset + page_key->levelsAbove;
  
  unsigned long int hashval;
  int i = 0;

  /* Convert our string to an integer */
  while( hashval < ULONG_MAX && i < strlen( key ) ) {
    hashval = hashval << 8;
    hashval += key[ i ];
    i++;
  }

  return hashval % HASH_SIZE;
}


/* Evict the LRU file from the cache */
void cache_evict( addressCache cache ) {
  openFile lruFile = cache->lruFileTail;

  // Flush out data pages
  openFile_flushDataPages(lruFile, cache);

  // Flush out metadata pages
  openFile_flushMetadataPages(lruFile, cache);
  
  // Remove file from LRU file list
  lru_removeFile(cache, lruFile);
}

/* Create a key-value pair. */
cacheEntry cache_newentry( addressCache cache, pageKey key, writeablePage wp ) {
  cacheEntry newentry;

  if ( cache->size >= cache->MAX_SIZE ) {
    cache_evict(cache);
  }

  if( ( newentry = malloc( sizeof( struct cacheEntry ) ) ) == NULL ) {
    return NULL;
  }

  // TODO: Check the pointer assignments???
  newentry->key = key;
  newentry->wp = wp;
  newentry->next = NULL;
  newentry->dirty = false;
  newentry->lruDataNext = NULL;
  newentry->lruDataPrev = NULL;
  newentry->fileDataNext = NULL;
  newentry->fileDataPrev = NULL;
  newentry->fileMetadataNext = NULL;
  newentry->fileMetadataPrev = NULL;

  cache->size++;

  return newentry;
}

/* Insert a key-value pair into a hash table. */
void cache_set( addressCache cache, pageKey key, writeablePage wp, bool isWrite) {
  int bin = 0;
  cacheEntry newentry = NULL;
  cacheEntry next = NULL;
  cacheEntry last = NULL;

  bin = cache_hash( cache, key );

  next = cache->table[ bin ];

  while( next != NULL && next->key != NULL && strcmp( key, next->key ) > 0 ) {
    last = next;
    next = next->next;
  }

  /* There's already a pair.  Let's replace that string. */
  if( next != NULL && next->key != NULL && strcmp( key, next->key ) == 0 ) {
    // TODO: Check pointer assignment
    free( next->wp );
    next->wp = wp;
    
    /* Nope, could't find it.  Time to grow a pair. */
  } else {
    newentry = cache_newentry( cache, key, wp );

    /* We're at the start of the linked list in this bin. */
    if( next == cache->table[ bin ] ) {
      newentry->next = next;
      cache->table[ bin ] = newentry;
	
      /* We're at the end of the linked list in this bin. */
    } else if ( next == NULL ) {
      last->next = newentry;
	
      /* We're in the middle of the list. */
    } else  {
      newentry->next = next;
      last->next = newentry;
    }
  }
}

/* Retrieve a key-value pair from a hash table. */
cacheEntry cache_get( addressCache cache, pageKey key ) {
  int bin = 0;
  cacheEntry pair;

  bin = cache_hash( cache, key );

  /* Step through the bin, looking for our value. */
  pair = cache->table[ bin ];
  while( pair != NULL && pair->key != NULL && strcmp( key, pair->key ) > 0 ) {
    pair = pair->next;
  }

  /* Did we actually find anything? */
  if( pair == NULL || pair->key == NULL || strcmp( key, pair->key ) != 0 ) {
    return NULL;

  } else {
    return pair->wp;
  }
}


int main( int argc, char **argv ) {

  hashtable_t *hashtable = cache_create( 65536 );

  cache_set( hashtable, "key1", "inky" );
  cache_set( hashtable, "key2", "pinky" );
  cache_set( hashtable, "key3", "blinky" );
  cache_set( hashtable, "key4", "floyd" );

  printf( "%s\n", cache_get( hashtable, "key1" ) );
  printf( "%s\n", cache_get( hashtable, "key2" ) );
  printf( "%s\n", cache_get( hashtable, "key3" ) );
  printf( "%s\n", cache_get( hashtable, "key4" ) );

  return 0;
}

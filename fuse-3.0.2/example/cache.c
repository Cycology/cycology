// Joseph Oh
// Based on https://gist.github.com/tonious/1377667

/* Enable certain library functions (strdup) on linux.  See feature_test_macros(7) */
#define _XOPEN_SOURCE 500 

#include <stdlib.h>
#include <stdio.h>
#include <limits.h>
#include <string.h>
#include <math.h>

#include "loggingDiskFormats.h"
#include "fuseLogging.h"
#include "cacheStructs.h"

/* Create a new hashtable. */
addressCache cache_create( int size ) {

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
  cache->lruDataHead = NULL;
  cache->lruDataTail = NULL;

  return cache;	
}

char* keyHash(pageKey page_key) {
  int numKey = 1 + (page_key->file->address * 11) +
    (page_key->siblingNum * 17) +
    (page_key->levelsAbove * 29);
  int length = (int)((ceil(log10(numKey))+1)*sizeof(char));
  char* strKey = malloc(sizeof(char) * length);
  sprintf(strKey, "%d", numKey);

  return strKey;
}

int keyCmp(pageKey key1, pageKey key2) {
  if (key1 == NULL || key2 == NULL || key1->file == NULL || key2->file == NULL) {
    printf("\n\n******ERROR: KEYCMP KEYS OR FILES ARE NULL*******\n\n");
    return -1;
  }
  
  char* str1 = keyHash(key1);
  char* str2 = keyHash(key2);
  int res = strcmp(str1, str2);
  free(str1);
  free(str2);

  return res;
}

/* Hash a string for a particular hash table. */
int cache_hash( addressCache cache, pageKey page_key ) {
  // Remember to free() strkey!
  char* strKey = keyHash(page_key);

  unsigned long hash = 5381;
  int c = 0;
  int index = 0;
  while ((c = strKey[index])) {
    hash = ((hash << 5) + hash) + c;
    index++;
  }

  free(strKey);

  return hash % cache->MAX_SIZE;
}


/* Create a key-value pair. */
cacheEntry cache_newEntry( addressCache cache, pageKey page_key, writeablePage wp ) {
  cacheEntry newentry;

  if ( cache->size >= cache->MAX_SIZE ) {
    return NULL;
  }

  if( ( newentry = malloc( sizeof( struct cacheEntry ) ) ) == NULL ) {
    return NULL;
  }

  cache->size++;
  if (cache->size > cache->MAX_SIZE) {
    printf("******************************************");
    printf("**************** CACHE OVERFLOW **********");
    return NULL;
  }

  // Initialize data fields
  pageKey key = malloc( sizeof(struct pageKey) );
  key->file = page_key->file;
  key->siblingNum = page_key->siblingNum;
  key->levelsAbove = page_key->levelsAbove;
  
  newentry->key = key;
  newentry->wp = wp;
  newentry->next = NULL;
  newentry->dirty = 0;
  newentry->lruDataNext = NULL;
  newentry->lruDataPrev = NULL;
  newentry->fileDataNext = NULL;
  newentry->fileDataPrev = NULL;
  newentry->fileMetadataNext = NULL;
  newentry->fileMetadataPrev = NULL;

  return newentry;
}

/* Insert a key-value pair into a hash table. */
cacheEntry cache_set( addressCache cache, pageKey key, writeablePage wp) {
  int bin = 0;
  cacheEntry newentry = NULL;
  cacheEntry next = NULL;
  cacheEntry last = NULL;

  // TODO: Check whether we went over our size limit
  bin = cache_hash( cache, key );

  next = cache->table[ bin ];

  while( next != NULL && next->key != NULL && keyCmp(key, next->key) > 0 ) {
    last = next;
    next = next->next;
  }

  /* There's already a pair.  Let's replace that string. */
  if( next != NULL && next->key != NULL && keyCmp(key, next->key) == 0 ) {
    newentry = next;
    free( next->wp );
    next->wp = wp;
    
    /* Nope, could't find it.  Time to grow a pair. */
  } else {
    newentry = cache_newEntry( cache, key, wp );

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

  // TODO: Debugging
  NAND_trackKey(key);
  
  return newentry;
}

/* Retrieve a key-value pair from a hash table. */
cacheEntry cache_get( addressCache cache, pageKey key ) {
  int bin = 0;
  cacheEntry pair;

  bin = cache_hash( cache, key );

  /* Step through the bin, looking for our value. */
  pair = cache->table[ bin ];
  while( pair != NULL && pair->key != NULL && keyCmp(key, pair->key) > 0 ) {
    pair = pair->next;
  }

  /* Did we actually find anything? */
  if( pair == NULL || pair->key == NULL || keyCmp(key, pair->key) != 0 ) {
    return NULL;

  } else {
    return pair;
  }
}

/*************************************************************
 *
 * LRU DATA PAGE LIST OPERATIONS
 *
 *************************************************************/
int dataLruContains(addressCache cache, cacheEntry entry) {
  cacheEntry cur = cache->lruDataHead;
  while (cur != NULL) {
    if (entry == cur) {
      return 1;
    } else {
      cur = cur->lruDataNext;
    }
  }

  return 0;
}


/* Is only called when entry already exists in the LRU list 
   Move an existing cacheEntry in the LRU Data List to the head */
void cache_updateDataLruHead(addressCache cache, cacheEntry entry) {
  // Don't do anything if it doesn't exist
  if (dataLruContains(cache, entry) == 0) {
    return;
  }

  // Move the entry to the head position
  cacheEntry prev = entry->lruDataPrev;
  cacheEntry next = entry->lruDataNext;

  if (prev != NULL) {
    prev->lruDataNext = next;
    prev->lruDataPrev = entry;
  } else {
    // Entry is already at head, so do nothing
    return;
  }

  // There's at least two entries 
  if (next != NULL) {
    next->lruDataPrev = prev;
  } else {
    // Entry is last, so update tail to be prev
    cache->lruDataTail = prev;
  }

  entry->lruDataNext = cache->lruDataHead;
  entry->lruDataPrev = NULL;
  
  cache->lruDataHead = entry;
}

/* PRE: Is only called when entry does not already exist in LRU list 
*  POST: Add a cacheEntry to the head of the LRU Data List */
void cache_addDataPageToLru(addressCache cache, cacheEntry entry) {
  // Don't do anything if already exists
  if (dataLruContains(cache, entry) == 1) {
    return;
  }
  
  // Add entry to the head of the LRU list
  cacheEntry curHead = cache->lruDataHead;

  entry->lruDataNext = curHead;
  entry->lruDataPrev = NULL;

  if (curHead == NULL) {
    // List is empty, set tail
    cache->lruDataTail = entry;
  } else {
    curHead->lruDataPrev = entry;
  }
  
  cache->lruDataHead = entry;
}

/* Remove from global LRU Data List. Updates the pointers of neighbor cacheEntries. */
cacheEntry cache_removeDataPageFromLru(addressCache cache, cacheEntry current) {
  if (current == cache->lruDataHead) {
    // If head, then change the head to point to next
    cache->lruDataHead = current->lruDataNext;
  } else {
    // Otherwise bypass current
    current->lruDataPrev->lruDataNext = current->lruDataNext;
  }

  if (current == cache->lruDataTail) {
    // Change the last to point to previous link
    cache->lruDataTail = current->lruDataPrev;
  } else {
    current->lruDataNext->lruDataPrev = current->lruDataPrev;
  }

  return current;
}

void cache_remove(addressCache cache, cacheEntry entry) {
  pageKey key = entry->key;
  int bin = cache_hash(cache, key);
  
  if (entry->key->levelsAbove == 0) {
    cache_removeDataPageFromLru(cache, entry);
  }

  // Remove the entry from the table
  cacheEntry prev = NULL;
  cacheEntry cur = cache->table[bin];
  if (keyCmp(cur->key, key) == 0) {
    // This entry is the first in the bin
    cache->table[bin] = entry->next;

  } else {
    // The entry is in the middle, take it out
    while (prev != NULL && prev->key != NULL &&
	   keyCmp(key, cur->key) > 0) {
      prev = cur;
      cur = cur->next;
    }

    if (keyCmp(key, cur->key) == 0) {
      prev->next = entry->next;
    } else {
      // Something is wrong
      printf("ERROR");
    }
  }

  free(entry->key);
  free(entry->wp);
  free(entry);
  cache->size--;
}

void cache_printSize(addressCache cache) {
  printf("\nCache entries: %d\n", cache->size);
}

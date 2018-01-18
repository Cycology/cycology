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

int keyCmp(pageKey key1, pageKey key2) {
  int num1 = key1->file->address + key1->dataOffset + key1->levelsAbove;
  int length1 = (int)((ceil(log10(num1))+1)*sizeof(char));
  char str1[length1];
  sprintf(str1, "%d", num1);

  int num2 = key2->file->address + key2->dataOffset + key2->levelsAbove;
  int length2 = (int)((ceil(log10(num2))+1)*sizeof(char));
  char str2[length2];
  sprintf(str2, "%d", num2);

  return strcmp(str1, str2);
}

/* Hash a string for a particular hash table. */
int cache_hash( addressCache cache, pageKey page_key ) {
  int numKey = page_key->file->address + page_key->dataOffset + page_key->levelsAbove;
  int length = (int)((ceil(log10(numKey))+1)*sizeof(char));
  char strKey[length];
  sprintf(strKey, "%d", numKey);

  unsigned long hash = 5381;
  int c = 0;
  int index = 0;

  while ((c = strKey[index])) {
    hash = ((hash << 5) + hash) + c;
    index++;
  }

  return hash % cache->size;
}


/* Create a key-value pair. */
cacheEntry cache_newEntry( addressCache cache, pageKey key, writeablePage wp ) {
  cacheEntry newentry;

  if ( cache->size >= cache->MAX_SIZE ) {
    return NULL;
  }

  if( ( newentry = malloc( sizeof( struct cacheEntry ) ) ) == NULL ) {
    return NULL;
  }

  // Initialize data fields
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

  cache->size++;

  return newentry;
}

/* Insert a key-value pair into a hash table. */
cacheEntry cache_set( addressCache cache, pageKey key, writeablePage wp) {
  int bin = 0;
  cacheEntry newentry = NULL;
  cacheEntry next = NULL;
  cacheEntry last = NULL;

  bin = cache_hash( cache, key );

  next = cache->table[ bin ];

  while( next != NULL && next->key != NULL && keyCmp(key, next->key) > 0 ) {
    last = next;
    next = next->next;
  }

  /* There's already a pair.  Let's replace that string. */
  if( next != NULL && next->key != NULL && keyCmp(key, next->key) == 0 ) {
    // TODO: Check pointer assignment
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

/* Is only called when entry already exists in the LRU list 
   Move an existing cacheEntry in the LRU Data List to the head */
void cache_updateDataLruHead(addressCache cache, cacheEntry entry) {
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
  if (entry->key->levelsAbove == 0) {
    cache_removeDataPageFromLru(cache, entry);
  }

  free(entry);
  cache->size--;
}

void cache_printSize(addressCache cache) {
  printf("\nCache entries: %d\n", cache->size);
}

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

typedef struct cacheEntry {
  char *key;
  page_addr value;
  cacheEntry next;
  
} *cacheEntry;

typedef struct addressCache {
  int size;
  struct cacheEntry **table;
  struct lruFileList *lruList;
  
} *addressCache;


/* Create a new hashtable. */
addressCache *cache_create( int size ) {

  addressCache cache = NULL;
  int i;

  if (size < 1) { return NULL; }

  /* Allocate the table itself. */
  if ((cache = malloc( sizeof(struct addressCache))) == NULL) {
    return NULL;
  }

  /* Allocate pointers to the head nodes. */
  if( ( cache->table = malloc( sizeof( cacheEntry ) * size ) ) == NULL ) {
    return NULL;
  }
  for( i = 0; i < size; i++ ) {
    cache->table[i] = NULL;
  }

  cache->size = size;

  return cache;	
}

/* Hash a string for a particular hash table. */
int cache_hash( addressCache cache, char *key ) {

  unsigned long int hashval;
  int i = 0;

  /* Convert our string to an integer */
  while( hashval < ULONG_MAX && i < strlen( key ) ) {
    hashval = hashval << 8;
    hashval += key[ i ];
    i++;
  }

  return hashval % cache->size;
}

/* Create a key-value pair. */
cacheEntry ht_newpair( char *key, char *value ) {
  cacheEntry newpair;

  if( ( newpair = malloc( sizeof( struct cacheEntry ) ) ) == NULL ) {
    return NULL;
  }

  if( ( newpair->key = strdup( key ) ) == NULL ) {
    return NULL;
  }

  if( ( newpair->value = strdup( value ) ) == NULL ) {
    return NULL;
  }

  newpair->next = NULL;

  return newpair;
}

/* Insert a key-value pair into a hash table. */
void cache_set( addressCache cache, char *key, char *value ) {
  int bin = 0;
  cacheEntry newpair = NULL;
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

    free( next->value );
    next->value = strdup( value );

    /* Nope, could't find it.  Time to grow a pair. */
  } else {
    newpair = cache_newpair( key, value );

    /* We're at the start of the linked list in this bin. */
    if( next == cache->table[ bin ] ) {
      newpair->next = next;
      cache->table[ bin ] = newpair;
	
      /* We're at the end of the linked list in this bin. */
    } else if ( next == NULL ) {
      last->next = newpair;
	
      /* We're in the middle of the list. */
    } else  {
      newpair->next = next;
      last->next = newpair;
    }
  }
}

/* Retrieve a key-value pair from a hash table. */
char* cache_get( addressCache cache, char *key ) {
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
    return pair->value;
  }
}


int main( int argc, char **argv ) {

  hashtable_t *hashtable = ht_create( 65536 );

  ht_set( hashtable, "key1", "inky" );
  ht_set( hashtable, "key2", "pinky" );
  ht_set( hashtable, "key3", "blinky" );
  ht_set( hashtable, "key4", "floyd" );

  printf( "%s\n", ht_get( hashtable, "key1" ) );
  printf( "%s\n", ht_get( hashtable, "key2" ) );
  printf( "%s\n", ht_get( hashtable, "key3" ) );
  printf( "%s\n", ht_get( hashtable, "key4" ) );

  return 0;
}

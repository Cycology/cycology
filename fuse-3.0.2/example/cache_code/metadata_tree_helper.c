// Joseph Oh


#include "cache.c"
#include "loggingDiskFormats.h"

int CACHE_SIZE = 10001;
addressCache* cache;

typedef struct cachedPage {
  fullPage dataPage;
  
  
} *cachedPage;

typedef struct pageKey {
  struct openFile file;
  int dataOffset;
  int levelsAbove;
} *pageKey;


int hash(pageKey key) {
  // Complex hash function
  // return (key->file.name + key->dataOffset + key->levelsAbove);
  
  // Simple hash function
  return (key->file.inode.i_file_no + key->dataOffset + key->levelsAbove) % HASH_SIZE;
}

cachedPage checkCache(pageKey key) {
  char* cachedEntry = ht_get(cache, key);
  return cachedEntry;
}


/* Add the page to cache */ 
int readIntoCache(page_addr address, pageKey key) {
  // TODO: Should we kick out the entire file? What if cache only has one (large) file?
  // TODO: Abstract this work into the cache.c file
  // Kick out LRU file
  if (cache->size >= CACHE_SIZE) {
    cache_evictLru(cache);
  } 

  cachedPage page = initCachedPage();
  if (readNAND(&page->contents, address) == SUCCESS) {
    return -1;
  }
  cache_set(cache, key, page);

  return page;
}

/*  */
int getNumPagesPerPointerAtLevel(openFile file, int level) {
  // Error; fix 
  if (level == file.tree_height) {
    return pow(INDIRECT_POINTERS, level - 2) * INODE_POINTERS;
  } else {
    return pow(POINTERS_PER_PAGE, level - 1);
  }
}


/* Get the physical address of the given data page from the parent */
page_addr extractAddress(cachedPage parent, pageKey key) {
  int parentStartOffset = getStartOfRange(key);
  int pages_per_pointer = getNumPagesPerPointerAtLevel(key->file, key->levelsAbove + 1);
  int pointer_index = (key->dataOffset - parentStartOffset) / pages_per_pointer;

  if (key->levelsAbove == file.tree_height) {
    // Parent is the inode
    return parent->directPage[pointer_index];
  } else {
    // Parent is an indirect page
    return parent->contents[pointer_index];
  }
}

/* Creates and adds a blank indirect page to the cache */
cachedPage allocatePage(page_addr address, pageKey key) {
  cachedPage page = initCachedPage(page);
  cache_set(key, page);
  return page;
}


/* Writes the physical address of the data page to its parent indirect page */
void updateIndirectPointer(cachedPage indirectPage, pageKey key,  page_addr destAddress) {
  int posToUpdate = ((key->dataOffset - indirectPage->dataOffset) / POINTER_SIZE)
    * POINTER_SIZE;

  // Write destAddress in this location
  indirectPage->contents[posToUpdate] = destAddress;
}

/* Adds the given data page to the cache*/
void addToCache(pageKey dataPageKey, writeablePage dataPage) {
  cache_set(dataPageKey, dataPage);
}


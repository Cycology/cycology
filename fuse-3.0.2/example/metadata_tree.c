// Joseph Oh

typedef struct pageKey {
  struct openFile file;
  int dataOffset;
  int levelsAbove;
} *pageKey;


int hash(pageKey key) {
  // Simple hash function
}

cachedPage checkCache(pageKey key) {
  char* cachedEntry = ht_get(cache, key);
  return cachedEntry;
}

int readIntoCache(page_addr address, pageKey key) {
  
}

page_addr extractAddress(cachedPage parent, pageKey key) {
  
}

void allocatePageInCache(page_addr address, pageKey key) {
  
}

int addToCache(pageKey key, writeablePage destination) {

}

void updateIndirectPointer(cachedPage indirectPage, pageKey key,  writeablePage destAddress) {
  int 
}


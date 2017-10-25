// Joseph Oh

int HASH_SIZE = 10001;

typedef struct pageKey {
  struct openFile file;
  int dataOffset;
  int levelsAbove;
} *pageKey;


int hash(pageKey key) {
  // Simple hash function
  return (key->file.inode.i_file_no + key->dataOffset + key->levelsAbove) % HASH_SIZE;
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

void updateIndirectPointer(cachedPage indirectPage, pageKey key,  page_addr destAddress) {
  int posToUpdate = (key->dataOffset - indirectPage->dataOffset) / POINTER_SIZE;
  // Write destAddress in this location
  // 
}


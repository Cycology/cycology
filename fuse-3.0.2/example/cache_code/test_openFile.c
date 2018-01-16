// Joseph Oh

#include <stdlib.h>
#include <stdio.h>

#include "loggingDiskFormats.h"
#include "cacheStructs.h"
#include "openFile.h"
#include "cache.h"
#include "filesystem.h"

addressCache cache = NULL;
openFile file = NULL;
pageKey keys[100];


void init() {
  cache = cache_create(200);
  file = malloc( sizeof(struct openFile) );
  file->address = 100;
}


void finish() {
  free(cache);
  free(file);
}


void test_addRemoveData() {  
  // Add data
  for (int i = 0; i < 20; i++) {
    pageKey key = malloc( sizeof(struct pageKey) );
    key->levelsAbove = 0;
    key->file = file;
    key->dataOffset = i*PAGESIZE;
    keys[i] = key;
    
    writeablePage wp = malloc( sizeof(struct writeablePage) );
    cacheEntry entry = cache_set(cache, key, wp);

    openFile_addDataPage(file, entry);
  }

  openFile_printData(file);

  // Remove data
  for (int i = 0; i < 20; i++) {
    pageKey key = keys[i];
    cacheEntry entry = cache_get(cache, key);
    
    openFile_removeDataPage(file, entry);
  }

  openFile_printData(file);
}

void test_addRemoveMetadata() {
   // Add Metadata
  for (int i = 0; i < 20; i++) {
    pageKey key = malloc( sizeof(struct pageKey) );
    key->levelsAbove = 1;
    key->file = file;
    key->dataOffset = i*PAGESIZE;
    keys[i] = key;
    
    writeablePage wp = malloc( sizeof(struct writeablePage) );
    cacheEntry entry = cache_set(cache, key, wp);

    openFile_addMetadataPage(file, entry);
  }

  openFile_printMetadata(file);

  // Remove Metadata
  for (int i = 0; i < 20; i++) {
    pageKey key = keys[i];
    cacheEntry entry = cache_get(cache, key);
    
    openFile_removeMetadataPage(file, entry);
  }

  openFile_printMetadata(file);
}


void test_flushData() {
  // Add data
  for (int i = 0; i < 20; i++) {
    pageKey key = malloc( sizeof(struct pageKey) );
    key->levelsAbove = 0;
    key->file = file;
    key->dataOffset = i*PAGESIZE;
    keys[i] = key;
    
    writeablePage wp = malloc( sizeof(struct writeablePage) );
    cacheEntry entry = cache_set(cache, key, wp);

    openFile_addDataPage(file, entry);
  }

  openFile_printData(file);

  // Flush
  fs_flushDataPages(cache, file);  
}

void test_flushMetadata() {
  // Add Metadata
  for (int i = 0; i < 20; i++) {
    pageKey key = malloc( sizeof(struct pageKey) );
    key->levelsAbove = 1;
    key->file = file;
    key->dataOffset = i*PAGESIZE;
    keys[i] = key;
    
    writeablePage wp = malloc( sizeof(struct writeablePage) );
    cacheEntry entry = cache_set(cache, key, wp);

    openFile_addMetadataPage(file, entry);
  }
  
  openFile_printMetadata(file);

  // Flush
  fs_flushMetadataPages(cache, file);
}

int main() {
  init();
  //test_addRemoveData();
  //test_addRemoveMetadata();
  test_flushData();
  //  finish();
}

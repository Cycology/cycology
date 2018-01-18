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
  file->inode.treeHeight = 4;
}


void finish() {
  free(cache);
  free(file);
}


void test_addRemoveData() {
  printf("\n*****TEST_ADD/RM_DATA*****\n");
  //init();
  
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

  //finish();
}

void test_addRemoveMetadata() {
  printf("\n*****TEST_ADD/RM_METADATA*****\n");
  //init();
  
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

  //finish();
}


void test_flushData() {
  printf("\n*****TEST_FLUSH_DATA*****\n");
  
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
  printf("\n");

  // Flush
  fs_flushDataPages(cache, file);

  openFile_printData(file);
  printf("\n");

  //finish();
}

void test_flushMetadata() {
  printf("\n*****TEST_FLUSH_METADATA*****\n");

  // Add Metadata
  printf("TEST_FLUSH_METADATA\n");
  for (int j = 1; j < 4; j++) {
    for (int i = 0; i < 2; i++) {
      pageKey key = malloc( sizeof(struct pageKey) );
      key->levelsAbove = j;
      key->file = file;
      key->dataOffset = i*PAGESIZE;
      keys[i] = key;
    
      writeablePage wp = malloc( sizeof(struct writeablePage) );
      cacheEntry entry = cache_set(cache, key, wp);

      openFile_addMetadataPage(file, entry);
    }
  }
  
  openFile_printMetadata(file);
  printf("\n");

  // Flush
  fs_flushMetadataPages(cache, file);

  openFile_printMetadata(file);
  printf("\n");
}

int main() {
  init();

  test_addRemoveData();
  test_addRemoveMetadata();
  test_flushData();
  test_flushMetadata();

  finish();
}

// Tom Murtagh
// Joseph Oh

// TODO: Split file into cache.c, lru.c, openFile.c

#include "cache.c"
#include "loggingDiskFormats.h"


/*** getPage() ***/

/* Return new key with levelsAbove incremented by 1 */
pageKey getParentKey(pageKey childKey) {
  // TODO: Check malloc() call here
  pageKey parentKey = malloc( sizeof (struct pageKey) );
  parentKey->file = childKey->file;
  parentKey->dataOffset = childKey->dataOffset;
  parentKey->levelsAbove = childKey->levelsAbove + 1;
  return parentKey;
}

/* Read a writeablePage into the cache from the given NAND address */
cacheEntry readIntoCache(page_addr address, pageKey key) {
  writeablePage wp = malloc( sizeof (struct writeablePage) );

  // Read from disk into page
  if (address != 0) {
    // TODO: Handle read errors
    readNAND(wp->nandPage, address);
  }
  cacheEntry entry = cache_set(cache, key, wp);
  
  return entry;
}

/* Get the physical address of the given data page from the parent indirect page */
int getAddressFromParent(cacheEntry parent, pageKey desiredKey) {
  int parentStartOffset = getStartOfRange(desiredKey);
  int pages_per_pointer = getNumPagesPerPointerAtLevel(desiredKey->file,
						       desiredKey->levelsAbove + 1);
  int pointer_index = (desiredKey->dataOffset - parentStartOffset) / pages_per_pointer;

  pageKey parentKey = getParentKey(desiredKey);
  if (isInode(parentKey)) {
    return parent->writeablePage.nandPage.directPages[pointer_index];
  } else {
    return parent->writeablePage.nandPage.contents[pointer_index];
  }
}

/* Return the requested page from cache memory */
cacheEntry getPage(pageKey desiredKey) {
  int maxFileHeight = desiredKey->file->inode->treeHeight;

  if (desiredKey->levelsAbove > maxFileHeight) {
    return NULL;

  } else if (desiredKey->levelsAbove == maxFileHeight) {
    // TODO: Should store a pointer to the writeablePage of the latest inode instead
    //       Thus should get it from the cache
    return desiredKey->file->inode;

  } else {
    cacheEntry desired = cache_get(cache, key);

    // Page is not in cache
    if (desired == null) {
      // TODO: getParentKey should shift the offset to the parent page's unique offset
      pageKey parentKey = getParentKey(desiredKey);
      cacheEntry parent = getPage(parentKey);
      
      page_addr desiredAddr = getAddressFromParent(parent, desiredKey);
      desired = readIntoCache(desiredAddr, desiredKey);
    }

    return desired;
  }
}




/*** writeData() ***/

/* Return address of the first free page of the first free block [PUFL only] */
int consumeFreeBlock(activeLog log, int preferCUFL) {
  freeList lists = &state->lists;
  if (preferCUFL == 0) {
    // TODO: Add support for getting from completely free list
    // Normally, check CUFL then go to PUFL, but if this is set, vice versa
  }

  // Return the current head block of the free list
  int freeBlockAddr = lists->partialHead + 1;

  // Update the log's last block's number of erases 
  log->lastErases = lists->partialHeadErases;

  // Read in the nextLogBlock and nextBlockErases from the last page of the block 
  struct fullPage lastPage;
  readNAND(&lastPage, lists->partialHead);

  // Update the list head pointer to next free block
  lists->partialHead = lastPage.nextLogBlock;
  lists->partialHeadErases = lastPage.nextBlockErases;
    
  return freeBlockAddr;
}

void firstPageOps(writeablePage freePage, activeLog log) {
  // Next free page is the next page in block
  log->nextPage++;

  // Set the erase count from the stored erase count
  freePage->page.eraseCount = log->lastErases;
}

void thirdToLastPageOps(writeablePage freePage, activeLog log) {
  // Next free page is the last page of the previous block
  log->nextPage = (log->log.prev)*BLOCKSIZE + (BLOCKSIZE-1);
}

void secondToLastPageOps(writeablePage freePage, activeLog log) {
  // Next free page is the first page of new block
  page_addr firstPageOfNewBlock = consumeFreeBlock(log, 0);

  // Update metadata in current page to reflect addition of the new (last) block
  freePage->page.nextLogBlock = firstPageOfNewBlock;
  freePage->page.nextBlockErases = log->lastErases;

  // Update the log to reflect the addition of the new (last) block
  log->nextPage = firstPageOfNewBlock;
  log->last = first/BLOCKSIZE;    
}

void lastPageOps(writeablePage freePage, activeLog log) {
  // Last page: Next free page is the 2nd-to-last page in the next block
  log->nextPage = (log->last)*BLOCKSIZE + (BLOCKSIZE-2);

  // Update page metadata to point to next block (the last block of the log)
  freePage->page.nextLogBlock = (log->last)*BLOCKSIZE;
  freePage->page.nextBlockErases = log->lastErases;    
}

/* Allocate a new writeablePage from the log */
void allocateFreePage(writeablePage freePage, activeLog log) {
  // Assign freePage to the next free NAND address
  freePage->address = log->nextPage;

  // Update freeList structs to reflect changes
  if (freePage->address % BLOCKSIZE == 0) {
    firstPageOps(freePage, log);    
  } else if ((freePage->address + 3) % BLOCKSIZE == 0) {
    thirdToLastPageOps(freePage, log);  
  } else if((freePage->address + 2) % BLOCKSIZE == 0) {
    secondToLastPageOps(freePage, log);    
  } else if((freePage->address + 1) % BLOCKSIZE == 0) {
    lastPageOps(freePage, log);    
  } else {
    // Middle pages: Next free page is the next physical page
    log->nextPage++;
  }

  // Increment number of active pages in log
  log->log.active++;
}

/* Create a writeable page that is ready to be written to NAND */
writeablePage newWriteablePage(pageKey key, char* data) {
  writeablePage wp = (writeablePage) malloc( sizeof( struct writeablePage ) );
  allocateFreePage(wp, key->file->log);
  memcpy(wp->nandPage.contents, data, sizeof(wp->nandPage.contents));  

  return wp;
}

/* Adds entry to the cache and updates metadata lists */
cacheEntry putDataPageInCache(writeablePage dataPage, pageKey key) {
  // Add/update to cache
  cacheEntry entry = cache_set(cache, key, dataPage);

  // Update OpenFile's data page linked list
  openFile_addDataPage(key->file, entry);
  
  // Update global LRU lists
  lru_addDataPage(cache, entry);
  lru_addFile(cache, key->file);

  return entry;
}

/* Write the physical address of the data page to its parent indirect page */
void updateParentPageInCache(pageKey childKey,  page_addr childAddress, bool dirty) {
  // Get the parent page
  pageKey parentKey = getParentKey(childKey);
  cacheEntry parentPage = getPage(parentKey);
  
  if (parentPage == NULL) {
    return;
  }

  // TODO: Write functions to get the dataOffset of the parent indirect page
  // Update the child pointer address in the parent
  int posToUpdate = ((childKey->dataOffset - parentPage->dataOffset)
		     / POINTER_SIZE) * POINTER_SIZE;
  parentPage->writeablePage->nandPage.contents[posToUpdate] = childAddress;

  // Add parent page to OpenFile's indirect page linked list
  parentPage->dirty = dirty;
  openFile_addIndirectPage(childkey->file, parentPage);
}

/* Write the given data to the offset in the file */ 
writeablePage writeData(pageKey dataKey, char * data) {
  writeablePage dataPage = newWriteablePage(dataKey, data);

  writeNAND(&dataPage->nandPage, dataPage->address, 0); // TODO: Error checking

  putDataPageInCache(dataPage, dataKey);
  updateParentPageInCache(dataKey, dataPage->address, dirty);

  return dataPage;
}

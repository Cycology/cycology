// Tom Murtagh
// Joseph Oh

#include "cache.c"
#include "loggingDiskFormats.h"

typedef struct pageKey {
  struct openFile file;
  int dataOffset;
  int levelsAbove;
} *pageKey;



/*** getPage() ***/

/* Return new key with levelsAbove incremented by 1 */
pageKey getParentKey(pageKey childKey) {
  pageKey parentKey;
  parentKey->file = childKey->file;
  parentKey->dataOffset = childKey->dataOffset;
  parentKey->levelsAbove = childKey->levelsAbove + 1;
  return parentKey;
}

/* Read in the fullPage from disk if it already exists */
int readFromDiskIntoPage(page_addr address, writeablePage desired) {
  return readNAND(desired->nandPage, address);
}

writeablePage newWriteablePage() {
  return malloc( sizeof( struct writeablePage ) );
}

/* Put the desired page into the cache */
writeablePage putInCache(page_addr address, pageKey key) {
  writeablePage desired = newWriteablePage();
  if (address != 0) {
    if (readFromDiskIntoPage(address, desired) != 0) {
      // TODO: Handle read errors
    }
  }
  cache_set(cache, key, desired);
  
  return desired;
}

bool isInode(desiredKey) {
  return (desiredKey->levelsAbove == desiredKey->file->inode->treeHeight);
}

/* Get the physical address of the given data page from the parent indirect page */
int getAddressFromParent(writeablePage parent, pageKey desiredKey) {
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
writeablePage getPage(pageKey desiredKey) {
  if (isInode(desiredKey)) {
    // TODO: Should store a pointer to the writeablePage of the latest inode instead
    //       Thus should get it from the cache
    return desiredKey->file->inode;
  } else {
    writeablePage desired = cache_get(cache, key);

    // Page is not in cache
    if (desired == null) {
      // TODO: getParentKey should shift the offset to the parent page's unique offset
      pageKey parentKey = getParentKey(desiredKey);
      writeablePage parent = getPage(parentKey);
      
      page_addr desiredAddr = getAddressFromParent(parent, desiredKey);
      desired = putInCache(desiredAddr, desiredKey);
    }

    return desired;
  }
}




/*** writeData() ***/

/* Write the physical address of the data page to its parent indirect page */
void updateParentPage(writeablePage parentPage, pageKey childKey,  page_addr childAddress) {
  // TODO: Write functions to get the dataOffset of the parent indirect page
  int posToUpdate = ((childKey->dataOffset - parentPage->dataOffset)
		     / POINTER_SIZE) * POINTER_SIZE;
  parentPage->nandPage.contents[posToUpdate] = childAddress;
}

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

/*
 * gets the next free page in a log
 * would have to call getFreeBlock, fill in the fields if necessary
 * updates active pages in logHeader
 */
void setAsFreePage(writeablePage freePage, activeLog log) {
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

/* Write the data into the wp, and prepare it for writing out to NAND */
void writeDataIntoPage(writeablePage wp, pageKey key, char* data) {
  setAsFreePage(wp, key->file->log);
  memcpy(wp->nandPage.contents, data, sizeof(wp->nandPage.contents));
}

/* Write the given data to the offset in the file */ 
writeablePage writeData(pageKey key, char * data) {
  // First write the data into the cache
  writeablePage wp = newWriteablePage();
  writeDataIntoPage(wp, key, data);
  cache_set(key, wp);

  // Then write the data out to storage
  // TODO: Shouldn't we write the data to NAND first before updating metadata? 
  if (writeNAND( &wp->nandPage, wp->address, 0 ) < 0) {
    // TODO: ERROR
  }

  // Update indirect pointer 1 level above data
  pageKey parentKey = getParentKey(key);
  writeablePage parentPage = getPage(parentKey);
  updateParentPage(parentPage, key, wp->address);
}

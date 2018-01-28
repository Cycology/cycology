// Tom Murtagh
// Joseph Oh

#define FUSE_USE_VERSION 30

#include <fuse.h>

#include <stdlib.h>
#include <string.h>
#include <stdio.h>

#include "loggingDiskFormats.h"
#include "fuseLogging.h"
#include "cacheStructs.h"
#include "cache.h"
#include "openFile.h"

/*************************************************************
 *
 * LRU FILE LIST OPERATIONS
 *
 *************************************************************/

/* Only called when file already exists in the LRU file list 
   Move an existing openFile to the head of the LRU File List */
void updateLruFileHead(addressCache cache, fileCache fCache, openFile file) {
  openFile prev = file->lruFilePrev;
  openFile next = file->lruFileNext;
  
  if (prev != NULL) {
    prev->lruFileNext = next;
    prev->lruFilePrev = file;
  } else {
    // File is already at head, so do nothing
    return;
  }

  // There's at least two files in LRU list 
  if (next != NULL) {
    next->lruFilePrev = prev;
  } else {
    // File was last, so update tail to be prev
    fCache->lruFileTail = prev;
  }

  file->lruFileNext = fCache->lruFileHead;
  file->lruFilePrev = NULL;
  
  fCache->lruFileHead = file;
}

/* Only called when file doesn't exist in the LRU file list 
   Add an openFile to the head of the LRU File List */
void addFileToLru(addressCache cache, fileCache fCache, openFile file) {
  openFile curHead = fCache->lruFileHead;
  file->lruFileNext = curHead;
  file->lruFilePrev = NULL;
      
  if (curHead != NULL) {
    curHead->lruFilePrev = file;
  } else {
    // List is empty, set tail
    fCache->lruFileTail = file;
  }
  
  fCache->lruFileHead = file;
}

/* Check if the LRU File list contains the given file */
int lruContains(openFile head, openFile file) {
  openFile cur = head;
  while (cur != NULL) {
    if (cur == file) {
      return 1;
    } else {
      cur = cur->lruFileNext;
    }
  }
  
  return 0;
}

/* OpenFile may or may not be in the LRU list 
   Brings the given file to the head of the LRU File List */
void fs_updateFileInLru(openFile file) {
  CYCstate state = fuse_get_context()->private_data;
  addressCache addrCache = state->addr_cache;
  fileCache fCache = state->file_cache;
  
  if (lruContains(fCache->lruFileHead, file) == 1) {
    updateLruFileHead(addrCache, fCache, file);
  } else {
    addFileToLru(addrCache, fCache, file);
  }
}

/* Remove an openFile from the LRU File list */
void fs_removeFileFromLru(openFile file) {
  CYCstate state = fuse_get_context()->private_data;
  fileCache fCache = state->file_cache;  
  
  openFile next = file->lruFileNext;
  openFile lruFile = fCache->lruFileTail;

  // Remove from global LRU File List
  if (file == fCache->lruFileHead) {
    // If head, then change the head to point to next
    fCache->lruFileHead = next;
  } else {
    // Otherwise bypass current
    file->lruFilePrev->lruFileNext = next;
  }

  if (file == fCache->lruFileTail) {
    // Change the last to point to previous link
    // TODO: Will this change the saved lruFile pointer as well? 
    fCache->lruFileTail = file->lruFilePrev;
  } else {
    next->lruFilePrev = file->lruFilePrev;
  }

  // TODO: Check free pointers
  // free(lruFile);
}

/*************************************************************
 *
 * Common Methods
 *
 *************************************************************/

/* Return new key with levelsAbove incremented by 1 */
pageKey getParentKey(pageKey childKey) {
  // TODO: Check malloc() call here
  pageKey parentKey = malloc( sizeof (struct pageKey) );

  parentKey->file = childKey->file;
  
  // Recalculate dataOffset of the parent
  int parentDataRange = PAGESIZE * (INDIRECT_PAGES ^ (childKey->levelsAbove + 1));
  int parentStartOffset = (childKey->dataOffset / parentDataRange) * POINTER_SIZE;
  parentKey->dataOffset = parentStartOffset;
  
  parentKey->levelsAbove = childKey->levelsAbove + 1;
  
  return parentKey;
}

/* Adds entry to the cache and updates metadata lists */
cacheEntry putPageIntoCache(addressCache cache, writeablePage page, pageKey key) {
  // Add/update to cache
  cacheEntry entry = cache_set(cache, key, page);
  int isData = key->levelsAbove == 0;

  // Update OpenFile and LRU data page lists
  if (isData == 1) {
    openFile_addDataPage(key->file, entry);
    cache_addDataPageToLru(cache, entry);
  } else {
    openFile_addMetadataPage(key->file, entry);
  }
  
  // Update global LRU File List
  fs_updateFileInLru(key->file);

  return entry;
}


/*************************************************************
 *
 * Page Read
 *
 *************************************************************/

/* Return a writeablePage with the contents of the given NAND address */
writeablePage readWpFromDisk(page_addr address, pageKey key) {
  writeablePage wp = malloc( sizeof (struct writeablePage) );

  // Read from disk into page
  if (address != 0) { // TODO: Can addresses be 0?
    // TODO: Handle read errors 
    readNAND(wp->nandPage, address);
  } else {
    // TODO: Return a newly initialized writeablePage if nothing
    memset(wp->nandPage.contents, 0, PAGESIZE * sizeof(char));
  }

  return wp;
}

int getIndexInInode(int childOffset, int fileHeight) {
  // TODO
  return ( (childOffset) / ((DIRECT_PAGES) * ((INDIRECT_PAGES) ^ (fileHeight - 2)) ) );
}
  
/* Get the index of the given data page from the parent indirect page's contents array */
int getIndexInParent(int childOffset, int parentOffset) {
  return ((childOffset - parentOffset) / INDIRECT_PAGES);
}

/* Return the requested page from cache memory */
cacheEntry fs_getPage(addressCache cache, pageKey desiredKey) {
  int maxFileHeight = desiredKey->file->inode.treeHeight;

  if (desiredKey->levelsAbove >= maxFileHeight) {
    return NULL;
  }
  
  cacheEntry desired = cache_get(cache, desiredKey);

  // Page is not in cache
  if (desired == NULL) {
    page_addr desiredAddr = 0;
    if ((maxFileHeight == 2 && desiredKey->levelsAbove == 0) ||
	desiredKey->levelsAbove == maxFileHeight - 1) {
      // If the top-level metadata page doesn't exist, should read it in from disk
      struct inode ind = desiredKey->file->inode;
      int desiredIndex = getIndexInInode(desiredKey->dataOffset, maxFileHeight);
      desiredAddr = ind.directPage[desiredIndex];
      	
    } else {
      // Get the lowest-level parent metadata page that is currently cached
      pageKey parentKey = getParentKey(desiredKey);
      cacheEntry parent = fs_getPage(cache, parentKey);

      int isInode = 0;
      int desiredIndex =
	getIndexInParent(desiredKey->dataOffset, parentKey->dataOffset);
      desiredAddr = parent->wp->nandPage.contents[desiredIndex];
    }
   
    writeablePage wp = readWpFromDisk(desiredAddr, desiredKey);
    desired = putPageIntoCache(cache, wp, desiredKey);
  }

  return desired;
}


/*************************************************************
 *
 * Data Page Write
 *
 *************************************************************/

/* Return address of the first free page of the first free block [PUFL only] */
int consumeFreeBlock(activeLog log, int preferCUFL) {
  
  freeList lists = &((CYCstate) fuse_get_context()->private_data)->lists;
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
  freePage->nandPage.eraseCount = log->lastErases;

  return 0;
}

void thirdToLastPageOps(writeablePage freePage, activeLog log) {
  // Next free page is the last page of the previous block
  log->nextPage = (log->log.prev)*BLOCKSIZE + (BLOCKSIZE-1);
}

void secondToLastPageOps(writeablePage freePage, activeLog log) {
  // Next free page is the first page of new block
  page_addr firstPageOfNewBlock = consumeFreeBlock(log, 0);

  // Update metadata in current page to reflect addition of the new (last) block
  freePage->nandPage.nextLogBlock = firstPageOfNewBlock;
  freePage->nandPage.nextBlockErases = log->lastErases;

  // Update the log to reflect the addition of the new (last) block
  log->nextPage = firstPageOfNewBlock;
  log->lastBlock = firstPageOfNewBlock/BLOCKSIZE;    
}

void lastPageOps(writeablePage freePage, activeLog log) {
  // Last page: Next free page is the 2nd-to-last page in the next block
  log->nextPage = (log->lastBlock)*BLOCKSIZE + (BLOCKSIZE-2);

  // Update page metadata to point to next block (the last block of the log)
  freePage->nandPage.nextLogBlock = (log->lastBlock)*BLOCKSIZE;
  freePage->nandPage.nextBlockErases = log->lastErases;    
}

/* Allocate a free NAND address from the log into the given writeablePage */
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
writeablePage prepareWriteablePage(pageKey key, char* data) {
  writeablePage wp = (writeablePage) malloc( sizeof( struct writeablePage ) );
  allocateFreePage(wp, key->file->mainExtentLog);
  memcpy(wp->nandPage.contents, data, sizeof(wp->nandPage.contents));  

  return wp;
}

/* Write the physical address of a page to its parent indirect page */
void fs_updateParentPage(addressCache cache, pageKey childKey, page_addr childAddress, int dirty) {
  // Get the parent page
  pageKey parentKey = getParentKey(childKey);
  cacheEntry parentPage = fs_getPage(cache, parentKey);
  
  if (parentPage == NULL) {
    return;
  }

  // Update the child pointer address in the parent
  int indexToUpdate = (((childKey->dataOffset - parentKey->dataOffset) /
			POINTER_SIZE) * POINTER_SIZE);
  if (indexToUpdate > 1016 || indexToUpdate < 0) {
    // Error
  } else {
    // TODO: Find out how to write the address (int) into the array (char*)
    // memcpy(parentPage->wp->nandPage.contents[indexToUpdate], childAddress, POINTER_SIZE);
  }

  // Mark the parent page as dirty
  parentPage->dirty = dirty;
}

/* Write the given data to the offset in the file */ 
writeablePage fs_writeData(addressCache cache, pageKey dataKey, char * data) {

  writeablePage dataPage = prepareWriteablePage(dataKey, data);

  writeNAND(&dataPage->nandPage, dataPage->address, 0); // TODO: Error checking

  putPageIntoCache(cache, dataPage, dataKey);
  int dirty = 1;
  fs_updateParentPage(cache, dataKey, dataPage->address, dirty);

  return dataPage;
}


/*************************************************************
 *
 * Flush a File
 *
 *************************************************************/

/* Write out all data pages in the file's data list */
void fs_flushDataPages(addressCache cache, openFile file) {
  cacheEntry current = file->dataHead;
  cacheEntry next;
  
  while (current != NULL) {
    next = current->fileDataNext;

    openFile_removeDataPage(file, current);
    cache_remove(cache, current);

    // Move to next node
    current = next;
  }
}

void fs_flushMetadataPages(addressCache cache, openFile file) {
  // Flush dirty metadata pages in lowest-to-highest-level order
  int max_level = file->inode.treeHeight;
  for (int level = 1; level < max_level; level++) {
    cacheEntry current = file->metadataHead;

    // Traverse through the whole list each time
    while (current != NULL) {
      cacheEntry next = current->fileMetadataNext;

      if (current->key->levelsAbove == level) {
	// Write out current metadata page to NAND
	if (current->dirty) {
	  writeablePage wp = current->wp;
	  allocateFreePage(wp, current->key->file->mainExtentLog);
	  writeNAND(&wp->nandPage, wp->address, 0);
	}
    
	// Remove current metadata page from openFile
	printf("Removed page at level: %d\n", current->key->levelsAbove);
	openFile_removeMetadataPage(file, current);
	cache_remove(cache, current);

	// Update the parent page with the written information
	fs_updateParentPage(cache, current->key, current->wp->address, current->dirty);
      }
      
      current = next;
    }
  }
}


void fs_closeFile(CYCstate state, openFile file) {
  addressCache addrCache = state->addr_cache;
  fileCache fCache = state->file_cache;

  // Flush out data pages
  fs_flushDataPages(addrCache, file);

  // Flush out metadata pages
  fs_flushMetadataPages(addrCache, file);
  
  // Remove file from LRU file list
  fs_removeFileFromLru(file);
}

/* Evict the LRU file from the cache */
void fs_evictLruFile(CYCstate state) {
  openFile lruFile = state->file_cache->lruFileTail;
  fs_closeFile(state, lruFile);
}


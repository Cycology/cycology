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
#include "vNANDlib.h"

/*************************************************************
 *
 * LRU FILE LIST OPERATIONS
 *
 *************************************************************/

/* Only called when file already exists in the LRU file list 
   Move an existing openFile to the head of the LRU File List */
void updateLruFileHead(fileCache fCache, openFile file) {
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
void addFileToLru(fileCache fCache, openFile file) {
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
int lruFileContains(openFile head, openFile file) {
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
  
  if (lruFileContains(fCache->lruFileHead, file) == 1) {
    updateLruFileHead(fCache, file);
  } else {
    addFileToLru(fCache, file);
  }
}

/* Remove an openFile from the LRU File list */
void fs_removeFileFromLru(openFile file) {
  CYCstate state = fuse_get_context()->private_data;
  fileCache fCache = state->file_cache;

  // Don't remove if not in the LRU file cache
  if (lruFileContains(fCache->lruFileHead, file) == 0) {
    return;
  }
  
  openFile next = file->lruFileNext;

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
}

/*************************************************************
 *
 * Common Methods
 *
 *************************************************************/

/* Return new key with levelsAbove incremented by 1 */
pageKey getParentKey(pageKey childKey) {
  // TODO: Check malloc() here!!!! SHOULD NOT BE MALLOC
  pageKey parentKey = malloc( sizeof (struct pageKey) );
  parentKey->file = childKey->file;  
  parentKey->siblingNum = childKey->siblingNum / INDIRECT_PAGES;
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
  // TODO: Perhaps only update files in LRU list when they're currently open?
  //       That is, files should only be in the LRU list after they're closed (and before
  //       flushed out. So active files will never be flushed out. 
  fs_updateFileInLru(key->file);

  return entry;
}


/*************************************************************
 *
 * Page Read
 *
 *************************************************************/

/* Return a writeablePage with the contents of the given NAND address */
writeablePage readWpFromDisk(page_addr address) {
  writeablePage wp = malloc( sizeof (struct writeablePage) );

  // Read from disk into page
  if (address != 0) { // TODO: Can addresses be 0?
    // TODO: Handle read errors 
    readNAND(&(wp->nandPage), address);
  } else {
    memset(wp->nandPage.contents, 0, PAGESIZE * sizeof(char));
  }

  return wp;
}
  
/* Get the index of the given data page from the parent indirect page's contents array */
page_vaddr getIndexInParent(int childSiblingNum) {
  return childSiblingNum % INDIRECT_PAGES;
}

/* Return the requested page from cache memory */
cacheEntry fs_getPage(addressCache cache, pageKey desiredKey) {
  int maxFileHeight = desiredKey->file->inode.treeHeight;

  if (desiredKey->levelsAbove >= maxFileHeight) {
    printf("\nERROR: Looking for page at invalid height \n");
    return NULL;
  }
  
  cacheEntry desired = cache_get(cache, desiredKey);

  // Page is not in cache, so get the NAND address of the page and read it in
  if (desired == NULL) {
    page_addr desiredAddr = 0;
    
    if (desiredKey->levelsAbove == maxFileHeight - 1) {
      // Looking for the inode
      return NULL;
	
    } else if (desiredKey->levelsAbove == maxFileHeight - 2) {
      // Looking for the highest level indirect page; parent is the inode
      struct inode ind = desiredKey->file->inode;
      desiredAddr = ind.directPage[desiredKey->siblingNum];
	
    } else if (desiredKey->levelsAbove >= 0 && desiredKey->levelsAbove < maxFileHeight - 2) {
      // Looking for data and middle indirect pages
      pageKey parentKey = getParentKey(desiredKey);
      cacheEntry parent = fs_getPage(cache, parentKey);
      page_vaddr desiredIndex = getIndexInParent(desiredKey->siblingNum);
  
      desiredAddr = ((page_vaddr *) parent->wp->nandPage.contents)[desiredIndex];

    } else {
      // ERROR
      printf("\nERROR: Invalid page requested \n");
      return NULL;
    }

    // Read in the desired page from the disk
    writeablePage wp = readWpFromDisk(desiredAddr);
    desired = putPageIntoCache(cache, wp, desiredKey);

    // TODO: Debugging
    
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

  // Update the logHeader's block information
  log->logH.prevBlock = log->lastBlock;
  log->logH.totalBlocks++;

  // Return the current head block of the free list (last page of old file)
  int freeBlockAddr = lists->partialHead + 1;

  // Update the activeLog's last block information
  log->lastBlockErases = lists->partialHeadErases;
  log->lastBlock = lists->partialHead;

  // Read in the nextLogBlock and nextBlockErases from the last page of the block
  // TODO: Check if lastPage has garbled contents after read
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
  freePage->nandPage.eraseCount = log->lastBlockErases;
}

void thirdToLastPageOps( activeLog log) {
  // Check if file is only one block long
  if (log->logH.prevBlock == -1 || log->lastBlock == log->logH.firstBlock) {
    log->nextPage++;
  } else {
    // Next free page is the last page of the previous block
    log->nextPage = log->logH.prevBlock + (BLOCKSIZE-1);
  }
}

void secondToLastPageOps(writeablePage freePage, activeLog log) {
  // Next free page is the first page of new block
  int preferCUFL = 0;
  page_addr firstPageOfNewBlock = consumeFreeBlock(log, preferCUFL);

  // Update metadata in current page to reflect addition of the new (last) block
  freePage->nandPage.nextLogBlock = firstPageOfNewBlock;
  freePage->nandPage.nextBlockErases = log->lastBlockErases;

  // Update the next page
  log->nextPage = firstPageOfNewBlock;
}

void lastPageOps(writeablePage freePage, activeLog log) {
  // Last page: Next free page is the 2nd-to-last page in the next block
  log->nextPage = log->lastBlock + (BLOCKSIZE-2);

  // Update page metadata to point to next block (the last block of the log)
  freePage->nandPage.nextLogBlock = log->lastBlock;
  freePage->nandPage.nextBlockErases = log->lastBlockErases;
}

/* Allocate a free NAND address from the log into the given writeablePage */
void allocateFreePage(writeablePage freePage, activeLog log) {
  // Assign freePage to the next free NAND address
  freePage->address = log->nextPage;

  // Update freeList structs to reflect changes
  if (freePage->address % BLOCKSIZE == 0) {
    firstPageOps(freePage, log);    
  } else if ((freePage->address + 3) % BLOCKSIZE == 0) {
    thirdToLastPageOps(log);  
  } else if((freePage->address + 2) % BLOCKSIZE == 0) {
    secondToLastPageOps(freePage, log);    
  } else if((freePage->address + 1) % BLOCKSIZE == 0) {
    lastPageOps(freePage, log);    
  } else {
    // Middle pages: Next free page is the next physical page
    log->nextPage++;
  }

  // Increment number of active pages in log
  log->logH.activePages++;
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
  int indexToUpdate = getIndexInParent(childKey->siblingNum);
  
  // Check if NULL because we were looking for the inode
  if (parentPage == NULL) {
    // ERROR
    if (childKey->levelsAbove != childKey->file->inode.treeHeight - 2) {
      printf("Error: trying to update invalid parent page");
    } else {
      // Update the inode's direct pages
      childKey->file->inode.directPage[childKey->siblingNum] = childAddress;
    }
      
  } else {
    // ERROR
    if (indexToUpdate >= INDIRECT_PAGES || indexToUpdate < 0) {
      printf("\nERROR: Tried to update at invalid index\n");
    } else {
      // Update the child pointer address in the parent indirect page
      page_addr* indirectAddresses = (page_addr *) parentPage->wp->nandPage.contents;
      indirectAddresses[indexToUpdate] = childAddress;
      // Mark the parent page as dirty
      parentPage->dirty = dirty;
    }
  }
}

/* Write the given data to the offset in the file */ 
writeablePage fs_writeData(addressCache cache, pageKey dataKey, char * data) {

  // Write out to NAND
  writeablePage dataPage = prepareWriteablePage(dataKey, data);
  writeNAND(&dataPage->nandPage, dataPage->address, 0);
  dataKey->file->modified = 1;

  // TODO: Debugging
  NAND_trackAddress(dataPage->address, dataKey);
  printf("\n\n\t******Wrote Data: (Level: %d, Sib: %d, Addr: %d)\n", dataKey->levelsAbove, dataKey->siblingNum, dataPage->address);

  // Increase the size of the file if necessary
  int newDataOffset = dataKey->siblingNum * PAGESIZE;
  if (newDataOffset >= dataKey->file->inode.i_size) {
    // Check if a new level needs to be added
    while (needsNewLevel(dataKey->file->inode.treeHeight, newDataOffset)) {
      addIndirectLevel(cache, dataKey->file);
    }

    dataKey->file->inode.i_size = newDataOffset + PAGESIZE;
  }

  // Update metadata in cache
  putPageIntoCache(cache, dataPage, dataKey);
  int dirty = 1;
  fs_updateParentPage(cache, dataKey, dataPage->address, dirty);

  return dataPage;
}

/* Returns true if the maxOffset is larger than current treeHeight supports. The maxOffset is the largest offset currently supported. */
int needsNewLevel(int treeHeight, int dataOffset) {
  int maxOffset = DIRECT_PAGES;
  maxOffset *= power(INDIRECT_PAGES, treeHeight-2) * PAGESIZE;
  maxOffset -= PAGESIZE;
  return dataOffset > maxOffset;
}

int power(int x, int y) {
  if (y < 0) {
    return -1;
  } else if (y == 0) {
    return 1;
  }

  int result = 1;
  while(y > 0) {
    result *= x;
    y--;
  }
  
  return result;
}

/* Increases the tree height of the file by 1 */
void addIndirectLevel(addressCache cache, openFile file) {
  // Create and initialize a new indirect page to hold prev. inode's directPages
  // Create a new inode that points to the newly created indirect page
  writeablePage indirectPage = malloc( sizeof( struct writeablePage ) );
  page_addr* indirectContents = (page_addr *) indirectPage->nandPage.contents;
  memset(indirectContents, 0, INDIRECT_PAGES*sizeof(page_addr));
  memcpy(indirectContents, file->inode.directPage, DIRECT_PAGES*sizeof(page_addr));
  // indirectPage.nandPage.pageType = PTYPE_INDIRECT;

  // Recalibrate the inode
  file->inode.treeHeight++;
  memset(file->inode.directPage, 0, DIRECT_PAGES*sizeof(page_addr));

  // Add to cache
  struct pageKey key_s;
  pageKey indirectKey = &key_s;
  indirectKey->file = file;
  indirectKey->levelsAbove = file->inode.treeHeight - 2;
  indirectKey->siblingNum = 0;
  cacheEntry entry = putPageIntoCache(cache, indirectPage, indirectKey);

  // Mark the entry as dirty
  entry->dirty = 1;
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

	  // TODO: Remove after debug
	  NAND_trackAddress(wp->address, current->key);

	  // Update the parent page with the written information
	  fs_updateParentPage(cache, current->key, current->wp->address, current->dirty);
	}

	// Remove current metadata page from openFile
	openFile_removeMetadataPage(file, current);
	cache_remove(cache, current);
      }
      
      current = next;
    }
  }
}


/***** HELPER METHODS ******/
int fs_printFileTree(struct inode ind) {
  int addrs[1000];
  int curIndex = 0;
  int lastIndex = 0;
  
  // Print the inode contents
  printf("******************* INODE *********************\n");
  printf("\n1:\t[");
  for (int i = 0; i < DIRECT_PAGES; i++) {
    printf("%d", ind.directPage[i]);
    if (i != DIRECT_PAGES - 1) {
      printf(",\t");
    }
    addrs[lastIndex++] = ind.directPage[i];
  }
  printf("]\n");

  // Print the indirect pages' contents
  int siblingsAtLevel = DIRECT_PAGES;
  for (int level = ind.treeHeight - 2; level > 0; level--) {
    printf("\n****************** LEVEL: %d **********************\n", level);
    // Get each page at this level
    for (int sibNum = 0; sibNum < siblingsAtLevel; sibNum++) {
      // Print the page's contents
      struct fullPage page;
      int pAddr = addrs[curIndex++];
      readNAND(&page, pAddr);
      int* contents = (int*) page.contents;

      printf("%d:\t[", sibNum);
      for (int i = 0; i < INDIRECT_PAGES; i++) {
	printf("%d", contents[i]);
	if (i != siblingsAtLevel - 1) {
	  printf(",\t");
	}
	addrs[lastIndex++] = contents[i];
      }
      printf("]\n");
    }
    
    siblingsAtLevel *= INDIRECT_PAGES; 
  }
}


/************************************************************
 *
 * Descriptor kept for each open file
 *
 ***********************************************************/
typedef struct openFile {
  int currentOpens;   /* Count of number of active opens */

  /* descriptor for log holding first/main extent of file */
  activeLog mainExtentLog;

  /* Pointer to array of descriptor for active extents
     reference through the file's triple indirect page
     (or NULL). If used, entries in the array will be
     NULL until an extent is referenced.
  */
  activeLog *(tripleExtents[EXTENT_PAGES]);

  /* Current inode for the associated file */
  struct inode inode;
  // TODO: This will be used in xmp_open and create() instead of the struct inode
  cacheEntry inodeCacheEntry;

  // Linked List fields for caching
  cacheEntry dataHead;     /* Head pointer to the list of cached data pages */
  cacheEntry metaHead;     /* Head pointer to the list of cached indirect pages
			      that are the direct parents of the dirty data pages */

  // Linked List fields for LRU File list
  openFile lruFileNext;
  openFile lruFilePrev;

  //virtual addr of inode, ie. file ID number
  page_vaddr address;

  /*flag to signify whether the file has been modified
    in xmp_write when writing to the file, must set this
    flag to 1
  */
  int modified;

} * openFile;

/* Add the entry to the tail of the data file list */ 
void openFile_addDataPage(openFile file, cacheEntry entry) {
  // TODO: Make this a doubly linked list

  // Only add if it doesn't already exist in the list
  if (entry->fileDataNext == NULL && entry->fileDataPrev == NULL) {
    entry->dataNext = file->dataHead;
    file->dataHead = entry;
  }
}

/* Add entry to the tail of the file's metadata list */
void openFile_addIndirectPage(openFile file, cacheEntry entry) {
  // TODO: Make this a doubly linked list

  // Only add if it doesn't already exist in the list
  if (entry->metadataNext == NULL && entry->metadatPrev == NULL) {
    entry->metadataNext = file->metadataHead;
    file->metadataHead = entry;
  }
}

/* Remove from global LRU Data List */
cacheEntry lru_removeDataPage(addressCache cache, cacheEntry current) {
  if (current == cache->lruDataHead) {
    // If head, then change the head to point to next
    cache->lruDataHead = current->fileDataNext;
  } else {
    // Otherwise bypass current
    current->lruDataPrev->lruDataNext = current->fileDataNext;
  }

  if (current == cache->lruDataTail) {
    // Change the last to point to previous link
    cache->lruDataTail = current->lruDataPrev;
  } else {
    current->fileDataNext->lruDataPrev = current->lruDataPrev;
  }
}

/* Remove cacheEntry from OpenFile Data List */
void openFile_removeDataPage(openFile file) {
  cacheEntry next = file->dataHead->fileDataNext;
  file->dataHead = next;
}

/* Write out all data pages in the file's data list */
void openFile_flushDataPages(openFile file, addressCache cache) {
  cacheEntry current = file->dataHead;
  cacheEntry next;
  
  while (current != NULL) {
    next = current->fileDataNext;

    lru_removeDataPage(cache, current);
    openFile_removeDataPage(file);

    // Move to next node
    free(current);
    current = next;
  }
}

void openFile_flushIndirectPages(openFile file, addressCache cache) {
  cacheEntry current = file->metadataHead;

  // Use the metadata linked list as the queue of dirty pages
  while (current != NULL) {
    cacheEntry next = current->fileMetadataNext;
    
    // Write out current metadata page to NAND
    if (current->dirty) {
      writeablePage wp = current->writeablePage;
      allocateFreePage(wp, current->key->file->mainExtentLog);
      writeNAND(&wp->nandPage, wp->address, 0);
    }
    
    // Remove current metadata page from openFile
    file->metadataHead = next;

    updateParentPageInCache(current->key, wp->address, current->dirty);

    // Move to next node
    free(current);
    current = next;
  }
}




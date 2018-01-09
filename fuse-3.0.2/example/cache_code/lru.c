/*************************************************************
 *
 * LRU DATA PAGE LIST OPERATIONS
 *
 *************************************************************/

/* Is only called when entry already exists in the LRU list 
   Move an existing cacheEntry in the LRU Data List to the head */
void lru_updateDataHead(addressCache cache, cacheEntry entry) {
  // Move the entry to the head position
  cacheEntry prev = entry->lruDataPrev;
  cacheEntry next = entry->lruDataNext;

  if (prev != NULL) {
    prev->lruDataNext = next;
    prev->lruDataPrev = entry;
  } else {
    // Entry is already at head, so do nothing
    return;
  }

  // There's at least two entries 
  if (next != NULL) {
    next->lruDataPrev = prev;
  } else {
    // Entry is last, so update tail to be prev
    cache->lruDataTail = prev;
  }

  entry->lruDataNext = cache->lruDataHead;
  entry->lruDataPrev = NULL;
  
  cache->lruDataHead = entry;
}

/* PRE: Is only called when entry does not already exist in LRU list 
*  POST: Add a cacheEntry to the head of the LRU Data List */
void lru_addDataPage(addressCache cache, cacheEntry entry) {
  // Add entry to the head of the LRU list
  cacheEntry curHead = cache->lruDataHead;

  entry->lruDataNext = curHead;
  entry->lruDataPrev = NULL;

  if (curHead == NULL) {
    // List is empty, set tail
    cache->lruDataTail = entry;
  } else {
    curHead->lruDataPrev = entry;
  }
  
  cache->lruDataHead = entry;
}

/* Remove from global LRU Data List */
cacheEntry lru_removeDataPage(addressCache cache, cacheEntry current) {
  if (current == cache->lruDataHead) {
    // If head, then change the head to point to next
    cache->lruDataHead = current->lruDataNext;
  } else {
    // Otherwise bypass current
    current->lruDataPrev->lruDataNext = current->lruDataNext;
  }

  if (current == cache->lruDataTail) {
    // Change the last to point to previous link
    cache->lruDataTail = current->lruDataPrev;
  } else {
    current->lruDataNext->lruDataPrev = current->lruDataPrev;
  }
}


/*************************************************************
 *
 * LRU FILE LIST OPERATIONS
 *
 *************************************************************/

/* Only called when file already exists in the LRU file list 
   Move an existing openFile to the head of the LRU File List */
void lru_updateFileHead(addressCache cache, openFile file) {
  openFile prev = file->lruFilePrev;
  openFile next = file->lruFilNext;
  
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
    cache->lruFileTail = prev;
  }

  file->lruFileNext = cache->lruFileHead;
  file->lruFilePrev = NULL;
  
  cache->lruFileHead = file;
}

/* Only called when file doesn't exist in the LRU file list 
   Add an openFile to the head of the LRU File List */
void lru_addFile(addressCache cache, openFile file) {
  openFile curHead = cache->lruFileHead;
  file->lruFileNext = curHead;
  file->lruFilePrev = NULL;
      
  if (curHead != NULL) {
    curHead->lruFilePrev = file;
  } else {
    // List is empty, set tail
    cache->lruFileTail = file;
  }
  
  cache->lruFileHead = file;
}

/* Remove an openFile from the LRU File list */
void lru_removeFile(addressCache cache, openFile file) {
  openFile next = file->lruFileNext;

  // Remove from global LRU File List
  if (file == cache->lruFileHead) {
    // If head, then change the head to point to next
    cache->lruFileHead = next;
  } else {
    // Otherwise bypass current
    file->lruFilePrev->lruFileNext = next;
  }

  if (file == cache->lruFileTail) {
    // Change the last to point to previous link
    cache->lruFileTail = file->lruFilePrev;
  } else {
    next->lruFilePrev = file->lruFilePrev;
  }

  free(lruFile);
}

/* OpenFile may or may not be in the LRU list 
   Brings the given file to the head of the LRU File List */
void lru_updateFile(addressCache cache, openFile file) {
  if (contains(cache->lruFileHead, file)) { // TODO: Implement contains function 
    lru_updateFileHead(cache, file);
  } else {
    lru_addFile(cache, file);
  }
}

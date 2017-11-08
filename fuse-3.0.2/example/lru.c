
void remove_LruFileList(addressCache cache, openFile file) {
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

/* Is only called when entry does not already exist in LRU list */
void addTo_LruDataList(addressCache cache, cacheEntry entry) {
  // Add entry to the head of the LRU list
  cacheEntry curHead = cache->lruDataHead;
  entry->lruDataNext = curHead;
  entry->lruDataPrev = NULL;

  if (curHead != NULL) {
    curHead->lruDataPrev = entry;
  } else {
    // List is empty, set tail
    cache->lruDataTail = entry;
  }
  
  cache->lruDataHead = entry;
}

/* Is only called when entry already exists in the LRU list */
void updateHead_LruDataList(addressCache cache, cacheEntry entry) {
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

/* Only called when file already exists in the LRU file list */
void updateHead_LruFileList(addressCache cache, openFile file) {
  // Move file to the head of the list
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

/* Only called when file doesn't exist in the LRU file list */
void addTo_LruFileList(addressCache cache, openFile file) {
  // Add file to the head of the list
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

/* OpenFile may or may not be in the LRU list */
void update_LruFileList(addressCache cache, openFile file) {
  if (contains(cache->lruFileHead, file)) {
    updateHead_LruFileList(cache, file);
  } else {
    addTo_LruFileList(cache, file);
  }
}

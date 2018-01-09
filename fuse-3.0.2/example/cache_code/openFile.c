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

  /* Head pointer to the list of cached data pages */
  cacheEntry dataHead;         
  cacheEntry dataTail;
  /* Head pointer to the list of cached metadata pages. As of now, this list contains all of
     the metadata pages without any level differentiation. */
  cacheEntry metadataHead;     
  cachEntry metadataTail;

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


/*************************************************************
 *
 * DATA PAGE LIST OPERATIONS
 *
 *************************************************************/

/* Add the entry to the tail of the data file list */ 
void openFile_addDataPage(openFile file, cacheEntry entry) {
  if (entry->fileDataNext == NULL && entry->fileDataPrev == NULL) {
    cacheEntry tail = file->dataTail;

    entry->fileDataPrev = tail;
    entry->fileDataNext = NULL;

    if (tail == NULL) {
      // List was empty, set head
      file->dataHead = entry;
    } else {
      tail->fileDataNext = entry;
    }

    file->dataTail = entry;
  }
}

/* Remove cacheEntry from OpenFile Data List */
void openFile_removeDataPage(openFile file, cacheEntry entry) {
  cacheEntry head = file->dataHead;
  cacheEntry tail = file->dataTail;

  if (entry == head) {
    file->dataHead = entry->fileDataNext;
  } else {
    entry->fileDataPrev->fileDataNext = entry->fileDataNext;
  }

  if (entry == tail) {
    file->dataTail = entry->fileDataPrev;
  } else {
    entry->fileDataNext->fileDataPrev = entry->fileDataPrev;
  }
}

/* Write out all data pages in the file's data list */
void openFile_flushDataPages(openFile file, addressCache cache) {
  cacheEntry current = file->dataHead;
  cacheEntry next;
  
  while (current != NULL) {
    next = current->fileDataNext;

    lru_removeDataPage(cache, current);
    openFile_removeDataPage(file, current);

    // Move to next node
    free(current);
    current = next;
  }
}

/*************************************************************
 *
 * META-DATA PAGE LIST OPERATIONS
 *
 *************************************************************/

/* Add entry to the tail of the file's metadata list */
void openFile_addMetadataPage(openFile file, cacheEntry entry) {
  // Only add if it doesn't already exist in the list
  if (entry->fileMetadataNext == NULL && entry->fileMetadataPrev == NULL) {
    cacheEntry tail = file->metadataTail;
    if (tail == NULL) {
      // This is the first entry in the list
      file->metadataTail = entry;
      file->metadataHead = entry;
    } else {
      tail->fileMetadataNext = entry;
      entry->fileMetadataPrev = tail;
      file->metadataTail = entry;
    }
  }
}

/* Remove the entry from the file's metadata pages list */
void openFile_removeMetadataPage(openFile file, cacheEntry entry) {
  cacheEntry head = file->metadataHead;
  cacheEntry tail = file->metadataTail;

  if (entry == head) {
    file->metadataHead = entry->fileMetadataNext;
  } else {
    entry->fileMetadataPrev->fileMetadataNext = entry->fileMetadataNext;
  }

  if (entry == tail) {
    file->metadataTail = entry->fileMetadataPrev;
  } else {
    entry->fileMetadataNext->fileMetadataPrev = entry->fileMetadataPrev;
  }
}

void openFile_flushMetadataPages(openFile file, addressCache cache) {
  // Flush dirty metadata pages in lowest-to-highest-level order
  int max_level = file->inode->treeHeight;
  for (int level = 1; level < max_level; level++) {
    cacheEntry current = file->metadataHead;

    // Traverse through the whole list each time
    while (current != NULL) {
      cacheEntry next = current->fileMetadataNext;

      if (current->key->levelsAbove == level) {
	// Write out current metadata page to NAND
	if (current->dirty) {
	  writeablePage wp = current->writeablePage;
	  allocateFreePage(wp, current->key->file->mainExtentLog);
	  writeNAND(&wp->nandPage, wp->address, 0);
	}
    
	// Remove current metadata page from openFile
	openFile_removeMetadataPage(file, current);

	// Update the parent page with the written information
	updateParentPage(current->key, wp->address, current->dirty);

	free(current);
      }
      
      current = next;
    }
  }
}





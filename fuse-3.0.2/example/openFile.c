#include <stdlib.h>
#include <stdio.h>

#include "loggingDiskFormats.h"
#include "fuseLogging.h"
#include "cacheStructs.h"
#include "cache.h"

/*************************************************************
 *
 * DATA PAGE LIST OPERATIONS
 *
 *************************************************************/

/* Check if the openFile contains a data page */
int containsDataPage(openFile file, cacheEntry entry) {
  cacheEntry cur = file->dataHead;
  while (cur != NULL) {
    if (entry == cur) {
      return 1;
    } else {
      cur = cur->fileDataNext;
    }
  }

  return 0;
}

/* Add the entry to the tail of the data file list */ 
void openFile_addDataPage(openFile file, cacheEntry entry) {
  // Don't add if already included
  if (containsDataPage(file, entry))
    return;
  
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


/*************************************************************
 *
 * META-DATA PAGE LIST OPERATIONS
 *
 *************************************************************/

/* Check if the openFile contains a data page */
int containsMetadataPage(openFile file, cacheEntry entry) {
  cacheEntry cur = file->metadataHead;
  while (cur != NULL) {
    if (entry == cur) {
      return 1;
    } else {
      cur = cur->fileMetadataNext;
    }
  }

  return 0;
}

/* Add entry to the tail of the file's metadata list */
void openFile_addMetadataPage(openFile file, cacheEntry entry) {
  // Only add if it doesn't already exist in the list
  if (containsMetadataPage(file, entry)) {
    return;
  }
  
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

/*************************************************************
 *
 * PRINT OPERATIONS
 *
 *************************************************************/

void openFile_printData(openFile file) {
  cacheEntry current = file->dataHead;
  int i = 0;
  while (current != NULL) {
    cacheEntry next = current->fileDataNext;    
    current = next;
    i++;
  }

  printf("\nThis file has %d data pages in cache.", i);
}


void openFile_printMetadata(openFile file) {
  cacheEntry current = file->metadataHead;
  int i = 0;
  while (current != NULL) {
    cacheEntry next = current->fileMetadataNext;    
    current = next;
    i++;
  }

  printf("\nThis file has %d metadata pages in cache.", i);
}

// Joseph Oh

#include "fuseLogging.h"

/************************************************************
 *
 * Entries stored in the cache
 *
 ***********************************************************/

typedef struct cacheEntry {
  // Content fields
  struct pageKey* key;
  struct writeablePage* wp;
  struct cacheEntry* next;
  int dirty;

  // Global LRU data pages list
  struct cacheEntry* lruDataNext;
  struct cacheEntry* lruDataPrev;

  // OpenFile's data pages list
  struct cacheEntry* fileDataNext;
  struct cacheEntry* fileDataPrev; 
  
  // OpenFile's dirty metadata pages list
  struct cacheEntry* fileMetadataNext;
  struct cacheEntry* fileMetadataPrev;
  
} *cacheEntry;

/************************************************************
 *
 * Descriptor kept for each open file
 *
 ***********************************************************/
typedef struct openFile {
  int currentOpens;   /* Count of number of active opens */

  /* descriptor for log holding first/main extent of file */
  struct activeLog* mainExtentLog;

  /* Pointer to array of descriptor for active extents
     reference through the file's triple indirect page
     (or NULL). If used, entries in the array will be
     NULL until an extent is referenced.
  */
  // TODO: CHECK THE POINTERS 
  activeLog *(tripleExtents[EXTENT_PAGES]);

  /* Current inode for the associated file */
  struct inode inode;

  /* Head pointer to the list of cached data pages */
  struct cacheEntry* dataHead;         
  struct cacheEntry* dataTail;
  /* Head pointer to the list of cached metadata pages. As of now, this list contains all of
     the metadata pages without any level differentiation. */
  struct cacheEntry* metadataHead;     
  struct cacheEntry* metadataTail;

  // Linked List fields for LRU File list
  struct openFile* lruFileNext;
  struct openFile* lruFilePrev;

  //virtual addr of inode, ie. file ID number
  page_vaddr address;

  /*flag to signify whether the file has been modified
    in xmp_write when writing to the file, must set this
    flag to 1
  */
  int modified;

} * openFile;

/************************************************************
 *
 * Three-tuple Key for the hash
 *
 ***********************************************************/

typedef struct pageKey {
  struct openFile* file;
  int dataOffset;
  int levelsAbove;
  
} *pageKey;

/************************************************************
 *
 * The cache that holds writeablePages
 *
 ***********************************************************/

typedef struct addressCache {
  int MAX_SIZE;               
  int size;
  
  struct cacheEntry** table;  /* Actual data table of the cacheEntries */

  // Global LRU data pages list
  struct cacheEntry* lruDataHead;     
  struct cacheEntry* lruDataTail;
  
} *addressCache;

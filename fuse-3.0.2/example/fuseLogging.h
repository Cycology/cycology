
/*************************************************************
 *
 * Descriptions of run time structures used by FUSE implementation
 * of multi-log log structured file system "CYCOLOGY".
 *
 **************************************************************/

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

/*************************************************************
 *
 * Descriptors for active logs
 *
 ************************************************************/
typedef struct activeLog {
  page_addr nextPage;    /* Address of next free page */
  page_addr lastBlock;   /* Address of the last block allocated
			    to this log */
  int lastBlockErases;        /* erase count of the last block allocated 
			    to this log */
  struct logHeader logH;  /* Mirror of log header from store */
  int activeFileCount;   /* The number of files in this log that are active */
} * activeLog;


/************************************************************
 *
 * Descriptor kept for each open file
 *
hel ***********************************************************/
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
  // TODO: UNCOMMENT  activeLog *(tripleExtents[EXTENT_PAGES]);

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
  int siblingNum;
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

/***************************************************************
 *
 * Mapping from file id number to virtual addresses of logHeader
 * containing that specific file's inode. An entry of
 * 0 indicates an unused logical address.
 *
 **************************************************************/
typedef struct addrMap {
  int size;          /* Number of usable virtual addresses */
  int freePtr;       /* Points to next avaible slot in map */
  page_addr map[];  /* The mapping */
} * addrMap;

/*
 * This structure will hold a fullPage as well as its physical page address
 */
typedef struct writeablePage {
  struct fullPage nandPage;
  page_addr address;
  
} *writeablePage;


/*************************************************************
 *
 * struct holding flag and openFile; stored in fi->fh
 *
 **************************************************************/

typedef struct log_file_info{
  int flag;
  openFile oFile;
  } *log_file_info;

/**************************************************************
 *
 * Used to maintain a cache of active pages from the store
 *
 *************************************************************/
/*typedef struct pageBuffer {
  page_addr address;
  struct pageBuffer * nextLRU, * prevLRU;
  char page[PAGESIZE];
  } * pageBuffer; */

typedef struct fileCache {
  int size;                                      
  struct cacheEntry * lruFileHead;
  struct cacheEntry * lruFileTail;
  openFile openFileTable[PAGEDATASIZE/4 - 2]; //table of ptrs to struct openFiles
  
  } * fileCache;

/****************************************************************
 *
 * Keeps track of variables important for page/block calculations
 * carried out by NAND library functions.
 *
 ****************************************************************/
typedef struct nandFeatures{
  int numBlocks;               //Number of blocks in NAND
  int memSize;                 //Total size of NAND memory
} *nandFeatures;

/*************************************************************
 *
 * Root of static state for file system
 *
 *************************************************************/

typedef struct CYCstate {
  char * rootPath;    /* Path to the root of the file system used
			 to hold the directory structure
		      */
  char * storePath;   /* Path to the file used as a virtual NAND
			 memory holding data and meta-data pages
			 for the file system.
		      */

  //Hold features of the virtual NAND
  struct nandFeatures nFeatures;

  /* The current version of the virtual address mapping table */
  struct addrMap * vaddrMap;

  /* The cache of pages from disk */
  struct addressCache * addr_cache;
  
  /* The cache of open files */
  struct fileCache * file_cache;

  //partially and completely free lists
  struct freeList lists;

} * CYCstate;

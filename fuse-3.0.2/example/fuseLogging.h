
/*************************************************************
 *
 * Descriptions of run time structures used by FUSE implementation
 * of multi-log log structured file system "CYCOLOGY".
 *
 **************************************************************/



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
 * Descriptors for active logs
 *
 ************************************************************/
typedef struct activeLog {
  page_addr nextPage;    /* Address of next free page */
  page_addr lastBlock;   /* Address of the last block allocated
			    to this log */
  int lastErases;        /* erase count of the last block allocated 
			    to this log */
  struct logHeader log;  /* Mirror of log header from store */
  int fileCount;         /* The number of files held in this log */
} * activeLog;

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

/*
typedef struct pageCache {
  int size;                                      
  pageBuffer headLRU, tailLRU;
  openFile openFileTable[PAGEDATASIZE/4 - 2]; //table of ptrs to struct openFiles
  } * pageCache; */

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

  /* The cache of pages from the store */
  struct pageCache * cache;

  //partially and completely free lists
  struct freeList lists;

} * CYCstate;

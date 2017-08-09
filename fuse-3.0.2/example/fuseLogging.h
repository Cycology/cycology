#include "loggingDiskFormats.h"

/*************************************************************
 *
 * Descriptions of run time structures used by FUSE implementation
 * of multi-log log structured file system "CYCOLOGY".
 *
 **************************************************************/

/*************************************************************
 *
 * struct holding flag and openFile; stored in fi->fh
 *
 **************************************************************/
typedef struct log_file_info{
  int flag;
  openFile oFile;
} *log_file_info;

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

/*************************************************************
 *
 * Descriptors for active logs
 *
 ************************************************************/
typedef struct activeLog {
	page_addr nextPage;    /* Address of next free page */
	block_addr last;       /* Address of the last block allocated
				  to this log */
        int lastErases;        /* erase count of the last block allocated 
				  to this log */
	struct logHeader log;  /* Mirror of log header from store */
} * activeLog;

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

        //virtual addr of inode, ie. file ID number
        page_vaddr address;

        /*flag to signify whether the file has been modified
          in xmp_write when writing to the file, must set this
          flag to 1
        */
        int modified;

} * openFile;


/**************************************************************
 *
 * Used to maintain a cache of active pages from the store
 *
 *************************************************************/
typedef struct pageBuffer {
	page_addr address;
	struct pageBuffer * nextLRU, * prevLRU;
	char page[PAGESIZE];
} * pageBuffer;


typedef struct pageCache {
	int size;                                      /* Number of active pages */
	pageBuffer headLRU, tailLRU;
        openFile openFileTable[PAGEDATASIZE/4 - 2]; //table of ptrs to struct openFiles
} * pageCache;

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

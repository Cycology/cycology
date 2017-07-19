#include <sys/types.h>

/* Size of data are in a page */
#define PAGESIZE 1024
#define BLOCKSIZE 16

/* The number of pointers to indirect pages that can fit into an extend descriptor page */
/*            This must leave room for log and extent headers.                          */
#define EXTENT_PAGES 128

/* The number of pointers directly to data pages that can fit into an inode.            */
#define DIRECT_PAGES 16

/* The maximum number of files that can be merged into a single log.                    */
#define MAX_FILES_IN_LOG 8

/* Type used to hold logical page addresses (i.e., pages that must be interpreted
   relative to the logical page map).  */
typedef int page_vaddr;

/* Type used to hold physical page addresses.    */
typedef int page_addr;

/* Type used to hold physical block addresses (could use page address and divide
   by block size, but distinguishing needed resolution seems more logical). */
typedef int block_addr;

/* Type used to store count of pages in a file */
typedef int pagecnt_t;

/* Because it would be painful to garbage collect a very large log, the maximum
   size of a log must be less than the maximum size of a file. So, large files will
   be allocated using pages from several logs. Each log associated with a file will
   hold data from a contiguous range of file pointer offsets. The boundaries between
   these ranges will correspond to boundaries between regions that can be described
   by a double-indirect page and its children. We call such regions extents.

   The file's inode, its direct data pages and all pages accessed using its single
   indirect page and double indirect page will form its first extent. Each of the
   pointers in the file's triple indirect page (which will be allocated within the
   file's primary log) will point to a double indirect page describing a separate extent.
   The double indirect pages pointed to by a triple indirect page will therefore not only
   hold an array of single indirect page pointres. Each double indirect page (except
   the file's first one) will hold a small structure identifying the
   file and log with which it is associated.
*/
struct extentHeader {
	page_vaddr       inode;      /* inode of file owning this extent */
	page_vaddr       lnode;      /* lnode of log holding this extent */
	page_vaddr       self;       /* virtual address of this extent   */
	page_addr        indirect[EXTENT_PAGES];  /* addresess of single indirect pages */
};


struct inode {
	mode_t		   i_mode;     /* See STAT command man page for
						      bit functions  */

	uid_t		    i_uid;      /* User and group id numbers for  */
	gid_t		    i_gid;      /* this file.  */

	unsigned int	    i_flags;    /* Probably unnecessary   */

	int	            i_file_no;   /* Unique file number, originally a page_vaddr */
	page_vaddr	    i_log_no;    /* Number of log associated with 1st extent */

	unsigned int        i_links_count;         /* Number of hard links to file */

	struct timespec	    i_atime;    /* Access, modification and     */
	struct timespec	    i_mtime;    /* status change times for the file  */
	struct timespec	    i_ctime;

    pagecnt_t           i_pages;    /* Number of active/allocated pages ? */
    loff_t              i_size;     /* File size in bytes */


    page_addr          indirect;   /* Address of single indirect block */
    page_addr          doubleInd;  /* Address of double indirect block */
    page_vaddr          tripleInd;  /* Address of triple indirect extent hdr */

    /* address of first data pages for file */
    page_addr          directPage[DIRECT_PAGES];
};


/* All the active blocks in the file system are organized into logs. A given log can
   be associated with a) a small collection of files whose data is stored in the log,
   b) a single extent of a large file whose data is stored in the log, c) a meta-data
   structure such as a snapshot of the virtual page number map or the journal of
   file system updates.

   For each log, we need to keep track of what entity/entities the log is associated
   with and statistics on space utilization within the log needed to determine
   when to scan logs to recover unused/unusable space.
*/
struct logHeader {
  unsigned long erases; /* Sum of erases of all blocks in log */
  page_vaddr logId;     /* Logical page address of the page holding this header  */
  block_addr prev;      /* Address of previous block allocated to this log */
  block_addr first;     /* Address of first block allocated to this log */
  unsigned int active;  /* Total number of active pages holding file data */
  unsigned int total;   /* Total number of blocks */
  short logType;        /* Does this log hold, files, and extent, or meta-data */
#define LTYPE_FILES 1
#define LTYPE_EXTENT 2
#define LTYPE_VADDRMAP 3
#define LTYPE_JOURNAL 4
	/* The following union depends on logType for discrimination. It holds
	   the latest version for the descriptor of the entity associated with
	   this log.
	*/
	union logType {
		struct {
		  int fileCount;                 //#files in this log
		     page_vaddr fileId[MAX_FILES_IN_LOG];
		     struct inode fInode;
		} file;
		struct extentHeader extent;
	} content;

} *logHeader;

//points to first page of partially and completely used free lists
typedef struct freeList {
  page_addr partial;
  page_addr complete;
} *freeList;

/* This page will hold information required to restart the system. In
   our experimental implementation, it will simply be written on page
   0. In a more realistic implementation it would need to migrate around
   the memory's address range while a wandering tree rooted at page 0
   would be used to record its latest location.
*/
typedef struct superPage {
         // free lists
         struct freeList freeLists;
 
         /* While running, the system will record information about critical
	   meta-data updates in a journal whose blocks will form a single
	   log. This field will hold the address of the latest log descriptor
	   recorded for this log.
	*/
	page_addr latest_journal;

	/* While running, the system will constantly (but slowly) write a
	   copy of the contents of the mapping from logical addresses for
	   inodes, Lnodes and extent headers to persistent store. This
	   field will hold the address of the latest log descriptor recorded
	   for the log holding the latest copy of this map.
	*/
	page_addr latest_vaddr_map;


	/* Depending on whether the system is shutdown deliberately or terminates
	   unexpectedly, the copy of the virtual address map pointed to by
	   latest_vaddr_map may or may not be complete. If it is incomplete, the
	   system will need to reconstruct the rest of the map using an older
	   copy and entries written to the latest and previous journal logs.
	   The next two fields provide access to these structures.
	*/
	page_addr prev_vaddr_map;
	page_addr prev_journal;
  

} *superPage;

/* Every page on a NAND memory includes an overhead area used to store information
   beyond the data held in the page. This is typically used for error
   detecting/correcting codes that help detect when a block wears out.
   In this system, we will also use this area to hold meta-data about the
   contents of each page and the entire block.
*/

#define PAGEDATASIZE 1024

#define PTYPE_DATA 1               /* Contains file data */
#define PTYPE_INODE 2              /* Contains an inode (and associated Lnode) */
#define PTYPE_INDIRECT 3           /* Contains a single indirect page */
#define PTYPE_DOUBIND_PRIMARY 4    /* Contains a file's first double-ind page */
#define PTYPE_TRIPIND 5            /* Contains a file's only triple indirect page */
#define PTYPE_DOUBIND_EXTENT 6     /* Contains a double-ind page describing an
				      separate extent pointed to by a file's
				      triple indirect page.
				   */


typedef struct fullPage {
	char contents[PAGEDATASIZE];
	int eraseCount;           /* In the first page of a block, this field will
				     hold the erase count for the block */

	page_addr nextLogBlock;  /* In last and next to last pages of a block,
				     this field will hold the address of the
				     next page of the log containing the pages
				  */

	int nextBlockErases;     /* The number of times the block pointed to by
				    nextLogBlock has been erased (this is used
				    to preserve this info against lost between
				    the block's erasure and the writing of its
				    first page).
				 */
	union metaData {
		/* the contents of this union are determined by pageType's value */
		/* (At this point, it is not at all clear that it is worth having this union) */

		struct dataPosition { /* if PTYPE_DATA or any indirect page */
			page_vaddr inode;
			int offset;   /* if the type is PTYPE_DATA, this will be
					 the offset within the file to the data
					 in contents. If this is an indirect page
					 of any level, this will hold the offset
					 to the first page of data covered by
					 the indirect page.
				      */
		} dataPage;




	} meta;

	char pageType;    /* one of the PTYPE constants define above */

} *fullPage;

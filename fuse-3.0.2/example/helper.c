#define FUSE_USE_VERSION 30

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE

//#include <fuse.h>

#ifdef HAVE_LIBULOCKMGR
#include <ulockmgr.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <dirent.h>
#include <errno.h>
#include <sys/time.h>
#ifdef HAVE_SETXATTR
#include <sys/xattr.h>
#endif
#include <sys/file.h> /* flock(2) */

#include "vNANDlib.h"
#include "helper.h"


void initCYCstate(CYCstate state)
{
  //read in superBlock
  char page[sizeof (struct fullPage)];
  readNAND(page, 0);
  superPage superBlock;
  superBlock = page;
    

  //init freeLists
  state->lists = &(superBlock->freeLists);
  
  //init vaddrMap
  readNAND(page, superBlock->latest_vaddr_map);
  addrMap map = (addrMap) page;
  state->vaddrMap = (vaddrMap)malloc(sizeof (struct addrMap) + map->size*sizeof(page_addr));
  memcpy(state->vaddrMap, page, sizeof (struct addrMap) + map->size*sizeof(page_addr));

  //someday, we need to consider the vaddrmap occupying multiple pages so we'd need a loop to copy stuff
  
  //init cache
  state->cache = (pageCache)malloc(sizeof (struct pageCache));
  state->cache->size = 0;
  state->cache->headLRU = NULL;
  state->cache->tailLRU = NULL;
  memset(state->cache->openFileTable, 0, (PAGEDATASIZE/4 - 2));
}

//Create an inode for a new file
void initInode(struct inode ind, mode_t mode, int fileID)
{
	ind.i_mode = mode;
	ind.i_file_no = fileID; //next free slot in vaddrMap
	ind.i_links_count = 0;
	ind.i_pages = 0;
	ind.i_size = 0;
}

void initLogHeader(struct logHeader logH, unsigned long erases,
		   page_vaddr logId, block_addr first,
		   short logType)
{
  logH.erases = erases;      //eraseCount = that of 1 block for now
  logH.logId = logId;
  logH.first = first;
  logH.prev = -1; //since we can't put NULL
  logH.active = 0; //file is empty initially, logHeader does not count as active page
  logH.total = 1; // we remove one block from the pfree list
  logH.logType = logType;
}

void initOpenFile(openFile oFile, struct activeLog log,
		  struct inode ind, page_vaddr fileID)
{
  oFile->currentOpens = 1;
  oFile->mainExtentLog = &log;
  oFile->inode = ind;
  oFile->address = fileID;
}

//return the next free slot in the virtual address map
//if there's only 1 free slot left, freePtr = 0
int getFreePtr(addrMap map)
{
  int ptr = map->freePtr;
  map->freePtr = abs(map->map[ptr]);
  return ptr;
}

//get eraseCount stored at this fullPage
int getEraseCount(page_addr k)
{
  struct fullPage page;
  readNAND(&page, k);
  return page.eraseCount;
}

void writeCurrLogHeader(openFile oFile)
{
  page_addr page = oFile->mainExtentLog->nextPage;
  struct logHeader logH = oFile->mainExtentLog->log;
  char buf[sizeof (struct fullPage)];
  readNAND(buf, page);
  memcpy(buf, &log, sizeof (struct logHeader));
  writeNAND(buf, page, 0);
}

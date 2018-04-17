// 2018

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

#include "loggingDiskFormats.h"
#include "fuseLogging.h"
#include "vNANDlib.h"
#include "helper.h"


void initCYCstate(CYCstate state)
{
  mcheck(NULL);
  //read in superBlock
  struct fullPage page;
  readNAND( &page, 0);
  superPage superBlock = (superPage) &page;
    
  //init freeLists
  state->lists = superBlock->freeLists;
  
  //init vaddrMap
  readNAND( &page, superBlock->latest_vaddr_map);
  addrMap map = (addrMap) page.contents;
  int size = sizeof (struct addrMap) + map->size*sizeof(page_addr);
  printf("Map size = %d Allocation size = %d \n", map->size, size);
  state->vaddrMap = (addrMap) malloc(size);

  // The following loop assumes that the vaddr map is stored in sequential pages starting
  // at superBlock->latest_vaddr_map. In reality, the NAND resident copy of the map will
  // ultimately be stored in the pages of a collection of blocks linked together like a
  // log. Thus, this code is temporary.
  
  int pages = ((sizeof (struct addrMap)) +
	       map->size*sizeof(page_addr) + ( PAGESIZE - 1) )/PAGESIZE;

  int remaining = size;
  for ( int p = 0; p < pages; p++ ) {
    readNAND( &page, superBlock->latest_vaddr_map + p );
    memcpy( ((char *) state->vaddrMap) + PAGESIZE, page.contents,
	                 (remaining < PAGESIZE) ? remaining : PAGESIZE  );
    remaining = remaining - PAGESIZE;
  }

  
  //init cache
  state->file_cache = (fileCache) malloc(sizeof (struct fileCache));
  state->file_cache->size = 0;
  state->file_cache->lruFileHead = NULL;
  state->file_cache->lruFileTail = NULL;
  memset(state->file_cache->openFileTable, 0, (PAGEDATASIZE/4 - 2));
}

//Create an inode for a new file
void initInode(struct inode *ind, mode_t mode,
	       page_vaddr fileID, page_vaddr logID)
{
	ind->i_mode = mode;
	ind->i_uid = 0;
	ind->i_gid = 0;
	ind->i_flags = 0;
	ind->i_file_no = fileID; //next free slot in vaddrMap
	ind->i_log_no = logID;
	ind->i_links_count = 0;
	ind->i_pages = 0;
	ind->i_size = 0;
	ind->treeHeight = 2;
	memset(ind->directPage, 0, DIRECT_PAGES * sizeof(page_addr));
}

void initLogHeader(struct logHeader *logH,
		   page_vaddr logId, short logType)
{
  logH->totalErases = 0;      //eraseCount = that of 1 block for now
  logH->logId = logId;
  logH->prevBlock = -1; //since we can't put NULL
  logH->prevBlockErases = 0;
  logH->firstBlock = -1;
  logH->firstBlockErases = 0;
  logH->activePages = 0;
  logH->totalBlocks = 1; // we remove one block from the pfree list
  logH->logType = logType;
  logH->content.file.fileCount = 1;
  memset(logH->content.file.fileId, 0, MAX_FILES_IN_LOG*sizeof(page_vaddr));
}

/*
 * Initializes the active log, given that a logHeader is already written
 */
void initActiveLog(struct activeLog *theLog, page_addr nextFreePage)
{
  // Initialize variables
  theLog->activeFileCount = 0;
  theLog->lastBlock = -1;
  theLog->lastBlockErases = 0;
  theLog->nextPage = nextFreePage;
  
  // if headerPage is 3rd to last, set nextPage to last page of prev block
  if((nextFreePage + 3) % BLOCKSIZE == 0) {
    theLog->nextPage = theLog->logH.prevBlock + (BLOCKSIZE-1);	
  }
  
  // if headerPage is 2nd to last, set nextPage to the first page of new block
  else if((nextFreePage + 2) % BLOCKSIZE == 0) {
    theLog->nextPage = theLog->lastBlock;
  }
  
  // if headerPage is last, set nextPage to 2nd to last page in next block
  else if((nextFreePage + 1) % BLOCKSIZE == 0) {
    theLog->nextPage = theLog->lastBlock + (BLOCKSIZE-2);
  }
  
  // if headerPage is elsewhere
  else
    theLog->nextPage++;
}

void initOpenFile(openFile oFile, struct activeLog *log,
		  struct inode *ind, page_vaddr fileID)
{
  oFile->currentOpens = 0;
  oFile->mainExtentLog = log;
  oFile->inode = *ind;
  oFile->address = fileID;
  oFile->modified = 0;

  oFile->dataHead = NULL;
  oFile->dataTail = NULL;
  oFile->metadataHead = NULL;
  oFile->metadataTail = NULL;
  oFile->lruFileNext = NULL;
  oFile->lruFilePrev = NULL;
}


//return the next free slot in the virtual address map
//if there's only 1 free slot left, freePtr = 0
int getFreePtr(addrMap map)
{
  int ptr = map->freePtr;
  map->freePtr = abs(map->map[ptr]);
  return ptr;
}


//returns the second to last page of the last block that was prev allocated
//returns erase count
//store in page_addr freePage the next freePage of the new block
//this function should be called when the next free page is the second to last
//page of a block
int getFreeBlock(freeList lists, page_addr *freePage, int newFile) {

  int freePageErases;
  struct fullPage buf;

  //if this is for a new file, prioritize pList
  if (newFile == 0 || lists->partialHead == 0) {
    
    // if there is nothing in the partially used free list, use the cList
    /*at some point there would be a check if completeList is empty also*/
    *freePage = lists->completeHead;
    freePageErases = lists->completeHeadErases;
      
    /*read in the last page of block
     *move list head pointer by saving its nextLogBlock and nextBlockErases
     *erase the entire block
     */
    readNAND( &buf, ((*freePage + BLOCKSIZE)/BLOCKSIZE)*BLOCKSIZE - 1);
    lists->completeHead = buf.nextLogBlock;
    lists->completeHeadErases = buf.nextBlockErases;
    page_addr freeNum = *freePage;
    eraseNAND(freeNum/BLOCKSIZE); // TODO: CHECK BLOCK ADDR
    
    //otherwise use the partial list
  } else {
    *freePage = lists->partialHead + 1;
    freePageErases = lists->partialHeadErases;
    
    /*read in the last page of block
     *move list head pointer by saving its nextLogBlock and nextBlockErases
     */
    readNAND( &buf, lists->partialHead);
    //fullPage page = (fullPage) buf;
    lists->partialHead = buf.nextLogBlock;
    lists->partialHeadErases = buf.nextBlockErases;
    //these 2 updates only work if we allow nextLogBlock and nextBlockErases
    //to appear anywhere, even in pages that are not 2nd to last or last
  }

  return freePageErases;
}

writeablePage getFreePage(freeList lists, activeLog log)
{
  writeablePage freePage = (writeablePage)malloc(sizeof (struct writeablePage));
  
  freePage->address = log->nextPage;
  
  // if nextPage is the first page of block, set eraseCount
  if(freePage->address%BLOCKSIZE == 0) {
    freePage->nandPage.eraseCount = log->lastBlockErases;
    log->nextPage++;
  }
  
  // if nextPage is 3rd to last, set nextPage to last page of prev block
  else if((freePage->address + 3) % BLOCKSIZE == 0) {
    log->nextPage = log->logH.prevBlock + (BLOCKSIZE-1);	
  }
  
  // if nextPage is 2nd to last, set nextPage to the first page of new block
  // and save eraseCount in activeLog
  else if((freePage->address + 2) % BLOCKSIZE == 0) {
    page_addr first;
    log->lastBlockErases = getFreeBlock(lists, &first, 0);
    freePage->nandPage.nextLogBlock = first;
    freePage->nandPage.nextBlockErases = log->lastBlockErases;
    log->nextPage = first;
    log->lastBlock = first;
  }
  
  // if nextPage is last, set nextPage to 2nd to last page in next block
  else if((freePage->address + 1) % BLOCKSIZE == 0) {
    log->nextPage = log->lastBlock + (BLOCKSIZE-2);
    freePage->nandPage.nextLogBlock = log->lastBlock;
    freePage->nandPage.nextBlockErases = log->lastBlockErases;
  }
  
  // if nextPage is elsewhere
  else {
    log->nextPage++;
  }
  
  log->logH.activePages++;
  
  return freePage;
}
  
//For now, returns a new log for a new file (modify in the future for retrieving existing log)
activeLog getLogForFile(CYCstate state, page_vaddr fileID,page_addr *logHeaderPage, mode_t mode)
{
  activeLog theLog = (activeLog) malloc(sizeof (struct activeLog));
  
  /* Create the logHeader of the log containing this file */
  page_vaddr logID = getFreePtr(state->vaddrMap); 
  initLogHeader(&(theLog->logH), logID, LTYPE_FILES);  

  /* Create inode for the new file */
  inode ind = &(theLog->logH.content.file.fInode);
  initInode(ind, mode, fileID, logID);

  /* Initialize the logHeader */
  // The logHeader containing the inode should be in the first unused page of a newly allocated block
  // we put fileCount = 1 and index of fileID = 0 because for now, there's only 1 file per log
  theLog->logH.totalErases = theLog->logH.firstBlockErases;
  theLog->logH.firstBlock = *logHeaderPage;
  theLog->logH.firstBlockErases = getFreeBlock(&(state->lists), logHeaderPage, 1);
  theLog->logH.content.file.fileCount = 1;
  theLog->logH.content.file.fileId[0] = fileID;

  /* Initialize activeLog's variables */
  theLog->activeFileCount = 1;
  theLog->lastBlock = theLog->logH.firstBlock;
  theLog->lastBlockErases = theLog->logH.firstBlockErases;
  theLog->nextPage = *logHeaderPage;

  /* Save the log and vaddress in the CYCstate */
  state->file_cache->openFileTable[logID] = (openFile) theLog;
  state->vaddrMap->map[logID] = *logHeaderPage;

  return theLog;
}

//read stub file for fileID
page_vaddr readStubFile(char *fullPath)
{
  //open stub file
  int stubfd = open(fullPath, O_RDWR);
  if (stubfd == -1) {
    perror("CANNOT OPEN STUB FILE IN readStubFile");
    return -1;
  }

  //read the file id number from stub file
  page_vaddr fileID;
  int res = read(stubfd, &fileID, sizeof (int));
  if ( res == -1)
    perror("FAIL TO READ STUB FILE");
  
  close(stubfd);
  return fileID;
}


/* /\* */
/*  * gets the next free page in a log */
/*  * 4 cases: */
/*  * - curPage is 2nd last of block, no previous block */
/*  * - curPage is 2nd last of block, exists previous block */
/*  * - curPage is last of block */
/*  * - curPage is elsewhere */
/*  *\/ */
/* page_addr getNextPageAddr(page_addr curPage, struct activeLog *log) */
/* { */
/*   page_addr nextPage; */
/*   if ((curPage + 2) % BLOCKSIZE == 0) { */
/*     if (log->log.total == 2) //there's no block previous to this one */
/*       nextPage = (log->last)*BLOCKSIZE; */
/*     else */
/*       nextPage = (log->log.prev + 1)*BLOCKSIZE - 1; */
/*   } */
/*   else if ((curPage + 1) % BLOCKSIZE == 0) */
/*     nextPage = (log->last)*BLOCKSIZE; */
/*   else */
/*     nextPage = curPage + 1; */

/*   return nextPage; */
/* } */

/* /\* */
/*  * Checks whether a page is the first, second to last, or last page of a block */
/*  * and fills up the respective fields of the struct fullPage. */
/*  * NB: the pageType field is filled in during the write */
/*  *\/ */
/* void getPageFields(page_addr pageAddr, int *eraseCount, */
/* 		   page_addr *nextLogBlock, int *nextBlockErases) */
/* { */
/*   char buf[sizeof struct fullPage]; */
/*   fullPage page = (fullPage) buf; */

/*   //If first page in the block, read the eraseCount */
/*   if(pageAddr % BLOCKSIZE == 0){ */
/*     readNAND(buf,pageAddr); */
/*     *eraseCount = *(page->eraseCount); */
/*     } */
/*   //If second to last or last page in the block, read the nextLogBlock */
/*   //and nextBlockErases */
/*   else if ((pageAddr % BLOCKSIZE == 1) || (pageAddr % BLOCKSIZE == 2)) { */
/*     readNAND(buf,pageAddr); */
/*     *nextLogBlock = *(page->nextLogBlock); */
/*     *nextBlockErases = *(page->nextBlockErases); */
/*   } */
/* } */



  //THIS IS GOING TO BE PART OF GETFREEPAGE SOMEDAY!
  
  /* //if there is no prev block, make the address -1 but fill in the other fields for reference */
  /* if(alog->log->prev == -1){ */
  /*   alog->log->first = freePage/BLOCKSIZE; */
  /*   thePage->address = -1; */
  /* } */
  /* //if there is a prev block, */
  /* //Save the page address of the second to last page of the */
  /* //previously allocated block */
  /* else { */
  /*   alog->log->prev = alog->last; */
  /*   thePage->address = (alog->log->prev)*BLOCKSIZE + (BLOCKSIZE - 2); */
  /* } */

  /* // Fill in the fields  */
  /* thePage->page.nextLogBlock = freePage; */
  /* thePage->page.nextBlockErases = freePageErases; */
  
  /* //updates the logheader and activelog fields */
  /* alog->log->erases = alog->log->erases + freePageErases; */
  /* alog->log->total = alog->log->total + 1; */
  /* alog->last = freePage/BLOCKSIZE; */
  
/*
 * gets the next free page in a log
 * would have to call getFreeBlock, fill in the fields if necessary
 * updates active pages in logHeader
 */
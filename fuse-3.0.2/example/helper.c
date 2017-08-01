#define FUSE_USE_VERSION 30#ifdef HAVE_CONFIG_H#include <config.h>#endif#define _GNU_SOURCE//#include <fuse.h>#ifdef HAVE_LIBULOCKMGR#include <ulockmgr.h>#endif#include <stdio.h>#include <stdlib.h>#include <string.h>#include <unistd.h>#include <fcntl.h>#include <sys/stat.h>#include <dirent.h>#include <errno.h>#include <sys/time.h>#ifdef HAVE_SETXATTR#include <sys/xattr.h>#endif#include <sys/file.h> /* flock(2) */#include "vNANDlib.h"#include "helper.h"void initCYCstate(CYCstate state){  //read in superBlock  char page[sizeof (struct fullPage)];  readNAND(page, 0);  superPage superBlock = (superPage) page;      //init freeLists  state->lists = superBlock->freeLists;    //init vaddrMap  readNAND(page, superBlock->latest_vaddr_map);  addrMap map = (addrMap) page;  state->vaddrMap = (addrMap) malloc(sizeof (struct addrMap) + map->size*sizeof(page_addr));  memcpy(state->vaddrMap, page, sizeof (struct addrMap) + map->size*sizeof(page_addr));  //someday, we need to consider the vaddrmap occupying multiple pages so we'd need a loop to copy stuff    //init cache  state->cache = (pageCache) malloc(sizeof (struct pageCache));  state->cache->size = 0;  state->cache->headLRU = NULL;  state->cache->tailLRU = NULL;  memset(state->cache->openFileTable, 0, (PAGEDATASIZE/4 - 2));}//Create an inode for a new filevoid initInode(struct inode *ind, mode_t mode,	       page_vaddr fileID, page_vaddr logID){	ind->i_mode = mode;	ind->i_file_no = fileID; //next free slot in vaddrMap	ind->i_log_no = logID;	ind->i_links_count = 0;	ind->i_pages = 0;	ind->i_size = 0;}void initLogHeader(struct logHeader *logH, unsigned long *erases,		   page_vaddr logId, block_addr first,		   short logType){  logH->erases = *erases;      //eraseCount = that of 1 block for now  logH->logId = logId;  logH->first = first;  logH->prev = -1; //since we can't put NULL  logH->active = 0; //file is empty initially, logHeader does not count as active page  logH->total = 1; // we remove one block from the pfree list  logH->logType = logType;}void initActiveLog(struct activeLog *theLog, page_addr headerPage,		   block_addr block){  //if headerPage is the 2nd to last page of the block  //nextPage is last page of prev block  if ((headerPage + 2) % BLOCKSIZE == 0)  //also need to handle case of headerPage being 2nd to last,  //and this log has only 1 block.  	  theLog->last = block;  //want to keep a mirror of logHeader, hence we don't pass in a pointer but a struct}void initOpenFile(openFile oFile, struct activeLog *log,		  struct inode *ind, page_vaddr fileID){  oFile->currentOpens = 1;  oFile->mainExtentLog = log;  oFile->inode = *ind;  oFile->address = fileID;}page_addr getNextPage(page_addr curPage, struct logHeader logH){  page_addr nextPage;  //if curPage is the 2nd to last page of the block  //nextPage is last page of prev block  if ((curPage + 2) % BLOCKSIZE == 0)  //also need to handle case of headerPage being 2nd to last,  //and this log has only 1 block.}//return the next free slot in the virtual address map//if there's only 1 free slot left, freePtr = 0int getFreePtr(addrMap map){  int ptr = map->freePtr;  map->freePtr = abs(map->map[ptr]);  return ptr;}page_addr getFreeBlock(freeList lists){  page_addr logHeaderPage;      char buf[sizeof (struct fullPage)];  // if there is nothing in the partially used free list, use the complete list  if (state->lists.partialHead == 0) {    /*at some point there would be a check if completeList is empty also*/    logHeaderPage = lists->completeHead;    *erases = getEraseCount(logHeaderPage);    //erase this block,    //update completeHead, to value pointed to by last page of the new block    readNAND(buf, ((logHeaderPage + BLOCKSIZE)/BLOCKSIZE)*BLOCKSIZE - 1);    fullPage page = (fullPage) buf;    lists->completeHead = page.nextLogBlock;    eraseNAND((logHeaderPage/BLOCKSIZE)*BLOCKSIZE);  //otherwise use the partial list  } else {    logHeaderPage = lists->partialHead;    *erases = getEraseCount((logHeaderPage/BLOCKSIZE)*BLOCKSIZE);    //update partialHead, to value pointed to by last page of the new block    readNAND(buf, logHeaderPage);    fullPage page = (fullPage) buf;    lists->partialHead = page.nextLogBlock;  }    return logHeaderPage;}//returns a log (new/existing) for a new fileactiveLog getLogForFile(CYCstate state, page_vaddr fileID, struct inode ind, mode_t mode,page_addr *logHeaderPage){  activeLog theLog = (activeLog) malloc(sizeof (struct activeLog));    //next free page where we write logHeader containing the inode  *logHeaderPage = getFreeBlock(&(state->lists));    //create inode for the new file  page_vaddr logID = getFreePtr(state->vaddrMap); //will already have logID if log exists  initInode(&ind, mode, fileID, logID);  //create the logHeader of the log containing this file  initLogHeader(&(theLog->log), erases, logID,		(block_addr) *logHeaderPage/BLOCKSIZE, LTYPE_FILES);    /*we put fileCount = 1 and index of fileID = 0   *because for now, there's only 1 file per log   */  theLog->log.content.file.fileCount = 1;  theLog->log.content.file.fileId[0] = fileID;  theLog->log.content.file.fInode = ind;    //Putting logHeader in activeLog  //nextPage field in activeLog is supposed to be the one next to logheader  initActiveLog(theLog, *logHeaderPage, theLog->log.first);    state->cache->openFileTable[logID] = (openFile) theLog;  state->vaddrMap->map[fileID] = *logHeaderPage;  state->vaddrMap->map[logID] = *logHeaderPage;  return theLog;}//get eraseCount stored at this fullPageint getEraseCount(page_addr k){  struct fullPage page;  readNAND((char *) &page, k);  return page.eraseCount;}//read stub file for fileIDpage_vaddr readStubFile(char *fullPath){  //open stub file  int stubfd = open(fullPath, O_RDWR);  if (stubfd == -1) {    perror("CANNOT OPEN STUB FILE IN readStubFile");    return -1;  }  //read the file id number from stub file  page_vaddr fileID;  int res = read(stubfd, &fileID, sizeof (int));  if ( res == -1)    perror("FAIL TO READ STUB FILE");    close(stubfd);  return fileID;}//write the most current log header to virtualNANDvoid writeCurrLogHeader(openFile oFile){  page_addr page = oFile->mainExtentLog->nextPage;  struct logHeader logH = oFile->mainExtentLog->log;  char buf[sizeof (struct fullPage)];  readNAND(buf, page);  memcpy(buf, &logH, sizeof (struct logHeader));  writeNAND(buf, page, 0);}
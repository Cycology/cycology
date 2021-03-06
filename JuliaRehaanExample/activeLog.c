#define PAGESIZE 1024
#define BLOCKSIZE 64
#define SMFILESIZE 128
#define FUSE_USE_VERSION 30

//These should set to whatever paths we decide on
#define ROOT_PATH //root path
#define STORE_PATH //store path

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE

#include <fuse.h>

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
#include "helper.h"

activeLog newActiveLog(fullPage logPage){
	//Currently passed an entire page, because contains needed information, but this is uncertain- pass virtual address, and read the page here?

	struct fuse_context *context = fuse_get_context();
	CYCstate state = context->private_data;

	logHeader log = &logPage->contents;
	activeLog actLog = (activeLog)malloc(struct activeLog);
	memcpy(actLog->logHeader, log);
	page_addr physAddr = state->vaddrMap->map[log->logId];

	//next to next last page, last block
	if (physAddr%BLOCKSIZE == BLOCKSIZE - 3){
		actLog->last = physAddr/BLOCKSIZE;
		actLog->nextPage = (log->prev+1)*BLOCKSIZE - 1;
	}
	///last page of preceding block
	else if(physAddr%BLOCKSIZE == BLOCKSIZE - 1){
		actLog->last = logPage->nextLogBlock;
		actLog->nextPage = actLog->last*BLOCKSIZE + BLOCKSIZE - 2;
	}
	//next to last page, last block
	else if(physAddr%BLOCKSIZE == BLOCKSIZE - 2){
		//Allocate next block
		actLog->last = logpage->nextBlock;
		actLog->nextPage = actLog->last * BLOCKSIZE;
		actLog->logHeader->prev = actLog->last; //assumes prev refers to latest block allocated
		//state->blockNumber++;
	}
	else{
		actLog->last = physAddr/BLOCKSIZE;
		actLog->nextPage = physAddr + 1;
	}
	return actLog;
}

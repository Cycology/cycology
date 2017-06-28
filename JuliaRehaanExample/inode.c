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

//inode constructor
//Only use when creating a new file. Otherwise, read page from memory (or cache) and cast to struct inode
void newInode(page_vaddr i_file_no, page_vaddr i_log_no, mode_t mode, struct inode* returnNode){
	//struct inode* returnNode = (struct inode*)malloc(sizeOf(struct inode));
	returnNode->i_mode = mode;
	returnNode->i_uid = getuid(); //I think these are the right methods
	returnNode->i_gid = getgid();
	//returnNode->i_flags = ?; //currently ignoring flags
	returnNode->i_file_no = i_file_no;
	returnNode->i_log_no = i_log_no;
	returnNode->i_links_count = 1;
	returnNode->i_atime = 0; //Unclear how we are getting/setting times
	returnNode->i_mtime = 0; //
	returnNode->i_ctime = 0; //
	returnNode->i_pages = 0;
	returnNode->i_size = 0;
	returnNode->indirect = NULL;
	returnNode->doubleInd = NULL;
	returnNode->tripleInd = NULL;
	//Probably should allocate array for direct pages
	//return returnNode;
}



//log constructor
//Only used when creating a new log
//Doesn't handle case where the log is an extent
void newLog(short logType, logHeader returnLog){
	struct fuse_context *context = fuse_get_context();
	CYCstate state  = context->private_data;

	//logHeader returnLog = (logHeader)malloc(sizeOf(struct logHeader));
	returnLog->erases = 0; //Always 0 for the moment
	returnLog->logId = getAvailableAddress();

	returnLog->prev = NULL;
	returnLog->first = state->blockNumber * BLOCKSIZE + 1;

	returnLog->active = 1;
	returnLog->total = 1;
	returnLog->logType = logType;
	returnLog->numFiles = 1;
	//returnLog->inodeList needs to be initialized. Should be fairly small
	returnLog->inodeList[0] = &inode;

	//return returnLog;
}

//This method should do more
void newFullPage(short pageType, fullPage newPage){
	//fullPage newPage = (fullPage)malloc(struct fullPage);
	newPage->pageType = pageType;
	//Should we do something with the metadata?
}

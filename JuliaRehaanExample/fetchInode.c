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

page_vaddr getVirtualAddress(const char *path){
	struct fuse_context *context = fuse_get_context();
	CYCstate state = context->private_data;

	page_vaddr vaddr;

	int fd = open(strcat(state->rootPath, path), "w+");
	pread(fd, vaddr, sizeof(page_vaddr), 0);
	close(fd);
	return vaddr;

}

//The following three functions can take physAddr instead...
//So everytime they're called, we need to do the logical to physical conversion first
//also, the passing it a pointer thing

void getInode(page_addr pAddr, struct inode* inode){ //will probably give "struct inode*" a convenient nickname

	logHeader logHead = getLog(pAddr);
	inode = logHead->contents.inode;
}

//Only called if we know log is not open
void getLog(page_addr paddr, logheader log){

	fullPage page = getFullPage(paddr);
	log = (logHeader) page->contents;
	//return log;
}

void getFullPage(page_addr physAddr, fullPage page){

	struct fuse_context *context = fuse_get_context();
	CYCstate state = context->private_data;

	//page_addr physAddr = state->vaddrMap->map[vddr];
	//Should Check if physAddr is 0, in which case file doesn't exist
	//Iffy about where we allocate
	char *buf = (char*)malloc(sizeof(struct fullPage));
	//perform read from storePath- cache will probably handle it
	xmp_read(state->storePath, buf, sizeof(struct fullPage), physAddr*sizeof(struct fullPage), state->storeFileDescriptor); //check return value
	page = (fullPage) buf;
	//return page;
}

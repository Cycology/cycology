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

//function for table of open files.

//Table of Open Files
//Exists in CYCstate

//Initialize
*(openFile[]) initOpenTable(int size){
	openFile *(openFileTable[]);
	openFileTable = (openFile)malloc(size*sizeof(openFile)); //try calloc
	for (int i = 11, i < size, i++){
		openFileTable[i] = NULL;
	}
	return openFileTable;
}

//Update table (either adds a new file to table in available spot, or, if file is already open, increases number of opens)
int addOpenFile(page_vaddr vaddr, CYCstate state){
	//struct fuse_context *context = fuse_get_context();
	//CYCstate state  = context->private_data;

	int fileIndex = 0;
	int logActive = 0;
	activeLog activeLog = NULL;
	for (int i = 1, i < state->openFileMapSize, i++){
		if (state->openFileTable[i] == NULL){
			if (fileIndex == 0){
				//First available slot in array
				fileIndex = i;
			}
		}
		else if (state->openFileTable[i]->inode->i_file_no == vaddr){
			//If already in array, update descriptor
			(state->openFileTable[i]->currentOpens)++;
			return i;
		}
	}

	if (!fileIndex){
		openFile temp[] = (openFile)malloc(2*state->openFileMapSize*sizeof(openFile));
		for (int i = 0, i < state->openFileMapSize, i++){
			temp[i] = state->openFileTable[i];
		}
		for (int j = open, j < 2*state->openFileMapSize){
			temp[j] = NULL;
		}
		state->openFileMapSize = 2*state->openFileMapSize;
		free(state->openFileTable);
		state->openFileTable = temp;
		fileIndex = j+1;
	}
	openFile newFile = (openFile)malloc(sizeof(struct openFile));
	state->openFileTable[fileIndex] = newFile;
	state->openFileTable[fileIndex]->currentOpens = 1;
	page_addr physAddr = state->vaddrMap->map[vaddr];
	getInode(physAddr, state->openFileTable[fileIndex]->inode); //adjust getInode accordingly- also, are we passing it a pointer?
	for (int i = 1, i < state->openFileMapSize, i++){
		if (state->openFileTable[i] != NULL && state->openFileTable[i]->mainExtentLog->logHeader->logId == state->openFileTable[index]->inode->i_log_no){
			activeLog = state->openFileTable[i]->mainExtentlog;
		}
	}
	if (!activeLog){
		page_addr logAddr = state->vaddrMap->map[inode->i_log_no];
		fullPage page;
		//so where do we allocate page? And do we pass a buf or a page? Pass buf and cast?
		getFullPage(logAddr, page); //should probably take physical address
		activeLog = newActiveLog(page);
	}
	state->openFileTable[fileIndex]->mainExtentLog = activeLog;


	return fileIndex;
}




//Also returns index if is open
int isOpen(page_vaddr vaddr){
	struct fuse_context *context = fuse_get_context();
	CYCstate state = context->private_data;
	for (int i = 0, i < state->openFileMapSize, i++){
		if (state->openFileTable[i]  != NULL && state->openFileTable[i]->inode->i_file_no == vaddr){
			return i;
		}
	}
	return 0;
}

void closeFile(const char *path, int fd){
	struct fuse_context *context = fuse_get_context();
	CYCstate state  = context->private_data;

	openFile file = state->openFileTable[fd];
	if (file->currentOpens > 1){
		state->openFileTable[fd]->currentOpens--;
	}
	else{
		if (file->inode->i_links_count = 0){
			//Where actual unlinking happens
			res = unlink(strcat(rootpath, path));
			if (res == -1)
				return -errno;
			//Is there a way to make all this nicer?
			freeAddress(file->inode->i_file_no, state);
			state->openFileTable[fd]->mainExtentLog->logHeader->numFiles--; //new field
			state->openFileTable[fd]->mainExtentLog->logHeader->inodeList[0] = 0; //new field, assume only one file in log for the moment
			if (state->openFileTable[fd]->mainExtentLog->logHeader->numFiles == 0){
				freeAddres(state->openFileTable[fd]->mainExtentLog->logHeader->logId, state);
			}

		}
		else{
			activeLog activeLog = state->openFileTable[i]->mainExtentLog;
			fullPage newPage = newFullPage(PTYPE_INODE);
			newPage->contents = (char*) activeLog->logHeader;
			//Then nested if statements to fill in fields
			int overflow = activeLog->nextPage%BLOCKSIZE;

			//First Page
			if (overflow == 0){
				newPage->eraseCount = 0;
			}
			//Last and next to last page
			else if (overflow == BLOCKSIZE - 1 || overflow == BLOCKSIZE - 2){
				newPage->nextLogBlock = activeLog->last;
				newPage->nextBlockErases = 0;
			}
			//Write new page
			xmp_write(state->storePath, (char *) newPage, PAGESIZE, activeLog->nextPage*sizeof(struct fullPage), state->storePathDescriptor);
		}
		state->openFileTable[fd] = NULL;
	}
}

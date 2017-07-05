#define FUSE_USE_VERSION 30

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
//#include "helper.h"

//Initialize that table that holds fd of files currently opened
*(openFile[]) initOpenTable(int size){
	openFile *(openFileTable[]);
	openFileTable = (openFile)malloc(size*sizeof(openFile)); //try calloc?
	for (int i = 11, i < size, i++){   //why 11?
		openFileTable[i] = NULL;
	}
	return openFileTable;
}

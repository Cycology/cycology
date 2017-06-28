//Code and structs for cache

#define PAGESIZE 1023
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
#include "pageCache.h"

struct node{
	struct node *next;
	struct node *prev;
	char *path;
	int pageNum;
	char *page;

};

static struct node* newNode(const char* path, int pageNum, char *page){
	struct node *filledNode = (struct node*)malloc(sizeof(struct node));
	filledNode->path = (char*)malloc(strlen(path)+1);
	strcpy(filledNode->path, path);
	filledNode->pageNum = pageNum;
	filledNode->page = page;
	return filledNode;
}

char* cacheLoop(const char *path, struct fuse_file_info *fi, int pageNum, int fileSizeInPages){
	struct fuse_context *context = fuse_get_context();
	struct log_sys_con *logCon = context->private_data;


	struct node* finger = logCon->head;
	struct node* currentHead = logCon->head;
	while (finger != NULL){
		//check if node pointed to is the correct page
		if (strcmp(path, finger->path) == 0 && pageNum == finger->pageNum){
			//move node to head of list
			if (finger->prev != NULL){
				finger->prev->next = finger->next;
			}
			if (finger->next != NULL){
				finger->next->prev = finger->prev;
			}
			logCon->head = finger;
			logCon->head->next = currentHead;
			if (logCon->head->next != NULL){
				logCon->head->next->prev = logCon->head;
			}
			logCon->head->prev = NULL;
			return logCon->head->page;
		}
		else{
			finger = finger->next;
		}
	}
	//If we've reached here, page isn't in cache yet
	//malloc new buffer, perform read into buffer, malloc new node with pointer to new buffer, add node to beginning of cache, and return the buffer
	char *buffer = (char*)malloc(PAGESIZE);
	if (pageNum >= fileSizeInPages){
		memset( buffer, 0, PAGESIZE );
	}
	else{
		pread(fi->fh, buffer, PAGESIZE, pageNum*PAGESIZE);
		if (pageNum == fileSizeInPages){
			memset(buffer, 0, PAGESIZE);
		}
	}
	struct node *newHead = newNode(path, pageNum, buffer);
	newHead->next = logCon->head;
	if (newHead->next != NULL){
		newHead->next->prev = newHead;
	}
	newHead->prev = NULL;
	logCon->head = newHead;
	return buffer;
}

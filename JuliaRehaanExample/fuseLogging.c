/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

/** @file
 * @tableofcontents
 *
 * fusexmp_fh.c - FUSE: Filesystem in Userspace
 *
 * \section section_compile compiling this example
 *
 * gcc -Wall fusexmp_fh.c `pkg-config fuse3 --cflags --libs` -lulockmgr -o fusexmp_fh
 *
 * \section section_source the complete source
 * \include fusexmp_fh.c
 */


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

//Small .c files- should they have some shared .h file?
#include "fuseLogging.h"
#include "helper.h"

static int xmp_getattr(const char *path, struct stat *stbuf)
{
	int res;

	/******************************************************
	 *
	 *  Do an lstat on the path provided. If it is a directory
	 *  of a symbolic link, just return the stat struct.
	 *  If the path leads to a regular file, read the file
	 *  to get the logical address of its inode and then
	 *  get the inode (cached copy of a read) and
	 *  extract the info for the stat struct from the inode
	 *
	 *****************************************************/
	struct fuse_context *context = fuse_get_context();
	CYCstate state  = context->private_data;


	res = lstat(path, stbuf);
	if (res == -1)
		return -errno;
	if (! S_ISREG( temp.st_mode ) ){
		return 0;
	}
	else{
		page_vaddr vAddr = getVirtualAddress(path);
		//Should we open the file instead? We're not changing the inode, just reading it, but this is inconsistant with other functions.
		index i = isOpen(vAddr);
		struct inode *inode;
		if (i){
			inode = state->openFileTable[i];
		}
		else{
			page_addr physAddr = state->vaddrMap->map[vAddr];
			getInode(physAddr, inode);
		}
		//Is st_dev from the lstat? If not, we don't need to call lstat for this case.
		stbuf->st_ino = inode->i_file_no;
		stbuf->st_mode  = inode->i_mode;
		stbuf->st_nlink  = inode->i_links_count;
		stbuf->st_uid = inode->i_uid;
		stbuf->st_gid = inode->i_gid;
		//Should we worry about st_rdev? If it's a regular file, it can't be a special file too
		stbuf->st_size = inode->i_size;
		stbuf->st_blksize = BLOCKSIZE; //Maybe PAGESIZE*BLOCKSIZE
		stbuf->st_blocks = state->blockNumber;
		return 0;
	}

	//Original Code
	res = lstat(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_fgetattr(const char *path, struct stat *stbuf,
			struct fuse_file_info *fi)
{
	int res;

	(void) path;

	/******************************************************
	 *
	 *  Should be just like getattr.
	 *
	 ******************************************************/
	struct fuse_context *context = fuse_get_context();
	CYCstate state  = context->private_data;


	res = lstat(path, stbuf);
	if (res == -1)
		return -errno;
	if (! S_ISREG( temp.st_mode ) ){
		memcpy(stbuf, temp);
		return 0;
	}
	else{


		inode = state->openFileTable[fi->fh]; //We know the file is open
		stbuf->st_ino = inode->i_file_no;
		stbuf->st_mode  = inode->i_mode;
		stbuf->st_nlink  = inode->i_links_count;
		stbuf->st_uid = inode->i_uid;
		stbuf->st_gid = inode->i_gid;
		//Should we worry about st_rdev? If it's a regular file, it can't be a special file too
		stbuf->st_size = inode->i_size;
		stbuf->st_blksize = BLOCKSIZE; //Maybe PAGESIZE*BLOCKSIZE
		stbuf->st_blocks = state->blockNumber;
		return 0;
	}

	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_access(const char *path, int mask)
{
	int res;

	/**********************************************************
	 *
	 * If we can arrange for the "regular" files that hold logical
	 * inode addresses to have the same ownership and mode as the
	 * files they refer to, then this operation may not require
	 * any new code.
	 *
	 ********************************************************/

	//Can't actually work, because we need to be able to write to the "regular" files, so we need to figure something else out (eventually)

	res = access(path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
	int res;

	/**********************************************************
	 *
	 * Let the underlying file system handle symbolic links.
	 * That is, no change to this function!
	 *
	 *********************************************************/

	res = readlink(path, buf, size - 1);
	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}

struct xmp_dirp {
	DIR *dp;
	struct dirent *entry;
	off_t offset;
};

static int xmp_opendir(const char *path, struct fuse_file_info *fi)
{
	/**********************************************************
	 *
	 *      NO CHANGE (I am pretty sure)
	 *
	 *********************************************************/

	int res;
	struct xmp_dirp *d = malloc(sizeof(struct xmp_dirp));
	if (d == NULL)
		return -ENOMEM;

	d->dp = opendir(path);
	if (d->dp == NULL) {
		res = -errno;
		free(d);
		return res;
	}
	d->offset = 0;
	d->entry = NULL;

	fi->fh = (unsigned long) d;
	return 0;
}

static inline struct xmp_dirp *get_dirp(struct fuse_file_info *fi)
{
	return (struct xmp_dirp *) (uintptr_t) fi->fh;
}

static int xmp_readdir(const char *path, void *buf, fuse_fill_dir_t filler,
		       off_t offset, struct fuse_file_info *fi,
		       enum fuse_readdir_flags flags)
{
	/**********************************************************
	 *
	 *      NO CHANGE (I hope)
	 *
	 *********************************************************/
	struct xmp_dirp *d = get_dirp(fi);

	(void) path;
	if (offset != d->offset) {
		seekdir(d->dp, offset);
		d->entry = NULL;
		d->offset = offset;
	}
	while (1) {
		struct stat st;
		off_t nextoff;
		enum fuse_fill_dir_flags fill_flags = 0;

		if (!d->entry) {
			d->entry = readdir(d->dp);
			if (!d->entry)
				break;
		}
#ifdef HAVE_FSTATAT
		if (flags & FUSE_READDIR_PLUS) {
			int res;

			res = fstatat(dirfd(d->dp), d->entry->d_name, &st,
				      AT_SYMLINK_NOFOLLOW);
			if (res != -1)
				fill_flags |= FUSE_FILL_DIR_PLUS;
		}
#endif
		if (!(fill_flags & FUSE_FILL_DIR_PLUS)) {
			memset(&st, 0, sizeof(st));
			st.st_ino = d->entry->d_ino;
			st.st_mode = d->entry->d_type << 12;
		}
		nextoff = telldir(d->dp);
		if (filler(buf, d->entry->d_name, &st, nextoff, fill_flags))
			break;

		d->entry = NULL;
		d->offset = nextoff;
	}

	return 0;
}

static int xmp_releasedir(const char *path, struct fuse_file_info *fi)
{
	/**********************************************************
	 *
	 *      NO CHANGE (I am pretty sure)
	 *
	 *********************************************************/

	struct xmp_dirp *d = get_dirp(fi);
	(void) path;
	closedir(d->dp);
	free(d);
	return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;

	/*********************************************************
	 *
	 * I recall seeing something in the FUSE documentation
	 * saying that in FUSE, this function is not used for regular
	 * files (create takes its place). So, the only change I
	 * would make is to test mode for regular file and to
	 * print a warning if this unexpected value is ever passed
	 * to the function.
	 *
	 *********************************************************/
	struct stat stBuf;

	int statRes = fstat( fi->fh, &stBuf );
	if (S_ISREG(stBuf.st_mode ) ){
		printf("Unexpected value passed to function\n"); //Warning for if given regular file
	}

	if (S_ISFIFO(mode))
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
	/**********************************************************
	 *
	 *      NO CHANGE
	 *
	 *********************************************************/


	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_unlink(const char *path)
{
	/********************************************************
	 *
	 * Test to see if path is a regular file (it could be a
	 * symbolic link or special file). If not, use existing
	 * code. Otherwise, use stat to see if the link count
	 * of the shadow file is greater than 1. If so, just do
	 * an unlink of the shadow file. Othewise, first read
	 * read the shadow file to get the logical
	 * i-node address for the "real" file and then remove
	 * the real file (or schedule its removal when all active
	 * opens of the file are closed).
	 *
	 ********************************************************/

	struct stat stBuf;

	int statRes = fstat( fi->fh, &stBuf );
	if (! S_ISREG(stBuf.st_mode )
	    int res;

	    res = unlink(path);
	    if (res == -1)
		    return -errno;

	    return 0;
	}
	else{

		struct fuse_context *context = fuse_get_context();
		CYCstate state  = context->private_data;

		page_vaddr vAddr = getVirtualAddress(path);
		int fd = addOpenFile(vAddr);  //open the file to change inode
		state->openFileTable[i]->inode->i_links_count--;
		closeFile(path, fd); //This will write the inode if necessary


		return 0;
	}

	int res;

	res = unlink(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rmdir(const char *path)
{
	/**********************************************************
	 *
	 *      NO CHANGE
	 *
	 *********************************************************/

	int res;

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_symlink(const char *from, const char *to)
{
	/**********************************************************
	 *
	 *      NO CHANGE
	 *
	 *********************************************************/

	int res;

	res = symlink(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rename(const char *from, const char *to, unsigned int flags)
{
	/**********************************************************
	 *
	 *      NO CHANGE  (I think. At least for now.
	 *
	 *      My concern here is that I don't know whether you can
	 *      use rename to move a file to a different file system.
	 *      If not, then just moving the shadow file that hold
	 *      the logical inode address of the real file should be
	 *      all that is required (and what the existing code does).
	 *      If so, then this could involve actual copying.
	 *      Some testing would be appropriate.
	 *
	 *********************************************************/
	//Run tests

	int res;

	/* When we have renameat2() in libc, then we can implement flags */
	if (flags)
		return -EINVAL;

	res = rename(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_link(const char *from, const char *to)
{
	/***********************************************
	 *
	 * I think we can make the underlying file system
	 * do most of the work of adding and removing
	 * links. To do this, we will just have one copy
	 * of the shadow file that holds the virtual inode
	 * address of our real file and allow the underlying
	 * file system to make multiple links to it.
	 * We will have to use the stat system call to
	 * determine how many links there are to a shadow
	 * file when it is removed so that we can do a real
	 * remove when the count goes to 0.
	 *
	 *************************************************/

	//Using inode's link count


	/*Do we need to check if regular? One can probably link directories...*/
	int res;


	res = link(from, to); //always happens, whether regular or not
	if (res == -1)
		return -errno;
	if (! S_ISREG(stBuf.st_mode )
	    return 0;
	}

	struct fuse_context *context = fuse_get_context();
	CYCstate state  = context->private_data;
	page_vaddr vAddr = getVirtualAddress(from); //from and to are the same now, so we can use either
	int fd = addOpenFile(vAddr);
	state->openFileTable[i]->inode->i_links_count++;
	//Now perform a close (inode gets written if was closed initially)
	closeFile(path, fd);

	//original code
	res = link(from, to);
	if (res == -1)
		return -errno;

	return 0;
}


static int xmp_chmod(const char *path, mode_t mode)
{
	/*****************************************************
	 *
	 * First check to see if the file is a regular file. If not,
	 * mimic the existing code. Otherwise, check that the user
	 * has the right to make the changes and then record them
	 * in our inode.
	 *
	 *******************************************************/

	int res;

	int statRes = fstat( fi->fh, &stBuf );
	if (! S_ISREG(stBuf.st_mode) ){
		res = chmod(path, modee);
		if (res == -1)
			return -errno;

		return 0;
	}

	struct fuse_context *context = fuse_get_context();
	CYCstate state  = context->private_data;

	page_vaddr vAddr = getVirtualAddress(path);
	//Currently not checking permissions
	int fd = addOpenFile(vAddr);
	state->openFileTable[i]->inode->i_mode = mode;
	//Now perform a close (inode gets written if was closed initially)
	closeFile(path, fd);

	//Original code
	res = chmod(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid)
{
	/********************************************************
	 *
	 * First check to see if the file is a regular file. If not,
	 * mimic the existing code. Otherwise, check that the user
	 * has the right to make the changes and then record them
	 * in our inode.
	 *
	 *******************************************************/


	//Then check permissions //how?
	//Then just change uid and gid
	int res;

	int statRes = fstat( fi->fh, &stBuf );
	if (! S_ISREG(stBuf.st_mode) ){
		res = lchown(path, uid, gid);
		if (res == -1)
			return -errno;

		return 0;
	}

	struct fuse_context *context = fuse_get_context();
	CYCstate state  = context->private_data;

	page_vaddr vAddr = getVirtualAddress(path);
	//Currently not checking permissions
	int fd = addOpenFile(vAddr);
	state->openFileTable[i]->inode->i_uid = uid;
	state->openFileTable[i]->inode->i_gid = gid;
	//Now perform a close (inode gets written if was closed initially)
	closeFile(path, fd);

	//Original code
	res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_truncate(const char *path, off_t size)
{
	/******************************************************
	 *
	 * Reimplement! (I believe this can only be applied to
	 * regular files. There is an interesting choice here.
	 * If truncate is being applied with size 0, and the
	 * current file is large, it may be apporpriate to make
	 * a new log rather then producing a log with lots of
	 * garbage in it.
	 *
	 ******************************************************/
	//Truncates a file - Do we know it's definitely closed? Could be called manually


	/* So, when we truncate, certain pages become garbage. But, we can probably truncate by something that's not a multiple of page size, so some pages become part 0s? So, they also become garbage, and we move them elsewhere, later in the log. Can also extend, so new pages allocated, filled in with 0s.... Can also create a new file which could probably be handled by if statement that calls creat. Ultimately this is very similar to write*/

	/*We should open the file, for the purpose of parallelism
	  Shrinking: We just need to get rid of pointers, except fort he last page, which we should use the caceh for, and essentially do a write
	  Growing: New pages will be empty, so we don't really have to write them... But we need the pointers to indicate that they exist

	 */
	int res;

	res = truncate(path, size);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_ftruncate(const char *path, off_t size,
			 struct fuse_file_info *fi)
{
	/**********************************************************
	 *
	 * Same as truncate.
	 *
	 *********************************************************/

	//truncates an open file (saves steps, since we don't need to open the file ourselves)

	int res;

	(void) path;

	res = ftruncate(fi->fh, size);
	if (res == -1)
		return -errno;

	return 0;
}

#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2])
{
	/*************************************************************
	 *
	 *  Check for regular file vs. directory and use shadow file
	 *  to find and update our inode for regular files.
	 *
	 *  We should investigate when this is used. I assume times
	 *  are updated directly when opens, reads and writes occur.
	 *  That is, I think this function is for some special purpose
	 *  means of updating file times.
	 *
	 ************************************************************/
	//So, how can we tell which timespec they want to change? There are 3
	//There's also the weird ts[2] param
	int res;

	int statRes = fstat( fi->fh, &stBuf );
	if (! S_ISREG(stBuf.st_mode) ){
		res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
		if (res == -1)
			return -errno;

		return 0;
	}


	struct fuse_context *context = fuse_get_context();
	CYCstate state  = context->private_data;

	page_vaddr vAddr = getVirtualAddress(path);
	int fd = addOpenFile(vAddr);
	state->openFileTable[i]->inode->i_atime = ts; //not sure which time field to update (atime chosen arbitrarily as an example)
	//Now perform a close (inode gets written if was closed initially)
	closeFile(path, fd);


	/* don't use utime/utimes since they follow symlinks */
	res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
	if (res == -1)
		return -errno;

	return 0;
}
#endif

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
	/*************************************************************
	 *
	 * Reimplement to allocate a log and create an initial inode and
	 * then to add a shadow file to the underlying file system that
	 * refers to the new inode.
	 *
	 ************************************************************/
	  //We need to fill fi->fh
	  //But we need fi for flags when opening?


	struct fuse_context *context = fuse_get_context();
	CYCstate state  = context->private_data;

	page_vaddr logicalAddr = getAvailableAddress();

	state->vaddrMap->map[logicalAddr] =  state->blockNumber * BLOCKSIZE;

	fullPage newPage = (fullPage)malloc(struct fullPage);
	newFullPage(PTYPE_INODE, newPage);
	logHeader log = newPage->contents;
	newLog(LTYPE_FILE, log);
	state->vaddrMap->map[log->logId] =  state->blockNumber * BLOCKSIZE;

	struct inode *inode = log->contents.file;
	newInode(logicalAddr, log->logId, mode, inode);
	  //*put inode in logheader
	//log->contents.file = inode;

	//Perform a write


	//newPage->contents = (char*) log;

	//This will go on the first page of a block for sure, so we only need to worry about erase count
	newPage->eraseCount = 0;
	//Write new page
	xmp_write(state->storePath, (char *) newPage, PAGESIZE, activeLog->nextPagestate->blockNumber*BLOCKSIZE*sizeof(struct fullPage), state->storePathDescriptor);

	//open first. That way we'll have an active log descriptor
	xmp_open(path, fi);

	//*call regular open for mini file
	//Need to do different stuff to concatanate
	char* fullPath = (char*)malloc(strlen(path)+strlen(rootpath)+1);
	strcopy(fullPath, rootpath);
	strcat(fullPath, path);
	int fd = open(fullpath, "w+");

	//*do pwrite into mini file
	pwrite( fd, (char*) logicalAddr, SMFILESIZE, aLog->nextPage * sizeof(struct fullPage)); //What's up with that offset?

	//close mini file
	close(fullPath);

	state->blockNumber++;


	//Original code
	int fd;

	fd = open(path, fi->flags, mode);
	if (fd == -1)
		return -errno;

	fi->fh = fd;
	return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
	int fd;

	/***************************************************************
	 * Somewhere I read something suggesting that open is only called
	 * on existing files (i.e., create is always used if the file
	 * being opened is new). Assuming this is correct, this function
	 * should open the shadow file to get the real file's logical
	 * inode page address, read the inode and verify access rights,
	 * allocate a file descriptor and return the descriptor number
	 * in fi->fh.
	 *
	 ***************************************************************/


	struct stat stBuf;         // Space for stat results

	int statRes = stat( path, &stBuf );
	if ( statRes == -1 ) {
		return -errno;
	}

	if ( ! S_ISREG( stBuf.st_mode ) ) { //Checking if regular file
		return -EINVAL;
	}

	//get private_data
	struct fuse_context *context = fuse_get_context();
	CYCstate state  = context->private_data;

	char* fullPath = (char*)malloc(strlen(path)+strlen(rootpath)+1);
	strcopy(fullPath, rootpath);
	strcat(fullPath, path);
	fd = open(fullPath, "w+");
	if (fd == -1)
		return -errno;

	char *shortBuf = (char*)malloc(SMFILESIZE);
	pread( fd, shortBuf, SMFILESIZE, 0);
	page_vaddr vaddr = (page_vaddr) shortBuf;

	int index = addOpenFile(vaddr); //Index is then stored in fh
	fi->fh = index;
	return 0;


	/* Create descriptor for log of main extent and link it */
	/* to the new open file descsriptor.                    */
	/* (This step is only required if some part of the file */
	/* will be modified which suggests it can be limited to */
	/* files opened for writing, but this is not true if we */
	/* want to track access time in the file's inode. So,for*/
	/* now we should do this for all files opened.)         */

	/* Return index of pointer to descriptor in open file array
	   in fi->fh field.
	*/


/* TENTATIVE CODE/PSEUDO-CODE ****************************************/


	//original code
	fd = open(path, fi->flags);
	if (fd == -1)
		return -errno;

	fi->fh = fd;
	return 0;
}

static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
	/*****************************************************************
	 *
	 * This function needs to be rewritten to find page addresses
	 * using the file's inode and indirect pages.
	 *
	 *****************************************************************/


	/* Necessary changes to read:

	   -most things are the same until we call the cache
	   -if we pass fi and path to the cache, it can do the calculations to find the real offset
	   -doesn't change data, so nothing moves, and inodes don't have to be updated
	   -so, we're reading the page at the offset rounded down to the nearest page, which will be that rounded offset (in pages) after the inod, so look at the direct and indirect pages using  that info (again, the cache loop should take care of this (if the page isn't already in the cache))
	   *Cache: will have hash table etc:
	   -We want a page- if it's in the cache, great, give it to us
	   -How do we know it's our page? Check file number and page number
	   -As stored, file will be filled out with zeroes to be divisible by page size
	   Most of our read only worries about what to do with the page that  cache loop returns, leaving the memory stuff to the cache
	 */

	//Our Read: (Note that cache loop is needed)


	printf("Read %d bytes at %d\n", (int) size, (int) offset);
	char *bufRead;

	int res = 0;

	(void) path;

	struct stat stBuf;
	int statRes = fstat( fi->fh, &stBuf );
	if ( statRes == -1 ) {

		return -errno;
	}
	int fileSize = stBuf.st_size;
	int fileSizeInPages = ( fileSize + PAGESIZE - 1 )/PAGESIZE;

	if (offset > fileSize){
		return 0;
	}
	if (offset + size  > fileSize){
		size = fileSize - offset;
	}

	int startDeviation  = offset%PAGESIZE;
	int endDeviation = (size + offset)%PAGESIZE;
	int numPages = (startDeviation+PAGESIZE+size-1)/PAGESIZE;

	int startPage = offset / PAGESIZE;



	for (int i = 0; i < numPages; i++){
		int pageNum = startPage + i;
		int startWithinPage;
		int toRead;
		int readSize = PAGESIZE;

		if (numPages == 1) {
			startWithinPage = startDeviation;
			toRead = size;
		} else if (i == 0) {
			startWithinPage = startDeviation;
			toRead = PAGESIZE - startDeviation;
		} else if (i == numPages - 1 && endDeviation != 0) {
			startWithinPage = 0;
			toRead = endDeviation;
		} else {
			startWithinPage = 0;
			toRead = PAGESIZE;
		}
		bufRead = cacheLoop(path, fi, pageNum, fileSizeInPages);
		memcpy(buf, bufRead + startWithinPage, toRead);
		buf += toRead;
		res += toRead;
	}

	if (res == -1){
		res = -errno;
	}
	return res;

	//Tom's Read
	char * bufPos = buf;
	char tempBuf[PAGESIZE];

	(void) path;

	fprintf( stderr, "Reading %s at %d for %d\n", path, (int) offset, (int) size );

	size_t res = 0;

	struct stat stBuf;

	int statRes = fstat( fi->fh, &stBuf );
	if ( statRes == -1 ) {
	  return -errno;
	}
	if ( offset > stBuf.st_size ) {
	  /* return end of file indication */
	  return 0;
	} else if ( offset + size > stBuf.st_size ) {
		size = stBuf.st_size - offset;
	}

	int pageNo = offset / PAGESIZE;
	offset = offset % PAGESIZE;

	size_t bytesRead = pread( fi->fh, tempBuf, PAGESIZE, pageNo*PAGESIZE );

	if ( bytesRead == PAGESIZE || bytesRead >= size ) {
	  if ( size >= PAGESIZE - offset ) {
	    memcpy( bufPos, tempBuf + offset, PAGESIZE - offset );
	    res = PAGESIZE - offset;
	    bufPos += res;
	  } else {
	    memcpy( bufPos, tempBuf + offset, size );
	    res = size;
	  }
	  size -= res;

	} else {
	  /* THIS SHOULD NEVER HAPPEN   */

	  perror( "ERROR IN xmp_read\n" );
	  return -1;
	}


	while ( size >= PAGESIZE ) {

	  pageNo++;
	  bytesRead = pread( fi->fh, bufPos, PAGESIZE, pageNo*PAGESIZE );
	  if ( bytesRead == PAGESIZE ) {
	    bufPos += PAGESIZE;
	    size -= PAGESIZE;
	    res += PAGESIZE;
	  } else {
	    /* THIS SHOULD NEVER HAPPEN   */

	    perror( "ERROR 2 IN xmp_read\n" );
	    return -1;

	  }

	}

	if ( size > 0 ) {
	  pageNo++;
	  bytesRead = pread( fi->fh, bufPos, size, pageNo*PAGESIZE );
	  if ( bytesRead == size ) {
	    res += size;
	    return res;
	  } else {
	    /* THIS SHOULD NEVER HAPPEN   */

	    perror( "ERROR 3 IN xmp_read\n" );
	    return -1;

	  }
	}

	return res;
}

static int xmp_read_buf(const char *path, struct fuse_bufvec **bufp,
			size_t size, off_t offset, struct fuse_file_info *fi)
{
	/*****************************************************************
	 *
	 * Eliminate this function
	 *
	 ****************************************************************/
	struct fuse_bufvec *src;

	fprintf( stderr, "In xmp_read_buf\n" );

	(void) path;

	src = malloc(sizeof(struct fuse_bufvec));
	if (src == NULL)
		return -ENOMEM;

	*src = FUSE_BUFVEC_INIT(size);

	src->buf[0].flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
	src->buf[0].fd = fi->fh;
	src->buf[0].pos = offset;

	*bufp = src;

	return 0;
}



static char * prepBuffer( char * buffer, struct fuse_file_info *fi,
		       int pageNum, int startWithinPage, int endWithinPage,
			  int fileSizeInPages, int fileSize ) {

  if ( pageNum > fileSizeInPages ) {

    // If new page, make sure unused portions of buffer are zeroes
    memset( buffer, 0, startWithinPage );
    memset( buffer + endWithinPage, 0, PAGESIZE - endWithinPage );
  } else {

    // If existing page not completely overwritten, read it in
    if ( startWithinPage > 0 || endWithinPage < PAGESIZE ) {

      int toRead;
      if ( pageNum == fileSizeInPages ) {
	toRead = fileSize - pageNum*PAGESIZE;
      } else {
	toRead = PAGESIZE;
      }

      int read = pread( fi->fh, buffer, toRead, pageNum*PAGESIZE );

      if ( read != toRead ) {
	fprintf( stderr, "Unexpected short read in prepBuffer\n" );
	return NULL;
      }

      memset( buffer + toRead, 0, PAGESIZE - toRead );
    }
  }

  return buffer;   // Silly return of buffer passed in preparation for later
		   // version where buffer will probably be part of cache.

}


static int xmp_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	/*****************************************************************
	 *
	 * Like read, this method will have to work with the file's inode
	 * to identify which pages need to be read by prepBuffer. When
	 * pages are written out, they should be written out to newly
	 * allocated pages and the inode or indirect pages should be
	 * updated (at least in RAM) to record these new allocations.
	 *
	 ***************************************************************/

	/*Get page from cache like read does, but then it's more complicated (write through? or not anymore?)
	  -Create the new page in the cache, as we do, but then also write in to a new page in the log, and update the pointer in the inode ( might involve indirects and extents and such) (Updates for now can go in the inode in the open file descriptor, and written to "NAND memory" at close,  but that should change at some point) (But what about writing the data itself? Not just changing the inode? That needs to get written before the inode is- write through for now...  So, write new page on the next page indicated by the active log, increment the next page (using the if statements, which might mean creating a function to avoid duplicate code, and which might involve allocating a new block), and store the physical address of the newly written page in the inode. If it's a new page, then if there's room put in the array, otherwise indirect pages)
	 */

  int res = 0;               // number of bytes written
  struct stat stBuf;         // Space for stat results
  char buffer[ PAGESIZE ];   // Space for one page of file

  (void) path;

  int statRes = fstat( fi->fh, &stBuf );
  if ( statRes == -1 ) {
    return -errno;
  }

  int fileSize = stBuf.st_size;
  int fileSizeInPages = ( fileSize + PAGESIZE - 1 )/PAGESIZE;

  int finalFileSize;

  if ( offset + size > fileSize ) {
    finalFileSize = offset + size;
  } else {
    finalFileSize = fileSize;
  }

  int startPage = offset / PAGESIZE;
  int startWithinPage = offset % PAGESIZE;

  int numPages = ( startWithinPage + size + PAGESIZE - 1 ) / PAGESIZE;

  for ( int relativePageNum = 0; relativePageNum < numPages; relativePageNum++ ) {
    int endWithinPage;

    if ( relativePageNum == numPages - 1 ) {
      endWithinPage = (offset + size) % PAGESIZE;
    } else {
      endWithinPage = PAGESIZE;
    }

    char * bufPtr = prepBuffer( buffer, fi, relativePageNum + startPage,
				startWithinPage, endWithinPage, fileSizeInPages, fileSize );

    int dataToMove = endWithinPage - startWithinPage;

    memcpy( bufPtr + startWithinPage, buf, dataToMove );


    int toWrite;

    if ( (relativePageNum + startPage + 1 )*PAGESIZE > finalFileSize ) {
      toWrite = finalFileSize - (relativePageNum + startPage )*PAGESIZE;
    } else {
      toWrite = PAGESIZE;
    }

    int written = pwrite( fi->fh, bufPtr, toWrite, (relativePageNum + startPage)*PAGESIZE );
    if ( written != toWrite ) {
      fprintf( stderr, "Unexpected write return value in xmp_write\n" );
      return -errno;
    }

    res += dataToMove;
    buf += dataToMove;

    startWithinPage = 0;
  }

  return res;
}


static int xmp_write_buf(const char *path, struct fuse_bufvec *buf,
		     off_t offset, struct fuse_file_info *fi)
{
	/*****************************************************************
	 *
	 * Eliminate this function
	 *
	 ****************************************************************/

	struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));

	(void) path;

	dst.buf[0].flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
	dst.buf[0].fd = fi->fh;
	dst.buf[0].pos = offset;

	return fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
	/**************************************************************
	 *
	 * Rewrite to return information about space available, etc.
	 *
	 *************************************************************/
	//fill in the appropriate field of the statvfs struct ourselves
	int res;

	res = statvfs(path, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_flush(const char *path, struct fuse_file_info *fi)
{

	/************************************************************
	 *
	 * Given that logging should make meta-data recoverable, this
	 * function can probably be treated as just a hint that it
	 * might be a good time to flush file metadata.
	 *
	 ***********************************************************/

	//Unsure how this should work. Should it just write all active inodes to memory?

	int res;

	(void) path;
	/* This is called from every close on an open file, so call the
	   close on the underlying filesystem.	But since flush may be
	   called multiple times for an open file, this must not really
	   close the file.  This is important if used on a network
	   filesystem like NFS which flush the data/metadata on close() */
	res = close(dup(fi->fh));
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
	/**********************************************************
	 *
	 * This appears to be close. So, this might be a good place
	 * to flush meta data for real.
	 *
	 *********************************************************/


	closeFile(path, fi->fh);


	//Old code:
	close(fi->fh);

	return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	/*******************************************************
	 *
	 * It looks like we can skip this.
	 *
	 ********************************************************/

	int res;
	(void) path;

#ifndef HAVE_FDATASYNC
	(void) isdatasync;
#else
	if (isdatasync)
		res = fdatasync(fi->fh);
	else
#endif
		res = fsync(fi->fh);
	if (res == -1)
		return -errno;

	return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int xmp_fallocate(const char *path, int mode,
			off_t offset, off_t length, struct fuse_file_info *fi)
{
	/*******************************************************
	 *
	 * It looks like we can skip this.
	 *
	 ********************************************************/

	(void) path;

	if (mode)
		return -EOPNOTSUPP;

	return -posix_fallocate(fi->fh, offset, length);
}
#endif

#ifdef HAVE_SETXATTR

/***********************************************************************
 *
 * Let's assume we ditch all the xattr operations!
 *
 **********************************************************************/

/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
	int res = lsetxattr(path, name, value, size, flags);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
	int res = lgetxattr(path, name, value, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
	int res = llistxattr(path, list, size);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
	int res = lremovexattr(path, name);
	if (res == -1)
		return -errno;
	return 0;
}
#endif /* HAVE_SETXATTR */

#ifdef HAVE_LIBULOCKMGR
static int xmp_lock(const char *path, struct fuse_file_info *fi, int cmd,
		    struct flock *lock)
{
	(void) path;

	return ulockmgr_op(fi->fh, cmd, lock, &fi->lock_owner,
			   sizeof(fi->lock_owner));
}
#endif

static int xmp_flock(const char *path, struct fuse_file_info *fi, int op)
{
	int res;
	(void) path;

	res = flock(fi->fh, op);
	if (res == -1)
		return -errno;

	return 0;
}

static void* xmp_init(struct fuse_conn_info *conn){
	CYCstate context = (CYCstate)malloc(sizeof(struct CYCstate));
	context->rootPath = ROOT_PATH;
	context->storePath = STORE_PATH; //strings
	int fd = open(path, "w+");
	context->storeFileDescriptor = fd; ///Add new field to CYCstate
	context->vaddrMap = initAddrMap();

	context->cache = (pageCache)malloc(sizeof(struct pageCache));
	context->cache->size = 0;
	context->cache->headLRU = NULL;
	context->cache->tailLRU = NULL;
	context->cache->headHash = NULL; //could init the cache in a function
	context->openFileMapSize = 1024; //magic constant? So, probably a #define
	context->openFileTable = initOpenTable(context->openFileMapSize);
	context->blockNumber = 0; //add to CYCstate, will eventually get replaced by the free list. Should be 1 if we have a superblock

	return context;
}

static struct fuse_operations xmp_oper = {
	.getattr	= xmp_getattr,
	.fgetattr	= xmp_fgetattr,
	.access		= xmp_access,
	.readlink	= xmp_readlink,
	.opendir	= xmp_opendir,
	.readdir	= xmp_readdir,
	.releasedir	= xmp_releasedir,
	.mknod		= xmp_mknod,
	.mkdir		= xmp_mkdir,
	.symlink	= xmp_symlink,
	.unlink		= xmp_unlink,
	.rmdir		= xmp_rmdir,
	.rename		= xmp_rename,
	.link		= xmp_link,
	.chmod		= xmp_chmod,
	.chown		= xmp_chown,
	.truncate	= xmp_truncate,
	.ftruncate	= xmp_ftruncate,
	.init           = xmp_init,
#ifdef HAVE_UTIMENSAT
	.utimens	= xmp_utimens,
#endif
	.create		= xmp_create,
	.open		= xmp_open,
	.read		= xmp_read,
	/*	.read_buf	= xmp_read_buf,  */
	.write		= xmp_write,
	/* .write_buf	= xmp_write_buf,         */
	.statfs		= xmp_statfs,
	.flush		= xmp_flush,
	.release	= xmp_release,
	.fsync		= xmp_fsync,
#ifdef HAVE_POSIX_FALLOCATE
	.fallocate	= xmp_fallocate,
#endif
#ifdef HAVE_SETXATTR
	.setxattr	= xmp_setxattr,
	.getxattr	= xmp_getxattr,
	.listxattr	= xmp_listxattr,
	.removexattr	= xmp_removexattr,
#endif
#ifdef HAVE_LIBULOCKMGR
	.lock		= xmp_lock,
#endif
	.flock		= xmp_flock,
};

int main(int argc, char *argv[])
{
	umask(0);
	return fuse_main(argc, argv, &xmp_oper, NULL);
}

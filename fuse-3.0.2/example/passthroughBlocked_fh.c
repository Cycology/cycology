/*
  FUSE: Filesystem in Userspace
  Copyright (C) 2001-2007  Miklos Szeredi <miklos@szeredi.hu>
  Copyright (C) 2011       Sebastian Pipping <sebastian@pipping.org>

  This program can be distributed under the terms of the GNU GPL.
  See the file COPYING.
*/

/** @file
 *
 * This file system mirrors the existing file system hierarchy of the
 * system, starting at the root file system. This is implemented by
 * just "passing through" all requests to the corresponding user-space
 * libc functions. This implementation is a little more sophisticated
 * than the one in passthrough.c, so performance is not quite as bad.
 *
 * Compile with:
 *
 *     gcc -Wall passthrough_fh.c `pkg-config fuse3 --cflags --libs` -lulockmgr -o passthrough_fh
 *
 * ## Source code ##
 * \include passthrough_fh.c
 */

#define PAGESIZE 1024
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

//.h files of our own
#include "vNANDlib.h"
#include "helper.h"

//struct holding flag and fd; pocominted to by fi->fh
typedef struct blocked_file_info{
  int flag;
  int fd;
} *blocked_file_info;

static char *makePath(const char *path)
{
  char *fullPath = (char*) malloc(strlen(path) + strlen(ROOT_PATH) + 1);
  strcpy(fullPath, ROOT_PATH);
  strcat(fullPath, path);
  return fullPath;
}

//Initialize the FS, saving the directory being mounted
static void *xmp_init(struct fuse_conn_info *conn,
		      struct fuse_config *cfg)
{
  CYCstate state = (CYCstate) malloc(sizeof (struct CYCstate));
  state->rootPath = ROOT_PATH;
  state->storePath = STORE_PATH;
  state->nFeatures = initNAND();
  
  addrMap map = (addrMap) malloc(sizeof (struct addrMap));
  initAddrMap(map);
  state->vaddrMap = map;

  pageCache cache = (pageCache) malloc(sizeof (struct pageCache));
  initCache(cache);
  state->cache = cache;

  freeList lists = (freeList) malloc(sizeof (struct freeList));
  initFreeLists(lists);
  state->lists = lists;
  
  stopNAND();
  return state;
  
	/* (void) conn; */
	/* cfg->use_ino = 1; */
	/* cfg->nullpath_ok = 1; */

	/* /\* Pick up changes from lower filesystem right away. This is *\/ */
	/* /\*    also necessary for better hardlink support. When the kernel *\/ */
	/* /\*    calls the unlink() handler, it does not know the inode of *\/ */
	/* /\*    the to-be-removed entry and can therefore not invalidate *\/ */
	/* /\*    the cache of the associated inode - resulting in an *\/ */
	/* /\*    incorrect st_nlink value being reported for any remaining *\/ */
	/* /\*    hardlinks to this inode. *\/ */
	/* cfg->entry_timeout = 0; */
	/* cfg->attr_timeout = 0; */
	/* cfg->negative_timeout = 0; */

	/* return NULL; */
}

static void xmp_destroy(void *private_data)
{
  free(private_data);
}

static int xmp_getattr(const char *path, struct stat *stbuf,
			struct fuse_file_info *fi)
{
	int res;

	(void) path;
	
	if(fi)
	  res = fstat(((blocked_file_info) fi->fh)->fd, stbuf);
	else
	  {
	  	char *fullPath = makePath(path);
		res = lstat(fullPath, stbuf);
		free(fullPath);
	  }
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_access(const char *path, int mask)
{
	int res;

	char *fullPath = makePath(path);
	
	res = access(fullPath, mask);
	free(fullPath);

	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
	int res;

	char *fullPath = makePath(path);
	
	res = readlink(fullPath, buf, size - 1);
	free(fullPath);

	if (res == -1)
		return -errno;

	buf[res] = '\0';
	return 0;
}

//it seems like this struct has to be stored in fi->fh, and not in fi->fh->fd
struct xmp_dirp {
	DIR *dp;
	struct dirent *entry;
	off_t offset;
};

static int xmp_opendir(const char *path, struct fuse_file_info *fi)
{
	int res;
	struct xmp_dirp *d = malloc(sizeof(struct xmp_dirp));
	if (d == NULL)
		return -ENOMEM;

	char *fullPath = makePath(path);
	d->dp = opendir(fullPath);
	free(fullPath);
	
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
	struct xmp_dirp *d = get_dirp(fi);
	(void) path;
	closedir(d->dp);
	free(d);
	return 0;
}

static int xmp_mknod(const char *path, mode_t mode, dev_t rdev)
{
	int res;
		
	if (S_ISFIFO(mode))
	  {
	  char *fullPath = makePath(path);
	  res = mkfifo(fullPath, mode);
	  free(fullPath);
	  }
	else
	  {
	  char *fullPath = makePath(path);
	  res = mknod(fullPath, mode, rdev);
	  free(fullPath);
	  }
	if (res == -1)
	  return -errno;
	return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
	int res;

	char *fullPath = makePath(path);
	
	res = mkdir(fullPath, mode);
	free(fullPath);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_unlink(const char *path)
{
	int res;

	char *fullPath = makePath(path);

	res = unlink(fullPath);
	free(fullPath);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_rmdir(const char *path)
{
	int res;

	char *fullPath = makePath(path);
	
	res = rmdir(fullPath);
	free(fullPath);
	if (res == -1)
		return -errno;
	return 0;
}

/*****************************************
 *
 * 3 scenarios for symlink & rename:
 * - 'from' in our FS, 'to' in other
 * - 'from' in other, 'to' in our FS
 * - both in our FS
 *
 *****************************************/

//Called when making a symlink only from WITHIN our FS
static int xmp_symlink(const char *from, const char *to)
{
	int res;

	printf("from: %s\n", from);
	char *fullPath = makePath(to);
	printf("to: %s\n", fullPath);
	
	res = symlink(from, fullPath);
	free(fullPath);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rename(const char *from, const char *to, unsigned int flags)
{
	int res;

	/* When we have renameat2() in libc, then we can implement flags */
	if (flags)
		return -EINVAL;

	char *fullFrom = makePath(from);
	char *fullTo = makePath(to);
	printf("from: %s\n", fullFrom);
	printf("to: %s\n", fullTo);
	
	res = rename(fullFrom, fullTo);
	free(fullFrom);
	free(fullTo);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_link(const char *from, const char *to)
{
	int res;

	char *fullFrom = makePath(from);
	char *fullTo = makePath(to);
	printf("from: %s\n", fullFrom);
	printf("to: %s\n", fullTo);

	res = link(fullFrom, fullTo);
	free(fullFrom);
	free(fullTo);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chmod(const char *path, mode_t mode,
		     struct fuse_file_info *fi)
{
	int res;

	char *fullPath = makePath(path);

	if(fi)
		res = fchmod(((blocked_file_info) fi->fh)->fd, mode);
	else
		res = chmod(fullPath, mode);
	if (res == -1)
		return -errno;

	free(fullPath);
	return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid,
		     struct fuse_file_info *fi)
{
	int res;

	if (fi)
		res = fchown(((blocked_file_info) fi->fh)->fd, uid, gid);
	else {
	        char *fullPath = makePath(path);
	        res = lchown(fullPath, uid, gid);
		free(fullPath);
	} if (res == -1)
		return -errno;

	return 0;
}

static int xmp_truncate(const char *path, off_t size,
			 struct fuse_file_info *fi)
{
	int res;

	if(fi)
		res = ftruncate(((blocked_file_info) fi->fh)->fd, size);
	else {
	  	char *fullPath = makePath(path);
		res = truncate(path, size);
		free(fullPath);
	} if (res == -1)
		return -errno;

	return 0;
}

#ifdef HAVE_UTIMENSAT
static int xmp_utimens(const char *path, const struct timespec ts[2],
		       struct fuse_file_info *fi)
{
	int res;

	/* don't use utime/utimes since they follow symlinks */
	if (fi)
		res = futimens(((blocked_file_info) fi->fh)->fd, ts);
	else {
	        char *fullPath = makePath(path);
	        res = utimensat(0, fullPath, ts, AT_SYMLINK_NOFOLLOW);
		free(fullPath);
	} if (res == -1)
		return -errno;

	return 0;
}
#endif

/************************************************************
 *
 * RIGHT NOW WE'RE ASSIGNING 1 FILE PER LOG, BUT WE MAY WANT
 * TO DO MULTIPLE FILES PER LOG
 * ALSO TRY TO WRITE LOGHEADER AFTER THE FILE
 *
 ***********************************************************/
static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
        //retrieve CYCstate
        struct fuse_context *context = fuse_get_context();
        CYCstate state = context->private_data;

	//create inode for new file
	struct inode ind;
	ind.i_file_no = state->vaddrMap->freePtr; //remember to change state->vaddrMap
	ind.i_links_count = 0;
	ind.i_pages = 0;
	ind.i_size = 0;

	// if there is nothing in the partially used free list, use the complete list
	blockData data = (blockData) malloc(sizeof (struct blockData));
	if (state->lists->partial == 0)
	  getBlockData(data, state->lists->complete);                    	
	else
	  getBlockData(data, state->lists->partial);

	//create the logHeader of the log containing this file
	struct logHeader logH;
	logH.erases = data->eraseCount;      //erase = that of 1 block
	logH.logId = state->lists->complete;

	logH.first = (logH.logId)/BLOCKSIZE;
	logH.prev = -1; //since we can't put NULL
	logH.active = 0; //file is empty initially, logHeader does not count as active page
	logH.total = 1; // we remove one block from the pfree list
	logH.logType = LTYPE_FILES;

	logH.content.file.fileCount = 1;
	logH.content.file.fileId[logH.content.file.fileCount - 1] = data->nextPage + 1; //nextPage stores the logHeader
	logH.content.file.fInode = ind;

	//Putting logHeader in activeLog
	struct activeLog theLog;
	theLog.nextPage = data->nextPage + 1; //the same as fileId? supposed to be the one next to logheader
	theLog.last = logH.first;
	theLog.log = logH;

	//Put activeLog in openFile and store it in cache
	struct openFile oFile;
	oFile.currentOpens = 1;
	oFile.mainExtentLog = &theLog;
	oFile.inode = ind;
	oFile.address = data->nextPage + 1;
	state->cache->openFileTable[++(state->cache->headLRU)] = &oFile;
	
	//create stub file
        char *fullPath = makePath(path);
	blocked_file_info fptr;
	fptr = (blocked_file_info) malloc(sizeof (struct blocked_file_info));
	fptr->flag = fi->flags;
	fptr->fd = open(fullPath, O_RDWR | O_CREAT, mode);
	free(fullPath);
	if (fptr->fd == -1)
		return -errno;
	fi->fh = (uint64_t) fptr;

	//write the file id number in stub file
	int res = write(fptr->fd, &(state->vaddrMap->freePtr), sizeof (int));
	if ( res == -1)
	  perror("FAIL TO WRITE STUB FILE");
	int curPtr = state->vaddrMap->freePtr;
	state->vaddrMap->freePtr = abs(state->vaddrMap->map[curPtr]);
	state->vaddrMap->map[curPtr] = data->nextPage + 1;

	//Write the logHeader to virtual NAND
	char buf[sizeof (struct fullPage)];
	memset(buf, 0, sizeof (struct fullPage));
	memcpy(buf, &logH, sizeof (struct logHeader));
	writeNAND(buf, data->nextPage, 0);
	      
	return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
	char *fullPath = makePath(path);
  
	blocked_file_info fptr;
	fptr = (blocked_file_info) malloc(sizeof (struct blocked_file_info));
	fptr->flag = fi->flags;
	fptr->fd = open(fullPath, O_RDWR);
	free(fullPath);
	if (fptr->fd == -1)
		return -errno;

	fi->fh = (uint64_t) fptr;
	return 0;
}

/*PRE:
  path - file's pathname
  buf - target buffer where read bytes go
  size - #bytes to read
  offset - starting offset in file
  fi - file info

  POST: #bytes read (should this be #pages instead?) */
static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
        (void) path;

	if (path != NULL) {
	  char *fullPath = makePath(path);
	  fprintf(stderr, "Reading %s at %d for %d\n", fullPath, (int) offset, (int) size);
	  free(fullPath);
	}
	
	size_t res = 0;//size_t or int?        //return value
	size_t bytesRead;                      //bytes read in for a single page
	struct stat stBuf;                     //space for file info

	printf("FLAG IN xmp_read: %x\n", fi->flags);
	if ((fi->flags & O_WRONLY) != 0) {
	  perror("FILE IS NOT READABLE");
	  return -1;
	}
	
	//save file info into stBuf
	int statRes = fstat(((blocked_file_info) fi->fh)->fd, &stBuf);
	if (statRes == -1)
	  return -errno;

	if (offset > stBuf.st_size)              //offset > file size, no byte read
	  return 0;
	else if (offset + size > stBuf.st_size)  //make sure only reading within file size
	  size = stBuf.st_size - offset;

	int pageNo = offset / PAGESIZE;          //starting page
	offset = offset % PAGESIZE;              //location of offset in that page

	//read in 1st page (containing file prefix)
	//We use a tempBuf since there are cases to check here
	char tempBuf[PAGESIZE];
	bytesRead = pread(((blocked_file_info) fi->fh)->fd, tempBuf, PAGESIZE, pageNo*PAGESIZE);
	if (bytesRead == PAGESIZE || bytesRead >= size) {
	  if (size <= PAGESIZE - offset) {                      //if we only have to read this page
	    memcpy(buf, tempBuf + offset, size);
	    res = size;//try return size here, and put size -= res inside other case
	  } else {                                              //if there are more pages to read
	    memcpy(buf, tempBuf + offset, PAGESIZE - offset);
	    res = PAGESIZE - offset;
	    buf += res;                                         //Do we need to update buf pointer for other case?
	  }
	  size -= res;                           
	} else {
	  perror("ERROR IN READING 1ST PAGE IN xmp_read\n");   //SHOULD NEVER HAPPEN
	  return -1;
	}
	
	//read in pages in-between
	while (size >= PAGESIZE) {
	  pageNo++;
	  bytesRead = pread(((blocked_file_info) fi->fh)->fd, buf, PAGESIZE, pageNo*PAGESIZE);
	  if (bytesRead == PAGESIZE) {
	    res += PAGESIZE;
	    buf += PAGESIZE;
	    size -= PAGESIZE;
	  } else {
	    perror("ERROR IN READING MIDDLE PAGES IN xmp_read\n");   //SHOULD NEVER HAPPEN
	    return -1;
	  }
	}

	//read in last page
	if (size > 0) {
	  pageNo++;
	  bytesRead = pread(((blocked_file_info) fi->fh)->fd, buf, size, pageNo*PAGESIZE);
	  if (bytesRead == size) {
	    return res + size;
	  } else {
	    perror("ERROR IN READING LAST PAGE IN xmp_read\n");
	    return -1;
	  }
	}

	fprintf(stderr,"READ %d BYTES\n", (int) res);
	return res;
}

static int xmp_read_buf(const char *path, struct fuse_bufvec **bufp,
			size_t size, off_t offset, struct fuse_file_info *fi)
{
	struct fuse_bufvec *src;

	fprintf(stderr, "IN xmp_read_buf\n");
	
	(void) path;

	src = malloc(sizeof(struct fuse_bufvec));
	if (src == NULL)
		return -ENOMEM;

	*src = FUSE_BUFVEC_INIT(size);

	src->buf[0].flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
	src->buf[0].fd = ((blocked_file_info) fi->fh)->fd;
	src->buf[0].pos = offset;

	*bufp = src;

	return 0;
}

static char *prepBuffer(char *buf, struct fuse_file_info *fi,
			int pageNo, int startOffset, int endOffset,
			int fileSizeInPages, int fileSize) {
  
  //if new page, unused parts before/after used buffer space should be 0s
  if (pageNo >= fileSizeInPages) {
    fprintf(stderr, "Clearing buffer to %d and from %d\n", startOffset, endOffset);
    memset(buf, 0, startOffset);
    memset(buf + endOffset, 0, PAGESIZE - endOffset);

  //if existing page not completely overwritten, read and store it
  } else if (startOffset > 0 || endOffset < PAGESIZE) {
    int toRead;
    if (pageNo == fileSizeInPages - 1)          //page being written is last page
      toRead = fileSize - pageNo*PAGESIZE;
    else
      toRead = PAGESIZE;

    //store page in buffer
    int bytesRead = pread(((blocked_file_info) fi->fh)->fd, buf, toRead, pageNo*PAGESIZE);
    if (bytesRead != toRead) {
      perror("ERROR IN PREPARING BUFFER FOR xmp_write");
      return NULL;
    }

    memset(buf + bytesRead, 0, PAGESIZE - bytesRead);   //set any bytes not obtained from file to 0s
  }

  return buf;     //later on this will be part of cache
}

static int xmp_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{
	(void) path;

	printf("FLAG IN xmp_write: %x\n", fi->flags);
	if ((fi->flags & (O_RDWR | O_WRONLY)) == 0) {
	  perror("FILE IS NOT WRITEABLE\n");
	  return -1;
	}
  
        int res = 0;              //return value
	char tempBuf[PAGESIZE];   //Space for one page being written
	struct stat stBuf;        //space for file info
	
	//save file info into stBuf
	int statRes = fstat(((blocked_file_info) fi->fh)->fd, &stBuf);
	if (statRes == -1)
	  return -errno;
	fprintf(stderr, "STAT RESULT IN xmp_write:\n %d\n", statRes);

        int fileSize = stBuf.st_size;
	int fileSizeInPages = (fileSize + PAGESIZE - 1) / PAGESIZE;
	int finalFileSize;

	if (offset + size > fileSize)
	  finalFileSize = offset + size;
	else
	  finalFileSize = fileSize;

	int startOffset = offset % PAGESIZE;
	int endOffset;
	int startPage = offset / PAGESIZE;                               //starting page
	int numPages = (startOffset + size + PAGESIZE - 1) / PAGESIZE;   //#pages being written

	for (int i = 0; i < numPages; i++) {

	  if (i == numPages - 1)                         //if writing last page of segment
	    endOffset = (offset + size) % PAGESIZE;
	  else
	    endOffset = PAGESIZE;

	  char *bufPtr = prepBuffer(tempBuf, fi, startPage + i,
				    startOffset, endOffset, fileSizeInPages, fileSize);
	  int dataToMove = endOffset - startOffset;
	  memcpy(bufPtr + startOffset, buf, dataToMove);          //dest and src are switched?

	  int toWrite;
	  if ((startPage + i + 1)*PAGESIZE > finalFileSize)       //if writing last page
	    toWrite = finalFileSize - (startPage + i)*PAGESIZE;
	  else
	    toWrite = PAGESIZE;

	  //Now we can write the page
	  fprintf(stderr, "WRITING %d BYTES AT %d\n", toWrite, (startPage + i)*PAGESIZE);
	  int written = pwrite(((blocked_file_info) fi->fh)->fd, bufPtr, toWrite, (startPage + i)*PAGESIZE);
	  if (written != toWrite) {
	    fprintf(stderr, "UNEXPECTED WRITE RETURN VALUE IN xmp_write\n");
	    return -errno;
	  }

	  res += dataToMove;
	  buf += dataToMove;
	  startOffset = 0;
	}
	
	fprintf(stderr, "WRITE RETURNING %d\n", res);
	return res;
}

static int xmp_write_buf(const char *path, struct fuse_bufvec *buf,
		     off_t offset, struct fuse_file_info *fi)
{
	struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));

	(void) path;

	dst.buf[0].flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
	dst.buf[0].fd = ((blocked_file_info) fi->fh)->fd;
	dst.buf[0].pos = offset;

	return fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
	int res;

	char *fullPath = makePath(path);
	
	res = statvfs(fullPath, stbuf);
	free(fullPath);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_flush(const char *path, struct fuse_file_info *fi)
{
	int res;

	(void) path;
	/* This is called from every close on an open file, so call the
	   close on the underlying filesystem.	But since flush may be
	   called multiple times for an open file, this must not really
	   close the file.  This is important if used on a network
	   filesystem like NFS which flush the data/metadata on close() */
	res = close(dup(((blocked_file_info) fi->fh)->fd));
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	close(((blocked_file_info) fi->fh)->fd);
	free((blocked_file_info) fi->fh);
	
	return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
	int res;
	(void) path;

#ifndef HAVE_FDATASYNC
	(void) isdatasync;
#else
	if (isdatasync)
		res = fdatasync(((blocked_file_info) fi->fh)->fd);
	else
#endif
		res = fsync(((blocked_file_info) fi->fh)->fd);
	if (res == -1)
		return -errno;

	return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int xmp_fallocate(const char *path, int mode,
			off_t offset, off_t length, struct fuse_file_info *fi)
{
	(void) path;

	if (mode)
		return -EOPNOTSUPP;

	return -posix_fallocate(((blocked_file_info) fi->fh)->fd, offset, length);
}
#endif

#ifdef HAVE_SETXATTR
/* xattr operations are optional and can safely be left unimplemented */
static int xmp_setxattr(const char *path, const char *name, const char *value,
			size_t size, int flags)
{
        char *fullPath = makePath(path);
  
	int res = lsetxattr(fullPath, name, value, size, flags);
	free(fullPath);
	if (res == -1)
		return -errno;
	return 0;
}

static int xmp_getxattr(const char *path, const char *name, char *value,
			size_t size)
{
        char *fullPath = makePath(path);
  
	int res = lgetxattr(fullPath, name, value, size);
	free(fullPath);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_listxattr(const char *path, char *list, size_t size)
{
        char *fullPath = makePath(path);
  
	int res = llistxattr(fullPath, list, size);
	free(fullPath);
	if (res == -1)
		return -errno;
	return res;
}

static int xmp_removexattr(const char *path, const char *name)
{
  	char *fullPath = makePath(path);
  
	int res = lremovexattr(fullPath, name);
	free(fullPath);
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

	return ulockmgr_op(fi->fh->fd, cmd, lock, &fi->lock_owner,
			   sizeof(fi->lock_owner));
}
#endif

static int xmp_flock(const char *path, struct fuse_file_info *fi, int op)
{
	int res;
	(void) path;

	res = flock(((blocked_file_info) fi->fh)->fd, op);
	if (res == -1)
		return -errno;

	return 0;
}

static struct fuse_operations xmp_oper = {
	.init           = xmp_init,
	.destroy        = xmp_destroy, //take this out if needed
	.getattr	= xmp_getattr,
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
#ifdef HAVE_UTIMENSAT
	.utimens	= xmp_utimens,
#endif
	.create		= xmp_create,
	.open		= xmp_open,
	.read		= xmp_read,
	//.read_buf	= xmp_read_buf,
	.write		= xmp_write,
	//.write_buf	= xmp_write_buf,
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

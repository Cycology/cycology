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
/*
struct log_sys_con{
	struct node *head;
	struct node *tail;
	};*/

static int xmp_getattr(const char *path, struct stat *stbuf)
{
	int res;

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

	res = fstat(fi->fh, stbuf);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_access(const char *path, int mask)
{
	int res;

	res = access(path, mask);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_readlink(const char *path, char *buf, size_t size)
{
	int res;

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
		res = mkfifo(path, mode);
	else
		res = mknod(path, mode, rdev);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_mkdir(const char *path, mode_t mode)
{
	int res;

	res = mkdir(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_unlink(const char *path)
{
	int res;

	res = unlink(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_rmdir(const char *path)
{
	int res;

	res = rmdir(path);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_symlink(const char *from, const char *to)
{
	int res;

	res = symlink(from, to);
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

	res = rename(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_link(const char *from, const char *to)
{
	int res;

	res = link(from, to);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chmod(const char *path, mode_t mode)
{
	int res;

	res = chmod(path, mode);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid)
{
	int res;

	res = lchown(path, uid, gid);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_truncate(const char *path, off_t size)
{
	printf("\nTruncate: %d\n", (int) size);
	int res;

	res = truncate(path, size);
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_ftruncate(const char *path, off_t size,
			 struct fuse_file_info *fi)
{
	printf("\nfTruncate:%d\n", (int) size);
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
	int res;

	/* don't use utime/utimes since they follow symlinks */
	res = utimensat(0, path, ts, AT_SYMLINK_NOFOLLOW);
	if (res == -1)
		return -errno;

	return 0;
}
#endif

static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
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

	fd = open(path, fi->flags);
	if (fd == -1)
		return -errno;

	fi->fh = fd;
	return 0;
}

//Goes through cache and returns a buffer. If the page is beyond the end of the file, don't read, memset...If page doesn't exist in cache, add it to the bginning. If page exists in cache, return it? When do we update it? Hmmm... We always want to get the buffer back, so we can either have it (in a read), or update it (in a write). Either way, we want a pointer to a buffer that is part of the cache. Use a loop. Have get/set helper methods that will call cacheLoop, and will do the updating. If not already in cache, and within file size, do a read...
/*static char* cacheLoop(const char *path, struct fuse_file_info *fi, int pageNum, int fileSizeInPages){
	//Note: Issue of last page of file still remains.
	struct fuse_context *context = fuse_get_context();
	struct log_sys_con *logCon = context->private_data;

	/*if (pageNum >= fileSizeInPages){
		//malloc a new buffer, memset it, malloc new node with pointer to new buffer, add the node to beginning of cache, and return the buffer
		char *buffer = (char*)malloc(PAGESIZE);
		memset( buffer, 0, PAGESIZE );
		struct node *newHead = (struct node*)malloc(sizeof(struct node));
		struct node *head = logCon->head;
		logCon->head = newHead;
		logCon->head->next = head;
		logCon->head->path = (char*)malloc(strlen(path)+1);
		strcpy(logCon->head->path, path);
		logCon->head->pageNum = pageNum;
		logCon->head->prev = NULL;
		logCon->head->page = buffer;
		return buffer;
	}
	else{
		//create pointer to head of linked list
	struct node* finger = logCon->head;
	struct node* currentHead = logCon->head;
	while (finger != NULL){
		//check if node pointed to is the correct page
		if (strcmp(path, finger->path) == 0 && pageNum == finger->pageNum){
			//move node to head of list
			if (finger->prev != NULL){
				finger->prev->next = finger->next; //prev is null so asking for next will cause our very favourite seg fault.
			}
			if (finger->next != NULL){
				finger->next->prev = finger->prev; //next could also be NULL
			}
			logCon->head = finger;
			logCon->head->next = currentHead;
			if (logCon->head->next != NULL){
				logCon->head->next->prev = logCon->head; //Could be NULL
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
	struct node *newHead = (struct node*)malloc(sizeof(struct node));
	newHead->next = logCon->head;
	if (newHead->next != NULL){
		newHead->next->prev = newHead;
	}
	newHead->path = (char*)malloc(strlen(path)+1);
	strcpy(newHead->path, path);
	newHead->pageNum = pageNum;
	newHead->prev = NULL;
	newHead->page = buffer;
	logCon->head = newHead;
	return buffer;
	}*/




static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{

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
}



static int xmp_read_buf(const char *path, struct fuse_bufvec **bufp,
			size_t size, off_t offset, struct fuse_file_info *fi)
{
	struct fuse_bufvec *src;

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




static int xmp_write(const char *path, const char *buf, size_t size,
		     off_t offset, struct fuse_file_info *fi)
{

	printf("\nWRITE %d bytes at %d\n", (int) size, (int) offset);

	int res = 0;               // number of bytes written
	struct stat stBuf;         // Space for stat results
	char* buffer;
	buffer = (char*)malloc(PAGESIZE);   // Space for one page of file

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

	printf("fsize %d, fsize in pages %d, final fsize %d, sPage %d, sWpage %d, numP %d\n", fileSize, fileSizeInPages, finalFileSize, startPage, startWithinPage, numPages);

	for ( int relativePageNum = 0; relativePageNum < numPages; relativePageNum++ ) {

		printf("rPnum %d\n", relativePageNum);
		int endWithinPage;


		if ( relativePageNum == numPages - 1 ) {
			endWithinPage = (offset + size) % PAGESIZE;
		} else {
			endWithinPage = PAGESIZE;
		}

		char * bufPtr = cacheLoop(path, fi,relativePageNum + startPage, fileSizeInPages);
			//prepBuffer( buffer, fi, relativePageNum + startPage,
			//		    startWithinPage, endWithinPage, fileSizeInPages, fileSize, relativePageNum, path);

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
	free(buffer);
	return res;
}





static int xmp_write_buf(const char *path, struct fuse_bufvec *buf,
		     off_t offset, struct fuse_file_info *fi)
{
	struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));

	(void) path;

	dst.buf[0].flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
	dst.buf[0].fd = fi->fh;
	dst.buf[0].pos = offset;

	return fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
}

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
	int res;

	res = statvfs(path, stbuf);
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
	res = close(dup(fi->fh));
	if (res == -1)
		return -errno;

	return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
	(void) path;
	close(fi->fh);

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
	(void) path;

	if (mode)
		return -EOPNOTSUPP;

	return -posix_fallocate(fi->fh, offset, length);
}
#endif

#ifdef HAVE_SETXATTR
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
	struct log_sys_con *context = (struct log_sys_con*)malloc(sizeof(struct log_sys_con));
	context->head = NULL;
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
#ifdef HAVE_UTIMENSAT
	.utimens	= xmp_utimens,
#endif
	.create		= xmp_create,
	.open		= xmp_open,
	.read		= xmp_read,
//	.read_buf	= xmp_read_buf,
	.write		= xmp_write,
///	.write_buf	= xmp_write_buf,
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
	.init           = xmp_init,
};

int main(int argc, char *argv[])
{
	umask(0);
	return fuse_main(argc, argv, &xmp_oper, NULL);
}

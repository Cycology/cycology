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
#include "loggingDiskFormats.h"
#include "fuseLogging.h"
#include "filesystem.h"
#include "cache.h"
#include "cacheStructs.h"
#include "vNANDlib.h"
#include "helper.h"
#include "logs.h"
/*
 * BECAUSE WE DID NOT IMPLEMENT THE RIGHT FUNCTIONS YET
 */

int unimplemented(void);

int unimplemented(void)
{
  int * bad = NULL;
  return * bad;
}

static char *makePath(const char *path)
{
  int newLen = strlen(path) + strlen(ROOT_PATH) + 1;
  char *fullPath = (char*) malloc(newLen);
  strcpy(fullPath, ROOT_PATH);
  strcat(fullPath, path);
  return fullPath;
}

//Initialize the FS, saving the directory being mounted
static void *xmp_init(struct fuse_conn_info *conn,
		      struct fuse_config *cfg)
{
  // Make the FUSE system calls correspond 1:1 with our system calls
  cfg->direct_io = 1;
  
  // Initialize the state
  CYCstate state = (CYCstate) malloc(sizeof (struct CYCstate));
  state->rootPath = ROOT_PATH;
  state->storePath = STORE_PATH;
  state->nFeatures = initNAND();
  state->addr_cache = cache_create(1000); //TODO: Change the size
  initCYCstate(state);

  //print CYCstate content
  printf("CYCstate:\n rootPath: %s\n storePath: %s\n numBlocks: %d\n memSize: %d\n vaddrMap.freePtr: %d\n", state->rootPath, state->storePath, state->nFeatures.numBlocks,
	 state->nFeatures.memSize, state->vaddrMap->freePtr);
  
  //keep CYCstate
  return state;
}

static void xmp_destroy(void *private_data)
{

  // This is a temporary version of xmp_destroy which simply write the superblock and
  // the virtual address map over the previous copy. This would not work in a real
  // system based on NAND. So, at some point, we need to implement a more realistic
  // version that writes these structures to new locations.

  // In addition, since at this point we are only writing initial inodes when we create
  // empty files, we don't have to worry about flushing dirty/cached meta-data to the
  // NAND. More work for later!
  
  
  struct fuse_context *context = fuse_get_context();
  CYCstate state = context->private_data;

  struct fullPage page;

  readNAND( &page, 0 );

  superPage superBlock = (superPage) &page.contents;

  superBlock->freeLists = state->lists;

  writeNAND( &page, 0, 1);
  
  page_addr mapPage = superBlock->latest_vaddr_map;

  int mapSize = sizeof( struct addrMap) + state->vaddrMap->size*sizeof(page_addr);
  memcpy( &page.contents, state->vaddrMap, mapSize );

  writeNAND( &page, mapPage, 1);
  

  /* //retrieve CYCstate */
  /* CYCstate state = private_data; */

  /* //read in superBlock */
  /* char buf[sizeof (struct fullPage)]; */
  /* readNAND(buf, 0); */
  /* superPage superBlock = (superPage) buf; */

  /* /\*write the updated vaddr map to NAND*\/ */
  /* char buf2[sizeof (struct fullPage)]; */
  /* page_addr previous = superBlock->latest_vaddr_map; */
  
  /* //if previous vaddrMap is in the 2nd last page of a block, */
  /* //allocate a new block */
  /* if ((previous + 2) % BLOCKSIZE == 0) { */
  /*   //NEED CODE HERE TO ALLOCATE NEW BLOCK, HAS TO BE DIFFERENT FROM GETFREEBLOCK METHOD */

  /*   //put previous block into Completely used free list */
  /*   page_addr pageAddr = state->lists.completeTail; */
  /*   struct fullPage page; */
  /*   page.nextLogBlock = (previous/BLOCKSIZE)*BLOCKSIZE; */
  /*   writeNAND((char*)page, pageAddr, 0); */
  /*   state->lists.completeTail = previous + 1;  */
  /* } */

  /* //write the most recent vaddrMap from CYCstate */
  /* readNAND(buf2, newBlock);    //next page to write vaddr map is right after the current one */
  /* memcpy(buf2, state->vaddrMap, sizeof (struct addrMap) + state->vaddrMap->size*sizeof(page_addr)); */
  /* writeNAND(buf2, newBlock, 0); */
  
  /* /\*save and update the superBlock*\/ */
  /* //superBlock->prev_vaddr_map = newPrevious; */
  /* superBlock->latest_vaddr_map = newBlock; */
  /* writeNAND(buf, 0, 1); */
  /* //we cheat here and write superBlock */
  /* //to the same page 0 everytime */
  
  /* free(private_data); */
  /* stopNAND(); */
}

/*
 * REMEMBER TO WRITE THIS FUNCTION!
 */
static int xmp_fstat(openFile oFile, struct stat *stbuf)
{
  //  stbuf->st_dev = 0;
  stbuf->st_ino = oFile->address;
  stbuf->st_mode = oFile->inode.i_mode;
  stbuf->st_nlink = oFile->inode.i_links_count;
  stbuf->st_uid = oFile->inode.i_uid;
  stbuf->st_gid = oFile->inode.i_gid;
  stbuf->st_rdev = 0;  // irrelevant since no special files
  stbuf->st_size = oFile->inode.i_size;

  stbuf->st_blksize = PAGESIZE;
  stbuf->st_blocks = oFile->inode.i_pages * (stbuf->st_blksize / 512 );

  //stbuf->st_atim = oFile->inode.i_atime;
  //stbuf->st_mtim = oFile->inode.i_mtime;
  //stbuf->st_ctim = oFile->inode.i_ctime;
  
  return 0;
}

static int xmp_getattr(const char *path, struct stat *stbuf,
		       struct fuse_file_info *fi)
{
  int res;

  (void) path;
	
  if(fi)
    res = xmp_fstat(((log_file_info) fi->fh)->oFile, stbuf);
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

  char *fullPath = makePath(path);
	
  if (S_ISFIFO(mode))
    res = mkfifo(fullPath, mode);
  else
    res = mknod(fullPath, mode, rdev);
	
  free(fullPath);
	
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
	
  //we don't check for hard/sym link, for now
  //assume all hard links for now

  //check how many hard links the file has left
  //we can also access hard link info in inode
  struct stat st;
  res = stat(fullPath, &st);
  if (res == -1)
    return -errno;

  //more than 1 hard links, we simply unlink normally
  if(st.st_nlink > 1) {
    res = unlink(fullPath);
    free(fullPath);
    if (res == -1)
      return -errno;
    return 0;
  }

  /*Otherwise this is the last hard link to file.
   *Check if the file is still open
   *(openFile does not exist if file is not open)
   */

  // Retrieve CYCstate
  struct fuse_context *context = fuse_get_context();
  CYCstate state = context->private_data;
	
  // Read stub file for fileID
  page_vaddr fileID = readStubFile(fullPath);

  // Check if there is any openFile
  openFile oFile = state->file_cache->openFileTable[fileID];

  // Last link to file, no openFile
  if (oFile == NULL) {
    // Get the logHeader of this file
    page_addr logHeaderAddr = state->vaddrMap->map[fileID];
    struct fullPage page;
    readNAND( & page, logHeaderAddr);
    logHeader logH = (logHeader) &(page.contents); // TODO: Double check

    // Get the most recent logHeader
    page_addr lastHeaderAddr = state->vaddrMap->map[logH->logId];
    struct fullPage  page2;
    readNAND( &page2, lastHeaderAddr);
    logHeader lastLogH = (logHeader) &(page2.contents);

    // Remove fileID from log's ID list.
    // Always remove from index 0 since for now, since there's only 1 file per log
    lastLogH->content.file.fileCount--;
    lastLogH->content.file.fileId[0] = 0;

    //if there's no more files in the log, recycle!
    if (lastLogH->content.file.fileCount == 0) {
      logs_recycle( lastLogH, lastHeaderAddr, page2.eraseCount, state );
    } else {
      // There was more than one file in the log so nothing else to do?
    }

    // Remove the stub file
    res = unlink(fullPath);
    free(fullPath);
    if (res == -1)
      return -errno;
    return 0;	
	  
  } else {
    // TODO: The file is currently open so we need to flag it for removal
    // when all opens are closed.
  }
	
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

  //IF THE FILE IS A DIRECTORY, WE JUST WANT TO PASS THE CMOD REQUEST TO THE FILE SYSTEM
  //IF IT IS NOT A DIRECTORY, WE NEED TO OPEN A FILE 

  unimplemented();
  if(fi)
    res = 0; //fchmod(((blocked_file_info) fi->fh)->fd, mode);
	 
  else {
    char *fullPath = makePath(path);
    res = chmod(fullPath, mode);
    free(fullPath);
  }

  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_chown(const char *path, uid_t uid, gid_t gid,
		     struct fuse_file_info *fi)
{
  int res;

  //IF THE FILE IS A DIRECTORY, WE JUST WANT TO PASS THE CHOWN REQUEST TO THE FILE SYSTEM
  //IF IT IS NOT A DIRECTORY, WE NEED TO OPEN A FILE 

  unimplemented();
  if (fi)
    res = 0; //fchown(((blocked_file_info) fi->fh)->fd, uid, gid);
  else {
    char *fullPath = makePath(path);
    res = lchown(fullPath, uid, gid);
    free(fullPath);
  } if (res == -1)
      return -errno;

  return 0;
}

/*
 * NEED TO WRITE THIS FUNCTION!!!
 */
static int xmp_truncate(const char *path, off_t size,
			struct fuse_file_info *fi)
{
  int res;

  unimplemented();
  if(fi)
    res = 0; //ftruncate(((blocked_file_info) fi->fh)->fd, size);
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

  //IF THE FILE IS A DIRECTORY, WE JUST WANT TO PASS THE REQUEST TO THE FILE SYSTEM
  //IF IT IS NOT A DIRECTORY, WE NEED TO OPEN A FILE 
  unimplemented();
  /* don't use utime/utimes since they follow symlinks */
  if (fi)
    res = 0; //futimens(((blocked_file_info) fi->fh)->fd, ts);
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
 * ALSO TRY TO WRITE LOGHEADER AFTER THE FILE, INSTEAD OF BEFORE
 *
 ***********************************************************/
static int xmp_create(const char *path, mode_t mode, struct fuse_file_info *fi)
{
  printf("WE GOT TO XMP_CREATE with mode %o \n", mode);
  //retrieve CYCstate
  struct fuse_context *context = fuse_get_context();
  CYCstate state = context->private_data;
  page_vaddr fileID = getFreePtr(state->vaddrMap);

  //next free page where we write logHeader containing the inode
  page_addr logHeaderPage;
	
  //Get log for file; this also handles creating new inode & logHeader for file
  activeLog theLog = getLogForFile(state, fileID, &logHeaderPage, mode);
  state->vaddrMap->map[fileID] = logHeaderPage;
		  
  //Put activeLog in openFile and store it in cache
  openFile oFile = (openFile) malloc(sizeof (struct openFile));
  (theLog->log.content.file.fInode.i_links_count)++;
  initOpenFile(oFile, theLog, &(theLog->log.content.file.fInode), fileID);
  state->file_cache->openFileTable[fileID] = oFile;

  // Increment the openFile Count
  oFile->currentOpens++;
	
  //create stub file (WITHOUT KEEPING TRACK OF FD)
  char *fullPath = makePath(path);
  log_file_info fptr;
  fptr = (log_file_info) malloc(sizeof (struct log_file_info));
  int stubfd = open(fullPath, O_RDWR | O_CREAT, S_IRWXU + S_IRGRP + S_IROTH);
  printf("About to open stub file %s\n", fullPath );
  free(fullPath);
  if (stubfd == -1) {
    perror("ERROR: failed to open stub file --- " );
    return -errno;
  }
  printf("stub file opened as %d\n", stubfd );
  fptr->flag = fi->flags;
  fptr->oFile = oFile;
  fi->fh = (uint64_t) fptr;

  //write the file id number in stub file
  int res = write(stubfd, &fileID, sizeof (int));
  if ( res == -1)
    perror("FAIL TO WRITE STUB FILE");
  close(stubfd);

  //Write the logHeader to virtual NAND
  writeablePage buf = getFreePage(&(state->lists),theLog);
	
  memcpy(buf->nandPage.contents, &(theLog->log), sizeof (struct logHeader));
  buf->nandPage.pageType = PTYPE_INODE;

  writeNAND( &(buf->nandPage), buf->address, 0);
	
  // I HAVE A FEELING THERE SHOULD BE A free(buf) HERE!!! - Tom 9/12/17
	
  return 0;
}

static int xmp_open(const char *path, struct fuse_file_info *fi)
{
  //retrieve CYCstate
  struct fuse_context *context = fuse_get_context();
  CYCstate state = context->private_data;

  //open stub file
  char *fullPath = makePath(path);
  page_vaddr fileID = readStubFile(fullPath);
  free(fullPath);	
  if (fileID == -1)
    return -1;
  log_file_info fptr = (log_file_info) malloc(sizeof (struct log_file_info));

  //verify access rights
	
  /*check if there is an existing openFile for the file
   *if not, create new openFile */
  openFile oFile = state->file_cache->openFileTable[fileID];
  if (oFile == NULL) {
    // Get the file's inode
    page_addr logHeaderAddr = state->vaddrMap->map[fileID];
    struct fullPage page;
    readNAND( & page, logHeaderAddr);
    logHeader fileLogHeader = (logHeader) &(page.contents);
    inode ind = &(fileLogHeader->content.file.fInode);
    page_vaddr logID = ind->i_log_no;
		    
    // Check if the file's log is already active
    activeLog actLog = (activeLog) state->file_cache->openFileTable[logID];

    if (actLog == NULL) {
      // Get the last written logHeader
      actLog = (activeLog) malloc(sizeof (struct activeLog));
      logHeader lastHeader;
      page_addr lastHeaderAddr = state->vaddrMap->map[logID];

      // Check if there is a more recent header
      if (lastHeaderAddr != logHeaderAddr) {
	struct fullPage page2;
	readNAND( & page2,lastHeaderAddr);
	lastHeader = (logHeader) &(page2.contents);
      } else {
	lastHeader = fileLogHeader;
      }

      // Initialize the active log with the most recent header
      actLog->log = *(lastHeader);      
      initActiveLog(actLog, lastHeaderAddr); // TODO: + 1);
      state->file_cache->openFileTable[logID] = (openFile) actLog;
    }

    // Increment activelog reference count
    actLog->activeFileCount++;
	
    // Make a new openFile
    oFile = (openFile) malloc(sizeof (struct openFile));
    initOpenFile(oFile, actLog, ind, fileID);

    // Update the openFileTable
    state->file_cache->openFileTable[fileID] = oFile;
  }
	
  oFile->currentOpens++;

  fptr->oFile = oFile;
  fptr->flag = fi->flags;
  fi->fh = (uint64_t) fptr;
  return 0;
}

/* */
static int xmp_read(const char *path, char *buf, size_t size, off_t offset,
		    struct fuse_file_info *fi)
{
  CYCstate state = fuse_get_context()->private_data;
  addressCache addrCache = state->addr_cache;

  // Only read up to the max amount of data in the file
  openFile file = ((log_file_info) fi->fh)->oFile;
  if (file->inode.i_size == 0) {
    printf("\nFILE HAS NO DATA\n");
    return -1;
  } else if (size > file->inode.i_size) {
    size = file->inode.i_size;
  } 
  
  (void) path;
  if (path != NULL) {
    char *fullPath = makePath(path);
    printf(stderr, "Reading %s at %d for %d\n", fullPath, (int) offset, (int) size);
    free(fullPath);
  }	

  // Verify if the file has read permission
  int flag = ((log_file_info) fi->fh)->flag;
  printf("FLAG IN xmp_read: %x\n", flag);
  if ((flag & O_WRONLY) != 0) {
    perror("FILE IS NOT READABLE");
    return -1;
  }	

  // Init variables
  struct pageKey key_s;

  pageKey key = &key_s;
  int bytesLeft = size;
  size_t bytesRead = 0;
  size_t bytesInPage = 0;
  
  // Prepare the first page
  unsigned int pageStartOffset = (offset / PAGESIZE) * PAGESIZE;
  unsigned int relativeOffset = offset - pageStartOffset;
  bytesInPage = PAGESIZE - relativeOffset;
  key->file = file;
  key->dataOffset = pageStartOffset;
  key->levelsAbove = 0;    

  // In case we need to read less than a full page
  if (bytesLeft < bytesInPage) {
    bytesInPage = bytesLeft;
  }
  
  // Read in the next pages
  while (bytesLeft > 0) {    
    // Get the next page
    cacheEntry entry = fs_getPage(addrCache, key);

    // Read into the buf
    memcpy(buf+bytesRead, entry->wp->nandPage.contents+relativeOffset, bytesInPage);

    // Set up for the next page
    key->dataOffset += PAGESIZE;
    bytesLeft -= bytesInPage;
    bytesRead += bytesInPage;
    relativeOffset = 0;

    if (bytesLeft >= PAGESIZE) {
      // Is middle page, read in the full page
      bytesInPage = PAGESIZE;
    } else {
      // Is the last page, only read whatever remains
      bytesInPage = bytesLeft;
    }
  }

  if (bytesLeft < 0) {
    printf("\nERROR: Read more bytes than given\n");
  }
    
  return bytesRead;
}

static int xmp_read_buf(const char *path, struct fuse_bufvec **bufp,
			size_t size, off_t offset, struct fuse_file_info *fi)
{
  unimplemented();
  struct fuse_bufvec *src;

  printf(stderr, "IN xmp_read_buf\n");
	
  (void) path;

  src = malloc(sizeof(struct fuse_bufvec));
  if (src == NULL)
    return -ENOMEM;

  *src = FUSE_BUFVEC_INIT(size);

  src->buf[0].flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
  src->buf[0].fd = 0; // ((blocked_file_info) fi->fh)->fd;
  src->buf[0].pos = offset;

  *bufp = src;

  return 0;
}

static char *prepBuffer(char *buf, struct fuse_file_info *fi,
			int pageNo, int startOffset, int endOffset,
			int fileSizeInPages, int fileSize) {
  
  //if new page, unused parts before/after used buffer space should be 0s
  if (pageNo >= fileSizeInPages) {
    printf(stderr, "Clearing buffer to %d and from %d\n", startOffset, endOffset);
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
    int bytesRead = 0; //pread(((blocked_file_info) fi->fh)->fd, buf, toRead, pageNo*PAGESIZE);
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
  if (size == 0) {
    printf("\nERROR: XMP_WRITE SIZE = 0\n");
    return -1;
  }
  
  CYCstate state = fuse_get_context()->private_data;
  addressCache addrCache = state->addr_cache;
  
  (void) path;

  //verify if the file has read permission
  int flag = ((log_file_info) fi->fh)->flag;
  // TODO: REMOVE printf("FLAG IN xmp_write: %x\n", flag);
  if ((flag & (O_RDWR | O_WRONLY)) == 0) {
    perror("FILE IS NOT WRITEABLE\n");
    return -1;
  }
  
  // Init variables
  char tempBuf[PAGESIZE];
  openFile oFile = ((log_file_info) fi->fh)->oFile;
  int fileSize = oFile->inode.i_size;

  size_t bytesToWrite = size;
  size_t bytesWritten = 0;
  size_t relativeOffset = offset % PAGESIZE;
  size_t startPageOffset = (offset / PAGESIZE) * PAGESIZE;

  // Modify the size 
  int finalFileSize = offset + bytesToWrite;
  if (finalFileSize > fileSize) {
    int remainder = finalFileSize % PAGESIZE;
    finalFileSize += PAGESIZE - remainder;
  } else {
    finalFileSize = fileSize;
  }

  // Create the key for the first page
  struct pageKey key_s;
  pageKey key = &key_s;
  key->file = oFile;
  key->dataOffset = startPageOffset;
  key->levelsAbove = 0;

  // Write out the first page
  size_t writeableBytesInPage = PAGESIZE - relativeOffset;
  if (writeableBytesInPage > size) {
    writeableBytesInPage = size;
    bytesToWrite = writeableBytesInPage;
  }
  cacheEntry page = fs_getPage(addrCache, key);
  memcpy(tempBuf, page->wp->nandPage.contents, PAGESIZE);
  memcpy(tempBuf+relativeOffset, buf, writeableBytesInPage);
  
  // TODO: REMOVE printf("\n**XMP_WRITE: First Page. BytesToWrite: %d\n", bytesToWrite);
  fs_writeData(addrCache, key, tempBuf);

  if (bytesToWrite < writeableBytesInPage) {
    bytesWritten = bytesToWrite;
    bytesToWrite = 0;
  } else {
    bytesWritten += writeableBytesInPage;
    bytesToWrite -= writeableBytesInPage;
  }

  key->dataOffset += PAGESIZE;
  
  // Write out the middle pages
  while (bytesToWrite > 0) {
    // TODO: REMOVE printf("**XMP_WRITE: Middle pages. BytesToWrite: %d\n", bytesToWrite);
    page = fs_getPage(addrCache, key);

    // Save the page's old contents in tempBuf if necessary
    if (bytesToWrite >= PAGESIZE) {
      // Middle pages
      writeableBytesInPage = PAGESIZE;

    } else {
      // Last page
      memcpy(tempBuf, page->wp->nandPage.contents, PAGESIZE);
      writeableBytesInPage = bytesToWrite;
    }

    // Write out the updated information to tempBuf
    memcpy(tempBuf, buf + bytesWritten, writeableBytesInPage);
    
    fs_writeData(addrCache, key, tempBuf);

    // Update for the next page
    bytesToWrite -= writeableBytesInPage;
    bytesWritten += writeableBytesInPage;
    key->dataOffset += PAGESIZE;
  }

  // TODO: Check final file size against actual file size afterwards
  
  // TODO: REMOVE printf("Finished XMP_WRITE(). Bytes Written: %d\n", bytesWritten);

  return bytesWritten;
}

static int xmp_write_buf(const char *path, struct fuse_bufvec *buf,
			 off_t offset, struct fuse_file_info *fi)
{
  struct fuse_bufvec dst = FUSE_BUFVEC_INIT(fuse_buf_size(buf));

  (void) path;

  dst.buf[0].flags = FUSE_BUF_IS_FD | FUSE_BUF_FD_SEEK;
  dst.buf[0].fd = 0; // ((blocked_file_info) fi->fh)->fd;
  dst.buf[0].pos = offset;

  return fuse_buf_copy(&dst, buf, FUSE_BUF_SPLICE_NONBLOCK);
}

/*
 *refer back to our "Function Design" document as well as the info we put in "statfstest.c"
 */

static int xmp_statfs(const char *path, struct statvfs *stbuf)
{
  //int res;
  //char *fullPath = makePath(path);	
  //res = statvfs(fullPath, stbuf);
  //free(fullPath);
  //if (res == -1)
  //	return -errno;
  struct fuse_context *context = fuse_get_context();
  CYCstate state = context->private_data;
	
  stbuf->f_bsize = PAGESIZE;
  stbuf->f_blocks = BLOCKSIZE*(state->nFeatures.numBlocks);
  //what do they mean by free pages in f_bfree?
  stbuf->f_bavail = BLOCKSIZE*(state->nFeatures.numBlocks - 2); //minus superBlock and addrMap block
  //we're not sure about file nodes in f_files and f_ffree either
  //f_fsid too?
  stbuf->f_namemax = 255;   //matches that of underlying fs
  //f_frsize and f_flag?
	
  return 0;
}

static int xmp_flush(const char *path, struct fuse_file_info *fi)
{
  /////////////////////////////////////////////////////////
  //  THIS FUNCTION IS NOT IMPLEMENTED. AT THIS POINT, IT
  //  DOES NOTHING
  /////////////////////////////////////////////////////////
  
  int res;

  (void) path;
  /* This is called from every close on an open file, so call the
     close on the underlying filesystem.	But since flush may be
     called multiple times for an open file, this must not really
     close the file.  This is important if used on a network
     filesystem like NFS which flush the data/metadata on close() */
  res = 0; //close(dup(((blocked_file_info) fi->fh)->fd));
  if (res == -1)
    return -errno;

  return 0;
}

static int xmp_release(const char *path, struct fuse_file_info *fi)
{
  // This is akin to xmp_close(), if that were to exist
  // This is where we flush out LRU files
  (void) path;

  // Retrieve CYCstate
  struct fuse_context *context = fuse_get_context();
  CYCstate state = context->private_data;
  addressCache addrCache = state->addr_cache;
  page_vaddr fileID = ((log_file_info) fi->fh)->oFile->inode.i_file_no;
  page_vaddr logID = ((log_file_info) fi->fh)->oFile->inode.i_log_no;
  openFile releasedFile = state->file_cache->openFileTable[fileID];

  // Decrement open counts
  releasedFile->currentOpens--;
  releasedFile->mainExtentLog->activeFileCount--;

  // Release the File if no more opens  
  if ((((log_file_info) fi->fh)->oFile->currentOpens) == 0) {
    // Flush all data/metadata pages
    fs_flushDataPages(addrCache, releasedFile);
    fs_flushMetadataPages(addrCache, releasedFile);
    fs_removeFileFromLru(releasedFile);
    
    // Write the most current logHeader from cache to NAND 
    if (releasedFile->modified == 1) {
      // Copy the current inode into the logHeader
      memcpy(&releasedFile->mainExtentLog->log.content.file.fInode, &releasedFile->inode, sizeof (struct inode));

      // Write out the logHeader to NAND
      writeablePage buf = getFreePage(&(state->lists), releasedFile->mainExtentLog);
      memcpy(buf->nandPage.contents, &(releasedFile->mainExtentLog->log), sizeof (struct logHeader));
      writeNAND( &(buf->nandPage), buf->address, 0);

      // TODO: free() buf!!!
      // Update the vAddr Map
      state->vaddrMap->map[fileID] = buf->address;
      state->vaddrMap->map[logID] = buf->address;
    }
	  
    //remove openFile from cache since it's not referenced anymore
    free(state->file_cache->openFileTable[fileID]);
    state->file_cache->openFileTable[fileID] = NULL;
  }

  // Release the log if no more files open
  activeLog aLog = state->file_cache->openFileTable[logID];
  if (aLog != NULL && aLog->activeFileCount == 0) {
    free(state->file_cache->openFileTable[logID]);
    state->file_cache->openFileTable[logID] = NULL;
  }

  free((log_file_info) fi->fh);

  // TODO: REMOVE printf("\nFILE CLOSED\n");
  
  return 0;
}

static int xmp_fsync(const char *path, int isdatasync,
		     struct fuse_file_info *fi)
{
  unimplemented();
  int res;
  (void) path;

#ifndef HAVE_FDATASYNC
  (void) isdatasync;
#else
  if (isdatasync)
    res = 0; //fdatasync(((blocked_file_info) fi->fh)->fd);
  else
#endif
    res = 0; // fsync(((blocked_file_info) fi->fh)->fd);
  if (res == -1)
    return -errno;

  return 0;
}

#ifdef HAVE_POSIX_FALLOCATE
static int xmp_fallocate(const char *path, int mode,
			 off_t offset, off_t length, struct fuse_file_info *fi)
{
  unimplemented();
  (void) path;
  
  if (mode)
    return -EOPNOTSUPP;
  
  return 0; //-posix_fallocate(((blocked_file_info) fi->fh)->fd, offset, length);
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
  unimplemented();
  int res;
  (void) path;

  res = 0; // flock(((blocked_file_info) fi->fh)->fd, op);
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

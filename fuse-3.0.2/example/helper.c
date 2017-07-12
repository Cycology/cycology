#define FUSE_USE_VERSION 30

#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#define _GNU_SOURCE

//#include <fuse.h>

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

#include "vNANDlib.h"
#include "helper.h"

//data structure to hold map
addrMap initAddrMap(void)
{
  initNAND();
  addrMap map = (addrMap) malloc(sizeof( struct addrMap));
  
  char page[sizeof (struct fullPage)];
  readNAND(page, BLOCKSIZE);
  char buf[sizeof (struct addrMap)];
  memcpy(buf, page, sizeof (struct addrMap));
  map = (addrMap) buf;    //do we need malloc?

  stopNAND();
  return map;
}

//data structure to hold cache
pageCache initCache(void)
{
  pageCache cache = (pageCache) malloc(sizeof (struct pageCache));
  cache->size = 0;
  cache->headLRU = NULL;
  cache->tailLRU = NULL;
  memset(cache->openFileTable, 0, (PAGEDATASIZE/4 - 2));
  return cache;
}

freeList initFreeLists(void)
{
  initNAND();
  
  char buf[sizeof (struct fullPage)];
  readNAND(buf, 0);
  freeList lists = (freeList) malloc(sizeof (struct freeList));
  memcpy(lists, buf, sizeof (struct freeList));
  
  stopNAND();
  return lists;
}

blockData nextFreeBlockData(freeList lists)
{
  //if there's nothing in the partially used free list
  if (lists->partial == 0)
    {
      initNAND();

      char buf[sizeof (struct block)];
      readNANDBlock(buf, lists->complete);
      
      stopNAND();
      return (blockData) buf;
    }
}

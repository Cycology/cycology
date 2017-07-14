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
void initAddrMap(addrMap map)
{
  initNAND();
  
  char page[sizeof (struct fullPage)];
  readNAND(page, BLOCKSIZE);
  memcpy(map, page, sizeof (struct addrMap));

  stopNAND();
}

//data structure to hold cache
void initCache(pageCache cache)
{
  cache->size = 0;
  cache->headLRU = NULL;
  cache->tailLRU = NULL;
  memset(cache->openFileTable, 0, (PAGEDATASIZE/4 - 2));
}

void initFreeLists(freeList lists)
{
  initNAND();
  
  char buf[sizeof (struct fullPage)];
  readNAND(buf, 0);
  memcpy(lists, buf, sizeof (struct freeList));
  
  stopNAND();
}

int getBlockData(blockData data, page_vaddr page)
{
      initNAND();

      int res = readBlockData((char *)data, page);
      
      stopNAND();
      return res;
}

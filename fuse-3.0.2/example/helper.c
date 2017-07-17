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


void initCYCstate(CYCstate state)
{
  //read in superBlock
  char page[sizeof (struct fullPage)];
  readNAND(page, 0);
  superPage superBlock;
  memcpy(superBlock, page, sizeof (struct superPage));

  //init freeLists
  state->lists = superBlock->freeLists;
  
  //init vaddrMap
  readNAND(page, superBlock->latest_vaddr_map);
  memcpy(state->vaddrMap, page, sizeof (struct addrMap));

  //init cache
  state->cache->size = 0;
  state->cache->headLRU = NULL;
  state->cache->tailLRU = NULL;
  memset(state->cache->openFileTable, 0, (PAGEDATASIZE/4 - 2));
}

//return the next free slot in the virtual address map
//if there's only 1 free slot left, freePtr = 0
int getFreePtr(addrMap map)
{
  int ptr = map->freePtr;
  map->freePtr = abs(map->map[ptr]);
  return ptr;
}

//data structure to hold map
/* void initAddrMap(addrMap map) */
/* { */
/*   char page[sizeof (struct fullPage)]; */
/*   readNAND(page, 0); */
/*   superPage superBlock; */
/*   memcpy(superBlock, page, sizeof (struct superPage)); */
/*   readNAND(page, superBlock->latest_vaddr_map); */
/*   memcpy(map, page, sizeof (struct addrMap)); */
/* } */

//data structure to hold cache
/* void initCache(pageCache cache) */
/* { */
/*   cache->size = 0; */
/*   cache->headLRU = NULL; */
/*   cache->tailLRU = NULL; */
/*   memset(cache->openFileTable, 0, (PAGEDATASIZE/4 - 2)); */
/* } */

/* void initFreeLists(freeList lists) */
/* { */
/*   char buf[sizeof (struct fullPage)]; */
/*   readNAND(buf, 0); */
/*   memcpy(lists, buf, sizeof (struct freeList)); */
/* } */

int getBlockData(blockData data, page_vaddr page)
{
      int res = readBlockData((char *)data, page);
      
      return res;
}

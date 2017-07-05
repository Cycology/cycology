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

#include "helper.h"
#include "fuseLogging.h"

//data structure to hold map
addrMap initAddrMap(){
  addrMap addressMap = (addrMap)malloc(sizeof(struct addrMap)); //would want to fix the size of pag_addr map[] field later on
  addressMap->size = PAGESIZE;
  addressMap->map = page_addr[PAGESIZE];
  for (int i = 0, i < addressMap->size, i++){
    addressMap->map[i] = 0;
  }
  return addrMap;
}

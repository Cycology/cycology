#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vNANDlib.h"

static struct superPage superBlock;

int main (int argc, char *argv[])
{
  if (argc > 1) {        //Takes no arguments
    printf("ERROR: makefs TAKES NO ARGS\n");
    return -1;
  }

  struct nandFeatures features = initNAND();
  
  //write superBlock to block 0
  struct fullPage page;
  
  memset( &page, 0, sizeof (struct fullPage));
  superBlock.latest_vaddr_map = BLOCKSIZE;  //first vaddr map is in block 1
  superBlock.freeLists.completeHead = BLOCKSIZE*2;  //free list starts at block 2
  superBlock.freeLists.completeTail = BLOCKSIZE*(features.numBlocks - 1);
  memcpy(&page.contents, &superBlock, sizeof (struct superPage));
  page.eraseCount = -1;
  page.pageType = PTYPE_SUPER;

  writeNAND( (char *) &page, 0, 0);

  //initialize vaddr map
  memset( &page, 0, sizeof (struct fullPage));
  addrMap map = (addrMap) &page.contents;
  map->size = PAGEDATASIZE/4 - 2;
  map->freePtr = 1;

  //link map entries to next available entry

  for (int i = 1; i < map->size - 1; i++) {
    map->map[i] = -(i+1);
  }
  map->map[map->size - 1] = 0;     //last map entry

  page.pageType = PTYPE_ADDRMAP;
  
  //write vaddr into block 1
  writeNAND( (char *)&page, BLOCKSIZE, 0);

  //linking all blocks from block 2 to last block
  memset( &page, 0, sizeof (struct fullPage));
  page.pageType = PTYPE_NEXTLINK;
  
  for (int i = 2; i < features.numBlocks - 1; i++) {
    page.nextLogBlock = (i+1)*BLOCKSIZE;
    writeNAND( (char *) &page, (i+1)*BLOCKSIZE - 1, 1);
    //writeNAND(array, (i+1)*BLOCKSIZE - 2, 1);
  }
  
  stopNAND();
}

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
  char array[sizeof (struct fullPage)];
  memset(array, 0, sizeof (struct fullPage));
  superBlock.latest_vaddr_map = BLOCKSIZE;  //first vaddr map is in block 1
  superBlock.freeLists.completeHead = BLOCKSIZE*2;  //free list starts at block 2
  superBlock.freeLists.completeTail = BLOCKSIZE*(features.numBlocks) - 1;
  memcpy(array, &superBlock, sizeof (struct superPage));
  writeNAND(array, 0, 0);

  //initialize vaddr map
  memset(array, 0, sizeof (struct fullPage));
  struct addrMap map;
  map.size = PAGEDATASIZE/4 - 2;
  map.freePtr = 1;

  //link map entries to next available entry
  int i = 1;
  while (i < map.size - 1) {
    ++i;
    map.map[i] = -i;
  }
  map.map[map.size - 1] = 0;     //last map entry

  //write vaddr into block 1
  memcpy(array, &map, sizeof (struct addrMap));
  writeNAND(array, BLOCKSIZE, 0);

  //linking all blocks from block 2 to last block
  memset(array, 0, sizeof (struct fullPage));
  struct fullPage page;

  for (i = 2; i < features.numBlocks - 1; i++) {
    page.nextLogBlock = (i+1)*BLOCKSIZE;
    memcpy(array, &page, sizeof (struct fullPage));
    writeNAND(array, (i+1)*BLOCKSIZE - 1, 1);
    //writeNAND(array, (i+1)*BLOCKSIZE - 2, 1);
  }
  
  stopNAND();
}

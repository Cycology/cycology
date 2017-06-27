#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fuseLogging.h"
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
  char array[PAGESIZE];
  memset(array, 0, PAGESIZE);
  superBlock.latest_vaddr_map = BLOCKSIZE;  //first vaddr map is in block 1
  superBlock.cFreeListPtr = BLOCKSIZE*2;    //free list starts at block 2
  memcpy(array, &superBlock, sizeof (struct superPage));
  writeNAND(array, 0, 0);

  //write vaddr map to block 1
  memset(array, 0, PAGESIZE);
  struct addrMap map;
  map.size = PAGESIZE; //what? comment on paper.
  memcpy(array, &map, sizeof (struct addrMap));
  writeNAND(array, BLOCKSIZE, 0);

  //linking all blocks from block 2 to last block
  int i = 2;
  int n;
  memset(array, 0, PAGESIZE);
  while (i < features.numBlocks - 1) {
    n = (++i)*BLOCKSIZE;
    itoa(n, array, 10); 
    writeNAND(array, (n - 1), 1);
  }  
}

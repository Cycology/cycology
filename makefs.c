#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>

#include "fuseLogging.h"
#include "vNANDlib.h"

static struct superPage superBlock;

int main (int argc, char *argv[])
{
  if (argc > 1) {        //Takes no arguments
    printf("ERROR: makefs TAKES NO ARGS");
    return -1;
  }

  struct nandFeatures features = initNAND();
  
  //write superBlock to block 0
  superBlock.latest_vaddr_map = BLOCKSIZE;
  superBlock.cFreeListPtr = BLOCKSIZE*2;
  writeNAND((char *) &superBlock, 0, 0);

  //linking all blocks from block 2 to last block
  int i = 2;
  char *ptr;
  while (i < features.numBlocks - 1) {
    *ptr = (++i)*BLOCKSIZE;
    writeNAND(ptr, (*ptr - 1), 1);
  }  
}

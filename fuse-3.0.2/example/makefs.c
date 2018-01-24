#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "loggingDiskFormats.h"
#include "fuseLogging.h"
#include "cacheStructs.h"
#include "vNANDlib.h"

static struct superPage superBlock;
static struct fullPage page;
static struct nandFeatures features;

void clearPage() {
  memset( &page, 0, sizeof (struct fullPage));
}

void writeOutSuperBlock() {
  clearPage();

  // Add all blocks to Partially Used Free List
  superBlock.latest_vaddr_map = BLOCKSIZE;  // first vaddr map is in block 1
  superBlock.freeLists.partialHead = BLOCKSIZE*2;  // free list starts at block 2
  superBlock.freeLists.partialTail = BLOCKSIZE*(features.numBlocks - 1);

  // Write SuperBlock page out to NAND memory
  memcpy(&page.contents, &superBlock, sizeof (struct superPage));
  page.eraseCount = -1;
  page.pageType = PTYPE_SUPER;
  writeNAND( &page, 0, 0);
}

void writeOutVaddrMap() {
  clearPage();

  //initialize vaddr maps
  addrMap map = (addrMap) page.contents;
  map->size = PAGEDATASIZE/4 - 2;
  map->freePtr = 1;

  // link map entries to next available entry
  // abs value of neg entries = next free entry
  for (int i = 1; i < map->size - 1; i++) {
    map->map[i] = -(i+1);
  }
  // last map entry
  map->map[map->size - 1] = 0;     

  page.pageType = PTYPE_ADDRMAP;
  writeNAND( &page, BLOCKSIZE, 0);
}

void linkFreeBlocks() {
  //linking all blocks from block 2 to last block
  clearPage();
  page.pageType = PTYPE_NEXTLINK;
  
  for (int i = 2; i < features.numBlocks - 1; i++) {
    page.nextLogBlock = (i+1)*BLOCKSIZE;
    writeNAND( &page, (i)*BLOCKSIZE, 0);
    //writeNAND(array, (i+1)*BLOCKSIZE - 2, 1);
  }
}

int main (int argc, char *argv[])
{
  if (argc > 1) {        //Takes no arguments
    printf("ERROR: makefs TAKES NO ARGS\n");
    return -1;
  }

  features = initNAND();

  // Block 0
  writeOutSuperBlock();
  // Block 1
  writeOutVaddrMap();
  // Blocks 2 - End
  linkFreeBlocks();
  
  stopNAND();
}


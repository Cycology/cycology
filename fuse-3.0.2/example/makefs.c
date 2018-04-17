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

// Used to determine how big the virtual address map should be as a function of the
// NAND memory size.
#define ESTIMATED_FILES_PER_BLOCK 2

void clearPage() {
  memset( &page, 0, sizeof (struct fullPage));
}

void writeOutSuperBlock( int firstfree, int numBlocks) {
  clearPage();

  // Add all blocks to Partially Used Free List
  superBlock.latest_vaddr_map = BLOCKSIZE;  // first vaddr map is in block 1
  superBlock.freeLists.partialHead = BLOCKSIZE*firstfree;  // free list starts at block 2
  superBlock.freeLists.partialTail = BLOCKSIZE*(numBlocks - 1);

  // Write SuperBlock page out to NAND memory
  memcpy(&page.contents, &superBlock, sizeof (struct superPage));
  page.eraseCount = -1;
  page.pageType = PTYPE_SUPER;
  writeNAND( &page, 0, 1);
}

void writeOutVaddrMap(int size, int blocks ) {
  clearPage();

  addrMap map = (addrMap) malloc(  sizeof( addrMap ) + sizeof( page_vaddr ) * size );


  //initialize vaddr maps
  map->size = size;
  map->freePtr = 1;

  // link map entries to next available entry
  // abs value of neg entries = next free entry
  for (int i = 1; i < map->size - 1; i++) {
    map->map[i] = -(i+1);
  }
  // last map entry
  map->map[map->size - 1] = 0;     


  int pages = blocks*BLOCKSIZE;
  
  page.pageType = PTYPE_ADDRMAP;
  
  for ( int pageNum = 0; pageNum < pages; pageNum++ ) {
    memcpy( page.contents, ((char *)map) + pageNum*PAGESIZE, PAGESIZE );
    writeNAND( &page, pageNum + BLOCKSIZE, 0 );
  }

}

void linkFreeBlocks(int firstFree, int numblocks) {
  //linking all blocks from block 2 to last block
  clearPage();
  page.pageType = PTYPE_NEXTLINK;
  
  for (int i = firstFree; i < features.numBlocks - 1; i++) {
    page.nextLogBlock = (i+1)*BLOCKSIZE;
    writeNAND( &page, (i)*BLOCKSIZE, 0);
  }
}

int main (int argc, char *argv[])
{
  if (argc > 1) {        //Takes no arguments
    printf("ERROR: makefs TAKES NO ARGS\n");
    return -1;
  }

  features = initNAND();

  // Calculate size for virtual address map in such a way that it can support
  // up to MAX_FILES_PER_BLOCK files per block of virtual NAND and so that the
  // virtual address map fills an integral number of blocks.

  int minVaddrs = ( features.numBlocks - 1 )*ESTIMATED_FILES_PER_BLOCK*2;
  int vaddrsPerBlock = BLOCKSIZE*PAGESIZE/sizeof(page_vaddr);

  // The following lines add and substract 2 to account for the fields of the
  // addrMap structure that hold the map size and its free list head pointer.
  int mapBlocks =  (minVaddrs + 2 + (vaddrsPerBlock - 1) ) / vaddrsPerBlock;
  int mapSize = mapBlocks * vaddrsPerBlock - 2;

  // Block 0
  writeOutSuperBlock(mapBlocks+1, features.numBlocks);
  // Block 1
  writeOutVaddrMap(mapSize, mapBlocks);
  // Blocks 2 - End
  linkFreeBlocks(mapBlocks+1, features.numBlocks);
  
  stopNAND();
}


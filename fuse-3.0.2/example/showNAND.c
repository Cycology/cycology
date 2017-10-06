#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "vNANDlib.h"

static struct fullPage page;
static struct fullPage superBlockPage;
static struct nandFeatures features;
static superPage superBlock;
static char* blockStat;

const int UNMARKED = 0;
const int COMPLETELY_FREE = 1;
const int PARTIALLY_FREE = 2;
const int USED = 3;


int getFreeListHead(int freeListType) {
  if (freeListType == COMPLETELY_FREE) {
    return superBlock->freeLists.completeHead;
  } else {
    return superBlock->freeLists.partialHead;
  }
}

void printPage(int blockPtr, int position) {
  printf( "%d, ", blockPtr );
  if ( position % 10 == 9 ) {
    printf("\n");
  }
}

int markBlockStatistics(int freeListType, int freeBlockPtr) {
  int blockType = blockStat[ freeBlockPtr/BLOCKSIZE];

  if (blockType == UNMARKED) { // Mark as given free list
    blockStat[ freeBlockPtr/BLOCKSIZE ] = freeListType;
  } else if (blockType = freeListType) {  // Duplicate marking, error detected
    printf( "\n\n****** CYCLE DETECTED IN FREE BLOCK LIST!\n" );
    return 1;
  } else { // Block is in two free lists at once, error 
    printf( "\n\n****** BLOCK IS IN BOTH FREE LISTS!\n" );
    return 1;
  }
    
  return 0;
}

int getNextLogBlock(int freeListType, int freeBlockPtr) {
  if (freeListType == COMPLETELY_FREE) {
    readNAND( &page, freeBlockPtr + BLOCKSIZE - 1);
  } else {
    readNAND( &page, freeBlockPtr );
  }

  return page.nextLogBlock;
}

bool isValidTail(int previous, int freeListType) {
  if (previous != 0) {
    if (freeListType == COMPLETELY_FREE) {
      return previous != superBlock->freeLists.completeTail;
    } else {
      return previous != superBlock->freeLists.partialTail;
    }
  }

  return true;
}

void showFreeList(int freeListType) {
  int freeCount = 0;
  int previous = 0;
  int freeBlockPtr = 0;

  freeBlockPtr = getFreeListHead(freeListType);

  while (freeBlockPtr != 0) {
    freeCount++;
    
    printPage(freeBlockPtr, freeCount);
    
    int error = markBlockStatistics(freeListType, freeBlockPtr);
    if (error) {
      break;
    }

    previous = freeBlockPtr;
    freeBlockPtr = getNextLogBlock(freeListType, freeBlockPtr);
  }
  
  if (!isValidTail(previous, freeListType)) {
    printf( "\n\n****** FREE BLOCK LIST TAIL INVALID IN SUPERBLOCK\n" );    
  }
    
  printf( "\n\n" );
}

void printFreeLists() {    
  printf("NAND memory contains %d blocks. Total size in bytes is %d\n",
	 features.numBlocks,
	 features.memSize);
  
  // Display contents of both free lists  
  printf("Completely used block freelist starts at page %d and ends at %d\n    ",
	 superBlock->freeLists.completeHead,
	 superBlock->freeLists.completeTail);

  showFreeList(COMPLETELY_FREE);
  
  printf("Partially used block freelist starts at page %d and ends at %d\n",
	 superBlock->freeLists.partialHead,
	 superBlock->freeLists.partialTail);

  showFreeList(PARTIALLY_FREE);

  printf("\n\n");

}


addrMap getVaddrMap() {
  readNAND( &page , BLOCKSIZE );
  return (addrMap) &page.contents;
}

void printActiveMappings(addrMap map) {
  int used = 0;
  for (int i; i < map->size; i++) {
    if( map->map[i] > 0 ) {
      used++;
      printf("       id %d maps to address %d\n", i, map->map[i] );
    }
  }
}

void printFreePositions(addrMap map) {
  int freePos = map->freePtr;
  int freeCount = 0;
  if ( freePos != 0 ) {
    printf( "Free entries in address map include:\n     ");
    while ( freePos > 0 ) {
      printf( "%d, ", freePos );
      freePos = -map->map[freePos];
      freeCount++;
      if ( freeCount % 10 == 9 ) {
	printf("\n     ");
      }
    }
  } else {
    printf( "Free list in address map is empty \n" );
  }
}

void printPageInfo(fullPage page) {  
  switch (page->pageType) {
  case PTYPE_INODE:
    {
      logHeader logH = (logHeader) & page->contents;
      printf("Inode for file %d\n", logH->content.file.fInode.i_file_no );
      printf("     Stored in log number %d\n", logH->content.file.fInode.i_log_no);
      printf("     Referenced by %d links\n", logH->content.file.fInode.i_links_count );
      printf("     File mode is %o\n", logH->content.file.fInode.i_mode );
      printf("     File size is %ld with %d active pages\n",
	     logH->content.file.fInode.i_size,
	     logH->content.file.fInode.i_pages );
      break;
    }
  }
}

void printBlock(int block) {
  // Iterate through all pages in block
  for ( int p = 0; p < BLOCKSIZE; p++ ) {
    int pAddr = block*BLOCKSIZE + p;
    readNAND( &page, pAddr );

    // Don't print erased pages
    if (page.pageType == PTYPE_ERASED) {
      break;
    } else {
      printf( "    Page %d:  ", pAddr );
      printPageInfo(&page);
    }
  } 
}

void updateBlockStatAndPrintUsedPages() {
  printf("Map of active pages\n");

  // Iterate through all blocks, printing out only the used ones
  for ( int b = 2; b < features.numBlocks; b++ ) {
    if ( blockStat[ b ] == UNMARKED ) {
      blockStat[b] = USED;

      printf( "Contents of used pages in block %d:\n", b );
      printBlock(b);
    }
  }
}

void printVaddrMap() {
  // Display vaddr map
  printf("Virtual address map stored at page %d\n",
	 superBlock->latest_vaddr_map);
  
  addrMap map = getVaddrMap();
  printf("Address map size = %d\n", map->size);

  printActiveMappings(map);
  printFreePositions(map);
  updateBlockStatAndPrintUsedPages();
}


int main (int argc, char *argv[])
{
  // Check it takes no args
  if (argc > 1) {        
    printf("ERROR: makefs TAKES NO ARGS\n");
    return -1;
  }

  // Initialize global variables
  features = initNAND();
  readNAND( &superBlockPage, 0 );
  superBlock = (superPage) & superBlockPage.contents;
  blockStat = (char *) malloc( features.numBlocks );
  memset( blockStat, UNMARKED, features.numBlocks );

  // Print NAND contents
  printFreeLists();
  printVaddrMap();

  stopNAND();
}

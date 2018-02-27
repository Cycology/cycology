#include "loggingDiskFormats.h"
#include "fuseLogging.h"
#include "vNANDlib.h"
#include "logs.h"
#include <string.h>

// Place the blocks of a log back on the free lists and
// release the logs virtual address
// Assumes log is a copy of the current log header and that
// nextPage is either the address after the page holding
// this log header (if the log is inactive) or the first
// free page at the end of the log (if the log was active
// at the time of removal with data written beyond the
// position of the header).
void logs_recycle( logHeader log, page_addr nextPage, int nextErases, CYCstate state ) {

  // Must be set to point to the last block to be added to the
  // completely used free list or 0 if there is no such block.
  block_addr lastFullBlock;

  // If the last block of the log isn't full, place it in the partially used block list
  if (nextPage % BLOCKSIZE <= BLOCKSIZE - 3) {
    
    if (state->lists.partialTail == 0) { 
      // pList is empty
      state->lists.partialHead = nextPage;
		  
    } else {                        
      // Make current last block point to new last block
      struct fullPage fPage;
      memset( &fPage, 0, sizeof( struct fullPage ) );
      // I am a bit nervous here. If the last page of a log
      // was empty when it was removed, this write might be
      // to page 0 of a block in which case it would need
      // to be careful to set the erase count
      fPage.nextLogBlock = nextPage;
      writeNAND( & fPage, state->lists.partialTail, 0);
    }

    // Change tail pointer
    state->lists.partialTail = nextPage;

    // Set the last full block depending on how many blocks the log had
    if ( nextPage != log->first ) {
      lastFullBlock = log->prev;
    } else {
      lastFullBlock = 0;
    }
  	
  } else {
    // The last block is the last full block
    lastFullBlock = (nextPage / BLOCKSIZE) * BLOCKSIZE; // TODO: Dbl-check - was (nextPage % BLOCKSIZE)
  }

  /* If log has 1 or more fully used blocks, put them on the fully used free block list. */
  if ( lastFullBlock != 0 ) {
    
    if ( state->lists.completeTail != 0 ) {
      // if the completely used free block list is not empty
      struct fullPage fPage;
      memset( &fPage, 0, sizeof( struct fullPage ) );
      fPage.nextLogBlock = log->first;
      fPage.nextBlockErases = log->firstErases;

      // The flag to allow non-sequential writes may need to be
      // set here for the case when the last block of the
      // fully used free list is actually a block that is
      // not quite fully used (it had more than 1 but less
      // than 2 free pages?
      // TODO: Change so that only completely used blocks are put on the CUFL
      writeNAND( &fPage, state->lists.completeTail, 1);
      
    } else {
      state->lists.completeHead = log->first;
      state->lists.completeHeadErases = log->firstErases;
    }

    // Set the CUFL tail to point to the last full block
    state->lists.completeTail = lastFullBlock;
  }
}

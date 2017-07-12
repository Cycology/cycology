#include "fuseLogging.h"

#define BLOCKSIZE 16

/****************************************************************
 *
 * Contains the contents and write/erase info of a single block.
 *
 ****************************************************************/
typedef struct block{
  struct fullPage contents[BLOCKSIZE];   //actual space to store data
  struct blockData {
    int nextPage;                        //next available empty page after last erasure
    int eraseCount;                      //times this particular block has been erased
  } *blockData;
} *block;

//POST: open big NAND file; save its nandFeatures field
struct nandFeatures initNAND(void);

/*PRE:
  char *buf - buffer to store data read
  page_addr k - absolute page address to read
  POST:
  returns number of bytes read; -1 if error
 */
int readNAND(char *buf, page_vaddr k);

/*PRE:
  char *buf - buffer of data to be written
  page_addr k - absolute page address to write
  POST:
  returns number of bytes written; -1 if error
 */
int writeNAND(char *buf, page_vaddr k, int random_access);

/*PRE:
  block_addr b - absolute block address to erase
  POST:
  returns eraseCount; erases block
 */
int eraseNAND(block_vaddr b);

//POST: close NAND file; free nandFeatures field
void stopNAND(void);

//POST: return block
int readNANDBlock(char *buf, page_vaddr b);

#include "fuseLogging.h"

#define BLOCKSIZE 16

//Define root path
#define ROOT_PATH ((char *)"/home/quan/Documents/cycology/fuse-3.0.2/example/rootdir/root")
#define STORE_PATH ((char *)"/home/quan/Documents/cycology/fuse-3.0.2/example/rootdir/virtualNAND")

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
int readBlockData(char *buf, page_vaddr b);

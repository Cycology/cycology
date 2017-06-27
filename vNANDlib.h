#define PAGESIZE 1024
#define BLOCKSIZE 16

/****************************************************************
 *
 * Keeps track of variables important for page/block calculations
 * carried out by NAND library functions.
 *
 ****************************************************************/
typedef struct nandFeatures{
  int numBlocks;               //Number of blocks in NAND
  int memSize;                 //Total size of NAND memory
} *nandFeatures;

/****************************************************************
 *
 * Contains the contents and write/erase info of a single block.
 *
 ****************************************************************/
typedef struct block{
  char contents[PAGESIZE*BLOCKSIZE];   //actual space to store data
  int nextPage;                        //next available empty page after last erasure
  int eraseCount;                      //times this particular block has been erased
} *block;

//POST: open big NAND file; save its nandFeatures field
struct nandFeatures initNAND(void);

/*PRE:
  char *buf - buffer to store data read
  page_addr k - absolute page address to read
  POST:
  returns number of bytes read; -1 if error
 */
int readNAND(char *buf, page_addr k);

/*PRE:
  char *buf - buffer of data to be written
  page_addr k - absolute page address to write
  POST:
  returns number of bytes written; -1 if error
 */
int writeNAND(char *buf, page_addr k, int random_access);

/*PRE:
  block_addr b - absolute block address to erase
  POST:
  returns eraseCount; erases block
 */
int eraseNAND(block_addr b);

//POST: close NAND file; free nandFeatures field
void stopNAND(void);

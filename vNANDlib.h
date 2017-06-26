#define PAGESIZE 1024
#define BLOCKSIZE 16

typedef struct nandFeatures{
  int numBlocks;
  int memSize;
} *nandFeatures;

typedef struct block{
  char contents[PAGESIZE*BLOCKSIZE];
  page_addr nextPage;               //what is nextPage if page is full?
  int eraseCount;
} *block;

//extern nandFeatures features;

void initNAND(void);
int readNAND(char *buf, page_addr k);
int writeNAND(char *buf, page_addr k);
int eraseNAND(block_addr b);
void stopNAND(void);

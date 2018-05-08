#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "loggingDiskFormats.h"
#include "fuseLogging.h"
#include "vNANDlib.h"

static int fd;
static struct nandFeatures features;

pageKey write_keys[20][100];
int write_maxSibs[20];
int write_maxLevels = 0;

pageKey read_keys[20][100];
int read_maxSibs[20];
int read_maxLevels = 0;

void NAND_trackKeys(pageKey temp_key, int write) {
  pageKey key = malloc(sizeof(struct pageKey));
  key->siblingNum = temp_key->siblingNum;
  key->levelsAbove = temp_key->levelsAbove;

  if (write == 1) {
    pageKey tKey = write_keys[key->levelsAbove][key->siblingNum];
    if (key->levelsAbove > write_maxLevels)
      write_maxLevels = key->levelsAbove;
    if (key->siblingNum > write_maxSibs[key->levelsAbove])
      write_maxSibs[key->levelsAbove] = key->siblingNum;
  } else {
    pageKey tKey = read_keys[key->levelsAbove][key->siblingNum];
    if (key->levelsAbove > read_maxLevels)
      read_maxLevels = key->levelsAbove;
    if (key->siblingNum > read_maxSibs[key->levelsAbove])
      read_maxSibs[key->levelsAbove] = key->siblingNum;
  }
}

void NAND_printKeys(int write) {
  if (write == 1) {
    printf("\n*****Printing Cache Record*****\n");
    for (int k = 0; k < write_maxLevels; k++) {
      printf("\n***********************************\n");
      for (int i = 0; i < write_maxSibs[k]; i++) {
	if (write_keys[k][i] != NULL) {
	  pageKey key = write_keys[k][i];
	  printf("(Level: %d, Sib: %d, Addr: %d)\n", key->levelsAbove, key->siblingNum, key->phys_addr);
	}
      }
    }

  } else {
    
  }
}

void NAND_trackAddress(int addr, pageKey key) {
  pageKey saved_key = write_keys[key->levelsAbove][key->siblingNum];
  if (saved_key == NULL) {
    pageKey new_key = malloc(sizeof(struct pageKey));
    new_key->siblingNum = key->siblingNum;
    new_key->levelsAbove = key->levelsAbove;
    new_key->phys_addr = addr;
    write_keys[key->levelsAbove][key->siblingNum] = new_key;

  } else {
    saved_key->phys_addr = addr;
  }

  if (key->levelsAbove > write_maxLevels)
    write_maxLevels = key->levelsAbove;
  if (key->siblingNum > write_maxSibs[key->levelsAbove])
    write_maxSibs[key->levelsAbove] = key->siblingNum;
}


//open the big NAND file and save features
struct nandFeatures initNAND(void)
{
  fd = open(STORE_PATH, O_RDWR);
  if (fd == -1) {
    perror("ERROR IN OPENING BIG NAND FILE");
    return features;     //Hmm what to return?
  }
  
  int res = read(fd, &features, (sizeof (struct nandFeatures)));
  if (res == -1)
    perror("ERROR IN READING BIG NAND FILE FOR NAND FEATURES");
  return features;
}

//lseek to position of page and read entire page
int readNAND( fullPage buf, page_addr k)
{
  int offset = sizeof (struct nandFeatures)
		+ (k/BLOCKSIZE)*(sizeof (struct block))
		+ (k%BLOCKSIZE)*(sizeof (struct fullPage));
  if (offset >= features.memSize) {
    printf("PAGE OFFSET OUTSIDE OF BIG FILE");
    return -1;
  }
  lseek(fd, offset, SEEK_SET);

  int res = read(fd, buf, sizeof (struct fullPage));
  if (res != (sizeof (struct fullPage)))
    perror("FAIL TO READ PAGE IN NAND");

  return res;
}

int writeNAND( fullPage buf, page_addr k, int random_access)
{
  //Read in the block containing target page
  struct block curBlock;
  int offset = (sizeof (struct nandFeatures)
		+ (k/BLOCKSIZE)*(sizeof (struct block)));
  if (offset >= features.memSize) {
    printf("BLOCK OFFSET OUTSIDE OF BIG FILE");
    return -1;
  }
  lseek(fd, offset, SEEK_SET);
  int res = read(fd, &curBlock, (sizeof (struct block)));
  if (res == -1) {
    printf("CANNOT READ BLOCK IN writeNAND");
    return -1;
  } 
  
  //index of page k in the block
  int kInBlock = k%BLOCKSIZE;
  if (random_access == 0 && kInBlock != curBlock.data.nextPage) {
    printf("DOES NOT WRITE TO NEXT FREE PAGE OF BLOCK IN NAND");
    // TODO: PUT BACK return -1;
  }
  
  //write from buffer to block; increase nextPage counter
  memcpy(&(curBlock.contents[kInBlock]), buf, sizeof (struct fullPage));
  if (random_access == 0)
    curBlock.data.nextPage++;
  lseek(fd, (sizeof (struct nandFeatures)
	     + (k/BLOCKSIZE)*(sizeof (struct block))), SEEK_SET);
  res = write(fd, &curBlock, (sizeof (struct block))); 
  if (res == -1)
    perror("FAIL TO WRITE PAGE IN NAND");

  return res;
}

//close the big file
void stopNAND(void)
{
  close(fd);
}

int eraseNAND(page_addr k)
{
  int offset = sizeof (struct nandFeatures)
    + (k+1)*(sizeof (struct block) - sizeof (int));
  if (offset >= features.memSize) {
    printf("OFFSET OF eraseCount OUTSIDE OF BIG FILE");
    return -1;
  }
  lseek(fd, offset, SEEK_SET);

  //lseek and read in the block's eraseCount
  struct block curBlock;
  int eraseCount;
  int res = read(fd, &eraseCount, sizeof(int));
  if (res == -1) {
    printf("CANNOT READ BLOCK'S eraseCount IN eraseNAND");
    return -1;
  }

  //increment eraseCount; create and paste the empty block into offset
  eraseCount++;
  memset(&curBlock, 0, (sizeof (struct fullPage))*BLOCKSIZE);
  //curBlock.contents[0].eraseCount = eraseCount;
  curBlock.data.nextPage = 0;
  curBlock.data.eraseCount = eraseCount;

  lseek(fd, sizeof (struct nandFeatures)
	+ k*(sizeof (struct block)), SEEK_SET);
  res = write(fd, &curBlock, (sizeof (struct block)));
  if (res == -1) {
    printf("CANNOT WRITE UPDATED BLOCK IN eraseNAND");
    return -1;
  }
  
  return eraseCount;
}

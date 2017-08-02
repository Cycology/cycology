#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "vNANDlib.h"

static int fd;
static struct nandFeatures features;

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
int readNAND(char *buf, page_addr k)
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

int writeNAND(char *buf, page_addr k, int random_access)
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
    return -1;
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

int eraseNAND(block_addr b)
{
  int offset = sizeof (struct nandFeatures)
    + (b+1)*(sizeof (struct block)) - sizeof(int);
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
	+ b*(sizeof (struct block)), SEEK_SET);
  res = write(fd, &curBlock, (sizeof (struct block)));
  if (res == -1) {
    printf("CANNOT WRITE UPDATED BLOCK IN eraseNAND");
    return -1;
  }
  
  return eraseCount;
}

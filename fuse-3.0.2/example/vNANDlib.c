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
  
  read(fd, &features, (sizeof (struct nandFeatures)));
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
  read(fd, &curBlock, (sizeof (struct block)));
  
  //index of page k in the block
  int kInBlock = k%BLOCKSIZE;
  if (random_access == 0 && kInBlock != curBlock.data.nextPage) {
    printf("DOES NOT WRITE TO NEXT FREE PAGE OF BLOCK IN NAND");
    return -1;
  }
  
  //write from buffer to block; increase nextPage counter
  memcpy((curBlock.contents[kInBlock]), buf, sizeof (struct fullPage));
  if (random_access == 0)
    curBlock.data.nextPage++;
  lseek(fd, (sizeof (struct nandFeatures)
	     + (k/BLOCKSIZE)*(sizeof (struct block))), SEEK_SET);
  int res = write(fd, &curBlock, (sizeof (struct block))); 
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
  int offset = (sizeof (struct nandFeatures)
		+ b*(sizeof (struct block)));
  if (offset >= features.memSize) {
    printf("BLOCK OFFSET OUTSIDE OF BIG FILE");
    return -1;
  }
  lseek(fd, offset, SEEK_SET);

  //lseek and read in the block's eraseCount
  struct block curBlock;
  read(fd, (&curBlock + (sizeof (struct block)) - sizeof(int)), sizeof(int));

  //increment eraseCount; paste the empty block into offset
  int eraseCount = ++(curBlock.data.eraseCount);
  lseek(fd, offset, SEEK_SET);
  write(fd, &curBlock, (sizeof (struct block)));

  return eraseCount;
}

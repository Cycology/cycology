#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fuseLogging.h"
#include "vNANDlib.h"

static int fd;
static nandFeatures features;

int main (int argc, char *argv[])
{
  printf("SIZE OF NANDFEATURES: %lu\n", (sizeof (struct nandFeatures)));
  printf("SIZE OF BLOCK: %lu\n", (sizeof (struct block)));
  initNAND();
  
  char array[PAGESIZE];
  strcpy(array, "hello");
  writeNAND(array, 48);
  
  char *buffer;
  buffer = malloc(PAGESIZE);
  readNAND(buffer, 48);
  printf("%s\n", buffer);

  int erase = eraseNAND(3);
  printf("Erase Count = %d\n", erase);
  erase = eraseNAND(3);
  printf("Erase Count = %d\n", erase);

  memset(buffer, 0, PAGESIZE);
  readNAND(buffer, 48);
  printf("%s", buffer);
  free(buffer);
  
  stopNAND();
}

//open the big NAND file and save features
void initNAND(void)
{
  fd = open("NANDexample", O_RDWR);
  if (fd == -1) {
    perror("ERROR IN OPENING BIG NAND FILE");
    return;
  }

  features = (nandFeatures) malloc(sizeof (struct nandFeatures));
  read(fd, features, (sizeof (struct nandFeatures)));
}

//lseek to position of page and read entire page
int readNAND(char *buf, page_addr k)
{
  int offset = lseek(fd, (sizeof (struct nandFeatures)
			  + (k/BLOCKSIZE)*(sizeof (struct block))
			  + (k%BLOCKSIZE)*PAGESIZE), SEEK_SET);
  if (offset >= features->memSize) {
    printf("PAGE OFFSET OUTSIDE OF BIG FILE");
    return -1;
  }
  
  int res = read(fd, buf, PAGESIZE);
  if (res == -1)
    perror("FAIL TO READ PAGE IN NAND");

  return res;
}


int writeNAND(char *buf, page_addr k)
{
  //Read in the block containing target page
  struct block curBlock;
  int offset = lseek(fd, (sizeof (struct nandFeatures)
			  + (k/BLOCKSIZE)*(sizeof (struct block))), SEEK_SET);
  if (offset >= features->memSize) {
    printf("BLOCK OFFSET OUTSIDE OF BIG FILE");
    return -1;
  }
  read(fd, &curBlock, (sizeof (struct block)));
  
  //position of page k in the block
  int kInBlock = k%BLOCKSIZE;  
  if (kInBlock != curBlock.nextPage) {
    printf("DOES NOT WRITE TO NEXT FREE PAGE OF BLOCK IN NAND");
    return -1;
  }
  
  //write from buffer to block; increase nextPage counter
  memcpy((curBlock.contents + PAGESIZE*kInBlock), buf, PAGESIZE);
  curBlock.nextPage++;
  lseek(fd, (sizeof (struct nandFeatures)
	     + (k/BLOCKSIZE)*(sizeof (struct block))), SEEK_SET);
  int res = write(fd, &curBlock, (sizeof (struct block))); 
  if (res == -1)
    perror("FAIL TO WRITE PAGE IN NAND");

  return res;
}

int eraseNAND(block_addr b)
{
  int offset = lseek(fd, (sizeof (struct nandFeatures)
			  + b*(sizeof (struct block))), SEEK_SET);
  if (offset >= features->memSize) {
    printf("OFFSET OUTSIDE OF BIG FILE");
    return -1;
  }

  //lseek and read in the block
  struct block curBlock;
  //curBlock = (block) malloc(sizeof (struct block));
  read(fd, &curBlock, (sizeof (struct block)));

  //clear block, set nextPage to 0, increment eraseCount
  memset(curBlock.contents, 0, PAGESIZE*BLOCKSIZE); //find a way not to have to read block in first
  curBlock.nextPage = 0;
  int eraseCount = ++(curBlock.eraseCount);

  //paste the empty block into offset
  lseek(fd, (sizeof (struct nandFeatures)
	     + b*(sizeof (struct block))), SEEK_SET);
  write(fd, &curBlock, (sizeof (struct block)));

  //free(curBlock);
  return eraseCount;
}

//close the big file
void stopNAND(void)
{
  close(fd);
  free(features);
}


#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <string.h>

#include "vNANDlib.h"

static struct nandFeatures features;

int main (int argc, char *argv[])
{
  if (argc < 2) {        //Takes 1 argument
    printf("ERROR: NEED 1 ARG TO SPECIFY number of blocks");
    return -1;
  }

  //hold #blocks and overall size of big file
  features.numBlocks = atoi(*++argv);
  features.memSize = (sizeof (struct nandFeatures)) + (sizeof (struct block))*(features.numBlocks);

  //create the big NAND file
  int fd;
  char *filename = "/home/quan/Documents/cycology/fuse-3.0.2/example/rootdir/virtualNAND";
  fd = open(filename, O_CREAT | O_RDWR, S_IRWXU);
  if (fd == -1) {
    perror("ERROR IN CREATING BIG FILE");
    return NULL;
  }

  //write the features in the big file
  int res = write(fd, &features, sizeof(struct nandFeatures));
  if (res == -1)
    perror("ERROR IN WRITING METADATA TO BIG FILE");

  //writing 0 at end of file to specify size
  lseek(fd, (sizeof(struct nandFeatures)
	     + ((sizeof (struct block))*(features.numBlocks))), SEEK_SET);
  res = write(fd, "\0", 1);

  close(fd);
  return 0;
}

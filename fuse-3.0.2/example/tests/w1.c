#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>

/*
 * Tests xmp_write
 */

int main(int argc, char *argv[])
{
  if (argc != 2) {
    perror("INVALID ARGS");
    return 0;
  }

  char* name;
  name = "../mntdir/t1";
  int fd = open(name, O_CREAT | O_RDWR, S_IRWXU);
  if (fd == -1) {
    perror("ERROR IN CREATING FILE");
    return 0;
  }

  int numPages = atoi(argv[1]);
  int writeSize = 1024 * numPages;
  int page[128];
  int bytesWritten = 0;
  int curOffset = 0;
  for (int i = 0; i < numPages; i++) {
    for (int j = 0; j < 128; j++)
      page[j] = i;
    bytesWritten += write(fd, page, 1024);
    //printf("%d\n", bytesWritten);

    // Skip every other page 
    curOffset += 1024 * 2;
    lseek(fd, curOffset, SEEK_SET);
    printf("curOffset: %d\n", curOffset);
  }


  printf("BYTES WRITTEN = %d\n", bytesWritten);
  close( fd );
  
  return 0;
}

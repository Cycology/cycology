#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>

/*
 * Tests xmp_read
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
  int readSize = 1024 * numPages;
  int intReadSize = 128 * numPages + 1;
  int buf[intReadSize];
  int bytesRead = 0;
  int curOffset = 0;
  int offset = 0;
  for (int i = 0; i < numPages; i++) {
    bytesRead += read(fd, buf+offset, 1024);
    offset += 128;
    printf("offset: %d\n", offset);

    // Skip every other page 
    curOffset += (1024 * 2);
    lseek(fd, curOffset, SEEK_SET);
    //printf("curOffset: %d\n", curOffset);
    printf("i: %d\n", i);
  }


  printf("BYTES READ = %d\n", bytesRead);
  for (int i = 0; i < intReadSize; i += 128) {
    printf("%d* ", buf[i]);
    if (i % 128 == 0) {
      printf("\n");
    }
  }
  close( fd );
  
  return 0;
}

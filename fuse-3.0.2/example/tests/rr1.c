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
  int intReadSize = 128 * numPages;
  int buf[intReadSize];
  int bytesRead = 0;
  int curOffset = 2048 * (numPages - 1);
  int offset = 0;
  for (int i = 0; i < numPages - 1; i++) {
    bytesRead += read(fd, buf+offset, 1024);
    printf("i: %d, \toffset: %d, \tbytesRead: %d, \tcurOffset: %d\n", i, offset, bytesRead, curOffset);
    offset += 128;

    // Skip every other page 
    curOffset -= 2048;
    lseek(fd, curOffset, SEEK_SET);
  }


  printf("BYTES READ = %d\n", bytesRead);
  for (int i = 0; i < intReadSize; i += 128) {
    printf("%d\n", buf[i]);
  }
  close( fd );
  
  return 0;
}

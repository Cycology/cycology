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
  name = "../mntdir/test123";
  int fd = open(name, O_CREAT | O_RDWR, S_IRWXU);
  if (fd == -1) {
    perror("ERROR IN CREATING FILE");
    return 0;
  }

  int writeSize = atoi(argv[1]);
  writeSize++;

  char buf[writeSize];
  int offset = 0;
  int i = 0;
  while (offset < writeSize) {
    char intbuf[8];
    sprintf(intbuf, "%7d", i);
    intbuf[7] = '\n';

    memcpy(buf + offset, intbuf, 8);
    offset += 8;
    i++;
  }
  buf[writeSize-1] = '\0';

  int bytesWritten = write(fd, buf, writeSize);
  printf("BYTES WRITTEN = %d\n", bytesWritten);
  close( fd );
  
  return 0;
}

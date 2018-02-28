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

  char* name;
  name = "mntdir/randomIO";
  int fd = open(name, O_CREAT | O_RDWR, S_IRWXU);
  if (fd == -1) {
    perror("ERROR IN CREATING FILE");
    return 0;
  }

  // Write at beginning
  lseek(fd, 0, SEEK_SET);

  char* buf;
  buf = "abcdefghijklmnop";
  int bytesWritten = write(fd, buf, 15);

  // Write to random jump
  lseek(fd, 30000, SEEK_SET);

  buf = "abcdefghijklmnop";
  bytesWritten += write(fd, buf, 15);
  printf("BYTES WRITTEN = %d\n", bytesWritten);

  close( fd );
  
  return 0;
}

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

  char* name;
  name = "mntdir/randomIO";
  int fd = open(name, O_RDWR);
  if (fd == -1) {
    perror("ERROR IN READING FILE");
    return 0;
  }

  int bytesRead = 0;
  char buf[200];

  // Read at beginning
  lseek(fd, 0, SEEK_SET);
  bytesRead = read(fd, buf, 15);

  // Read at middle
  lseek(fd, 30000, SEEK_SET);
  bytesRead += read(fd, buf+bytesRead, 15);
  if (bytesRead > 0) {
    buf[bytesRead] = '\0';
    printf("READ: %s, %d\n", buf, bytesRead);
  }
  
  close( fd );
  
  return 0;
}

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
  name = "mntdir/test123";
  int fd = open(name, O_RDWR);
  if (fd == -1) {
    perror("ERROR IN READING FILE");
    return 0;
  }

  int bytesRead = 0;
  char buf[17001];
  lseek(fd, 0, SEEK_SET);
  bytesRead = read(fd, buf, 170001);

  if (bytesRead > 0) {
    buf[bytesRead] = '\0';
    printf("READ: %s, %d\n", buf, bytesRead);
  }
    
  close( fd );
  
  return 0;
}

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
  name = "mntdir/test123";
  int fd = open(name, O_CREAT | O_RDWR, S_IRWXU);
  if (fd == -1) {
    perror("ERROR IN CREATING FILE");
    return 0;
  }
    
  char buf[17001];
  int offset = 0;
  int i = 0;
  while (offset < 17000) {
    char intbuf[8];
    memset(intbuf, '_', 7);
    sprintf(intbuf, "%7d", i);
    intbuf[7] = '\n';

    memcpy(buf + offset, intbuf, 8);
    offset += 8;
    i++;
  }
  buf[17000] = '\0';

  int bytesWritten = write(fd, buf, 17001);
  printf("BYTES WRITTEN = %d\n", bytesWritten);
  close( fd );
  
  return 0;
}

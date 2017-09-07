#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>

/*
 * Tests xmp_create
 */

int main(int argc, char *argv[])
{
  if (argc > 1) {              //No argument
    printf("ERROR: NO ARGUMENT");
    return 1;
  }

  char *filename = "/home/parallels/Documents/fuse-3.0.2/example/mntdir/createdFile";
  int fd = open(filename, O_CREAT | O_RDWR, S_IRWXU);
  if (fd == -1) {
    perror("ERROR IN CREATING FILE");
    return 0;
  }

  close(fd);

  return 0;
}

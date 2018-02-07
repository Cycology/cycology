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

  if (argc == 1) {       
    printf("ERROR: PROVIDE FILE NAMES AS ARGUMENTS");
    return 1;
  }

  char name[200];
  for ( int i = 1; i < argc; i++ ) {
    sprintf( name, "mntdir/%s", argv[i] );
    int fd = open(name, O_RDWR);
    if (fd == -1) {
      perror("ERROR IN READING FILE");
      return 0;
    }

    char* buf;
    lseek(fd, 0, SEEK_SET);
    read(fd, buf, 10);
    printf("PRINTING WHAT IS WRITTEN: %s\n", buf);
    //close( fd );
  }
  
  return 0;
}

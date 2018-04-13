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

  if (argc == 1) {       
    printf("ERROR: PROVIDE FILE NAMES AS ARGUMENTS");
    return 1;
  }

  char name[200];
  for ( int i = 1; i < argc; i++ ) {
    sprintf( name, "mntdir/%s", argv[i] );
    int fd = open(name, O_CREAT | O_RDWR, S_IRWXU);
    if (fd == -1) {
      perror("ERROR IN CREATING FILE");
      return 0;
    } else {
      close( fd );
    }
  }
  
  return 0;
}

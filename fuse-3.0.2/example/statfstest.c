#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/statvfs.h>

/*
 *Retrieving “struct statvfs” for "test1" gives:
 f_bsize: 4096     (equivalent to our PAGESIZE, different from our block)
 f_blocks: 3442816     (how many pages are there)
 f_bfree: 1671474       (how many free pages are there)
 f_bavail: 1490828     (pages available to unprivileged users)
 f_files: 883008   total file nodes (do we have file nodes?)
 f_ffree: 524207   free file nodes
 F_fsid: 5998731228084866749
 f_namemax: 255    max length of file names
 f_frsize: 4096    fragment size (also not sure how this corresponds w our file system)
 f_flag: 4096     mount flags of filesystem (what is this?)
 */

int main(int argc, char *argv[])
{
  if (argc == 1) {              //No argument
    printf("ERROR: NO ARGUMENT");
    return 1;
  }
  
  struct statvfs stat;

  int res = statvfs(*++argv, &stat);
  if (res == -1) {
    perror("CAN'T GET STATFS");
    return 1;
  }

  printf("f_bsize: %lu\n f_frsize: %lu\n f_blocks: %lu\n f_bfree: %lu\n f_bavail: %lu\n f_files: %lu\n f_ffree: %lu\n f_favail: %lu\n f_fsid: %lu\n f_flag: %lu\n f_namemax: %lu\n",
	 stat.f_bsize, stat.f_frsize, stat.f_blocks, stat.f_bfree,
	 stat.f_bavail, stat.f_files, stat.f_ffree, stat.f_favail,
	 stat.f_fsid, stat.f_flag, stat.f_namemax);
    
  //close(fd);
    
 return 0;
}

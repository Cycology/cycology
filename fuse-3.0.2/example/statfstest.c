#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/vfs.h>

/*
 *Running this on the file "test1" gives:
 f_type: ef53      hexadecimal id of fs type
 f_bsize: 4096     equivalent to our PAGESIZE (different from our block)
 f_blocks: 3442816 how many pages are there 
 f_bfree: 1774666  how many free pages are there
 f_bavail: 1594020 pages available to unprivileged users
 f_files: 883008   total file nodes (do we have file nodes?)
 f_ffree: 558447   free file nodes
 f_fsid            we skipped this because it' struct <anonymous>
 f_namelen: 255    max length of file names
 f_frsize: 4096    fragment size
 f_flags: 4128     mount flags of filesystem
 */

int main(int argc, char *argv[])
{
  if (argc == 1) {              //No argument
    printf("ERROR: NO ARGUMENT");
    return 1;
  }
  
  struct statfs stat;

  int res = statfs(*++argv, &stat);
  if (res == -1) {
    perror("CAN'T GET STATFS");
    return 1;
  }

  printf("f_type: %lx\n f_bsize: %lu\n f_blocks: %lu\n f_bfree: %lu\n f_bavail: %lu\n f_files: %lu\n f_ffree: %lu\n f_namelen: %ld\n f_frsize: %ld\n f_flags: %ld\n", stat.f_type, stat.f_bsize, stat.f_blocks, stat.f_bfree,
	 stat.f_bavail, stat.f_files, stat.f_ffree,
	 stat.f_namelen, stat.f_frsize, stat.f_flags);
    
  //close(fd);
    
 return 0;
}

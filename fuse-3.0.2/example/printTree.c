// Joseph Oh

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>

#include "loggingDiskFormats.h"
#include "fuseLogging.h"
#include "vNANDlib.h"

int main (int argc, char *argv[]) {
  
}

int printFile(struct inode ind) {
  int addrs[1000];
  int curIndex = 0;
  int lastIndex = 0;
  
  // Print the inode contents
  printf("\n1:\t[");
  for (int i = 0; i < DIRECT_PAGES; i++) {
    printf("%d,\t", ind.directPage[i]);
    addrs[lastIndex++] = ind.directPage[i];
  }
  printf("]\n");
  printf("****************************************\n");

  // Print the indirect pages' contents
  for (int level = ind.treeHeight - 2; l > 0; l--) {
    // Get each page at this level
    for (int sibNum = 0; sibNum < INDIRECT_PAGES; sibNum++) {
      // Print the page's contents
      struct fullPage page;
      int pAddr = addrs[curIndex++];
      readNAND(&page, pAddr);
      int[] contents = (int[]) page.contents;

      printf("%d:\t[", sibNum);
      for (int i = 0; i < INDIRECT_PAGES; i++) {
	printf("%d,\t", contents[i]);
	addrs[lastIndex++] = contents[i];
      }
      printf("]\n");
    }

    printf("****************************************\n");
  }
}

#include <unistd.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "fuseLogging.h"
#include "vNANDlib.h"

int main (int argc, char *argv[])
{
  printf("SIZE OF NANDFEATURES: %lu\n", (sizeof (struct nandFeatures)));
  printf("SIZE OF BLOCK: %lu\n", (sizeof (struct block)));
    printf("SIZE OF PAGE: %lu\n", (sizeof (struct fullPage)));
  initNAND();
  
  char array[PAGESIZE];
  strcpy(array, "hello");
  writeNAND(array, 48, 0);
  
  memset(array, 0, PAGESIZE);
  readNAND(array, 48);
  printf("%s\n", array);

  int erase = eraseNAND(3);
  printf("Erase Count = %d\n", erase);
  erase = eraseNAND(3);
  printf("Erase Count = %d\n", erase);

  memset(array, 0, PAGESIZE);
  readNAND(array, 48);
  printf("%s", array);
  
  stopNAND();
}

//Test File

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>



int main(int argc, char **argv)
{
	char *fileName = 0;
	FILE *f = stdin;
	int fd = 0;

	if(argc > 1){
		f = fopen((fileName = argv[1]), "r");
		fd = open((fileName = argv[1]), O_RDWR);
		printf("%s\n", fileName);
	}



//	fpos_t *ptr = (fpos_t*)malloc(sizeof(fpos_t));
//	fgetpos(f, ptr);
//	fsetpos(f, ptr);
	char * buf = (char*)malloc(3009);
	int res = read(fd, buf, 3008);
	printf("%d %s\n", res, buf);
}

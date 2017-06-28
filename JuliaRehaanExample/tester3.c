//Test File

#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv)
{
	char *fileName = 0;
	FILE *f = stdin;

	if(argc > 1){
		f = fopen((fileName = argv[1]), "w+");

	printf("%s\n", fileName);
	}



//	fpos_t *ptr = (fpos_t*)malloc(sizeof(fpos_t));
//	fgetpos(f, ptr);
//	fsetpos(f, ptr);
	char* mine = (char*)malloc(20*sizeof(char));
	fgets(mine, 20, f);
	fseek(f, ftell(f)-19 , SEEK_SET);
	fgets(mine,

}

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
		f = fopen((fileName = argv[1]), "w+");
		fd = open((fileName = argv[1]), O_RDWR);
		printf("%s\n", fileName);
	}



//	fpos_t *ptr = (fpos_t*)malloc(sizeof(fpos_t));
//	fgetpos(f, ptr);
//	fsetpos(f, ptr);
	char * buf = "first sectionhhgjgfhdjsgejsgejsgehejdgejdhsgehd 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,.// 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,. 1234567890qwertyuiopasdfghjkl;'zxcvbnm,./1234567890---QWERTYUIOP[]ASDFGHJKL;'ZXCVBNM,.\n";
	write(fd, buf, 3008);
	//int size = ftell(f);
	lseek(fd, 1008, SEEK_SET);
	write(fd, "*********************Does this work?************AAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAAA******\n", 1064);

}

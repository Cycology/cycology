all: vNANDlibTest makeVirtualNAND

vNANDlibTest: vNANDlibTest.o vNANDlib.o
	gcc -g -o vNANDlibTest vNANDlibTest.o vNANDlib.o

makeVirtualNAND: makeVirtualNAND.c vNANDlib.h
	gcc -g -o makeVirtualNAND makeVirtualNAND.c

vNANDlibTest.o: vNANDlibTest.c vNANDlib.h
	gcc -g -c vNANDlibTest.c

vNANDlib.o: vNANDlib.c vNANDlib.h
	gcc -g -c vNANDlib.c

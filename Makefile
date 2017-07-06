all: vNANDlibTest makeVirtualNAND makefs passthroughBlocked_fh

vNANDlibTest: vNANDlibTest.o vNANDlib.o
	gcc -g -o vNANDlibTest vNANDlibTest.o vNANDlib.o

makefs: makefs.o vNANDlib.o
	gcc -g -o makefs makefs.o vNANDlib.o

makeVirtualNAND: makeVirtualNAND.c vNANDlib.h
	gcc -g -o makeVirtualNAND makeVirtualNAND.c

passthroughBlocked_fh: passthroughBlocked_fh.c 
	gcc -g -o passthroughBlocked_fh passthroughBlocked_fh.c

vNANDlibTest.o: vNANDlibTest.c vNANDlib.h
	gcc -g -c vNANDlibTest.c

makefs.o: makefs.c vNANDlib.h
	gcc -g -c makefs.c

vNANDlib.o: vNANDlib.c vNANDlib.h
	gcc -g -c vNANDlib.c





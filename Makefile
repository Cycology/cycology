all: vNANDlib makeVirtualNAND

vNANDlib: vNANDlib.c
	gcc -g -o vNANDlib vNANDlib.c

makeVirtualNAND: makeVirtualNAND.c
	gcc -g -o makeVirtualNAND makeVirtualNAND.c


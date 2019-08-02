#Makefile
FC=ftn
CC=cc
mpiio:
	$(CC) -O3 -o mpiio.x mpiio.cpp
clean:
	rm -rf *.x flush testFile

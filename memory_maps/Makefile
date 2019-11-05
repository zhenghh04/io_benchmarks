#Makefile
#!/bin/sh
CFLAGS=-O3

CC=g++
HPCTW=$(HOME)/opt/hpctw
MPI_LIBS=-Wl,-rpath,$(HOME)/opt/openmpi-4.0.0-clang/lib -L$(HOME)/opt/openmpi-4.0.0-clang/lib -lmpi
HPCTW_LIBS=-Wl,-rpath,$(HPCTW) -L$(HPCTW) -lhpmprof_c -lbfd -liberty -lz -lintl

#CC=mpicc
#HPCTW=$(HOME)/soft/hpctw/intel
#HPCTW_LIBS=-Wl,-rpath,$(HPCTW) -L$(HPCTW) -lhpmprof_c -lbfd -liberty -lz 

LIBS=$(HPCTW_LIBS) $(MPI_LIBS)

all: memory_maps
memory_maps:
	$(CC) $(CFLAGS) -o memory_maps.x memory_maps.cpp $(LIBS)

clean:
	rm -rf *.o *~ *.x a.out data* scratch/* 

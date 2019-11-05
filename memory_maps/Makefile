#Makefile
#!/bin/sh
CFLAGS=-O3
#CC=mpicc
CC=g++
HPCTW=$(HOME)/opt/hpctw
HPCTW_LIB=-Wl,-rpath,$(HPCTW) -L$(HPCTW) -lhpmprof_c -lbfd -liberty -lz -lintl -L$(HOME)/opt/openmpi-4.0.0-clang/lib -lmpi
#CC=CC
#HPCTW=$(HOME)/soft/hpctw/cray

all: memory_maps
memory_maps:
	$(CC) $(CFLAGS) -o memory_maps.x memory_maps.cpp $(HPCTW_LIB)

clean:
	rm -rf *.o *~ *.x a.out data* scratch/* 

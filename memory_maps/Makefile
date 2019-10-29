#Makefile
#!/bin/sh
CFLAGS=-O3
#CC=mpicc
CC=g++
HPCTW=$(HOME)/opt/hpctw
HPCTW_LIB=-L$(HOME)/opt/openmpi-4.0.0-clang/lib -lmpi
#HPCTW_LIB=-L$(HPCTW) -lhpmprof_c -lbfd -liberty -lz
#CC=CC
#HPCTW=$(HOME)/soft/hpctw/cray

all: memory_maps memory_maps_async
memory_maps:
	$(CC) $(CFLAGS) -o memory_maps.x memory_maps.cpp $(HPCTW_LIB)
memory_maps_async:
	$(CC) $(CFLAGS) -o memory_maps_async.x memory_maps_async.cpp $(HPCTW_LIB)
clean:
	rm -rf *.o *~ *.x a.out data* scratch/* 

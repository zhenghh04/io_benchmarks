#Makefile
#!/bin/sh

CC=mpicc
CFLAGS=-O3
#HPCTW_LIB=-L$(HOME)/opt/openmpi-4.0.0-clang/lib -lmpi
ifeq ($(HOSTNAME), zion)
	CC=g++
	HPCTW=$(HOME)/opt/hpctw
else
	HPCTW=$(HOME)/soft/hpctw/cray
endif
HPCTW_LIB=-L$(HPCTW) -lhpmprof_c -lbfd -liberty -lz -lintl
CC=CC


all: memory_maps memory_maps_async
memory_maps:
	$(CC) $(CFLAGS) -o memory_maps.x memory_maps.cpp $(HPCTW_LIB)
memory_maps_async:
	$(CC) $(CFLAGS) -o memory_maps_async.x memory_maps_async.cpp $(HPCTW_LIB)
clean:
	rm -rf *.o *~ *.x a.out data* scratch/* 

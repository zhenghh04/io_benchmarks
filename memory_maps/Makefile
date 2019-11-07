#Makefile
#!/bin/sh
HOST=$(shell hostname)
CFLAGS=-O3 -g
ifeq ($(HOST), zion)
	CC=g++
	HPCTW=$(HOME)/opt/hpctw
	MPI_LIBS=-Wl,-rpath,$(HOME)/opt/openmpi-4.0.0-clang/lib -L$(HOME)/opt/openmpi-4.0.0-clang/lib -lmpi
	HPCTW_LIBS=-Wl,-rpath,$(HPCTW) -L$(HPCTW) -lhpmprof_c -lbfd -liberty -lz -lintl
else
	CC=mpicc
	HPCTW=$(HOME)/soft/hpctw/intel
	HPCTW_LIBS=-Wl,-rpath,$(HPCTW) -L$(HPCTW) -lhpmprof_c -lbfd -liberty -lz
endif

LIBS=$(HPCTW_LIBS) $(MPI_LIBS)

all: memory_maps test_ssd_posix

memory_maps:
	$(CC) $(CFLAGS) -o memory_maps.x memory_maps.cpp $(LIBS)

test_ssd_posix:
	$(CC) $(CFLAGS) -o test_ssd_posix.x test_ssd_posix.cpp $(LIBS)

clean:
	rm -rf *.o *~ *.x a.out data* scratch/* 

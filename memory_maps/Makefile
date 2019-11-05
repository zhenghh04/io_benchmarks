#Makefile
#!/bin/sh
CFLAGS=-O3
ifeq ($(HOSTNAME), zion)
	CC=g++
	HPCTW=$(HOME)/opt/hpctw
	MPI_LIBS=-Wl,-rpaht,$(HOME)/opt/openmpi-4.0.0-clang/lib -L$(HOME)/opt/openmpi-4.0.0-clang/lib -lmpi
	HPCTW_LIBS=-Wl,-rpath,$(HPCTW) -L$(HPCTW) -lhpmprof_c -lbfd -liberty -lz -lintl
else
	CC=mpicc
	HPCTW=$(HOME)/soft/hpctw/intel
	HPCTW_LIBS=-Wl,-rpath,$(HPCTW) -L$(HPCTW) -lhpmprof_c -lbfd -liberty -lz 
endif
LIBS=$(HPCTW_LIBS) $(MPI_LIBS)

all: memory_maps
memory_maps:
	$(CC) $(CFLAGS) -o memory_maps.x memory_maps.cpp $(HPCTW_LIBS)

clean:
	rm -rf *.o *~ *.x a.out data* scratch/* 

#Makefile
#!/bin/sh
HOST=$(shell hostname)
HPCTW=1
CFLAGS=-O3 -g -DSSD_CACHE_DEBUG
ifeq ($(HOST), zion)
	CC=g++
	HPCTW=$(HOME)/opt/hpctw
	MPI_LIBS=-Wl,-rpath,$(HOME)/opt/openmpi-4.0.0-clang/lib -L$(HOME)/opt/openmpi-4.0.0-clang/lib -lmpi
	HPCTW_LIBS=-Wl,-rpath,$(HPCTW) -L$(HPCTW) -lhpmprof_c -lbfd -liberty -lz -lintl
else
	CC=mpicxx
	HPCTW=$(HOME)/soft/hpctw/intel
	CFLAGS += -DTHETA
	HPCTW_LIBS=-Wl,-rpath,$(HPCTW) -L$(HPCTW) -lhpmprof_c -lbfd -liberty -lz
endif
# turn off HPCTW profiling
ifeq ($(HPCTW), 0)
	HPCTW_LIBS=
endif

#LIBS=$(HPCTW_LIBS) $(MPI_LIBS)
LIBS=$(MPI_LIBS)
#all: memory_maps test_ssd_posix

test_ssd_posix_io:
	$(CC) $(CFLAGS) -o test_ssd_cache_io.x test_ssd_cache_io.cpp ssd_cache_io_pthread.cpp $(LIBS)

memory_maps:
	$(CC) $(CFLAGS) -o memory_maps.x memory_maps.cpp ssd_cache_io_pthread.cpp $(LIBS)

test_ssd_posix:
	$(CC) $(CFLAGS) -o test_ssd_posix.x test_ssd_posix.cpp $(LIBS)

clean:
	rm -rf *.o *~ *.x a.out data* scratch/* mpi_profile.* hpm*

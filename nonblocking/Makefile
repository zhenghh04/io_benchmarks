#Makefile
#!/bin/sh
#CC=mpicxx 
CC=CC
#CC=mpicxx
#CC=icc -I/home/hzheng/opt/mpich-3.3.1-ugni/include
#HPCTW=/home/hzheng/opt/mpich-3.3.1-ugni/lib/libmpi.a
CFLAGS=-O3
#HPCTW=-L/soft/perftools/hpctw/lib -lmpitrace
#HPCTW=-L$(HOME)/mylibs -lmpitrace

all: prefetch prepare_datasets

prefetch:
	$(CC) $(CFLAGS) -o test_prefetch.x test_prefetch.cpp $(HPCTW)

prepare_datasets:
	$(CC) $(CFLAGS) -o prepare_datasets.x prepare_datasets.cpp $(HPCTW)

clean:
	rm -rf *.x mpi_profile.*

remove_datasets:
	rm -rf datasets/*



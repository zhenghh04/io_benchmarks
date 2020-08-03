#Makefile
#!/bin/sh
#CC=mpicxx 
#CC=pgc++
CC=mpicxx

CFLAGS=-O3

all: write_datasets read_datasets
write_datasets:
	$(CC) $(CFLAGS) -o write_datasets.x write_datasets.cpp $(HPCTW)

read_datasets:
	$(CC) $(CFLAGS) -o read_datasets.x read_datasets.cpp $(HPCTW)

clean:
	rm -rf *.x mpi_profile.*

remove_datasets:
	rm -rf datasets/*



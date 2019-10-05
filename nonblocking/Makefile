#Makefile
#!/bin/sh
CC=mpicxx 
CFLAGS=-O3
HPCTW=-L/soft/perftools/hpctw/lib -lmpitrace
all: prefetch datasets

prefetch:
	$(CC) $(CFLAGS) -o test_prefetch.x test_prefetch.cpp $(HPCTW)

prepare_datasets:
	$(CC) $(CFLAGS) -o prepare_datasets.x prepare_datasets.cpp $(HPCTW)
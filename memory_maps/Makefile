#Makefile
#!/bin/sh
CC=CC
CFLAGS=-O3 
all: memory_maps memory_maps_async
memory_maps:
	$(CC) $(CFLAGS) -o memory_maps.x memory_maps.cpp -L$(HOME)/soft/hpctw/cray -lhpmprof_c -lbfd -liberty -lz
memory_maps_async:
	$(CC) $(CFLAGS) -o memory_maps_async.x memory_maps_async.cpp -L$(HOME)/soft/hpctw/cray -lhpmprof_c -lbfd -liberty -lz
clean:
	rm -rf *.o *~ *.x a.out data* scratch/* 

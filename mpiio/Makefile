#Makefile
FC=ftn
CC=CC
mpiio_rw:
	$(CC) -O3 -o mpiio_rw.x mpiio_rw.cpp
mpiio_no_cache:
	$(CC) -O3 -o mpi_read.x mpi_read.cpp
	$(CC) -O3 -o mpi_write.x mpi_write.cpp

mpiio:
	$(CC) -O3 -o mpiio.x mpiio.cpp
clean:
	rm -rf *.x flush testFile

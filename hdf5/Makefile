#Makefile
#!/bin/sh
CC=mpicc
CXX=mpicxx
CFLAGS=-O3 -DDEBUG
HPCTW=$(HOME)/opt/hpctw
LIBS=-Wl,-rpath,$(HDF5_ROOT)/lib -L$(HDF5_ROOT)/lib -lhdf5  -Wl,-rpath,$(HOME)/opt/hpctw -L$(HOME)/opt/hpctw -lhpmprof_c -liberty -lbfd -lz
#LIBS=-Wl,-rpath,$(HDF5_ROOT)/lib -L$(HDF5_ROOT)/lib -lhdf5 -L/home/morozov/lib -lhpmprof_c -lbfd -liberty -lz

parallel_hdf5:
	$(CXX) $(CFLAGS) -o parallel_hdf5.x parallel_hdf5.cpp $(LIBS)

dset:
	$(CXX) $(CFLAGS) -o create_dset.x create_dset.cpp $(LIBS)
	./create_dset.x
	h5dump ./file.h5 
clean:
	rm -rf *.x *.h5 *.hdf5

#Makefile
#!/bin/sh
CC=mpicc
CXX=mpicxx #-Wl,-rpath,/home/hzheng/soft/libfabric/cray-intel-2018/lib/
CFLAGS=-O3 -DDEBUG
HPCTW=$(HOME)/soft/hpctw/intel
LIBS=-Wl,-rpath,$(HDF5_ROOT)/lib -L$(HDF5_ROOT)/lib -lhdf5 -Wl,-rpath,$(HPCTW) -L$(HPCTW) -lhpmprof_c -liberty -lbfd -lz


parallel_hdf5:
	$(CXX) $(CFLAGS) -o parallel_hdf5.x parallel_hdf5.cpp $(LIBS)

dset:
	$(CXX) $(CFLAGS) -o create_dset.x create_dset.cpp $(LIBS)
	./create_dset.x
	h5dump ./file.h5 
clean:
	rm -rf *.x *.h5 *.hdf5

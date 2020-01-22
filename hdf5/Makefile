#Makefile
#!/bin/sh
CC=cc
CXX=mpicxx -dynamic  #-Wl,-rpath,/home/hzheng/soft/libfabric/cray-intel-2018/lib/
CFLAGS=-O3 -DSSD_CACHE_DEBUG -DDEBUG -I$(HDF5_ROOT)/include
#HPCTW=$(HOME)/opt/hpctw
#HPCTW=/home/morozov/lib/
#HPCTW_LIBS=-Wl,-rpath,$(HPCTW) -L$(HPCTW) -lhpmprof_c -liberty -lbfd -lz -lintl
LIBS=-Wl,-rpath,$(HDF5_ROOT)/lib -L$(HDF5_ROOT)/lib -lhdf5  $(HPCTW_LIBS) -pthread
#CXX=mpiicc #-Wl,-rpath,/home/hzheng/soft/libfabric/cray-intel-2018/lib/
#CFLAGS=-O3 -DDEBUG -I$(HDF5_ROOT)/include
#HPCTW=$(HOME)/soft/hpctw/intel
#HDF5_ROOT=/home/hzheng/Aurora_SDL/hdf5_quarterly_benchmarks/install/hdf5-ccio/
#HPCTW=-Wl,-rpath,$(HOME)/mylibs -L$(HOME)/mylibs -lmpitrace
#LIBS=-Wl,-rpath,$(HDF5_ROOT)/lib -L$(HDF5_ROOT)/lib -lhdf5 $(HPCTW) #-Wl,-rpath,$(HPCTW) -L$(HPCTW) -lhpmprof_c -liberty -lbfd -lz
#


parallel_hdf5:
	$(CXX) $(CFLAGS) -o parallel_hdf5.x parallel_hdf5.cpp H5SSD.c $(LIBS)

dset:
	$(CXX) $(CFLAGS) -o create_dset.x create_dset.cpp $(LIBS)
	./create_dset.x
	h5dump ./file.h5 
clean:
	rm -rf *.x *.h5 *.hdf5

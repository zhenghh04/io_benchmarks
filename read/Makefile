#Makefile
CXX=mpicxx
CFLAGS=-I$(HDF5_ROOT)/include -O3 -I../utils
HDF5_LIB=-L$(HDF5_ROOT)/lib -lhdf5
HPCTW=-L$(HOME)/soft/hpctw/gnu -lhpmprof -liberty -lbfd -lz
%.o: %.cpp
	$(CXX) $(CFLAGS) $(CPPFLAGS) -o $@ -c $< 


all: read_dataset prepare_dataset
read_dataset: read_dataset.o
	$(CXX) $(CFLAGS) -o $@ $< $(HDF5_LIB)  $(HPCTW)

prepare_dataset: prepare_dataset.o
	$(CXX) $(CFLAGS) -o $@ $< $(HDF5_LIB)

clean:
	rm -rf *.o read_dataset prepare_dataset mpi_profile.*

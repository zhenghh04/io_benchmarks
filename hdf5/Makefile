#Makefile
#!/bin/sh
include make.inc

all: thread_hdf5 parallel_hdf5 test_dset_id

thread_hdf5:
	$(CXX) $(CFLAGS) -o thread_hdf5.x thread_hdf5.cpp H5SSD.c $(LIBS)

test_dset_id:
	$(CXX) $(CFLAGS) -o test_dset_id.x test_dset_id.c dset_id.c $(LIBS)

parallel_hdf5:
	$(CXX) $(CFLAGS) -o parallel_hdf5.x parallel_hdf5.cpp H5SSD.c $(LIBS)

dset:
	$(CXX) $(CFLAGS) -o create_dset.x create_dset.cpp $(LIBS)
	./create_dset.x
	h5dump ./file.h5 
clean:
	rm -rf *.x *.h5 *.hdf5

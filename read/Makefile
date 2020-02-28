#Makefile
CXX=mpicxx
CFLAGS=-I$(HDF5_ROOT)/include -O3 -I../Utils
HDF5_LIB=-L$(HDF5_ROOT)/lib -lhdf5
%.o: %.cpp
	$(CXX) $(CFLAGS) $(CPPFLAGS) -o $@ -c $< 
read_dataset: read_dataset.o
	$(CXX) $(CFLAGS) -o $@ $< $(HDF5_LIB)

clean:
	rm -rf *.o read_data

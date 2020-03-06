#Makefile
#!bin/sh
CXX=mpicxx
CFLAGS=-O3 -I$(HDF5_ROOT)/include -I../utils -g
cp: 
	$(CXX) $(CFLAGS) -o hdf5_mem hdf5_mem.cpp -L/Users/zhenghh/opt/hdf5/ccio//lib -lhdf5

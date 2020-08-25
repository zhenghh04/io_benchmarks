# HPC-IO Benchmarks 
This is a set of I/O benchmark functions on HPC systems including: 
* IOR 
* VPIC -- for HDF5 write tests
* Exerciser -- for HDF5 read & write & meta data tests
* cache -- for node local storage cache tests

* Nonblocking I/O test 
    nonblocking/prepare_datasets.cpp -- prepare the datasets
    nonblocking/test_prefetch.cpp -- tesing the prefetch

* Memory map files performance test
    memory_maps.cpp -- testing writing data to memory map files. 

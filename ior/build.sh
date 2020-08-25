#!/bin/bash
module load hdf5-ccio
cd HPC-IOR
./configure --with-hdf5=yes --with-gpfs=no --with-lustre=yes --prefix=/home/hzheng/soft/hdf5/ccio-abi

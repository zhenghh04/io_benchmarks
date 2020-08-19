#!/bin/sh
# loading the module that has ccio

module load hdf5-ccio

# build the vpic benchmark program

make -f Makefile.col

make -f Makefile.ind

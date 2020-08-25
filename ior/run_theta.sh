#!/bin/bash
#COBALT -A datascience -q default --attrs mcdram=cache:numa=quad -n 256 -t 4:00:00
# This is to mimic the vpic test / with 8m particles per rank

module load intelpython36

buf=32m
for i in 2 4 8 16 32 64 128 256
do
    python ./run_theta.py --numNodes $i --transferSize $buf --directory MPIIO/n$i.b$buf.col --collective &
    python ./run_theta.py --numNodes $i --transferSize $buf --directory MPIIO/n$i.b$buf.ind &
done

for i in 2 4 8 16 32 64 128 256
do
    python ./run_theta.py --numNodes $i --transferSize $buf --directory HDF5/n$i.b$buf.col --api HDF5 --collective &
#    python ./run.py --numNodes $i --transferSize $buf --directory HDF5/n$i.b$buf.ccio --api HDF5 --collective --cb_nodes 48 --cb_size 64m &
    python ./run_theta.py --numNodes $i --transferSize $buf --directory HDF5/n$i.b$buf.ind --api HDF5 &
done

wait

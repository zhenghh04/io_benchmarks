#!/bin/bash
#COBALT -A datascience -n 4 -t 1:00:00
#Example to run the jobs
#
run=mpirun
#$run -np 48 -ppn 12 ./prepare_datasets.x --num_batches 480 --batch_size 512
echo "==========================="
$run -np 48 -ppn 12 ./test_prefetch.x --num_batches 480 --batch_size 512 --compute_time 0.5 --nonblocking 0 
echo "==========================="
$run -np 48 -ppn 12 ./test_prefetch.x --num_batches 480 --batch_size 512 --compute_time 0.5 --nonblocking 1 

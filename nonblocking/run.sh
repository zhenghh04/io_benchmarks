#!/bin/bash
#Example to run the jobs
run=mpirun
$run -np 4 prepare_datasets.x --num_batches 16 --batch_size 32
$run -np 4 test_prefetch.x --num_batches 16 --batch_size 32 --compute_time 4

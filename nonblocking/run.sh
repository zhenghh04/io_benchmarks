#!/bin/bash
#COBALT -A datascience -n 4 -t 1:00:00
#Example to run the jobs
#
run=jsrun
I_MPI_ASYNC_PROGRESS_PIN=0 I_MPI_ASYNC_PROGRESS=1 $run -n 8 --smpiarg="--async" ./test_prefetch.x --num_batches 16 --batch_size 64 --compute_time 0.01 --nonblocking 1

I_MPI_ASYNC_PROGRESS_PIN=0 I_MPI_ASYNC_PROGRESS=1 $run -n 8 --smpiarg="--async" ./test_prefetch.x --num_batches 16 --batch_size 64 --compute_time 0.01 --nonblocking 0


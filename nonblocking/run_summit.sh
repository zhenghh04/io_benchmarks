#!/bin/bash
#BSUB -nnodes 1 
#BSUB -W 10
#Example to run the jobs
#
run=jsrun
[ -e datasets ] || mkdir datasets
echo "****Writing datasets"
$run -n 8 --smpiarg="--async" ./write_datasets.x --num_batches 16 --batch_size 64 --nonblocking 1
echo "==================="
echo ""
echo "****Reading datasets with async calls"
$run -n 8 --smpiarg="--async" ./read_datasets.x --num_batches 16 --batch_size 64 --compute_time 0.01 --nonblocking 1
echo "==================="
echo ""
echo "****Reading datasets with blocking calls"
$run -n 8 --smpiarg="--async" ./read_datasets.x --num_batches 16 --batch_size 64 --compute_time 0.01 --nonblocking 0


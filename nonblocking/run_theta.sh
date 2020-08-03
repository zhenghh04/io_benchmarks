#!/bin/bash
#COBALT -n 1 
#COBALT -t 10
#Example to run the jobs
#
run=aprun
[ -e datasets ] || mkdir datasets
echo "****Writing datasets"
$run -n 8 ./write_datasets.x --num_batches 16 --batch_size 64 --nonblocking 1
echo "==================="
echo ""
echo "****Reading datasets with async calls"
$run -n 8 ./read_datasets.x --num_batches 16 --batch_size 64 --compute_time 0.01 --nonblocking 1
echo "==================="
echo ""
echo "****Reading datasets with blocking calls\n"
$run -n 8 ./read_datasets.x --num_batches 16 --batch_size 64 --compute_time 0.01 --nonblocking 0


# Nonblocking I/O test
Author: Huihuo Zheng <huihuo.zheng@anl.gov>

This is to test whether a system support nonblocking I/O calls such as MPI_File_iread, MPI_File_iwrite

We first write the datasets using write_datasets.x, and then read them using read_datasets.x. 

* write_datasets.x  -- writing binary datasets 
   - --num_batches: number of independent files [default: 1024]
   - --batch_sizes: size of each files (batch_sizes * 224 * 224 * 3 * sizeof(int)) [default: 32]
   - --nonblocking: whether to use nonblocking write or not [default: 0]

* read_datasets.x -- reading bindary datasets
   - --num_batches: number of independent files to read [default: 1024]
   - --batch_sizes: size of each files (batch_sizes * 224 * 224 * 3 * sizeof(int)) [default: 32]
   - --nonblocking: whether to use nonblocking read or not [default: 0]
   - --compute_time: simulated compute time (second) using sleep [default: 0]

* Submission scripts
   - run_theta.sh -- submission script on Theta @ ALCF
   - run_summit.sh -- submission script on Summit @ OLCF


If the nonblocking I/O works, we will see the write/read time is small. Most of the I/O time shows up in wait. 

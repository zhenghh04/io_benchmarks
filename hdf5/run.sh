#module load hdf5-ccio-intel-2019
#lfs setstripe 
export FOR_IGNORE_EXCEPTIONS=true
export HDF5_CCIO_WR='yes'
export HDF5_CCIO_CB_SIZE=1048576
mpirun -np 64  ./parallel_hdf5.x --dim 128 4096

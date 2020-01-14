#module load hdf5-ccio-intel-2019
#lfs setstripe 
export FOR_IGNORE_EXCEPTIONS=true
export HDF5_CCIO_WR='no'
export HDF5_CCIO_DEBUG='yes'
export HDF5_CCIO_CB_SIZE=1048576
mpirun -np 2  ./parallel_hdf5.x --dim 4096 4096

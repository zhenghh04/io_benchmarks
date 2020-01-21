#module load hdf5-ccio-intel-2019
#lfs setstripe 
export FOR_IGNORE_EXCEPTIONS=true
export HDF5_CCIO_DEBUG='yes'
export HDF5_CCIO_WR='yes'
export HDF5_CCIO_CB_SIZE=1048576
export HDF5_CCIO_CB_NODES=8
mpirun -np 8  ./parallel_hdf5.x --dim 16 1024576

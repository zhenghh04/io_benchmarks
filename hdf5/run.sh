export FOR_IGNORE_EXCEPTIONS=true
export HDF5_CCIO_WR='yes'
export HDF5_CCIO_CB_SIZE=1048576
aprun -n 128 -j 2 ./parallel_hdf5.x --dim 128 $1

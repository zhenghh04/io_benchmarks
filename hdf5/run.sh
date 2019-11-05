mpirun -np 4 ./parallel_hdf5.x --dim 2048 1024 --niter 50
export FOR_IGNORE_EXCEPTIONS=true
export HDF5_CCIO_WR='yes'

mpirun -np 4 ./parallel_hdf5.x --dim 2048 1024 --niter 50

n#module load hdf5-ccio-intel-2019
#lfs setstripe 
export FOR_IGNORE_EXCEPTIONS=true
export HDF5_CCIO_DEBUG='yes'
#export HDF5_CCIO_WR='yes'
export HDF5_CCIO_CB_SIZE=1048576
export HDF5_CCIO_DEBUG='yes'
#mpirun -n 4 ./parallel_hdf5.x --dim 1024 2048 
#aprun -n 128 -j 2 ./parallel_hdf5.x --dim 128 $1

export SSD_CACHE_PATH=$PWD/SSD/; mpirun -np 1 ./parallel_hdf5.x --scratch $PWD/scratch --niter 8

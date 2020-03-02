/*
  This file is for testing reading the data set in parallel in data paralle training. 
  We assume that the dataset is in a single HDF5 file. Each dataset is stored in the 
  following way: 

  (nsample, d1, d2 ..., dn)

  Each sample are an n-dimensional array 
  
  When we read the data, each rank will read a batch of sample randomly or contiguously
  from the HDF5 file. Each sample has a unique id associate with it. At the begining of 
  epoch, we mannually partition the entire dataset with nproc pieces - where nproc is 
  the number of workers. 

  Huihuo Zheng @ ALCF
  Revision history: 

  Feb 29, 2020: Added debug info support.
  Feb 28, 2020: Created with simple information. 

 */
#include <iostream>
#include "hdf5.h"
#include "mpi.h"
#include "stdlib.h"
#include "string.h"
#include "stdio.h"
#include <random>
#include <algorithm>
#include <vector>
#include "timing.h"
#include <assert.h>
#include "debug.h"
#include <iostream>
#include <unistd.h>
// POSIX I/O
#include <sys/stat.h> 
#include <fcntl.h>
using namespace std;
#define MAXDIM 1024

void dim_dist(hsize_t gdim, int nproc, int rank, hsize_t *ldim, hsize_t *start) {
  *ldim = gdim/nproc;
  *start = *ldim*rank; 
  if (rank < gdim%nproc) {
    *ldim += 1;
    *start += rank;
  } else {
    *start += gdim%nproc;
  }
}

int main(int argc, char **argv) {
  int rank, nproc; 
  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  char fname[255] = "images.h5";
  char dataset[255] = "dataset";
  bool shuffle = false;
  bool mpio_collective = false; 
  bool mpio_independent = false; 
  int epochs = 1;
  int num_batches = 16;
  int batch_size = 32; 
  int rank_shift = 0;
  int i=0;
  Timing tt(rank==io_node()); 
  while (i<argc) {
    if (strcmp(argv[i], "--input")==0) {
      strcpy(fname, argv[i+1]); i+=2; 
    } else if (strcmp(argv[i], "--dataset")==0) {
      strcpy(dataset, argv[i+1]); i+=2; 
    } else if (strcmp(argv[i], "--num_batches")==0) {
      num_batches = int(atof(argv[i+1])); i+=2;
    } else if (strcmp(argv[i], "--batch_size")==0) {
      batch_size = int(atof(argv[i+1])); i+=2; 
    } else if (strcmp(argv[i], "--shuffle")==0) {
      shuffle = true; i=i+1; 
    } else if (strcmp(argv[i], "--mpio_independent")==0) {
      mpio_independent = true; i=i+1; 
    } else if (strcmp(argv[i], "--mpio_collective")==0) {
      mpio_collective = true; i=i+1; 
    } else if (strcmp(argv[i], "--epochs") == 0) {
      epochs = int(atof(argv[i+1])); i+=2; 
    } else if (strcmp(argv[i], "--rank_shift")==0) {
      rank_shift = int(atof(argv[i+1])); i+=2; 
    } else {
      i=i+1; 
    }
  }
  hid_t plist_id = H5Pcreate(H5P_FILE_ACCESS);
  if (mpio_collective or mpio_independent) {
    H5Pset_fapl_mpio(plist_id, MPI_COMM_WORLD, MPI_INFO_NULL);
  }
  hid_t fd = H5Fopen(fname, H5F_ACC_RDONLY, plist_id);
  hid_t dset = H5Dopen(fd, dataset, H5P_DEFAULT);
  hid_t fspace = H5Dget_space(dset);
  hsize_t gdims[MAXDIM];
  int ndims = H5Sget_simple_extent_dims(fspace, gdims, NULL);
  if (rank==io_node()) {
    cout << "\n====== dataset info ======" << endl; 
    cout << "Dataset file: " << fname << endl;
    cout << "Dataset: " << dataset << endl; 
    cout << "Number of samples in the dataset: " << gdims[0] << endl; 
    cout << "Dimension of the sample: " << ndims - 1 << endl;
    cout << "Size in each dimension: "; 
    for (int i=1; i<ndims; i++) {
      cout << " " << gdims[i];
    }
    cout << endl;
    cout << "\n====== I/O & MPI info ======" << endl; 
    cout << "MPIO_COLLECTIVE: " << mpio_collective << endl; 
    cout << "MPIO_INDEPENDENT: " << mpio_independent << endl; 
    cout << "rank shift: " << rank_shift << endl; 
    cout << "\n====== training info ======" << endl; 
    cout << "Batch size: " << batch_size << endl; 
    cout << "Number of batches per epoch: " << num_batches << endl;
    cout << "Number of epochs: " << epochs << endl;
    cout << "Shuffling the samples: " << shuffle << endl;
    cout << "Number of workers: " << nproc << endl; 
  }
  hsize_t dim = 1; // compute the size of a single sample
  hsize_t *ldims = new hsize_t [ndims]; // for one batch of data
  hsize_t *offset = new hsize_t [ndims]; // the offset
  hsize_t *count = new hsize_t [ndims]; // number of samples to read 
  hsize_t *sample = new hsize_t [ndims]; // for one sample
  for(int i=0; i<ndims; i++) {
    dim = dim*gdims[i];
    ldims[i] = gdims[i];
    offset[i] = 0;
    count[i] = 1;
    sample[i] = gdims[i];
  }
  sample[0]=1;
  dim = dim/gdims[0];

  vector<int> id;
  id.resize(gdims[0]);
  for(int i=0; i<gdims[0]; i++) id[i] = i;
  mt19937 g(100);

  hsize_t ns_loc; // number of sample per worker
  hsize_t fs_loc;  // first sample 
  dim_dist(gdims[0], nproc, rank, &ns_loc, &fs_loc);

  float *dat = new float[dim*batch_size];// buffer to store one batch of data
  ldims[0] = batch_size; 
  hid_t mspace = H5Screate_simple(ndims, ldims, NULL); // memory space for one bach of data
  hid_t dxf_id = H5Pcreate(H5P_DATASET_XFER);
  if (mpio_collective) {
    H5Pset_dxpl_mpio(dxf_id, H5FD_MPIO_COLLECTIVE);
  } else if (mpio_independent) {
    H5Pset_dxpl_mpio(dxf_id, H5FD_MPIO_INDEPENDENT); 
  } 
  for (int ep = 0; ep < epochs; ep++) {
    if (rank_shift > 0)
      dim_dist(gdims[0], nproc, (rank+ep*rank_shift)%nproc, &ns_loc, &fs_loc);
    if ((io_node()==rank) and (debug_level()>0)) cout << "Clear cache" << endl; 
    // shuffle the indices
    if (shuffle==1) {
      ::shuffle(id.begin(), id.end(), g);
      if (rank==io_node() and debug_level()>1) {
	cout << "Shuffled index" << endl;
	cout << "* "; 
	for (int i=0; i<gdims[0]; i++) {
	  cout << " " << id[i] << "("<< i<< ")";
	  if (i%8==7) cout << "\n* ";
	}
	cout << endl; 
      }
    }


    for(int nb = 0; nb < num_batches; nb++) {
      // select the file space according to the indices
      tt.start_clock("Select");
      offset[0] = id[fs_loc+(nb*batch_size)%ns_loc]; // set the offset
      H5Sselect_hyperslab(fspace, H5S_SELECT_SET, offset, NULL, sample, count);
      for(int i=1; i<batch_size; i++) {
	offset[0] = id[fs_loc+(i+nb*batch_size)%ns_loc]; 
	H5Sselect_hyperslab(fspace, H5S_SELECT_OR, offset, NULL, sample, count);
      }
      tt.stop_clock("Select");
      // reading the dataset
      tt.start_clock("H5Dread"); 
      H5Dread(dset, H5T_NATIVE_FLOAT, mspace, fspace, dxf_id, dat);
      tt.stop_clock("H5Dread");
      // sanity check whether this is what we want. 
      if (rank==io_node() and debug_level()>2) {
	cout << "=== batch: "<< nb << " \n* " << endl;
	vector<int> b = vector<int> (id.begin() + fs_loc+(nb*batch_size)%ns_loc, id.begin() + fs_loc+((nb+1)*batch_size)%ns_loc);
	sort(b.begin(), b.end()); 
	for(int i=0; i<batch_size; i++) {
	  cout << dat[i*dim] << "-" << b[i] << " ";
	  if (i%8==7) {
	    cout << "\n* " << endl; 
	  }
	}
      }
    }

  }

  H5Pclose(plist_id);
  H5Sclose(mspace);
  H5Sclose(fspace);
  H5Dclose(dset);
  H5Fclose(fd);
  double w = num_batches*epochs*batch_size/tt["H5Dread"].t*nproc;

  if (rank==io_node()) {
    cout << "\n===== I/O rate =====" << endl;
    cout << "# of images/sec: " << w << endl;
    cout << "Read rate: " << w*sizeof(float)*dim/1024/1024 << " MB/s" << endl; 
  }

  delete [] dat;
  delete [] ldims;
  delete [] offset;
  delete [] count;
  delete [] sample; 
  MPI_Finalize();
  return 0;
}

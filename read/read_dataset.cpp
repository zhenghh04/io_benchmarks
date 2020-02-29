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
  int shuffle = 0;
  int epochs = 1;
  int num_batches = 16;
  int batch_size = 32; 
  int i=0;
  Timing tt(rank==0); 
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
      shuffle = int(atof(argv[i+1])); i+=2; 
    } else if (strcmp(argv[i], "--epochs") == 0) {
      epochs = int(atof(argv[i+1])); i+=2; 
    } else {
      i=i+1; 
    }
  }
  hid_t plist_id = H5Pcreate(H5P_FILE_ACCESS);
  hid_t fd = H5Fopen(fname, H5F_ACC_RDONLY, plist_id);
  hid_t dset = H5Dopen(fd, dataset, H5P_DEFAULT);
  hid_t fspace = H5Dget_space(dset);
  hsize_t gdims[MAXDIM];
  int ndims = H5Sget_simple_extent_dims(fspace, gdims, NULL);
  if (rank==0) {
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
    cout << "\n====== training info ======" << endl; 
    cout << "Batch size: " << batch_size << endl; 
    cout << "Number of batches per epoch: " << num_batches << endl;
    cout << "Number of epochs: " << epochs << endl;
    cout << "Shuffling the samples: " << shuffle << endl; 
  }
  hsize_t dim = 1;
  hsize_t *ldims = new hsize_t [ndims];
  hsize_t *offset = new hsize_t [ndims];
  hsize_t *count = new hsize_t [ndims];
  hsize_t *sample = new hsize_t [ndims];
  for(int i=0; i<ndims; i++) {
    dim = dim*gdims[i];
    ldims[i] = gdims[i];
    offset[i] = 0;
    count[i] = 1;
    sample[i] = gdims[i];
  }
  sample[0]=1;

  vector<int> id;
  id.resize(gdims[0]);
  for(int i=0; i<gdims[0]; i++) id[i] = i;
  mt19937 g(100);


  hsize_t ns_loc; // number of sample per worker
  hsize_t fs_loc;  // first sample 
  dim_dist(gdims[0], nproc, rank, &ns_loc, &fs_loc);

  
  float *dat = new float[dim/gdims[0]*batch_size];
  ldims[0] = batch_size; 
  hid_t mspace = H5Screate_simple(ndims, ldims, NULL);

  for (int ep = 0; ep < epochs; ep++) {
    // shuffle the indices
    if (shuffle==1) 
      ::shuffle(id.begin(), id.end(), g);

    for(int nb = 0; nb < num_batches; nb++) {
      // select the file space according to the indices
      tt.start_clock("Select");
      offset[0] = id[fs_loc+nb*batch_size]; 
      H5Sselect_hyperslab(fspace, H5S_SELECT_SET, offset, NULL, sample, count);
      for(int i=1; i<batch_size; i++) {
	offset[0] = id[fs_loc+i+nb*batch_size]; 
	H5Sselect_hyperslab(fspace, H5S_SELECT_OR, offset, NULL, sample, count);
      }
      tt.stop_clock("Select");
      // reading the dataset
      tt.start_clock("H5Dread"); 
      H5Dread(dset, H5T_NATIVE_FLOAT, mspace, fspace, H5P_DEFAULT, dat);
      tt.stop_clock("H5Dread"); 
      if (rank==0)
	for(int i=0; i<batch_size; i++) {
	  cout << dat[i*dim/gdims[0]] << " " << id[fs_loc+i+nb*batch_size] << endl; 
	}
    }
  }
  H5Pclose(plist_id);
  H5Sclose(mspace);
  H5Sclose(fspace);
  H5Dclose(dset);
  H5Fclose(fd);
  double w = num_batches*epochs*batch_size/tt["H5Dread"].t*nproc;

  if (rank==0) {
    cout << "\n===== I/O rate =====" << endl;
    cout << "# of images/sec: " << w << endl;
    cout << "Read rate: " << w*sizeof(float)*dim/gdims[0]/1024/1024 << " MB/s" << endl; 
  }
  delete [] dat;
  delete [] ldims;
  delete [] offset;
  delete [] count;
  delete [] sample; 
  MPI_Finalize();
  return 0;
}

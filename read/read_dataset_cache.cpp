/*
  This benchmark is for testing reading the data set in parallel in data parallel training. 
  We assume that the entire dataset is in a single HDF5 file. Each dataset is stored in the 
  following way: 

  (nsample, d1, d2 ..., dn), where each sample is an n-dimensional array. 
  

  When we read the data, each rank will read a batch of sample randomly or contiguously
  from the HDF5 file depends on whether we do shuffling or not. 
  Each sample has a unique id associate with it. 

  At the begining of epoch, we mannually partition the entire dataset with nproc pieces - where nproc is 
  the number of workers. 

  Huihuo Zheng @ ALCF
  Revision history: 

  Mar 1, 2020: added MPIIO support
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
// Memory map
#include <sys/mman.h>
using namespace std;
#define MAXDIM 1024
struct mmap_file {
  void *buf;
  size_t size;
  char filename[255];
  int fd;
  MPI_Win win;
  int rank; 
};
mmap_file mf;
void get_samples_from_filespace(hid_t fspace, vector<int> &samples, bool &contiguous, bool verbose);

herr_t H5Dread_to_cache( hid_t dataset_id, hid_t mem_type_id, hid_t mem_space_id, hid_t fspace, hid_t xfer_plist_id, void * buf ) {
  /*
  bool contig = false;
  vector<int> b;
  get_samples_from_filespace(fspace, b, contig);
  if (contig) {
    int disp = b[0]%ns_loc*dim;
    memcpy(&membuf[disp], dat, b.size()*dim*sizeof(float));
  } else {
    MPI_Win_fence(MPI_MODE_NOPRECEDE, win);
    for(int i=0; i<b.size(); i++) {
      int dest = b[i]; 
      int src = dest/ns_loc;
      int disp = (dest%ns_loc)*dim;
      if (src==rank)
	memcpy(&membuf[disp], &dat[i*dim], dim*sizeof(float));
      else 
	MPI_Put(&dat[i*dim], dim, MPI_FLOAT, src, disp, dim, MPI_FLOAT, win);
    }
    MPI_Win_fence(MPI_MODE_NOPRECEDE | MPI_MODE_NOSTORE, win);
  }
  return H5Dread(dataset_id,  mem_type_id, mem_space_id, fspace, xfer_plist_id, buf );
  */
}

herr_t H5Dread_from_cache( hid_t dataset_id, hid_t mem_type_id, hid_t mem_space_id, hid_t file_space_id, hid_t xfer_plist_id, void * buf );

void get_samples_from_filespace(hid_t fspace, vector<int> &samples, bool &contiguous, bool verbose=false) {
  hssize_t numblocks = H5Sget_select_hyper_nblocks(fspace);
  hsize_t gdims[MAXDIM];
  int ndims = H5Sget_simple_extent_dims(fspace, gdims, NULL);
  hsize_t *block_buf = new hsize_t [numblocks*2*ndims];
  H5Sget_select_hyper_blocklist(fspace, 0, numblocks, block_buf);
  samples.resize(0); 
  int n=0; 
  for(int i=0; i<numblocks; i++) {
    int start = block_buf[2*i*ndims];
    int end = block_buf[2*i*ndims+ndims];
    for(int j=start; j<end+1; j++) {
      samples.push_back(j);
      n=n+1;
    }
  }
  contiguous = H5Sis_regular_hyperslab(fspace);
  if (verbose) {
    cout << "* Selection is contiguous: " << contiguous << endl; 
    cout << "** List of samples: " << endl; 
    for(size_t i=0; i<samples.size(); i++) {
      cout << samples[i] << " ";
      if (i%5==4) cout << endl; 
    }
    cout << endl; 
  }
}


void create_mmap(size_t size, char *path, char *fname, mmap_file &f) {
  f.size = size;
  strcpy(f.filename, path);
  strcat(f.filename, "/");
  strcat(f.filename, fname);
  int fh = open(f.filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  char a = 'A';
  ::pwrite(fh, &a, 1, f.size);
  fsync(fh);
  close(fh);
  f.fd = open(f.filename, O_RDWR);
  f.buf = mmap(NULL, f.size, PROT_READ | PROT_WRITE, MAP_SHARED, fh, 0);
  msync(f.buf, f.size, MS_SYNC);
}


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
  bool cache = true; 
  MPI_Win win; 
  int epochs = 1;
  int num_batches = 16;
  int batch_size = 32; 
  int rank_shift = 0;
  int i=0;
  Timing tt(rank==io_node());
  // Input 
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
    } else if (strcmp(argv[i], "--cache") ==0){
      cache = true; i=i+1; 
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
  int ndims = H5Sget_simple_extent_ndims(fspace);
  hsize_t *gdims = new hsize_t [ndims];
  H5Sget_simple_extent_dims(fspace, gdims, NULL);
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

  // Preparing the memory mapped buffer put them in MPI_Win
  

  char mmap[255]="mmapf-";
  strcat(mmap, to_string(rank).c_str());
  strcat(mmap, ".dat");
  create_mmap(sizeof(float)*dim*ns_loc, "./", mmap, mf);
  float *membuf = (float*) mf.buf;
  ///====================================================================================
  MPI_Barrier(MPI_COMM_WORLD);
  MPI_Win_create(mf.buf, mf.size, sizeof(float), MPI_INFO_NULL, MPI_COMM_WORLD, &win);
  if (rank==io_node()) cout << " * MPI_Win created" << endl; 
  // ----------------------------------- ====
  if (mpio_collective) {
    H5Pset_dxpl_mpio(dxf_id, H5FD_MPIO_COLLECTIVE);
  } else if (mpio_independent) {
    H5Pset_dxpl_mpio(dxf_id, H5FD_MPIO_INDEPENDENT); 
  } 

  // First epoch
  if (shuffle) ::shuffle(id.begin(), id.end(), g);
  
  for(int nb = 0; nb < num_batches; nb++) {
    // hyperslab selection for a batch of data to read for all the workers
    tt.start_clock("Select");
    
    offset[0] = id[fs_loc+(nb*batch_size)%ns_loc]; // set the offset
    H5Sselect_hyperslab(fspace, H5S_SELECT_SET, offset, NULL, sample, count);
    for(int i=1; i<batch_size; i++) {
      offset[0] = id[fs_loc+(i+nb*batch_size)%ns_loc]; 
      H5Sselect_hyperslab(fspace, H5S_SELECT_OR, offset, NULL, sample, count);
    }
    tt.stop_clock("Select");
    
    tt.start_clock("H5Dread");
    H5Dread(dset, H5T_NATIVE_FLOAT, mspace, fspace, dxf_id, dat);
    tt.stop_clock("H5Dread");
    // Write the batch to SSD
    
    bool contig = false;
    vector<int> b;
    get_samples_from_filespace(fspace, b, contig);
    
    if (contig) {
      int disp = b[0]%ns_loc*dim;
      memcpy(&membuf[disp], dat, b.size()*dim*sizeof(float));
    } else {
      MPI_Win_fence(MPI_MODE_NOPRECEDE, win);
      for(int i=0; i<b.size(); i++) {
	int dest = b[i]; 
	int src = dest/ns_loc;
	int disp = (dest%ns_loc)*dim;
	if (src==rank)
	  memcpy(&membuf[disp], &dat[i*dim], dim*sizeof(float));
	else 
	  MPI_Put(&dat[i*dim], dim, MPI_FLOAT, src, disp, dim, MPI_FLOAT, win);
      }
      MPI_Win_fence(MPI_MODE_NOPRECEDE | MPI_MODE_NOSTORE, win);
    }
  }    

  if (rank==io_node()) 
    printf("Epoch: %d  ---  time: %6.2f (sec) --- throughput: %6.2f (imgs/sec) --- rate: %6.2f (MB/sec)\n", 0, tt["H5Dread"].t, nproc*num_batches*batch_size/tt["H5Dread"].t, num_batches*batch_size*dim*sizeof(float)/tt["H5Dread"].t/1024/1024*nproc);
  
  for(int e =1; e < epochs; e++) {
    double t1 = 0.0;
    dim_dist(gdims[0], nproc, (rank+e*rank_shift)%nproc, &ns_loc, &fs_loc);
    for (int b = 0; b < num_batches; b++) {
      if (io_node()==rank and debug_level() > 1) cout << "Batch: " << b << endl; 
      double t0 = MPI_Wtime();
      MPI_Win_fence(MPI_MODE_NOPUT, win);
      if (shuffle) {
	for(int i=0; i< batch_size; i++) {
	  int dest = id[fs_loc+(b*batch_size + i)];
	  int src = dest/ns_loc;
	  int disp = (dest%ns_loc)*dim;
	  if (src==rank)
	    memcpy(&dat[i*dim], &membuf[disp], dim*sizeof(float));
	  else
	    MPI_Get(&dat[i*dim], dim, MPI_FLOAT, src, disp, dim, MPI_FLOAT, win);
	}
      } else {
	int dest = id[fs_loc + (b*batch_size)%ns_loc]; 
	int src = dest/ns_loc; 
	int disp = dest%ns_loc*dim; 
	memcpy(dat, &membuf[disp], batch_size*dim*sizeof(float));
      }
      MPI_Win_fence(MPI_MODE_NOPUT, win);
      t1 += MPI_Wtime() - t0; 
      if (io_node()==rank and debug_level()>1) {
	for(int i=0; i<batch_size; i++) {
	  cout << "  " << dat[i*dim] << "(" << id[fs_loc+(b*batch_size+i)] << ")  ";
	  if (i%5==4) cout << endl; 
	}
	cout << endl;
      }
    }
    if (io_node()==rank) 
      printf("Epoch: %d  ---  time: %6.2f (sec) --- throughput: %6.2f (imgs/sec) --- rate: %6.2f (MB/sec)\n", e, t1, nproc*num_batches*batch_size/t1, num_batches*batch_size*dim*sizeof(float)/t1/1024/1024*nproc);
  }
  H5Pclose(plist_id);
  H5Sclose(mspace);
  H5Sclose(fspace);
  H5Dclose(dset);
  H5Fclose(fd);
  close(mf.fd);
  delete [] dat;
  delete [] ldims;
  delete [] offset;
  delete [] count;
  delete [] sample; 
  MPI_Finalize();
  return 0;
}

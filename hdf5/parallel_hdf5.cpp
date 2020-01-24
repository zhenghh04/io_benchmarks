#include "hdf5.h"
#include "mpi.h"
#include "stdlib.h"
#include "stdio.h"
#include <sys/time.h>
#include <string.h>
#include "timing.h"
#include "H5SSD.h"
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "mpi.h"
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/statvfs.h>
#include <stdlib.h>
int main(int argc, char **argv) {
  Timing tt; 
  // Assuming that the dataset is a two dimensional array of 8x5 dimension;
  int d1 = 2048; 
  int d2 = 2048; 
  int niter = 10; 
  char scratch[255] = "/tmp/";
  for(int i=1; i<argc; i++) {
    if (strcmp(argv[i], "--dim")==0) {
      d1 = int(atoi(argv[i+1])); 
      d2 = int(atoi(argv[i+2])); 
      i+=2; 
    } else if (strcmp(argv[i], "--niter")==0) {
      niter = int(atoi(argv[i+1])); 
      i+=1; 
    } else if (strcmp(argv[i], "--scratch")==0) {
      strcpy(scratch, argv[i+1]);
      i+=1;
    }
  }

  hsize_t gdims[2] = {d1, d2};
  hsize_t oned = d1*d2;
  MPI_Comm comm = MPI_COMM_WORLD;
  MPI_Info info = MPI_INFO_NULL;
  int rank, nproc; 
  MPI_Init(&argc, &argv);
  MPI_Comm_size(comm, &nproc);
  MPI_Comm_rank(comm, &rank);
  printf("       MPI: I am rank %d of %d \n", rank, nproc);
  // find local array dimension and offset; 
  if (rank==0) {
    printf("     Dim: %llu x %llu\n",  gdims[0], gdims[1]);
    printf(" scratch: %s\n", scratch); 
  }
  hsize_t ldims[2] = {d1/nproc, d2};
  hsize_t offset[2] = {0, 0};
  if(gdims[0]%ldims[0]>0)
    if (rank==nproc-1)
      ldims[0] += gdims[0]%ldims[0]; 
  // setup file access property list for mpio
  hid_t plist_id = H5Pcreate(H5P_FILE_ACCESS);
  H5Pset_fapl_mpio(plist_id, comm, info);



  char f[255];
  strcpy(f, scratch);
  strcat(f, "/parallel_file.h5");

  tt.start_clock("H5Fcreate");   
  hid_t file_id = H5Fcreate_cache(f, H5F_ACC_TRUNC, H5P_DEFAULT, plist_id);
  tt.stop_clock("H5Fcreate"); 

  // create memory space
  hid_t memspace = H5Screate_simple(2, ldims, NULL);

  // define local data
  int* data = new int[ldims[0]*ldims[1]];
  // set up dataset access property list 
  hid_t dxf_id = H5Pcreate(H5P_DATASET_XFER);
  H5Pset_dxpl_mpio(dxf_id, H5FD_MPIO_COLLECTIVE);
  // define local memory space
  
  // create file space and dataset
  hsize_t ggdims[2] = {gdims[0]*niter, gdims[1]};
  hid_t filespace = H5Screate_simple(2, ggdims, NULL);

  hid_t dt = H5Tcopy(H5T_NATIVE_INT);
  tt.start_clock("H5Dcreate");
  hid_t dset_id = H5Dcreate(file_id, "dset", dt, filespace, H5P_DEFAULT,
			    H5P_DEFAULT, H5P_DEFAULT);
  tt.stop_clock("H5Dcreate"); 
  hsize_t size;
  size = get_buf_size(memspace, dt);
  if (rank==0) printf(" mspace size: %f MB | sizeof (element) %lu", float(size)/1024/1024, H5Tget_size(H5T_NATIVE_INT));
  size = get_buf_size(filespace, dt);
  if (rank==0) printf(" fspace size: %f MB | sizeof (element) %lu", float(size)/1024/1024, H5Tget_size(H5T_NATIVE_INT));
  
  hsize_t count[2] = {1, 1};
  for (int i=0; i<niter; i++) {
    tt.start_clock("Init_array"); 
    for(int j=0; j<ldims[0]*ldims[1]; j++)
      data[j] = i;
    tt.stop_clock("Init_array"); 
    if (rank==0) printf("  **Memory rate: %f MB/s\n", d1*d2*sizeof(int)/tt["Init_array"].t/1024/1024);
    offset[0]= i*gdims[0] + rank*ldims[0];
    // select hyperslab
    H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL, ldims, count);
    tt.start_clock("H5Dwrite");
    hid_t status = H5Dwrite_cache(dset_id, H5T_NATIVE_INT, memspace, filespace, dxf_id, data); // write memory to file
    //    H5Fwait();
    //H5Fflush(file_id, H5F_SCOPE_LOCAL);
    tt.stop_clock("H5Dwrite");
  }
  delete [] data;
  Timer T = tt["H5Dwrite"]; 
  if (rank==0) printf("Write rate: %f MB/s\n", size/T.t/1024/1024);
  tt.start_clock("H5close");
  H5Pclose_cache(dxf_id);
  H5Pclose_cache(plist_id);
  H5Dclose_cache(dset_id);
  H5Sclose_cache(filespace);
  H5Sclose_cache(memspace);
  H5Fclose_cache(file_id);
  tt.stop_clock("H5close");
  bool master = (rank==0); 
  tt.PrintTiming(master); 
  MPI_Finalize();
  return 0;
}


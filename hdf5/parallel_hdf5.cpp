#include "hdf5.h"
#include <iostream>
#include "mpi.h"
#include "stdlib.h"
#include "stdio.h"
#include <sys/time.h>
#include <string.h>
#include "timing.h"
using namespace std; 
int main(int argc, char **argv) {
  Timing tt; 
  // Assuming that the dataset is a two dimensional array of 8x5 dimension;
  int d1 = 9; 
  int d2 = 5; 
  int niter = 10; 
  for(int i=1; i<argc; i++) {
    if (strcmp(argv[i], "--dim")==0) {
      d1 = int(atoi(argv[i+1])); 
      d2 = int(atoi(argv[i+2])); 
      i+=2; 
    } else if (strcmp(argv[i], "--niter")==0) {
      niter = int(atoi(argv[i+1])); 
      i+=1; 
    }
  }

  hsize_t gdims[2] = {d1, d2};
  MPI_Comm comm = MPI_COMM_WORLD;
  MPI_Info info = MPI_INFO_NULL;
  int rank, nproc; 
  MPI_Init(&argc, &argv);
  MPI_Comm_size(comm, &nproc);
  MPI_Comm_rank(comm, &rank);
  cout << "I am rank " << rank << " of " << nproc << endl; 
  // find local array dimension and offset; 
  hsize_t ldims[2];
  hsize_t offset[2];
  if (rank==0)
    cout << "Dim: " << gdims[0] << "x" << gdims[1] << endl; 
  ldims[0] = gdims[0]/nproc;
  ldims[1] = gdims[1];
  offset[0] = rank*ldims[0];
  offset[1] = 0;
  if(gdims[0]%ldims[0]>0)
    if (rank==nproc-1)
      ldims[0] += gdims[0]%ldims[0]; 
  // setup file access property list for mpio
  hid_t plist_id = H5Pcreate(H5P_FILE_ACCESS);
  H5Pset_fapl_mpio(plist_id, comm, info);
  // Create file

  tt.start_clock("H5Fcreate"); 
  hid_t file_id = H5Fcreate("parallel_file.h5", H5F_ACC_TRUNC, H5P_DEFAULT, plist_id);
  tt.stop_clock("H5Fcreate"); 
  H5Pclose(plist_id);
#ifdef DEBUG
  if (rank==0) cout << "Created file: " << endl; 
#endif
  // create dataspace
  hid_t filespace = H5Screate_simple(2, gdims, NULL);
  tt.start_clock("H5Dcreate"); 
  hid_t dset_id = H5Dcreate(file_id, "dset", H5T_NATIVE_INT, filespace, H5P_DEFAULT,
			    H5P_DEFAULT, H5P_DEFAULT);
  tt.stop_clock("H5Dcreate"); 
#ifdef DEBUG
  if (rank==0) cout << "Created dataspace: " << endl; 
#endif

  // define local data
  int* data = new int[ldims[0]*ldims[1]];
  tt.start_clock("array"); 
  for(int i=0; i<ldims[0]*ldims[1]; i++)
    data[i] = rank + 10;
  tt.stop_clock("array"); 
  if (rank==0) printf("Memory rate: %f MB/s\n", d1*d2*sizeof(int)/tt["array"].t/1024/1024);
  // set up dataset access property list 
  plist_id = H5Pcreate(H5P_DATASET_XFER);


  H5Pset_dxpl_mpio(plist_id, H5FD_MPIO_COLLECTIVE);
  // define local memory space
  hid_t memspace = H5Screate_simple(2, ldims, NULL);
  H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL, ldims, NULL);
  // write dataset;
#ifdef DEBUG
  if (rank==0) cout << "H5Dwrite ... " << endl; 
#endif
  for (int i=0; i<niter; i++) {
    tt.start_clock("H5Dwrite"); 
    hid_t status = H5Dwrite(dset_id, H5T_NATIVE_INT, memspace, filespace, plist_id, data);
    tt.stop_clock("H5Dwrite"); 
  }
  Timer T = tt["H5Dwrite"]; 
  if (rank==0) printf("Write rate: %f MB/s", d1*d2*sizeof(int)/T.t*T.num_call/1024/1024); 
  H5Pclose(plist_id);
  delete [] data;
  H5Dclose(dset_id);
  H5Sclose(filespace);
  H5Sclose(memspace);
  tt.start_clock("H5Dclose"); 
  H5Fclose(file_id);
  tt.stop_clock("H5Dclose"); 
  bool master = (rank==0); 
  tt.PrintTiming(master); 
  MPI_Finalize();
  return 0;
}

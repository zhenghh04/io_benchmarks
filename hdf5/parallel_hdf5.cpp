#include "hdf5.h"
#include <iostream>
#include "mpi.h"
#include "stdlib.h"
#include "stdio.h"
#include <sys/time.h>
#include <string.h>
#include "timing.h"
#include "H5SSD.h"
using namespace std;
hsize_t get_buf_size(hid_t mspace, hid_t tid) {
  int n= H5Sget_simple_extent_ndims(mspace);
  hsize_t *dim =	new hsize_t[n];
  hsize_t *mdim = new hsize_t[n];
  H5Sget_simple_extent_dims(mspace, dim, mdim);
  hsize_t s = 1;
  for(int i=0; i<n; i++) {
    s=s*dim[i];
  }
  s = s*H5Tget_size(tid);
  return s;
}
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
  cout << "I am rank " << rank << " of " << nproc << endl; 
  // find local array dimension and offset; 
  hsize_t ldims[2];
  hsize_t offset[2];
  if (rank==0) {
    cout << "Dim: " << gdims[0] << "x" << gdims[1] << endl; 
    cout << "scratch: " << scratch << endl; 
    cout << "buffer: " << float(d1*d2)/1024/1024*sizeof(int) << " MB" << endl; 
  }
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
  char f[255];
  strcpy(f, scratch);
  strcat(f, "/parallel_file.h5");
  MPI_Comm c2;
  MPI_Info i2; 
  H5Pget_fapl_mpio(plist_id, &c2, &i2);
  cout << comm << " vs " << c2 << endl;  
  hid_t file_id = H5Fcreate_cache(f, H5F_ACC_TRUNC, H5P_DEFAULT, plist_id);

  tt.stop_clock("H5Fcreate"); 

#ifdef DEBUG
  if (rank==0) cout << "Created file: " << endl; 
#endif
  // create dataspace
  hid_t memspace = H5Screate_simple(2, ldims, NULL);
  tt.start_clock("H5Dcreate");
  hid_t dt = H5Tcopy(H5T_NATIVE_INT);  
  hid_t dset_id = H5Dcreate(file_id, "dset", dt, memspace, H5P_DEFAULT,
			    H5P_DEFAULT, H5P_DEFAULT);
  cout << "space: " << H5Sget_simple_extent_ndims(memspace) << endl;
  
  hsize_t size;
  //  H5Dvlen_get_buf_size(dset_id, dt,  memspace,  &size );
  //  cout << "size: " << size << endl; 
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
  hid_t dxf_id = H5Pcreate(H5P_DATASET_XFER);

  H5Pset_dxpl_mpio(dxf_id, H5FD_MPIO_COLLECTIVE);
  // define local memory space
  hid_t filepace = H5Screate_simple(2, ldims, NULL);
  //H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL, ldims, NULL);
  // write dataset;
#ifdef DEBUG
  if (rank==0) cout << "H5Dwrite ...\n" << endl; 
#endif
  size = get_buf_size(memspace, dt);
  cout << "size: " << float(size)/1024/1024 << "MB - " << H5Tget_size(H5T_NATIVE_INT) << endl; 
  for (int i=0; i<niter; i++) {
    tt.start_clock("H5Dwrite"); 
    hid_t status = H5Dwrite_cache(dset_id, H5T_NATIVE_INT, memspace, H5S_ALL, dxf_id, data); // write memory to file
    //    H5Fflush(file_id, H5F_SCOPE_LOCAL);
    tt.stop_clock("H5Dwrite"); 
  }
  Timer T = tt["H5Dwrite"]; 
  if (rank==0) printf("Write rate: %f MB/s\n", d1*d2*sizeof(int)/T.t*T.num_call/1024/1024); 
  H5Pclose(dxf_id);
  delete [] data;
  H5Pclose(plist_id);
  H5Dclose(dset_id);
  //H5Sclose(filespace);
  H5Sclose(memspace);
  tt.start_clock("H5Fclose"); 
  H5Fclose_cache(file_id);
  tt.stop_clock("H5Fclose"); 
  bool master = (rank==0); 
  tt.PrintTiming(master); 
  MPI_Finalize();
  return 0;
}


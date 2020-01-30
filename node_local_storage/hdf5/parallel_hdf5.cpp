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
#include <unistd.h>
#include <stdio.h>
#include <sys/statvfs.h>
#include <stdlib.h>
#include "stat.h"
int msleep(long miliseconds)
{
  struct timespec req, rem;

  if(miliseconds > 999)
    {   
      req.tv_sec = (int)(miliseconds / 1000);                            /* Must be Non-Negative */
      req.tv_nsec = (miliseconds - ((long)req.tv_sec * 1000)) * 1000000; /* Must be in range of 0 to 999999999 */
    }   
  else
    {   
      req.tv_sec = 0;                         /* Must be Non-Negative */
      req.tv_nsec = miliseconds * 1000000;    /* Must be in range of 0 to 999999999 */
    }   
  return nanosleep(&req , &rem);
}

int main(int argc, char **argv) {
  char ssd_cache [255] = "no";
  if (getenv("SSD_CACHE")) {
    strcpy(ssd_cache, getenv("SSD_CACHE"));
  }
  bool cache = false; 
  if (strcmp(ssd_cache, "yes")==0) {
    cache=true;
  }
  Timing tt; 
  // Assuming that the dataset is a two dimensional array of 8x5 dimension;
  int d1 = 2048; 
  int d2 = 2048; 
  int niter = 10; 
  char scratch[255] = "/tmp/";
  double sleep=0.0;
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
    } else if (strcmp(argv[i],"--sleep")==0) {
      sleep = atof(argv[i+1]); 
      i+=1; 
    }
  }
  hsize_t ldims[2] = {d1, d2};

  hsize_t oned = d1*d2;
  MPI_Comm comm = MPI_COMM_WORLD;
  MPI_Info info = MPI_INFO_NULL;
  int rank, nproc; 
  MPI_Init(&argc, &argv);
  MPI_Comm_size(comm, &nproc);
  MPI_Comm_rank(comm, &rank);
  //printf("     MPI: I am rank %d of %d \n", rank, nproc);
  // find local array dimension and offset; 
  hsize_t gdims[2] = {d1*nproc, d2};
  if (rank==0) {
    printf("=============================================\n");
    printf(" Buf dim: %llu x %llu\n",  ldims[0], ldims[1]);
    printf("Buf size: %f MB\n", float(d1*d2)/1024/1024*sizeof(int));
    printf(" Scratch: %s\n", scratch); 
    printf("   nproc: %d\n", nproc);
    printf("=============================================\n");
    if (cache) printf("** using SSD as a cache **\n"); 
  }
  hsize_t offset[2] = {0, 0};
  // setup file access property list for mpio
  hid_t plist_id = H5Pcreate(H5P_FILE_ACCESS);
  H5Pset_fapl_mpio(plist_id, comm, info);

  char f[255];
  strcpy(f, scratch);
  strcat(f, "/parallel_file.h5");

  tt.start_clock("H5Fcreate");   
  hid_t file_id;
  if (cache)
    file_id = H5Fcreate_cache(f, H5F_ACC_TRUNC, H5P_DEFAULT, plist_id);
  else 
    file_id = H5Fcreate(f, H5F_ACC_TRUNC, H5P_DEFAULT, plist_id);
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
  if (rank==0) 
    printf(" mspace size: %5.5f MB | sizeof (element) %lu\n", float(size)/1024/1024, H5Tget_size(H5T_NATIVE_INT));
  size = get_buf_size(filespace, dt);
  if (rank==0) 
    printf(" fspace size: %5.5f MB \n", float(size)/1024/1024);
  
  hsize_t count[2] = {1, 1};
  tt.start_clock("Init_array"); 
  for(int j=0; j<ldims[0]*ldims[1]; j++)
    data[j] = 0;
  tt.stop_clock("Init_array"); 
  for (int i=0; i<niter; i++) {
    offset[0]= i*gdims[0] + rank*ldims[0];
    // select hyperslab
    H5Sselect_hyperslab(filespace, H5S_SELECT_SET, offset, NULL, ldims, count);
    tt.start_clock("H5Dwrite");
    if (cache)
      hid_t status = H5Dwrite_cache(dset_id, H5T_NATIVE_INT, memspace, filespace, dxf_id, data); // write memory to file
    else {
      hid_t status = H5Dwrite(dset_id, H5T_NATIVE_INT, memspace, filespace, dxf_id, data); // write memory to file
      H5Fflush(file_id, H5F_SCOPE_LOCAL);
    }
    tt.stop_clock("H5Dwrite");
    tt.start_clock("compute");
    msleep(int(sleep*1000));
    tt.stop_clock("compute");
  }
  tt.start_clock("H5close");
  if (cache) {
    H5Pclose_cache(dxf_id);
    H5Pclose_cache(plist_id);
    H5Dclose_cache(dset_id);
    H5Sclose_cache(filespace);
    H5Sclose_cache(memspace);
    H5Fclose_cache(file_id);
  } else {
    H5Pclose(dxf_id);
    H5Pclose(plist_id);
    H5Dclose(dset_id);
    H5Sclose(filespace);
    H5Sclose(memspace);
    H5Fclose(file_id);
  }
  tt.stop_clock("H5close");
  bool master = (rank==0); 
  delete [] data;
  Timer T = tt["H5Dwrite"]; 
  double avg = 0.0; 
  double std = 0.0; 
  stat(&tt["H5Dwrite"].t_iter[0], niter, avg, std, 'i');
  if (rank==0) printf("  Write rate: %f +/- %f MB/s\n", size*avg/niter/1024/1024, size/niter*std/1024/1024);

  tt.PrintTiming(master); 
  MPI_Finalize();
  return 0;
}

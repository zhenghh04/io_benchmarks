#include <iostream>
#include "hdf5.h"
#include "stdlib.h"
#include "stdio.h"
#include "mpi.h"
#include "string.h"
#include "timing.h"

using namespace std; 
int main(int argc, char **argv) {
  int rank, nproc;

  MPI_Init(&argc, &argv);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  int i = 1;
  bool mem = false;
  while(i<argc) {
    if (strcmp(argv[i], "--mem")==0) {
      mem = true; i=i+1; 
    } else {
      i = i+1; 
    }
  }
  char fname[255] = "test.h5";
  Timing tt(rank==0); 
  int ms[3] = {96, 7404, 13806};
  int cs[3] = {1, 1, 64};
  hid_t fspace[3];
  hid_t mspace[3];
  hid_t dset[3];
  char *data[3];
  size_t block_size = 1048576;
  hbool_t backing_store = 1;
  hid_t apl = H5Pcreate(H5P_FILE_ACCESS);

  if (mem) {
    H5Pset_fapl_core(apl, block_size, backing_store);
  }
  H5Pset_fapl_mpio(apl, MPI_COMM_WORLD, MPI_INFO_NULL);
  hid_t dxf_id = H5Pcreate(H5P_DATASET_XFER);
  H5Pset_dxpl_mpio(dxf_id, H5FD_MPIO_COLLECTIVE);
  hid_t fd = H5Fcreate(fname, H5F_ACC_TRUNC, H5P_DEFAULT, apl);
  char dname[255] = "dset_"; 
  for(int i=0; i<3; i++) {
    data[i] = new char [ms[i]];
    hsize_t gdim[1] = {ms[i]*nproc};
    hsize_t ldim[1] = {ms[i]};
    fspace[i] = H5Screate_simple(1, gdim, NULL);
    mspace[i] = H5Screate_simple(1, ldim, NULL);
    hsize_t offset[1] = {rank*ms[i]};
    hsize_t count[1] = {1};
    H5Sselect_hyperslab(fspace[i], H5S_SELECT_SET, offset, NULL, ldim, count);
    for (int j=0; j<cs[i]; j++) {
      char fn[255];
      strcpy(fn, dname);
      strcat(fn, to_string(i).c_str());
      strcat(fn, "-");
      strcat(fn, to_string(j).c_str());
      dset[i] = H5Dcreate(fd, fn, H5T_NATIVE_CHAR, fspace[i], H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
      tt.start_clock("H5Dwrite");
      H5Dwrite(dset[i], H5T_NATIVE_CHAR, mspace[i], fspace[i], dxf_id, data[i]);
      tt.stop_clock("H5Dwrite");
      H5Dclose(dset[i]);
    }
    H5Sclose(fspace[i]);
    H5Sclose(mspace[i]);
  }
  H5Pclose(dxf_id);
  H5Pclose(apl);
  tt.start_clock("H5Fclose");
  H5Fclose(fd);
  tt.stop_clock("H5Fclose");
  MPI_Finalize();
  return 0; 
}

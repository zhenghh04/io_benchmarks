#include <hdf5.h>
#include "stdlib.h"
#include "stdio.h"
void checkItem();
void addItem(hid_t a);
int main() {
  hsize_t ldims[2] = {4, 6};
  hid_t file_id = H5Fcreate("test.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT); 
  hid_t memspace = H5Screate_simple(2, ldims, NULL);
  hid_t dt = H5Tcopy(H5T_NATIVE_INT);
  hid_t dset_id  = H5Dcreate(file_id, "dset", dt, memspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT); 
  for(int i=0; i<16; i++) {
    addItem(dset_id);
  }
  checkItem();
  checkItem(); 
  return 0; 
}

/* 
   This example shows how to create a HDF5 file and write dataset to it. 
   Huihuo Zheng
 */
#include "hdf5.h"
#include <iostream>
using namespace std; 
int main() {
  hid_t file_id;
  herr_t status;
  
  file_id = H5Fcreate("file.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT);

  hid_t group_id = H5Gcreate(file_id, "MyGroup", H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT);
  status =  H5Gclose(group_id); 
  const hsize_t dims[2] = {4, 6};
  /* create the data space for the dataset */
  
  hid_t dataspace_id = H5Screate_simple(2, dims, NULL);
  /* create the data set */
  hid_t dataset_id = H5Dcreate(file_id, "/dset", H5T_STD_I32BE, 
			       dataspace_id, H5P_DEFAULT, H5P_DEFAULT, 
			       H5P_DEFAULT);
  /* write attribute to dataset*/
  hsize_t d = 2; 
  dataspace_id = H5Screate_simple(1, &d, NULL);
  hid_t attribute_id = H5Acreate2 (dataset_id, "Units", H5T_STD_I32BE, dataspace_id, H5P_DEFAULT, H5P_DEFAULT);
  char *buf="Meters per second"; 
  status = H5Awrite(attribute_id, H5T_NATIVE_CHAR, buf);
  status = H5Aclose(attribute_id); 
  
  int dset[4][6];
  for(int i=0; i<4; i++)
    for (int j=0; j<6; j++)
      dset[i][j] =  i*6 + j+1;
  int dset_r[4][6];
  status = H5Dwrite(dataset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, dset);
  status = H5Dread(dataset_id, H5T_NATIVE_INT, H5S_ALL, H5S_ALL, H5P_DEFAULT, dset_r);
  for(int i=0; i<4; i++)
    for (int j=0; j<6; j++)
      cout << dset_r[i][j]<< " "  <<  i*6 + j+1 << endl;
  
  status = H5Dclose(dataset_id);
  status = H5Sclose(dataspace_id); 
  status = H5Fclose(file_id);
  return status; 
}

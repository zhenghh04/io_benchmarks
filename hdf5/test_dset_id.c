#include <hdf5.h>
#include "stdlib.h"
#include "stdio.h"
typedef struct _dset_list {
  hid_t dset_id; 
  struct _dset_list *next; 
} dset_list; 
dset_list *dsl=NULL;
dset_list *head=NULL; 
void addItem(hid_t dset_id) {
  if (dsl=NULL) {
    dsl = (dset_list*) malloc(sizeof(dset_list));
   head = dsl; 
  }
  printf("adding atoms; ");
  dsl->dset_id = dset_id; 
  printf("dset_id: %d- %d\n", dsl->dset_id, H5Iget_type(dsl->dset_id));
  dsl->next = (dset_list*) malloc(sizeof(dset_list));
  dsl = dsl->next; 
} 

void checkItem() {
  dset_list *data=head; 
  while (data!=NULL) {
    printf("dset_id: %d- %d\n", data->dset_id, H5Iget_type(data->dset_id));
    data = data->next; 
  }
}

int main() {
  hsize_t ldims[2] = {4, 6};
  hid_t file_id = H5Fcreate("test.h5", H5F_ACC_TRUNC, H5P_DEFAULT, H5P_DEFAULT); 
  hid_t memspace = H5Screate_simple(2, ldims, NULL);
  hid_t dt = H5Tcopy(H5T_NATIVE_INT);
  hid_t dset_id  = H5Dcreate(file_id, "dset", dt, memspace, H5P_DEFAULT, H5P_DEFAULT, H5P_DEFAULT); 
  for(int i=0; i<16; i++) 
    addItem(dset_id);
  checkItem(); 
  return 0; 
}

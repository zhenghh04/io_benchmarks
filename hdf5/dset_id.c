#include "hdf5.h"
#include "stdio.h"
#include "stdlib.h"
typedef struct _dset_list {
  hid_t dset_id;
  int id; 
  struct _dset_list *next; 
} dset_list; 
dset_list *dsl=NULL;
dset_list *head=NULL; 
void addItem(hid_t dset_id) {
  if (dsl==NULL) {
    dsl = (dset_list*) malloc(sizeof(dset_list));
    head = dsl;
    dsl->id = 0;
  }
  dsl->dset_id = dset_id; 
  printf("dset_id: %lld - %d (%d)\n", dsl->dset_id, H5Iget_type(dsl->dset_id), dsl->id);
  dsl->next = (dset_list*) malloc(sizeof(dset_list));
  dsl->next->id = dsl->id + 1; 
  dsl = dsl->next; 
} 

void checkItem() {
  dset_list *data=head; 
  while (data->next!=NULL) {
    printf("check dset_id: %lld - %d (%d)\n", data->dset_id, H5Iget_type(data->dset_id), data->id);
    data = data->next; 
  }
}

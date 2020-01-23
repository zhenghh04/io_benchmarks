#ifndef H5FSSD_H_
#define H5FSSD_H_
#include "hdf5.h"
typedef struct _thread_data_t {
// we will use the link structure in C to build the list of I/O tasks
  hid_t dataset_id; 
  hid_t mem_type_id; 
  hid_t mem_space_id; 
  hid_t file_space_id; 
  hid_t xfer_plist_id; 
  int id;
  const void *buf; // there are two questions: how to save the buffer before conversion? There is a #byte difference between HDF5 and user definition. 
  struct _thread_data_t *next; 

} thread_data_t;

typedef struct _SSD_CACHE_IO {
  int fd;
  char fname[255];
  char path[255];
  double mspace_total;
  double mspace_left;
  int num_request;
  int offset;
  int ppn;
  int rank;
  void *mmap_ptr;
  MPI_Comm comm;
  pthread_cond_t master_cond;
  pthread_cond_t io_cond;
  pthread_mutex_t request_lock;
  pthread_t pthread;
  thread_data_t *request_list, *current_request, *head; 
} SSD_CACHE_IO; 
hsize_t get_buf_size(hid_t mspace, hid_t tid); 
hid_t H5Fcreate_cache( const char *name, unsigned flags, 
		       hid_t fcpl_id, hid_t fapl_id );

herr_t H5Fclose_cache( hid_t file_id );
herr_t H5Dwrite_cache(hid_t dset_id, hid_t mem_type_id, 
		      hid_t mem_space_id, hid_t file_space_id, 
		      hid_t dxpl_id, const void *buf);
#endif
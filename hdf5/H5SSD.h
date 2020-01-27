#ifndef H5FSSD_H_
#define H5FSSD_H_
#include "hdf5.h"
typedef struct _thread_data_t {
  // we will use the link structure in C to build the list of I/O tasks
  char fname[255];
  hid_t dataset_id; 
  hid_t mem_type_id; 
  hid_t mem_space_id; 
  hid_t file_space_id; 
  hid_t xfer_plist_id; 
  int id;
  hid_t offset;
  hsize_t size; 
  void *buf; 
  struct _thread_data_t *next; 

} thread_data_t;

void check_pthread_data(thread_data_t *pt);
typedef struct _SSD_CACHE_IO {
  int fd;
  char fname[255];
  char path[255];
  double mspace_total;
  double mspace_left;
  int num_request;
  hsize_t offset;
  int ppn;
  int rank;
  int local_rank;
  void *mmap_ptr;
  MPI_Comm comm;
  pthread_cond_t master_cond;
  pthread_cond_t io_cond;
  pthread_mutex_t request_lock, write_lock;
  pthread_t pthread;
  thread_data_t *request_list, *current_request, *first_request; 
} SSD_CACHE_IO; 
hsize_t get_buf_size(hid_t mspace, hid_t tid); 
hid_t H5Fcreate_cache( const char *name, unsigned flags, 
		       hid_t fcpl_id, hid_t fapl_id );

herr_t H5Fclose_cache( hid_t file_id );
herr_t H5Dwrite_cache(hid_t dset_id, hid_t mem_type_id, 
		      hid_t mem_space_id, hid_t file_space_id, 
		      hid_t dxpl_id, const void *buf);
herr_t H5Dclose_cache( hid_t id);
herr_t H5Pclose_cache( hid_t id);
herr_t H5Sclose_cache( hid_t id);
void H5Fwait();
void test_mmap_buf();
#endif //H5SSD_H_

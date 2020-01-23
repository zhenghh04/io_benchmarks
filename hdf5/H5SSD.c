#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "mpi.h"
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/statvfs.h>
#include <stdlib.h>

#define H5F_FRIEND              /* suppress error about including H5Fpkg  */

/***********/
/* Headers */
/***********/

#include "hdf5.h"
#include "H5FDmpio.h"
#include "H5SSD.h"
SSD_CACHE_IO
H5SSD = {
	 .num_request = 0,
	 .offset = 0,
	 .ppn = 1,
	 .rank = 0,
	 .master_cond =PTHREAD_COND_INITIALIZER,
	 .io_cond = PTHREAD_COND_INITIALIZER,
	 .request_lock = PTHREAD_MUTEX_INITIALIZER,
	 .request_list = NULL,
	 .current_request = NULL,
};
hid_t ddset, dx; 
int setH5SSD() {
#ifdef THETA
  strcpy(H5SSD.path, "/local/scratch/");
  H5SSD.mspace_total = 137438953472;
#else
  char *ssd=getenv("SSD_CACHE_PATH");
  strcpy(H5SSD.path, ssd);
  H5SSD.mspace_total = 137438953472;
#endif
  H5SSD.mspace_left = H5SSD.mspace_total;
  return 0; 
}

/* for storing the request list */
void check_pthread_data(thread_data_t *pt) {
  printf("********************************************\n");
  printf("***values: dset-%lld, mtype-%lld, mspace-%lld, fspace-%lld, xfer-%lld\n",
	 pt->dataset_id,
	 pt->mem_type_id,
	 pt->mem_space_id,
	 pt->file_space_id,
	 pt->xfer_plist_id);
  printf("***  type: dset-%d, mtype-%d, mspace-%d, fspace-%d, xfer-%d\n",
	 H5Iget_type(pt->dataset_id),
	 H5Iget_type(pt->mem_type_id),
	 H5Iget_type(pt->mem_space_id),
	 H5Iget_type(pt->file_space_id),
	 H5Iget_type(pt->xfer_plist_id));
  printf("********************************************\n\n");
}


void *H5Dwrite_pthread_func(void *arg) {
  pthread_mutex_lock(&H5SSD.request_lock);
  while (H5SSD.num_request>=0) {
    if (H5SSD.num_request >0) {
      thread_data_t *data = H5SSD.current_request; 
#ifdef SSD_CACHE_DEBUG
      if (H5SSD.rank ==0) {

	printf("Request#: %d of %d\n", data->id, H5SSD.num_request);
	printf("IO: *dataset_id: %lld - %d - %d\n", data->dataset_id, H5Iget_type(data->dataset_id), H5Iget_type(dx));
	check_pthread_data(data);
      }
#endif      
      H5Dwrite(data->dataset_id, data->mem_type_id, 
	       data->mem_space_id, data->file_space_id, 
	       data->xfer_plist_id, data->buf);
      printf("finished H5Dwrite\n"); 
      H5SSD.current_request=H5SSD.current_request->next;
      H5SSD.num_request--; 
    } if (H5SSD.num_request == 0) {
      pthread_cond_signal(&H5SSD.master_cond);
      pthread_cond_wait(&H5SSD.io_cond, &H5SSD.request_lock);
    }
  }
  pthread_mutex_unlock(&H5SSD.request_lock);
  pthread_exit(NULL);
  return NULL; 
}
int rc = pthread_create(&H5SSD.pthread, NULL, H5Dwrite_pthread_func, NULL);

hid_t H5Fcreate_cache( const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id ) {
  srand(time(NULL));   // Initialization, should only be called once.
  setH5SSD();
  hid_t fd = H5Fcreate(name, flags, fcpl_id, fapl_id);
  MPI_Comm comm;
  MPI_Info info; 
  H5Pget_fapl_mpio(fapl_id, &comm, &info);
  MPI_Comm_split_type(comm, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &H5SSD.comm);
  MPI_Comm_rank(H5SSD.comm, &H5SSD.rank);
  MPI_Comm_size(H5SSD.comm, &H5SSD.ppn);
  strcpy(H5SSD.fname, H5SSD.path);
  char rnd[255];
  sprintf(rnd, "%d", rand());
  strcat(H5SSD.fname, rnd);
  strcat(H5SSD.fname, "-"); 
  sprintf(rnd, "%d", H5SSD.ppn);
  strcat(H5SSD.fname, rnd);
#ifdef SSD_CACHE_DEBUG
  if (H5SSD.rank==0) {
    printf("SSD_CACHE: H5Fcreate\n"); 
  }
  printf("SSD file: %s\n", H5SSD.fname);
#endif
  H5SSD.fd = open(H5SSD.fname,  O_RDWR | O_CREAT | O_TRUNC, 0600);
  H5SSD.request_list = (thread_data_t*) malloc(sizeof(thread_data_t)); 
  //pthread_mutex_lock(&H5SSD.request_lock);
  H5SSD.request_list->id = 0; 
  H5SSD.current_request = H5SSD.request_list; 
  H5SSD.head = H5SSD.request_list; 
  //pthread_mutex_unlock(&H5SSD.request_lock);
  return fd; 
}


herr_t
H5Dwrite_cache(hid_t dataset_id, hid_t mem_type_id, hid_t mem_space_id,
	 hid_t file_space_id, hid_t dxpl_id, const void *buf) {
  
#ifdef SSD_CACHE_DEBUG
  if (H5SSD.rank==0)
    printf("SSD_CACHE: H5Dwrite_cache\n"); 
#endif
  hsize_t size; 
  size = get_buf_size(H5Dget_space(dataset_id), mem_type_id);
  //size = 16*1024*1024;
  printf("buffer size: %llu\n", size/1024/1024); 
  if (H5SSD.mspace_left < size) {
#ifdef SSD_CACHE_DEBUG
    printf("H5SSD.mspace_left is not enough, waiting for previous I/O jobs to finish first\n");
#endif
    pthread_mutex_lock(&H5SSD.request_lock);
    while(H5SSD.num_request>0)  {
      pthread_cond_signal(&H5SSD.io_cond);
      pthread_cond_wait(&H5SSD.master_cond, &H5SSD.request_lock);
    }
    pthread_mutex_unlock(&H5SSD.request_lock);
    H5SSD.offset=0;
    H5SSD.mspace_left = H5SSD.mspace_total;
  }
  pwrite(H5SSD.fd, (char*)buf, size, H5SSD.offset);
  fsync(H5SSD.fd);
  // add task to the list
  H5SSD.request_list->dataset_id = dataset_id; 
  H5SSD.request_list->mem_type_id = mem_type_id;
  H5SSD.request_list->mem_space_id = mem_space_id;
  H5SSD.request_list->file_space_id =file_space_id;
  H5SSD.request_list->xfer_plist_id = dxpl_id;
  dx = dxpl_id;
  check_pthread_data(H5SSD.request_list);
  printf("dxpl_id: %lld, %d,%d\n", dxpl_id, H5Iget_type(dxpl_id), H5Iget_type(dx));
  H5SSD.mmap_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, H5SSD.fd, H5SSD.offset);
  msync(H5SSD.mmap_ptr, size, MS_SYNC);
  H5SSD.request_list->buf = H5SSD.mmap_ptr;
  H5SSD.request_list->next = (thread_data_t*) malloc(sizeof(thread_data_t));
  H5SSD.request_list->next->id = H5SSD.request_list->id + 1;

  thread_data_t *data = H5SSD.request_list;   
  H5SSD.request_list = H5SSD.request_list->next;  
  pthread_mutex_lock(&H5SSD.request_lock);
  H5SSD.num_request++;
  pthread_mutex_unlock(&H5SSD.request_lock);
  pthread_cond_signal(&H5SSD.io_cond);// wake up I/O thread rightawayx
  printf("Number of request: %d\n", H5SSD.num_request); 

  H5SSD.offset += size;
  // compute how much space left
  H5SSD.mspace_left -= size*H5SSD.ppn;
  return 0; 
}



herr_t H5Fclose_cache( hid_t file_id ) {
#ifdef SSD_CACHE_DEBUG
  if (H5SSD.rank==0)
    printf("SSD_CACHE: H5Fclose\n"); 
#endif

  thread_data_t *data = H5SSD.head;
  printf("dset_id -head : %lld (%d)\n", H5SSD.head->dataset_id, H5Iget_type(H5SSD.head->dataset_id)); 
  while(data->next != NULL) {
    printf("dset_id: %lld (%d) - %d\n", data->dataset_id, data->id, H5Iget_type(ddset)); 
    data = data->next; 
  }
  pthread_mutex_lock(&H5SSD.request_lock);
  while(H5SSD.num_request>0)  {
    pthread_cond_signal(&H5SSD.io_cond);
    pthread_cond_wait(&H5SSD.master_cond, &H5SSD.request_lock);
  }
  pthread_mutex_unlock(&H5SSD.request_lock);
  close(H5SSD.fd);
  H5SSD.mspace_left = H5SSD.mspace_total;
  remove(H5SSD.fname);
  return H5Fclose(file_id);
}

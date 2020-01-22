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
/****************/
/* Module Setup */
/****************/


#define H5F_FRIEND              /* suppress error about including H5Fpkg  */

/***********/
/* Headers */
/***********/

#include "hdf5.h"
#include "H5FDmpio.h"
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

SSD_CACHE_IO
H5SSD = {
	 .master_cond =PTHREAD_COND_INITIALIZER,
	 .io_cond = PTHREAD_COND_INITIALIZER,
	 .request_lock = PTHREAD_MUTEX_INITIALIZER,
	 .request_list = NULL,
	 .current_request = NULL,
	 .num_request = 0,
	 .offset = 0,
	 .ppn = 1,
	 .rank = 0,
};


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


hsize_t get_buf_size(hid_t mspace, hid_t tid); 

/* for storing the request list */

void *H5Dwrite_pthread_func(void *arg) {
  pthread_mutex_lock(&H5SSD.request_lock);
  while (H5SSD.num_request>=0) {
    if (H5SSD.num_request >0) {
      thread_data_t *data = H5SSD.current_request; 
      
#ifdef SSD_CACHE_DEBUG
      if (H5SSD.rank ==0) {
	printf("Request#: %d of %d\n", data->id, H5SSD.num_request);
	printf("H5Dwrite, ....\n");
	printf("IO: *dataset_id: %d\n", data->dataset_id); 
      }
#endif      
      sleep(1);
      printf("dataset id check: %d\n ", H5Iget_type(data->dataset_id)); 
      H5Dwrite(data->dataset_id, data->mem_type_id, 
	       data->mem_space_id, data->file_space_id, 
	       data->xfer_plist_id, data->buf);
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
    printf("SSD_CACHE: MPI_File_open\n"); 
  }
  printf("SSD file: %s\n", H5SSD.fname);
#endif
  H5SSD.fd = open(H5SSD.fname,  O_RDWR | O_CREAT | O_TRUNC, 0600);
  H5SSD.request_list = (thread_data_t*) malloc(sizeof(thread_data_t)); 
  pthread_mutex_lock(&H5SSD.request_lock);
  H5SSD.request_list->id = 0; 
  H5SSD.current_request = H5SSD.request_list; 
  H5SSD.head = H5SSD.request_list; 
  pthread_mutex_unlock(&H5SSD.request_lock);
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
  // map the buf
  H5SSD.mmap_ptr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, H5SSD.fd, H5SSD.offset);
  msync(H5SSD.mmap_ptr, size, MS_SYNC);
  H5SSD.request_list->buf = H5SSD.mmap_ptr;
  H5SSD.request_list->next = (thread_data_t*) malloc(sizeof(thread_data_t));
  H5SSD.request_list->next->id = H5SSD.request_list->id + 1; 
  printf("master write\n");
  thread_data_t *data = H5SSD.request_list;   
  printf("*dataset_id: %d - %d\n", data->dataset_id, dataset_id); 
  H5Dwrite(data->dataset_id, data->mem_type_id, 
	   data->mem_space_id, data->file_space_id, 
	   data->xfer_plist_id, data->buf);
  printf("%d\n", H5Iget_type(data->dataset_id));
  printf("%d\n", H5Iget_type(dataset_id));
  
  H5SSD.request_list = H5SSD.request_list->next;
  //
  printf("master write done\n");
        //pthread_mutex_lock(&H5SSD.request_lock);
  H5SSD.num_request++;
  printf("Number of request: %d\n", H5SSD.num_request); 
  //pthread_cond_signal(&H5SSD.io_cond);// wake up I/O thread rightawayn
  //pthread_mutex_unlock(&H5SSD.request_lock);
  H5SSD.offset += size;
  // compute how much space left
  H5SSD.mspace_left -= size*H5SSD.ppn;
  return 0; 
}



herr_t H5Fclose_cache( hid_t file_id ) {
  thread_data_t *data = H5SSD.head; 
  while(data->next != NULL) {
    printf("dset_id: %d (%d) - %d\n", data->dataset_id, data->id, H5Iget_type(data->dataset_id)); 
    data = data->next; 
  }
#ifdef SSD_CACHE_DEBUG
  if (H5SSD.rank==0)
    printf("SSD_CACHE: H5Fclose\n"); 
#endif
  pthread_mutex_lock(&H5SSD.request_lock);
  while(H5SSD.num_request>0)  {
    pthread_cond_signal(&H5SSD.io_cond);
    pthread_cond_wait(&H5SSD.master_cond, &H5SSD.request_lock);
  }
  pthread_mutex_unlock(&H5SSD.request_lock);
  close(H5SSD.fd);
  H5SSD.mspace_left = H5SSD.mspace_total;
  remove(H5SSD.fname);
  return 0; 
  //return H5Fclose(file_id);
}

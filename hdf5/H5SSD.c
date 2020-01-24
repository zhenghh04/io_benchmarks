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
	 .local_rank = 0,
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
  printf("********************************************\n");
}

hsize_t get_buf_size(hid_t mspace, hid_t tid) {
  int n= H5Sget_simple_extent_ndims(mspace);
  hsize_t *dim =  new hsize_t[n];
  hsize_t *mdim = new hsize_t[n];
  H5Sget_simple_extent_dims(mspace, dim, mdim);
  hsize_t s = 1;
  for(int i=0; i<n; i++) {
    s=s*dim[i];
  }
  s = s*H5Tget_size(tid);
  return s;
}

void *H5Dwrite_pthread_func(void *arg) {
  while (H5SSD.num_request>=0) {
    if (H5SSD.num_request >0) {
      thread_data_t *data = H5SSD.current_request;
#ifdef SSD_CACHE_DEBUG
      if (H5SSD.rank = 0) printf("\n== H5Dwrite from pthread ==\n");
      int *p = (int*) data->buf;
      if (H5SSD.rank==0) printf("== test mmap prp: %d\n", p[0]);
      if (H5SSD.rank==0) {
	printf("== pthread starts H5Dwrite\n");
	check_pthread_data(data);
      }
#endif
      H5Dwrite(data->dataset_id, data->mem_type_id, 
	       data->mem_space_id, data->file_space_id, 
	       data->xfer_plist_id, data->buf);
#ifdef SSD_CACHE_DEBUG
      if (H5SSD.rank==0)
	printf("== pthread finished H5Dwrite\n");
#endif       
      H5SSD.current_request=H5SSD.current_request->next;
      pthread_mutex_lock(&H5SSD.request_lock);
      H5SSD.num_request--;
      pthread_mutex_unlock(&H5SSD.request_lock);
    } if (H5SSD.num_request == 0) {
      pthread_mutex_lock(&H5SSD.request_lock);
      pthread_cond_signal(&H5SSD.master_cond);
      pthread_cond_wait(&H5SSD.io_cond, &H5SSD.request_lock);
      pthread_mutex_unlock(&H5SSD.request_lock);
    }
  }

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
  MPI_Comm_rank(comm, &H5SSD.rank);
  MPI_Comm_split_type(comm, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &H5SSD.comm);
  MPI_Comm_rank(H5SSD.comm, &H5SSD.local_rank);
  MPI_Comm_size(H5SSD.comm, &H5SSD.ppn);
  strcpy(H5SSD.fname, H5SSD.path);
  char rnd[255];
  sprintf(rnd, "%d", rand());
  strcat(H5SSD.fname, rnd);
  strcat(H5SSD.fname, "-"); 
  sprintf(rnd, "%d", H5SSD.local_rank);
  strcat(H5SSD.fname, rnd);
#ifdef SSD_CACHE_DEBUG
  if (H5SSD.rank==0) {
    printf("SSD_CACHE: H5Fcreate\n"); 
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
  size = get_buf_size(mem_space_id, mem_type_id);
#ifdef SSD_CACHE_DEBUG
  if (H5SSD.rank==0)
    printf("buffer size: %f\n", float(size)/1024/1024);
#endif
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
#ifdef SSD_CACHE_DEBUG
  if (H5SSD.rank==0) printf("Offset: %llu\n", H5SSD.offset);
#endif
  int err = pwrite(H5SSD.fd, (char*)buf, size, H5SSD.offset);
  // add task to the list
  H5SSD.request_list->dataset_id = dataset_id; 
  H5SSD.request_list->mem_type_id = mem_type_id;
  H5SSD.request_list->mem_space_id = mem_space_id;
  H5SSD.request_list->file_space_id =file_space_id;
  H5SSD.request_list->xfer_plist_id = dxpl_id;
  dx = dxpl_id;
  H5SSD.request_list->buf = mmap(NULL, size, PROT_READ, MAP_SHARED, H5SSD.fd, H5SSD.offset);

  msync(H5SSD.request_list->buf, size, MS_SYNC);
  fsync(H5SSD.fd);
  H5SSD.request_list->next = (thread_data_t*) malloc(sizeof(thread_data_t));
  H5SSD.request_list->next->id = H5SSD.request_list->id + 1;
  thread_data_t *data = H5SSD.request_list;   
  H5SSD.request_list = H5SSD.request_list->next;
  pthread_mutex_lock(&H5SSD.request_lock);
  H5SSD.num_request++;
  pthread_cond_signal(&H5SSD.io_cond);// wake up I/O thread rightawayx
#ifdef SSD_CACHE_DEBUG
  if (H5SSD.rank==0) 
    printf("Number of request: %d\n", H5SSD.num_request);
#endif
  pthread_mutex_unlock(&H5SSD.request_lock);
  H5SSD.offset += size;
  H5SSD.mspace_left -= size*H5SSD.ppn;
  return err; 
}
void H5Fwait() {
  pthread_mutex_lock(&H5SSD.request_lock);
  while(H5SSD.num_request>0)  {
    pthread_cond_wait(&H5SSD.master_cond, &H5SSD.request_lock);
    pthread_cond_signal(&H5SSD.io_cond);
  }
  pthread_mutex_unlock(&H5SSD.request_lock);
}

herr_t H5Fclose_cache( hid_t file_id ) {
#ifdef SSD_CACHE_DEBUG
  if (H5SSD.rank==0)
    printf("SSD_CACHE: H5Fclose\n"); 
#endif
  H5Fwait();
  close(H5SSD.fd);
  H5SSD.mspace_left = H5SSD.mspace_total;
  remove(H5SSD.fname);
  return H5Fclose(file_id);
}


herr_t H5Pclose_cache(hid_t dxf_id) {
  H5Fwait();
  return H5Pclose(dxf_id);
}

herr_t H5Dclose_cache(hid_t dset_id) {
  H5Fwait();
  return H5Dclose(dset_id);
}
void test_mmap_buf() {
  thread_data_t *data = H5SSD.head; 
  while(data->next!=NULL) {
    int *p = (int*) data->buf;
    printf("Test buffer: %d\n", p[0]);
    data = data->next; 
  }
}
herr_t H5Sclose_cache(hid_t filespace) {
  H5Fwait();
  return H5Sclose(filespace);
}

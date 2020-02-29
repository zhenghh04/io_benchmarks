/*
   This is for the prototype design of using node local storage
   to improve parallel I/O performance. We modify the H5Dwrite function
   so that the data will write to the local SSD first and then the 
   background thread will take care of the data migration from 
   the local SSD to the file system. 
   
   We create a pthread for doing I/O work using a first-in-first-out 
   framework. 
   
   Huihuo Zheng <huihuo.zheng@anl.gov>
   1/24/2020
 */
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

/***********/
/* Headers */
/***********/

#include "hdf5.h"
#include "H5FDmpio.h"
#include "H5SSD.h"
/* 
   Global variable to define information related to the local storage
*/
SSD_CACHE_IO
H5SSD = {
  .mspace_total = 137438953472,
  .mspace_left = 137438953472,
  .num_request = 0,//number of I/O request
  .offset = 0, // current offset in the mmap file
  .ppn = 1, // number of proc per node
  .rank = 0, // rank id in H5F comm
  .local_rank = 0, // local rank id in a node
  .master_cond =PTHREAD_COND_INITIALIZER, // condition variable
  .io_cond = PTHREAD_COND_INITIALIZER, 
  .request_lock = PTHREAD_MUTEX_INITIALIZER,
  .write_lock = PTHREAD_MUTEX_INITIALIZER,
  .request_list = NULL,
  .current_request = NULL,
  .first_request = NULL
};

/*
  Function for set up the local storage path and capacity.
*/
int setH5SSD() {
  if (getenv("SSD_CACHE_PATH")) {
    strcpy(H5SSD.path, getenv("SSD_CACHE_PATH"));
  } else {
    strcpy(H5SSD.path, "/local/scratch/");
  }
  return 0; 
}

/*
  Function for print out debug information about current I/O task; 
*/
void check_pthread_data(thread_data_t *pt) {
  printf("********************************************\n");
  printf("***task id: %d\n", pt->id);
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

/*
  Obtain size of the buffer from the memory space and type;
*/
hsize_t get_buf_size(hid_t mspace, hid_t tid) {
  int n= H5Sget_simple_extent_ndims(mspace);
  hsize_t *dim = (hsize_t *) malloc(sizeof(hsize_t)*n); 
  hsize_t *mdim = (hsize_t *) malloc(sizeof(hsize_t)*n); 
  H5Sget_simple_extent_dims(mspace, dim, mdim);
  hsize_t s = 1;
  for(int i=0; i<n; i++) {
    s=s*dim[i];
  }
  s = s*H5Tget_size(tid);
  free(dim);
  free(mdim);
  return s;
}

/* 
   Test current memory buffer by print out the 
   first and last element of all the I/O request.
*/
void test_mmap_buf() {
  thread_data_t *data = H5SSD.first_request; 
  while((data->next!=NULL) && (data->buf!=NULL)) {
    int *p = (int*) data->buf;
    printf("Test buffer: %d, %d\n", p[0], p[data->size/sizeof(int)-1]);
    data = data->next; 
  }
}

/*
  Thread function for performing H5Dwrite. This function will create 
  a memory mapped buffer to the file that is on the local storage which 
  contains the data to be written to the file system. 

  On Theta, the memory mapped buffer currently does not work with H5Dwrite, 
  we instead allocate a buffer directly to the memory. 
*/
void *H5Dwrite_pthread_func(void *arg) {
  pthread_mutex_lock(&H5SSD.request_lock);
  while (H5SSD.num_request>=0) {
    if (H5SSD.num_request >0) {
      thread_data_t *data = H5SSD.current_request;
      data->buf = mmap(NULL, data->size, PROT_READ, MAP_SHARED, H5SSD.fd, data->offset);
      msync(data->buf, data->size, MS_SYNC);
#ifdef THETA
      char *buf = (char*) malloc(data->size);
      char *pp = (char*)data->buf; 
      for (int i=0; i<data->size; i++) buf[i] = pp[i];
      H5Dwrite(data->dataset_id, data->mem_type_id, 
	       data->mem_space_id, data->file_space_id, 
	       data->xfer_plist_id, buf);
      free(buf);
#else
      H5Dwrite(data->dataset_id, data->mem_type_id, 
	       data->mem_space_id, data->file_space_id, 
	       data->xfer_plist_id, data->buf);
#endif
      munmap(data->buf, data->size);
      H5SSD.current_request=H5SSD.current_request->next;
      H5SSD.num_request--;
    } if (H5SSD.num_request == 0) {
      pthread_cond_signal(&H5SSD.master_cond);
      pthread_cond_wait(&H5SSD.io_cond, &H5SSD.request_lock);
    }
  }
  pthread_mutex_unlock(&H5SSD.request_lock);
  return NULL; 
}

/* 
   Create HDF5 file. Each rank will create a file on the local storage
   for temperally store the data to be written to the file system. 
   We also create a local communicator including all the processes on the node.
   A pthread is created for migrating data from the local storage to the 
   file system asynchonously. 
   
   The function return directly without waiting the I/O thread to finish 
   the I/O task. However, if the space left on the local storage is not 
   enough for storing the buffer of the current task, it will wait for the 
   I/O thread to finsh all the previous tasks.
 */
hid_t H5Fcreate_cache( const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id ) {
  int rc = pthread_create(&H5SSD.pthread, NULL, H5Dwrite_pthread_func, NULL);
  srand(time(NULL));   // Initialization, should only be called once.
  setH5SSD();
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
  sprintf(rnd, "%d", H5SSD.rank);
  strcat(H5SSD.fname, rnd);
  H5SSD.request_list = (thread_data_t*) malloc(sizeof(thread_data_t)); 
  pthread_mutex_lock(&H5SSD.request_lock);
  H5SSD.request_list->id = 0; 
  H5SSD.current_request = H5SSD.request_list; 
  H5SSD.first_request = H5SSD.request_list; 
  pthread_mutex_unlock(&H5SSD.request_lock);
  H5SSD.fd = open(H5SSD.fname,  O_RDWR | O_CREAT | O_TRUNC, 0600);
  return H5Fcreate(name, flags, fcpl_id, fapl_id);
}

/*
  This is the write function appears to the user. 
  The function arguments are the same with H5Dwrite. 
  This function writes the buffer to the local storage
  first and It will create an I/O task and add it to the task 
  lists, and then wake up the I/O thread to execute 
  the H5Dwrite function. 
*/
herr_t
H5Dwrite_cache(hid_t dataset_id, hid_t mem_type_id, hid_t mem_space_id,
	       hid_t file_space_id, hid_t dxpl_id, const void *buf) {
  //  H5Fwait();
  hsize_t size = get_buf_size(mem_space_id, mem_type_id);
  
  if (H5SSD.mspace_left < size) {
    H5Fwait();
    H5SSD.offset=0;
    H5SSD.mspace_left = H5SSD.mspace_total;
  }
  int err = pwrite(H5SSD.fd, (char*)buf, size, H5SSD.offset);
#ifdef __APPLE__
  fcntl(H5SSD.fd, F_NOCACHE, 1);
#else
  fsync(H5SSD.fd);
#endif
  H5SSD.request_list->dataset_id = dataset_id; 
  H5SSD.request_list->mem_type_id = mem_type_id;
  H5SSD.request_list->mem_space_id = mem_space_id;
  H5SSD.request_list->file_space_id =file_space_id;
  H5SSD.request_list->xfer_plist_id = dxpl_id;
  H5SSD.request_list->size = size; 
  H5SSD.request_list->offset = H5SSD.offset; 
  H5SSD.request_list->next = (thread_data_t*) malloc(sizeof(thread_data_t));
  H5SSD.request_list->next->id = H5SSD.request_list->id + 1;
  thread_data_t *data = H5SSD.request_list;   
  H5SSD.request_list = H5SSD.request_list->next;
  pthread_mutex_lock(&H5SSD.request_lock);
  H5SSD.num_request++;
  pthread_cond_signal(&H5SSD.io_cond);// wake up I/O thread rightawayx
  pthread_mutex_unlock(&H5SSD.request_lock);
  hsize_t pagesize = sysconf(_SC_PAGE_SIZE);  
  H5SSD.offset += (size/pagesize+1)*pagesize;
  H5SSD.mspace_left = H5SSD.mspace_total - H5SSD.offset*H5SSD.ppn;
  return err; 
}

/*
  Wait for the pthread to finish all the I/O request
*/
void H5Fwait() {
  pthread_mutex_lock(&H5SSD.request_lock);
  while(H5SSD.num_request>0)  {
    pthread_cond_signal(&H5SSD.io_cond);
    pthread_cond_wait(&H5SSD.master_cond, &H5SSD.request_lock);
  }
  pthread_mutex_unlock(&H5SSD.request_lock);
}

/*
  Terminate the pthread through joining
 */
void H5TerminatePthread() {
  pthread_mutex_lock(&H5SSD.request_lock);
  H5SSD.num_request=-1;
  pthread_cond_signal(&H5SSD.io_cond);
  pthread_mutex_unlock(&H5SSD.request_lock);
  pthread_join(H5SSD.pthread, NULL);
}

/* 
   Wait for the pthread to finish the work and close the file 
   and terminate the pthread, remove the files on the SSD. 
 */
herr_t H5Fclose_cache( hid_t file_id ) {
  H5Fwait();
  H5TerminatePthread();
  close(H5SSD.fd);
  remove(H5SSD.fname);
  H5SSD.mspace_left = H5SSD.mspace_total;
  return H5Fclose(file_id);
}

/* 
   Wait for pthread to finish the work and close the property
 */
herr_t H5Pclose_cache(hid_t dxf_id) {
  H5Fwait();
  return H5Pclose(dxf_id);
}

/*
  Wait for pthread to finish the work and close the dataset 
 */
herr_t H5Dclose_cache(hid_t dset_id) {
  H5Fwait();
  return H5Dclose(dset_id);
}


/*
  Wait for the pthread to finish the work and close the memory space
 */
herr_t H5Sclose_cache(hid_t filespace) {
  H5Fwait();
  return H5Sclose(filespace);
}

#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include "mpi.h"
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include <sys/statvfs.h>
#include <stdlib.h>
#include <sstream>
#include <cstring>
/****************/
/* Module Setup */
/****************/


#define H5F_FRIEND              /* suppress error about including H5Fpkg  */

/***********/
/* Headers */
/***********/

#include "hdf5.h"
#include "H5FDmpio.h"
using namespace std;
int SSD_CACHE_FD;// file descripter of SSD
char SSD_CACHE_FNAME[255]; // file name
char SSD_CACHE_PATH[255];
#ifdef THETA
double SSD_CACHE_MSPACE_TOTAL=100000000000.0; // the unit is GigaByte, I hardcoded here for now; 
double SSD_CACHE_MSPACE_LEFT=100000000000.0;
#else
double SSD_CACHE_MSPACE_TOTAL=100000000.0; // the unit is GigaByte, I hardcoded here for now; 
double SSD_CACHE_MSPACE_LEFT=100000000.0;
#endif

int SSD_CACHE_NUM_REQUEST=0;
void *SSD_CACHE_MMAP;// mmap pointer
int SSD_CACHE_OFFSET=0; // offset
int SSD_CACHE_PPN=0; // node ppn 
int SSD_CACHE_RANK=0;  // node rank 
MPI_Comm SSD_CACHE_COMM; // node communicator

// condition variables 
pthread_cond_t SSD_CACHE_MASTER_COND = PTHREAD_COND_INITIALIZER; 
pthread_cond_t SSD_CACHE_IO_COND = PTHREAD_COND_INITIALIZER; 

// declaring mutex 
pthread_mutex_t SSD_CACHE_REQUEST_LOCK = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t SSD_CACHE_REQUEST_LIST_LOCK = PTHREAD_MUTEX_INITIALIZER;

pthread_t SSD_CACHE_PTHREAD;


typedef struct _thread_data_t {
// we will use the link structure in C to build the list of I/O tasks
  hid_t dataset_id; 
  hid_t mem_type_id; 
  hid_t mem_space_id; 
  hid_t file_space_id; 
  hid_t xfer_plist_id; 
  const void *buf;
  struct _thread_data_t *next; 
} thread_data_t;
thread_data_t *SSD_CACHE_REQUEST_LIST=NULL, *SSD_CACHE_REQUEST=NULL;

void *H5Dwrite_pthread_func(void *arg) {
  pthread_mutex_lock(&SSD_CACHE_REQUEST_LOCK);
  while (SSD_CACHE_NUM_REQUEST >=0) {
    if (SSD_CACHE_NUM_REQUEST >0) {
      thread_data_t *data = SSD_CACHE_REQUEST;
      H5Dwrite(data->dataset_id, data->mem_type_id, 
	       data->mem_space_id, data->file_space_id, 
	       data->xfer_plist_id, data->buf);
      SSD_CACHE_REQUEST=SSD_CACHE_REQUEST->next; 
      SSD_CACHE_NUM_REQUEST--;
    } if (SSD_CACHE_NUM_REQUEST == 0) {
      pthread_cond_signal(&SSD_CACHE_MASTER_COND);
      pthread_cond_wait(&SSD_CACHE_IO_COND, &SSD_CACHE_REQUEST_LOCK);
    }
  }
  pthread_mutex_unlock(&SSD_CACHE_REQUEST_LOCK);
  return NULL; 
}



int rc = pthread_create(&SSD_CACHE_PTHREAD, NULL, H5Dwrite_pthread_func, NULL);

int set_SSD_PATH(void) {
#ifdef THETA
  strcpy(SSD_CACHE_PATH, "/local/scratch/");
#else
  char *ssd=getenv("SSD_CACHE_PATH");
  //  getcwd(SSD_CACHE_PATH, sizeof(SSD_CACHE_PATH));
  strcpy(SSD_CACHE_PATH, ssd);
#endif
  return 0; 
}

hid_t H5Fcreate_cache( const char *name, unsigned flags, hid_t fcpl_id, hid_t fapl_id ) {
  srand(time(NULL));   // Initialization, should only be called once.
  hid_t fd = H5Fcreate(name, flags, fcpl_id, fapl_id);
  MPI_Comm comm;
  MPI_Info info; 
  H5Pget_fapl_mpio(fapl_id, &comm, &info);
  MPI_Comm_split_type(comm, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &SSD_CACHE_COMM);
  MPI_Comm_rank(SSD_CACHE_COMM, &SSD_CACHE_RANK);
  MPI_Comm_size(SSD_CACHE_COMM, &SSD_CACHE_PPN);
  set_SSD_PATH();
  strcpy(SSD_CACHE_FNAME, SSD_CACHE_PATH);
  char rnd[255];
  sprintf(rnd, "%d", rand());
  strcat(SSD_CACHE_FNAME, rnd);
  strcat(SSD_CACHE_FNAME, "-"); 
  sprintf(rnd, "%d", SSD_CACHE_RANK);
  strcat(SSD_CACHE_FNAME, rnd);
#ifdef SSD_CACHE_DEBUG
  if (SSD_CACHE_RANK==0) {
    printf("SSD_CACHE: MPI_File_open\n"); 
  }
  printf("SSD file: %s\n", SSD_CACHE_FNAME);
#endif
  SSD_CACHE_FD = ::open(SSD_CACHE_FNAME,  O_RDWR | O_CREAT | O_TRUNC, 0600);
  SSD_CACHE_REQUEST_LIST = (thread_data_t*) malloc(sizeof(thread_data_t)); 
  pthread_mutex_lock(&SSD_CACHE_REQUEST_LOCK);
  SSD_CACHE_REQUEST = SSD_CACHE_REQUEST_LIST; 
  pthread_mutex_unlock(&SSD_CACHE_REQUEST_LOCK);
  return fd; 
}



herr_t
H5Dwrite_cache(hid_t dset_id, hid_t mem_type_id, hid_t mem_space_id,
	 hid_t file_space_id, hid_t dxpl_id, const void *buf) {
  
#ifdef SSD_CACHE_DEBUG
  if (SSD_CACHE_RANK==0)
    printf("SSD_CACHE: MPI_File_write_at_all\n"); 
#endif
  hsize_t size; 
  H5Dvlen_get_buf_size(dset_id, mem_type_id, mem_space_id, &size );

  if (SSD_CACHE_MSPACE_LEFT < size) {
#ifdef SSD_CACHE_DEBUG
    printf("SSD_CACHE_MSPACE_LEFT is not enough, waiting for previous I/O jobs to finish first\n");
#endif
    pthread_mutex_lock(&SSD_CACHE_REQUEST_LOCK);
    while(SSD_CACHE_NUM_REQUEST>0)  {
      pthread_cond_signal(&SSD_CACHE_IO_COND);
      pthread_cond_wait(&SSD_CACHE_MASTER_COND, &SSD_CACHE_REQUEST_LOCK);   
    }
    pthread_mutex_unlock(&SSD_CACHE_REQUEST_LOCK);
    SSD_CACHE_OFFSET=0;
    SSD_CACHE_MSPACE_LEFT = SSD_CACHE_MSPACE_TOTAL; 
  }
  ::pwrite(SSD_CACHE_FD, (char*)buf, size, SSD_CACHE_OFFSET); 
  ::fsync(SSD_CACHE_FD);

  SSD_CACHE_REQUEST_LIST->dataset_id = dset_id; 
  SSD_CACHE_REQUEST_LIST->mem_type_id = mem_type_id;
  SSD_CACHE_REQUEST_LIST->mem_space_id = mem_space_id;
  SSD_CACHE_REQUEST_LIST->file_space_id = file_space_id;
  SSD_CACHE_REQUEST_LIST->xfer_plist_id = dxpl_id;
  // map the buf
  SSD_CACHE_MMAP = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, SSD_CACHE_FD, SSD_CACHE_OFFSET);
  msync(SSD_CACHE_MMAP, size, MS_SYNC); 
  SSD_CACHE_REQUEST_LIST->buf = SSD_CACHE_MMAP; 

  SSD_CACHE_REQUEST_LIST->next = (thread_data_t*) malloc(sizeof(thread_data_t)); 
  SSD_CACHE_REQUEST_LIST = SSD_CACHE_REQUEST_LIST->next; 
  pthread_mutex_lock(&SSD_CACHE_REQUEST_LOCK);
  SSD_CACHE_NUM_REQUEST++;
  pthread_cond_signal(&SSD_CACHE_IO_COND);// wake up I/O thread rightawayn
  pthread_mutex_unlock(&SSD_CACHE_REQUEST_LOCK);
  SSD_CACHE_OFFSET += size;
  // compute how much space left
  SSD_CACHE_MSPACE_LEFT -= size*SSD_CACHE_PPN;
}



herr_t H5Fclose_cache( hid_t file_id ) {
#ifdef SSD_CACHE_DEBUG
  if (SSD_CACHE_RANK==0)
    printf("SSD_CACHE: MPI_File_close\n"); 
#endif
  pthread_mutex_lock(&SSD_CACHE_REQUEST_LOCK);
  while(SSD_CACHE_NUM_REQUEST>0)  {
    pthread_cond_signal(&SSD_CACHE_IO_COND); 
    pthread_cond_wait(&SSD_CACHE_MASTER_COND, &SSD_CACHE_REQUEST_LOCK); 
  }
  pthread_mutex_unlock(&SSD_CACHE_REQUEST_LOCK);
  close(SSD_CACHE_FD);
  SSD_CACHE_MSPACE_LEFT = SSD_CACHE_MSPACE_TOTAL; 
  remove(SSD_CACHE_FNAME);
  return H5Fclose(file_id);
}

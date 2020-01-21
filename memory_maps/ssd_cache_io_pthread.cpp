
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include "mpi.h"
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include <stdio.h>
#include "stat.h"
#include "ssd_cache_io.h"
#include <sys/statvfs.h>
#include <stdlib.h>
#include <sstream>
#include <cstring>
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
pthread_mutex_t SSD_CACHE_MASTER_LOCK = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t SSD_CACHE_IO_LOCK = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t SSD_CACHE_OFFSET_LOCK = PTHREAD_MUTEX_INITIALIZER;

/* create thread argument struct for thr_func() 
    This is the function interface.
*/
#define WRITE 0
#define WRITE_AT 1
#define WRITE_AT_ALL 2

typedef struct _thread_data_t {
// we will use the link structure in C to build the list of I/O tasks
  MPI_File fd; 
  const void *buf;
  int count;
  MPI_Datatype datatype;
  MPI_Status *status;
  int func;
  MPI_Offset offset; 
  struct _thread_data_t *next; 
} thread_data_t;
/* SSD_CACHE_REQUEST_LIST is for storing the list of I/O tasks; SSD_CACHE_REQUEST is a pointer, pointing to the current task. This will be modified only by the I/O thread.
*/
thread_data_t *SSD_CACHE_REQUEST_LIST=NULL, *SSD_CACHE_REQUEST=NULL;

/* thread function */
void *pthread_write_func(void *arg) {
  pthread_mutex_lock(&SSD_CACHE_REQUEST_LOCK);
  while (SSD_CACHE_NUM_REQUEST >=0) {
    if (SSD_CACHE_NUM_REQUEST >0) {
      thread_data_t *data = SSD_CACHE_REQUEST;
      if (data->func == WRITE) 
        PMPI_File_write(data->fd, data->buf, data->count, data->datatype, data->status);
      else if (data->func == WRITE_AT)
        PMPI_File_write_at(data->fd, data->offset, data->buf, data->count, data->datatype, data->status);
      else if (data->func == WRITE_AT_ALL)
        PMPI_File_write_at_all(data->fd, data->offset, data->buf, data->count, data->datatype, data->status);
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
  
// create a global pthread
// creating p threads
pthread_t SSD_CACHE_PTHREAD;
int rc = pthread_create(&SSD_CACHE_PTHREAD, NULL, pthread_write_func, NULL);

using namespace std; 

template <typename T>
string itoa ( T Number)
{
  ostringstream ss;
  ss << Number;
  return ss.str(); 
}

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

int MPI_File_open_cache(MPI_Comm comm, const char *filename,
		  int amode, MPI_Info info,
		  MPI_File *fh) {
  srand(time(NULL));   // Initialization, should only be called once.
  int ierr = PMPI_File_open(comm, filename, amode, info, fh);
  MPI_Comm_split_type(comm, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &SSD_CACHE_COMM);
  MPI_Comm_rank(SSD_CACHE_COMM, &SSD_CACHE_RANK);
  MPI_Comm_size(SSD_CACHE_COMM, &SSD_CACHE_PPN);
  set_SSD_PATH();
  strcpy(SSD_CACHE_FNAME, SSD_CACHE_PATH);
  strcat(SSD_CACHE_FNAME, itoa(rand()).c_str());
  strcat(SSD_CACHE_FNAME, "-"); 
  strcat(SSD_CACHE_FNAME, itoa(SSD_CACHE_RANK).c_str());
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
  return ierr; 
}

int MPI_File_write_at_all_cache(MPI_File fh, MPI_Offset offset,
			  const void *buf, int count,
			  MPI_Datatype datatype,
			  MPI_Status *status) {
#ifdef SSD_CACHE_DEBUG
  if (SSD_CACHE_RANK==0)
    printf("SSD_CACHE: MPI_File_write_at_all\n"); 
#endif

  int dt_size; 
  MPI_Type_size(datatype, &dt_size);
  size_t size = dt_size*count; 
  if (SSD_CACHE_MSPACE_LEFT < size) {
#ifdef SSD_CACHE_DEBUG
    printf("SSD_CACHE_MSPACE_LEFT is not enough, waiting for previous I/O jobs to finish first\n");
#endif
    pthread_mutex_lock(&SSD_CACHE_REQUEST_LOCK);
    while(SSD_CACHE_NUM_REQUEST>0)  {
      pthread_cond_signal(&SSD_CACHE_IO_COND);
      pthread_cond_wait(&SSD_CACHE_MASTER_COND, &SSD_CACHE_REQUEST_LOCK);     }
    pthread_mutex_unlock(&SSD_CACHE_REQUEST_LOCK);
    SSD_CACHE_OFFSET=0;
    SSD_CACHE_MSPACE_LEFT = SSD_CACHE_MSPACE_TOTAL; 
  }
  printf("Write SSD data\n"); 
  ::pwrite(SSD_CACHE_FD, (char*)buf, size, SSD_CACHE_OFFSET); 
  ::fsync(SSD_CACHE_FD);
  printf("Done Write SSD data\n"); 
  SSD_CACHE_MMAP = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, SSD_CACHE_FD, SSD_CACHE_OFFSET);
  msync(SSD_CACHE_MMAP, size, MS_SYNC); 
  // Adding request ...
  SSD_CACHE_REQUEST_LIST->fd = fh;
  SSD_CACHE_REQUEST_LIST->offset = offset; 
  SSD_CACHE_REQUEST_LIST->buf = buf;
  SSD_CACHE_REQUEST_LIST->count = count;
  SSD_CACHE_REQUEST_LIST->datatype = datatype;
  SSD_CACHE_REQUEST_LIST->func = WRITE_AT_ALL; 
  SSD_CACHE_REQUEST_LIST->status = status; 
  SSD_CACHE_REQUEST_LIST->next = (thread_data_t*) malloc(sizeof(thread_data_t)); 
  SSD_CACHE_REQUEST_LIST = SSD_CACHE_REQUEST_LIST->next; 
  pthread_mutex_lock(&SSD_CACHE_REQUEST_LOCK);
  SSD_CACHE_NUM_REQUEST++;
  pthread_cond_signal(&SSD_CACHE_IO_COND);// wake up I/O thread rightawayn
  pthread_mutex_unlock(&SSD_CACHE_REQUEST_LOCK);
  SSD_CACHE_OFFSET += size;
  // compute how much space left
  SSD_CACHE_MSPACE_LEFT -= size*SSD_CACHE_PPN;

  return 0; 
}



int MPI_File_write_at_cache(MPI_File fh, MPI_Offset offset,
		      const void *buf, int count,
		      MPI_Datatype datatype,
		      MPI_Status *status) {
#ifdef SSD_CACHE_DEBUG
  if (SSD_CACHE_RANK==0)
    printf("SSD_CACHE: MPI_File_write_at\n"); 
#endif
  
  int dt_size; 
  MPI_Type_size(datatype, &dt_size);
  size_t size = dt_size*count;
  if (SSD_CACHE_MSPACE_LEFT < size) {
#ifdef SSD_CACHE_DEBUG
    printf("SSD_CACHE_MSPACE_LEFT is not enough, waiting for previous I/O jobs to finish first\n");
#endif
    pthread_mutex_lock(&SSD_CACHE_REQUEST_LOCK);
    while(SSD_CACHE_NUM_REQUEST>0)  {
      pthread_cond_signal(&SSD_CACHE_IO_COND);
      pthread_cond_wait(&SSD_CACHE_MASTER_COND, &SSD_CACHE_REQUEST_LOCK);     }
    pthread_mutex_unlock(&SSD_CACHE_REQUEST_LOCK);
    SSD_CACHE_OFFSET=0;
    SSD_CACHE_MSPACE_LEFT = SSD_CACHE_MSPACE_TOTAL; 
  }
  if (SSD_CACHE_COMM !=NULL) 
    ::pwrite(SSD_CACHE_FD, (char*)buf, size, SSD_CACHE_OFFSET);
  fsync(SSD_CACHE_FD);
  SSD_CACHE_MMAP = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, SSD_CACHE_FD, SSD_CACHE_OFFSET);
  msync(SSD_CACHE_MMAP, size, MS_SYNC); 
  SSD_CACHE_REQUEST_LIST->fd = fh;
  SSD_CACHE_REQUEST_LIST->offset = offset; 
  SSD_CACHE_REQUEST_LIST->buf = buf;
  SSD_CACHE_REQUEST_LIST->count = count;
  SSD_CACHE_REQUEST_LIST->datatype = datatype;
  SSD_CACHE_REQUEST_LIST->func = WRITE_AT; 
  SSD_CACHE_REQUEST_LIST->status = status; 
  SSD_CACHE_REQUEST_LIST->next = (thread_data_t*) malloc(sizeof(thread_data_t)); 
  SSD_CACHE_REQUEST_LIST = SSD_CACHE_REQUEST_LIST->next; 
  
  pthread_mutex_lock(&SSD_CACHE_REQUEST_LOCK);
  SSD_CACHE_NUM_REQUEST++;
  pthread_cond_signal(&SSD_CACHE_IO_COND);// wake up I/O thread rightaway
  pthread_mutex_unlock(&SSD_CACHE_REQUEST_LOCK);
  SSD_CACHE_OFFSET += size;
  SSD_CACHE_MSPACE_LEFT -= size*SSD_CACHE_PPN;
  // compute how much space left
  return 0; 
}


    
int MPI_File_write_cache(MPI_File fh,
		   const void *buf, int count,
		   MPI_Datatype datatype,
		   MPI_Status *status) {
#ifdef SSD_CACHE_DEBUG
  if (SSD_CACHE_RANK==0)
    printf("SSD_CACHE: MPI_File_write\n"); 
#endif
  int dt_size; 
  MPI_Type_size(datatype, &dt_size);
  size_t size = dt_size*count;
  struct statvfs st;
  statvfs(SSD_CACHE_PATH, &st);
  SSD_CACHE_MSPACE_LEFT = st.f_bsize * st.f_bfree;
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
  }
  
  ::pwrite(SSD_CACHE_FD, (char*)buf, size, SSD_CACHE_OFFSET);
  fsync(SSD_CACHE_FD);
  SSD_CACHE_MMAP = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, SSD_CACHE_FD, SSD_CACHE_OFFSET);
  msync(SSD_CACHE_MMAP, size, MS_SYNC); 
  fsync(SSD_CACHE_FD);
  SSD_CACHE_REQUEST_LIST->fd = fh;
  SSD_CACHE_REQUEST_LIST->buf = buf;
  SSD_CACHE_REQUEST_LIST->count = count;
  SSD_CACHE_REQUEST_LIST->datatype = datatype;
  SSD_CACHE_REQUEST_LIST->func = WRITE; 
  SSD_CACHE_REQUEST_LIST->status = status; 
  SSD_CACHE_REQUEST_LIST->next = (thread_data_t*) malloc(sizeof(thread_data_t)); 
  SSD_CACHE_REQUEST_LIST = SSD_CACHE_REQUEST_LIST->next; 
  pthread_mutex_lock(&SSD_CACHE_REQUEST_LOCK);
  SSD_CACHE_NUM_REQUEST++;
  pthread_cond_signal(&SSD_CACHE_IO_COND);// wake up I/O thread rightaway
  pthread_mutex_unlock(&SSD_CACHE_REQUEST_LOCK);
  SSD_CACHE_OFFSET += size;
  SSD_CACHE_MSPACE_LEFT -= size*SSD_CACHE_PPN;
  // compute how much space left
  return 0; 
}

int MPI_File_close_cache(MPI_File *fh) {
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
  return PMPI_File_close(fh); 
}

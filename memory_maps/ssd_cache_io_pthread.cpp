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

#define WRITE 0
#define WRITE_AT 1
#define WRITE_AT_ALL 2

SSD_CACHE_IO
SSD = {
       .fd = 0,
       .mspace_total = 12800000000,
       .mspace_left = 12800000000,
       .num_request = 0,
       .offset = 0,
       .ppn = 1,
       .rank = 0,
       .local_rank = 0,
       .mmap_ptr = NULL,
       .request_lock = PTHREAD_MUTEX_INITIALIZER,
       .request_list = NULL,
       .current_request = NULL,
       .first_request = NULL
};

/* thread function */
void *pthread_write_func(void *arg) {
  while (SSD.num_request >=0) {
    if (SSD.num_request >0) {
      thread_data_t *data = SSD.current_request;
      printf("number of request: %d\n", SSD.num_request);
      printf("request id: %d\n", data->id); 
      if (data->func == WRITE) 
        PMPI_File_write(data->fd, data->buf, data->count, data->datatype, data->status);
      else if (data->func == WRITE_AT)
        PMPI_File_write_at(data->fd, data->offset, data->buf, data->count, data->datatype, data->status);
      else if (data->func == WRITE_AT_ALL)
        PMPI_File_write_at_all(data->fd, data->offset, data->buf, data->count, data->datatype, data->status);
      SSD.current_request=SSD.current_request->next;
      pthread_mutex_lock(&SSD.request_lock);
      SSD.num_request--;
      pthread_mutex_unlock(&SSD.request_lock);
    } if (SSD.num_request == 0) {
      pthread_cond_signal(&SSD.master_cond);
      pthread_mutex_lock(&SSD.request_lock);
      pthread_cond_wait(&SSD.io_cond, &SSD.request_lock);
      pthread_mutex_unlock(&SSD.request_lock);
    }
  }
  pthread_exit(NULL);
  return NULL; 
}
  
// create a global pthread
// creating p threads


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
  strcpy(SSD.path, ssd);
#endif
  return 0; 
}

int MPI_File_open_cache(MPI_Comm comm, const char *filename,
		  int amode, MPI_Info info,
		  MPI_File *fh) {
  int rc = pthread_create(&SSD.pthread, NULL, pthread_write_func, NULL);
  srand(time(NULL));   // Initialization, should only be called once.
  int ierr = PMPI_File_open(comm, filename, amode, info, fh);
  MPI_Comm_split_type(comm, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &SSD.comm);
  MPI_Comm_rank(comm, &SSD.rank);
  MPI_Comm_rank(SSD.comm, &SSD.local_rank);
  MPI_Comm_size(SSD.comm, &SSD.ppn);
  set_SSD_PATH();
  strcpy(SSD.fname, SSD.path);
  strcat(SSD.fname, itoa(rand()).c_str());
  strcat(SSD.fname, "-"); 
  strcat(SSD.fname, itoa(SSD.rank).c_str());
#ifdef SSD_CACHE_DEBUG
  if (SSD.rank==0) {
    printf("SSD_CACHE: MPI_File_open\n"); 
  }
  printf("SSD file: %s\n", SSD.fname);
#endif
  SSD.fd = open(SSD.fname,  O_RDWR | O_CREAT | O_TRUNC, 0600);
  SSD.request_list = (thread_data_t*) malloc(sizeof(thread_data_t));
  SSD.request_list->id = 0; 
  pthread_mutex_lock(&SSD.request_lock);
  SSD.current_request = SSD.request_list;
  SSD.first_request = SSD.request_list; 
  pthread_mutex_unlock(&SSD.request_lock);
  return ierr; 
}

int MPI_File_write_wrapper(MPI_File fh, MPI_Offset offset,
			   const void *buf, int count,
			   MPI_Datatype datatype,
			   MPI_Status *status, int wtype=WRITE_AT_ALL) {
#ifdef SSD_CACHE_DEBUG
  if (SSD.rank==0)
    printf("SSD_CACHE: MPI_File_write_at_all\n"); 
#endif
  int dt_size; 
  MPI_Type_size(datatype, &dt_size);
  size_t size = dt_size*count; 
  if (SSD.mspace_left < size) {
#ifdef SSD_CACHE_DEBUG
    printf("SSD_CACHE_MSPACE_LEFT is not enough, waiting for previous I/O jobs to finish first\n");
#endif
    MPI_pthread_wait();
    SSD.offset=0;
    SSD.mspace_left = SSD.mspace_total; 
  }
  sleep(2);
  int err = pwrite(SSD.fd, (char*)buf, size, SSD.offset); 
  fsync(SSD.fd);
  SSD.request_list->buf = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, SSD.fd, SSD.offset);
  msync(SSD.request_list->buf, size, MS_SYNC); 
  SSD.request_list->fd = fh;
  SSD.request_list->offset = offset; 
  SSD.request_list->count = count;
  SSD.request_list->datatype = datatype;
  SSD.request_list->func = wtype; 
  SSD.request_list->status = status; 
  SSD.request_list->next = (thread_data_t*) malloc(sizeof(thread_data_t));
  SSD.request_list->next->id = SSD.request_list->id + 1;
  SSD.request_list = SSD.request_list->next;
  pthread_mutex_lock(&SSD.request_lock);
  SSD.num_request++;
  pthread_mutex_unlock(&SSD.request_lock);
  pthread_cond_signal(&SSD.io_cond);// wake up I/O thread rightawayn
  SSD.offset += size;
  SSD.mspace_left -= size*SSD.ppn;
  return err; 
}



int MPI_File_write_at_cache(MPI_File fh, MPI_Offset offset,
		      const void *buf, int count,
		      MPI_Datatype datatype,
		      MPI_Status *status) {
  return MPI_File_write_wrapper(fh, offset, buf, count, datatype, status, WRITE_AT); 
}


    
int MPI_File_write_cache(MPI_File fh,
		   const void *buf, int count,
		   MPI_Datatype datatype,
		   MPI_Status *status) {
  int offset; // this is just dummy variable
  return MPI_File_write_wrapper(fh, offset, buf, count, datatype, status, WRITE); 
}

int MPI_File_write_at_all_cache(MPI_File fh, MPI_Offset offset,
		   const void *buf, int count,
		   MPI_Datatype datatype,
		   MPI_Status *status) {
  return MPI_File_write_wrapper(fh, offset, buf, count, datatype, status, WRITE_AT_ALL); 
}


int MPI_File_close_cache(MPI_File *fh) {
#ifdef SSD_CACHE_DEBUG
  if (SSD.rank==0)
    printf("SSD_CACHE: MPI_File_close\n"); 
#endif
  MPI_pthread_wait();
  printf("pthread_wait finished\n");
  MPI_pthread_join();
  printf("pthread_wait joined\n");
  close(SSD.fd);
  SSD.mspace_left= SSD.mspace_total;
  remove(SSD.fname);
  return PMPI_File_close(fh); 
}
void MPI_pthread_wait() {
  //  pthread_mutex_lock(&SSD.request_lock);
  printf("locked request_lock\n");
  while(SSD.num_request>0)  {
    pthread_cond_signal(&SSD.io_cond);
    pthread_cond_wait(&SSD.master_cond, &SSD.request_lock); 
  }
  //pthread_mutex_unlock(&SSD.request_lock);
}

void MPI_pthread_join() {
  pthread_mutex_lock(&SSD.request_lock);
  SSD.num_request=-1;
  pthread_cond_signal(&SSD.io_cond); 
  pthread_mutex_unlock(&SSD.request_lock);
  pthread_join(SSD.pthread, NULL);
}

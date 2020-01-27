#ifndef SSD_CACHE_IO_H_
#define SSD_CACHE_IO_H_
#include "mpi.h"
int MPI_File_open_cache(MPI_Comm comm, const char *filename,
		  int amode, MPI_Info info,
		  MPI_File *fh);
int MPI_File_write_at_all_cache(MPI_File fh, MPI_Offset offset,
			  const void *buf, int count,
			  MPI_Datatype datatype,
			  MPI_Status *status);
int MPI_File_write_at_cache(MPI_File fh, MPI_Offset offset,
		      const void *buf, int count,
		      MPI_Datatype datatype,
		      MPI_Status *status);
int MPI_File_write_cache(MPI_File fh, const void *buf,
		   int count, MPI_Datatype datatype,
		   MPI_Status *status); 
int MPI_File_close_cache(MPI_File *fh);


typedef struct _thread_data_t {
// we will use the link structure in C to build the list of I/O tasks
  MPI_File fd; 
  void *buf;
  int count;
  MPI_Datatype datatype;
  MPI_Status *status;
  int func;
  MPI_Offset offset;
  int id; 
  struct _thread_data_t *next; 
} thread_data_t;

typedef struct _SSD_CACHE_IO {
  int fd;
  char fname[255];
  char path[255];
  double mspace_total;
  double mspace_left;
  int num_request;
  size_t offset;
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

void MPI_pthread_wait();
void MPI_pthread_join();
#endif

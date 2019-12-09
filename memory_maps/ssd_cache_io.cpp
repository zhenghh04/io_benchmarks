#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include "time.h"
#include "mpi.h"
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "stat.h"
#include "ssd_cache_io.h"
#include <sys/statvfs.h>
#include <stdlib.h>
#include <sstream>

#ifndef SSD_CACHE_PARAMETERS
#define SSD_CACHE_PARAMETERS
#define SSD_CACHE_MAXITER 10000
int SSD_CACHE_FD;
char SSD_CACHE_FNAME[255];
char* SSD_CACHE_PATH="./SSD/"; // we will change this into environmental variables
int SSD_CACHE_MSPACE=0; // the unit is GigaByte, I hardcoded here for now; 
int SSD_CACHE_MSPACE_LEFT=0;
int SSD_CACHE_ITER=0;
void *SSD_CACHE_MMAP;
int SSD_CACHE_OFFSET=0;
int SSD_CACHE_PPN=0;
int SSD_CACHE_RANK=0; 
MPI_Comm SSD_CACHE_COMM; 
MPI_Request SSD_CACHE_REQUEST[SSD_CACHE_MAXITER];
MPI_Status SSD_CACHE_STATUS[SSD_CACHE_MAXITER];
#endif
using namespace std; 
template <typename T>
string itoa ( T Number)
{
  ostringstream ss;
  ss << Number;
  return ss.str(); 
}

int MPI_File_open_cache(MPI_Comm comm, const char *filename,
		  int amode, MPI_Info info,
		  MPI_File *fh) {
  srand(time(NULL));   // Initialization, should only be called once.
  int ierr = MPI_File_open(comm, filename, amode, info, fh);
  MPI_Comm comm_t;
  if (SSD_CACHE_PPN == 0) {
    MPI_Comm_dup(comm, &comm_t); 
    MPI_Comm_split_type(comm_t, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &SSD_CACHE_COMM);
    MPI_Comm_rank(SSD_CACHE_COMM, &SSD_CACHE_RANK);
    MPI_Comm_size(SSD_CACHE_COMM, &SSD_CACHE_PPN);
  }
  strcpy(SSD_CACHE_FNAME, SSD_CACHE_PATH);
  strcat(SSD_CACHE_FNAME, itoa(rand()).c_str());
  strcat(SSD_CACHE_FNAME, "-"); 
  strcat(SSD_CACHE_FNAME, itoa(SSD_CACHE_RANK).c_str());
  cout << SSD_CACHE_FNAME << endl; 
  if (SSD_CACHE_COMM !=NULL) {
    SSD_CACHE_FD = open(SSD_CACHE_FNAME,  O_RDWR | O_CREAT | O_TRUNC, 0600);
  }
  struct statvfs st;
  statvfs(SSD_CACHE_PATH, &st);
  SSD_CACHE_MSPACE_LEFT = st.f_bsize * st.f_bfree;
  cout << SSD_CACHE_MSPACE_LEFT << endl; 
  return ierr; 
}

int MPI_File_write_at_all_cache(MPI_File fh, MPI_Offset offset,
			  const void *buf, int count,
			  MPI_Datatype datatype,
			  MPI_Status *status) {

  int dt_size; 
  MPI_Type_size(datatype, &dt_size);

  size_t size = dt_size*count;
  struct statvfs st;
  statvfs(SSD_CACHE_PATH, &st);
  SSD_CACHE_MSPACE_LEFT = st.f_bsize * st.f_bfree;
  cout << SSD_CACHE_MSPACE_LEFT << endl; 
  if (SSD_CACHE_MSPACE_LEFT < float(size)*SSD_CACHE_PPN) {
    MPI_Waitall(SSD_CACHE_ITER, SSD_CACHE_REQUEST, SSD_CACHE_STATUS);
    SSD_CACHE_ITER = 0;
    SSD_CACHE_OFFSET = 0;
  }
  if (SSD_CACHE_COMM !=NULL) 
    ::pwrite(SSD_CACHE_FD, (char*)buf, size, SSD_CACHE_OFFSET);
  SSD_CACHE_MMAP = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, SSD_CACHE_FD, SSD_CACHE_OFFSET);
  msync(SSD_CACHE_MMAP, size, MS_SYNC); 
  char *p = (char*) SSD_CACHE_MMAP; 
  int ierr = MPI_File_iwrite_at_all(fh, offset,
				    SSD_CACHE_MMAP,
				    size, datatype,
				    &SSD_CACHE_REQUEST[SSD_CACHE_ITER]);
  SSD_CACHE_ITER++;
  SSD_CACHE_OFFSET += size;
  // compute how much space left
  return ierr; 
}

int MPI_File_write_at_cache(MPI_File fh, MPI_Offset offset,
		      const void *buf, int count,
		      MPI_Datatype datatype,
		      MPI_Status *status) {
  struct statvfs st;
  int dt_size; 
  MPI_Type_size(datatype, &dt_size);
  size_t size = dt_size*count;
  statvfs(SSD_CACHE_PATH, &st);
  SSD_CACHE_MSPACE_LEFT = st.f_bsize * st.f_bfree; 
  if (SSD_CACHE_MSPACE_LEFT < float(size)*SSD_CACHE_PPN) {
    MPI_Waitall(SSD_CACHE_ITER, SSD_CACHE_REQUEST, SSD_CACHE_STATUS);
    SSD_CACHE_ITER = 0;
    SSD_CACHE_OFFSET = 0;
  }
  if (SSD_CACHE_COMM !=NULL) 
    ::pwrite(SSD_CACHE_FD, (char*)buf, size, SSD_CACHE_OFFSET);
  SSD_CACHE_MMAP = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, SSD_CACHE_FD, SSD_CACHE_OFFSET);
  msync(SSD_CACHE_MMAP, size, MS_SYNC); 
  char *p = (char*) SSD_CACHE_MMAP; 
  int ierr = MPI_File_iwrite_at(fh, offset,
				SSD_CACHE_MMAP,
				size, datatype,
				&SSD_CACHE_REQUEST[SSD_CACHE_ITER]);
  SSD_CACHE_ITER++;
  SSD_CACHE_OFFSET+= size;
  return ierr; 
}

int MPI_File_write_cache(MPI_File fh, const void *buf,
		   int count, MPI_Datatype datatype,
		   MPI_Status *status) {
  struct statvfs st;
  int dt_size; 
  MPI_Type_size(datatype, &dt_size);
  size_t size = dt_size*count;
  statvfs(SSD_CACHE_PATH, &st);
  SSD_CACHE_MSPACE_LEFT = st.f_bsize * st.f_bfree;
  cout << SSD_CACHE_MSPACE_LEFT << endl; 
  if (SSD_CACHE_MSPACE_LEFT < float(size)*SSD_CACHE_PPN) {
    MPI_Waitall(SSD_CACHE_ITER, SSD_CACHE_REQUEST, SSD_CACHE_STATUS);
    SSD_CACHE_ITER = 0;
    SSD_CACHE_OFFSET = 0;
  }
  if (SSD_CACHE_COMM !=NULL) 
    ::pwrite(SSD_CACHE_FD, (char*)buf, size, SSD_CACHE_OFFSET);
  SSD_CACHE_MMAP = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, SSD_CACHE_FD, SSD_CACHE_OFFSET);
  msync(SSD_CACHE_MMAP, size, MS_SYNC); 
  char *p = (char*) SSD_CACHE_MMAP; 
  int ierr = MPI_File_iwrite(fh, SSD_CACHE_MMAP,
			     size, datatype,
			     &SSD_CACHE_REQUEST[SSD_CACHE_ITER]);
  SSD_CACHE_ITER++;
  SSD_CACHE_OFFSET += size;
  return ierr; 
}

int MPI_File_close_cache(MPI_File *fh) {
  if (SSD_CACHE_ITER > 0) {
    MPI_Waitall(SSD_CACHE_ITER, SSD_CACHE_REQUEST, SSD_CACHE_STATUS);
    SSD_CACHE_ITER = 0;
    SSD_CACHE_OFFSET = 0;
    SSD_CACHE_MSPACE_LEFT=SSD_CACHE_MSPACE;
  }
  close(SSD_CACHE_FD);
  remove(SSD_CACHE_FNAME);
  SSD_CACHE_PPN = 0; 
  MPI_File_close(fh); 
}

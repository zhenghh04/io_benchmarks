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

// global variables for SSD_CACHE_IO
#ifndef SSD_CACHE_PARAMETERS
#define SSD_CACHE_PARAMETERS
#define SSD_CACHE_MAXITER 10000
int SSD_CACHE_FD;
char SSD_CACHE_FNAME[255];
char* SSD_CACHE_PATH="/local/scratch"; // we will change this into environmental variables
size_t SSD_CACHE_MSPACE; // the unit is GigaByte, I hardcoded here for now; 
size_t SSD_CACHE_MSPACE_LEFT;
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

int MPI_File_open(MPI_Comm comm, const char *filename,
		  int amode, MPI_Info info,
		  MPI_File *fh) {
// Open the file on the file systems
  int ierr = PMPI_File_open(comm, filename, amode, info, fh);

  if (SSD_CACHE_PPN == 0) {
    MPI_Comm_split_type(comm, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL, &SSD_CACHE_COMM);
    MPI_Comm_rank(SSD_CACHE_COMM, &SSD_CACHE_RANK);
    MPI_Comm_size(SSD_CACHE_COMM, &SSD_CACHE_PPN);
  }

// Open tmp. files on SSD (file per ranks)
  srand(time(NULL));   // Initialization, should only be called once.
  strcpy(SSD_CACHE_FNAME, SSD_CACHE_PATH);
  strcat(SSD_CACHE_FNAME, itoa(rand()).c_str());
  strcat(SSD_CACHE_FNAME, "-"); 
  strcat(SSD_CACHE_FNAME, itoa(SSD_CACHE_RANK).c_str());
  if (SSD_CACHE_COMM !=NULL) {
    SSD_CACHE_FD = open(SSD_CACHE_FNAME,  O_RDWR | O_CREAT | O_TRUNC, 0600);
  }
// check the space left. 
  struct statvfs st;
  statvfs(SSD_CACHE_PATH, &st);
  SSD_CACHE_MSPACE_LEFT = st.f_bsize * st.f_bfree;
  return ierr; 
}

int MPI_File_write_at_all(MPI_File fh, MPI_Offset offset,
			  const void *buf, int count,
			  MPI_Datatype datatype,
			  MPI_Status *status) {
// Compute the size of the local buffer
  int dt_size; 
  MPI_Type_size(datatype, &dt_size);
  size_t size = dt_size*count;

// check whether the space left on SSD is enough, if yet, proceed, otherwise, place a wait for previous writes to finish. 
  struct statvfs st;
  statvfs(SSD_CACHE_PATH, &st);
  if (SSD_CACHE_MSPACE_LEFT < float(size)*SSD_CACHE_PPN) {
    MPI_Waitall(SSD_CACHE_ITER, SSD_CACHE_REQUEST, SSD_CACHE_STATUS);
    SSD_CACHE_ITER = 0;
    SSD_CACHE_OFFSET = 0;
  }

// Write local buffer to SSD and create a memory map file
  if (SSD_CACHE_COMM !=NULL) 
    ::pwrite(SSD_CACHE_FD, (char*)buf, size, SSD_CACHE_OFFSET);
  SSD_CACHE_MMAP = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, SSD_CACHE_FD, SSD_CACHE_OFFSET);
  msync(SSD_CACHE_MMAP, size, MS_SYNC); 
  char *p = (char*) SSD_CACHE_MMAP; 

// Asynchonously write memory mapped buffers to file systems.
  int ierr = MPI_File_iwrite_at_all(fh, offset,
				    SSD_CACHE_MMAP,
				    size, datatype,
				    &SSD_CACHE_REQUEST[SSD_CACHE_ITER]);
  SSD_CACHE_ITER++;
  SSD_CACHE_OFFSET += size;
// compute how much space left
  return ierr; 
}

int MPI_File_write_at(MPI_File fh, MPI_Offset offset,
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

int MPI_File_write(MPI_File fh, const void *buf,
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

int MPI_File_close(MPI_File *fh) {
// Wait until all the files have been write to the filesystems
  if (SSD_CACHE_ITER > 0) {
    MPI_Waitall(SSD_CACHE_ITER, SSD_CACHE_REQUEST, SSD_CACHE_STATUS);
    SSD_CACHE_ITER = 0;
    SSD_CACHE_OFFSET = 0;
    SSD_CACHE_MSPACE_LEFT=SSD_CACHE_MSPACE;
  }
// close the files on SSD
  close(SSD_CACHE_FD);
  remove(SSD_CACHE_FNAME);
  SSD_CACHE_PPN = 0; 
// close the file 
  PMPI_File_close(fh); 
}

/* How to use the program: 
  Case A. 
  for (int iter = 0; iter < niter; iter++) {
    MPI_File_open();
  
    MPI_File_write_at_all( *buf); 
  
    ... perform other works related to *buf ...  // important for overlapping the compute with I/O
  
    MPI_File_close();
  }
 
  Case B. 
  MPI_File_open();
  for (int iter = 0; iter < niter; iter++) {
    MPI_File_write_at_all( *buf); 
  
    ... perform other works related to *buf ... 
    
  }
  MPI_File_close();
*/
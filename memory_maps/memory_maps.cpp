/* 
   This benchmark is to understand how much performance can we gain for collective I/O if we use 
   node-local SSD as a Cache to the lustre file system. 
   Huihuo Zheng <huihuo.zheng@anl.gov>
 */
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
using namespace std; 

#include <sstream>
struct IOT
{
  float open=0.0; 
  float raw=0.0;
  float close=0.0; 
  float wait=0.0; 
  int rep=0;
};

template <typename T>
string itoa ( T Number)
{
  std::ostringstream ss;
  ss << Number;
  return ss.str(); 
}

int fail(char *filename, int linenumber) { 
  fprintf(stderr, "%s:%d %s\n", filename, linenumber, strerror(errno)); 
  exit(1);
  return 0; /*Make compiler happy */
}
#define QUIT fail(__FILE__, __LINE__ )

int main(int argc, char *argv[]) {
  int i=1;
  int dim=1024*1024*2;
  int niter=1;
  char *ssd = "/local/scratch/";
  char *lustre="./scratch/"; 
  MPI_File handle;
  MPI_Info info = MPI_INFO_NULL;
  MPI_Status status;
  int rank, nproc; 
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);

  while (i<argc) {
    if (strcmp(argv[i], "--dim") == 0) {
      dim = atoi(argv[i+1]); 
      i+=2;

    } else if (strcmp(argv[i], "--niter") == 0) {
      niter = atoi(argv[i+1]); 
      i+=2;

    } else if (strcmp(argv[i], "--SSD") == 0) {
      ssd = argv[i+1];
      i+=2;
    } else if (strcmp(argv[i], "--lustre") == 0) {
      lustre = argv[i+1];
      i+=2;
    } else {
      i++; 
    }
  }
  long size = sizeof(int) * dim;
  MPI_Comm local_comm; 
  MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL,
			&local_comm);

  // Get local rank and world rank for cross comm establishment.
  int local_rank, ppn; 
  MPI_Comm_rank(local_comm, &local_rank);
  MPI_Comm_size(local_comm, &ppn);

  if (rank==0)  {
    printf("----------- SSD Staging test---------------\n"); 
    printf(" *           Dimension: %d\n", dim); 
    cout <<" *   Local buffer size: " << size/1024./1024. << " MB" << endl;
    printf(" *        SSD location: %s\n", ssd); 
    printf(" *     Lustre location: %s\n", lustre); 
    printf(" *     Number of iter.: %d\n", niter); 
    printf(" * Total num. of ranks: %d\n", nproc);
    printf(" *                 PPN: %d\n", ppn);
    printf("-------------------------------------------\n"); 
  }

  clock_t t0, t1; 
  int *myarray = new int [dim];
  for(int i=0; i<dim; i++) {
    myarray[i] = i; 
  }

  char f1[100]; 
  strcpy(f1, lustre); 
  strcat(f1, "/file-mem2lustre.dat"); 
  IOT M2L, M2S, M2MMF, MMF2L; 
  for(int i=0; i<niter; i++) {
    t0 = clock();
    MPI_File_open(MPI_COMM_WORLD, f1, MPI_MODE_WRONLY | MPI_MODE_CREATE, info, &handle);
    t1 = clock();
    M2L.open += float(t1 - t0)/CLOCKS_PER_SEC;
    MPI_File_write_at_all(handle, rank*dim*sizeof(int), myarray, dim, MPI_INT, MPI_STATUS_IGNORE);
    t0 = clock();
    M2L.raw += float(t0 - t1)/CLOCKS_PER_SEC;
    MPI_File_close(&handle);
    t1  = clock();
    M2L.close +=  float(t1 - t0)/CLOCKS_PER_SEC;
    M2L.rep++; 
  }
  if (rank==0) {
    cout << "\n--------------- Memory to Lustre (direct) -----" << endl; 
    cout << "Open time (s): " << M2L.open/M2L.rep << endl; 
    cout << "Write time (s): " << M2L.raw/M2L.rep << endl; 
    cout << "Close time (s): " << M2L.close/M2L.rep << endl; 
    cout << "Write rate: " << size/M2L.raw/1024/1024*M2L.rep*nproc << " MB/sec" << endl;
    cout << "-----------------------------------------------" << endl; 
  }


  char fn[100]; 
  strcpy(fn, ssd); 
  strcat(fn, "/file-mem2ssd.dat"); 
  for(int i=0; i<niter; i++) {
    t0 = clock();
    MPI_File_open(MPI_COMM_WORLD, fn, MPI_MODE_WRONLY | MPI_MODE_CREATE, info, &handle);
    t1 = clock();
    M2S.open += float(t1 - t0)/CLOCKS_PER_SEC;
    MPI_File_write_at_all(handle, rank*dim*sizeof(int), myarray, dim, MPI_INT, MPI_STATUS_IGNORE);
    t0 = clock();
    M2S.raw += float(t0 - t1)/CLOCKS_PER_SEC;
    MPI_File_close(&handle);
    t1  = clock();
    M2S.close +=  float(t1 - t0)/CLOCKS_PER_SEC;
    M2S.rep++; 
  }
  if (rank==0) {
    cout << "\n--------------- Memory to SSD (direct) -----" << endl; 
    cout << "Open time (s): " << M2S.open/M2S.rep << endl;
    cout << "Write time (s): " << M2S.raw/M2S.rep << endl;

    cout << "Close time (s): " << M2S.close/M2S.rep << endl; 
    cout << "Write rate: " << size/M2S.raw/1024/1024*M2S.rep*nproc << " MB/sec" << endl;
    cout << "-----------------------------------------------" << endl; 
  }



  
  //staging time
  char *name; 
  int *resultlen; 

  ofstream myfile;
  int err;
  char f[100]; 
  strcpy(f, ssd); 
  strcat(f, "/file-"); 
  strcat(f, itoa(local_rank).c_str()); 
  strcat(f, ".dat"); 
  t0 = clock();
  int fd = open(f, O_RDWR | O_CREAT | O_TRUNC, 0600); //6 = read+write for me!
  lseek(fd, size, SEEK_SET);
  write(fd, "A", 1);
  void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == (void*) -1 ) QUIT;
  int *array = (int*) addr;
  for(int j=0; j<niter; j++) {
    for(int i=0; i<dim; i++)
      array[i] = i+j;
  }
  msync(addr, size, err);
  t1 = clock();
  M2MMF.raw += float(t1 - t0)/CLOCKS_PER_SEC;
  munmap(addr, size);
  if (rank==0) {
    cout << "\n--------------- Memory to mmap file -------" << endl; 
    cout << "Write time (s): " << M2MMF.raw/niter << endl; 
    cout << "Write Rate: " << size/M2MMF.raw/1024/1024*niter*nproc << " MB/sec" << endl;
    cout << "------------------------------------------------" << endl;
  }
#ifdef DEBUG
  printf("SSD files: %s (Rank-%d)", f, rank);
#endif
  addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  char f2[100]; 
  strcpy(f2, lustre); 
  strcat(f2, "/file-mmf2lustre.dat");
  niter =1;
  int *array2 = (int*) addr;
  MPI_Request request[niter]; 
  MPI_Status statusall[niter];
  for (int i=0; i<niter; i++) {
    t0 = clock(); 
    MPI_File_open(MPI_COMM_WORLD, f2, MPI_MODE_WRONLY | MPI_MODE_CREATE, info, &handle);
    t1 = clock();
    MMF2L.open += float(t1 - t0)/CLOCKS_PER_SEC; 
    MPI_File_iwrite_at_all(handle, rank*dim*sizeof(int), array2, dim, MPI_INT, &request[i]);
    t0 = clock();
    MMF2L.raw += float(t0 - t1)/CLOCKS_PER_SEC; 
    MPI_Waitall(niter, request, statusall); 
    t1 = clock();
    MMF2L.wait += float(t1 - t0)/CLOCKS_PER_SEC; 
    MPI_File_close(&handle);
    t0 = clock();
    MMF2L.close += float(t0 - t1)/CLOCKS_PER_SEC; 
    MMF2L.rep ++;
  }
  if (rank==0) {
    cout << "\n--------------- memmap file to lustre -----" << endl; 
    cout << "Open time (s): " << MMF2L.open/MMF2L.rep << endl; 
    cout << "Write time (s): " << MMF2L.raw/MMF2L.rep << endl; 
    cout << "Wait time (s): " << MMF2L.wait/MMF2L.rep << endl; 
    cout << "Close time (s): " << MMF2L.close/MMF2L.rep << endl; 
    cout << "Write rate: " << size/(MMF2L.raw+MMF2L.wait)/1024/1024*MMF2L.rep*nproc << " MB/sec" << endl;
    cout << "---------------------------------------------" << endl;
  }

  
  munmap(addr, size);
  MPI_Finalize();
  return 0;
}

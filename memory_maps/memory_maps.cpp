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
#include <sys/stat.h>
#include <unistd.h>
#include "timing.h"
using namespace std; 
Timing tt; 
#include <sstream>
struct IOT
{
  float open, close, raw, wait, sync;
  int rep;
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
  bool fsync=false; 
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
    } else if (strcmp(argv[i], "--fsync")==0) {
      fsync = true; i++;
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
    printf(" *               fsync: %d\n", fsync);
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
    tt.start_clock("m2l_open");
    MPI_File_open(MPI_COMM_WORLD, f1, MPI_MODE_WRONLY | MPI_MODE_CREATE, info, &handle);
    tt.stop_clock("m2l_open");
    tt.start_clock("m2l_write"); 
    MPI_File_write_at_all(handle, rank*dim*sizeof(int), myarray, dim, MPI_INT, MPI_STATUS_IGNORE);
    tt.stop_clock("m2l_write"); 
    tt.start_clock("m2l_sync"); 
    if (fsync) MPI_File_sync(handle);
    tt.stop_clock("m2l_sync"); 
    tt.start_clock("m2l_close"); 
    MPI_File_close(&handle);
    tt.stop_clock("m2l_close"); 
  }
  M2L.open = tt["m2l_open"].t;
  M2L.raw = tt["m2l_write"].t  +tt["m2l_write"].t;
  M2L.close = tt["m2l_close"].t;
  M2L.rep = tt["m2l_open"].num_call; 
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
    tt.start_clock("m2s_open"); 
    MPI_File_open(MPI_COMM_WORLD, fn, MPI_MODE_WRONLY | MPI_MODE_CREATE, info, &handle);
    tt.stop_clock("m2s_open");
    tt.start_clock("m2s_write"); 
    MPI_File_write_at_all(handle, rank*dim*sizeof(int), myarray, dim, MPI_INT, MPI_STATUS_IGNORE);
    tt.stop_clock("m2s_write");
    tt.start_clock("m2s_sync"); 
    if (fsync) MPI_File_sync(handle);
    tt.stop_clock("m2s_sync"); 
    tt.start_clock("m2s_close"); 
    MPI_File_close(&handle);
    tt.stop_clock("m2s_close"); 
  }
  M2S.open = tt["m2s_open"].t;
  M2S.raw = tt["m2s_write"].t + tt["m2s_sync"].t; 
  M2S.close = tt["m2s_close"].t;
  M2S.rep = tt["m2s_open"].num_call; 

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
  for(int j=0; j<niter; j++) {
    int fd = open(f, O_RDWR | O_CREAT | O_TRUNC, 0600); //6 = read+write for me!
    lseek(fd, size, SEEK_SET);
    write(fd, "A", 1);
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == (void*) -1 ) QUIT;
    int *array = (int*) addr;
    tt.start_clock("m2mmf_write");
    for(int i=0; i<dim; i++)
      array[i] = i+j;
    msync(addr, size, MS_SYNC);
    if (fsync) ::fsync(fd);
    tt.stop_clock("m2mmf_write");
    close(fd); 
    munmap(addr, size);
  }
  M2MMF.raw = tt["m2mmf_write"].t; 

  if (rank==0) {
    cout << "\n--------------- Memory to mmap file -------" << endl; 
    cout << "Write time (s): " << M2MMF.raw/niter << endl; 
    cout << "Write Rate: " << size/M2MMF.raw/1024/1024*niter*nproc << " MB/sec" << endl;
    cout << "------------------------------------------------" << endl;
  }
#ifdef DEBUG
  printf("SSD files: %s (Rank-%d)", f, rank);
#endif
  int fd = open(f, O_RDWR, 0600); //6 = read+write for me!
  void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  char f2[100]; 
  strcpy(f2, lustre); 
  strcat(f2, "/file-mmf2lustre.dat");
  int *array2 = (int*) addr;
  MPI_Request request; 
  for (int i=0; i<niter; i++) {
    tt.start_clock("mmf2l_open"); 
    MPI_File_open(MPI_COMM_WORLD, f2, MPI_MODE_WRONLY | MPI_MODE_CREATE, info, &handle);
    tt.stop_clock("mmf2l_open");

    tt.start_clock("mmf2l_iwrite"); 
    MPI_File_iwrite_at_all(handle, rank*dim*sizeof(int), array2, dim, MPI_INT, &request);
    tt.stop_clock("mmf2l_iwrite");

    tt.start_clock("mmf2l_Wait"); 
    MPI_Waitall(1, &request, &status);
    tt.stop_clock("mmf2l_Wait");

    tt.start_clock("mmf2l_sync"); 
    if (fsync) MPI_File_sync(handle);
    tt.stop_clock("mmf2l_sync"); 

    tt.start_clock("mmf2l_close"); 
    MPI_File_close(&handle);
    tt.stop_clock("mmf2l_close"); 
  }
  MMF2L.open = tt["mmf2l_open"].t;
  MMF2L.raw = (tt["mmf2l_iwrite"].t + tt["mmf2l_Wait"].t + tt["mmf2l_sync"].t);
  MMF2L.close = tt["mmf2l_close"].t;
  MMF2L.rep = niter; 
  if (rank==0) {
    cout << "\n--------------- memmap file to lustre -----" << endl; 
    cout << "Open time (s): " << MMF2L.open/MMF2L.rep << endl; 
    cout << "Write time (s): " << MMF2L.raw/MMF2L.rep << endl; 
    cout << "Close time (s): " << MMF2L.close/MMF2L.rep << endl; 
    cout << "Write rate: " << size/(MMF2L.raw)/1024/1024*MMF2L.rep*nproc << " MB/sec" << endl;
    cout << "---------------------------------------------" << endl;
  }
  munmap(addr, size);
  tt.PrintTiming(rank==0);
  MPI_Finalize();
  return 0;
}

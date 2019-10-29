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
  IOT M2L, M2S, S2L; 
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
  //staging time
  char *name; 
  int *resultlen; 

  ofstream myfile;

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
  t1 = clock();
  M2S.raw += float(t1 - t0)/CLOCKS_PER_SEC;
  munmap(addr, size);
  if (rank==0) {
    cout << "\n--------------- Memory to node-local SSD -------" << endl; 
    cout << "Write time (s): " << M2S.raw/niter << endl; 
    cout << "Write Rate: " << size/M2S.raw/1024/1024*niter*nproc << " MB/sec" << endl;
    cout << "------------------------------------------------" << endl;
  }
#ifdef DEBUG
  printf("SSD files: %s (Rank-%d)", f, rank);
#endif
  addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);

  char f2[100]; 
  strcpy(f2, lustre); 
  strcat(f2, "/file-ssd2lustre.dat"); 
  int *array2 = (int*) addr;
  niter = 1;
  MPI_Request request[niter]; 
  MPI_Status statusall[niter]; 
  t0 = clock(); 
  MPI_File_open(MPI_COMM_WORLD, f2, MPI_MODE_WRONLY | MPI_MODE_CREATE, info, &handle);
  t1 = clock();
  S2L.open += float(t1 - t0)/CLOCKS_PER_SEC; 
  MPI_File_iwrite_at_all(handle, rank*dim*sizeof(int), array2, dim, MPI_INT, &request[i]);
  t0 = clock();
  S2L.raw += float(t0 - t1)/CLOCKS_PER_SEC; 
  MPI_Waitall(niter, request, statusall); 
  t1 = clock();
  S2L.wait += float(t1 - t0)/CLOCKS_PER_SEC; 
  MPI_File_close(&handle);
  t0 = clock();
  S2L.close += float(t0 - t1)/CLOCKS_PER_SEC; 
  S2L.rep = 1;
  if (rank==0) {
    cout << "\n--------------- Node-local SSD to lustre -----" << endl; 
    cout << "Open time (s): " << S2L.open/S2L.rep << endl; 
    cout << "iWrite time (s): " << S2L.raw/S2L.rep << endl; 
    cout << "Wait time (s): " << S2L.wait/S2L.rep << endl; 
    cout << "Close time (s): " << S2L.close/S2L.rep << endl; 
    cout << "Write rate: " << size/(S2L.raw+S2L.wait)/1024/1024*S2L.rep*nproc << " MB/sec" << endl;
    cout << "---------------------------------------------" << endl;
  }

  
  munmap(addr, size);
  MPI_Finalize();
  return 0;
}

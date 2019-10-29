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

  clock_t t0 = clock(); 
  int *myarray = new int [dim];
  for(int i=0; i<dim; i++) {
    myarray[i] = i; 
  }
  t0 = clock();
  char f1[100]; 
  strcpy(f1, lustre); 
  strcat(f1, "/file-mem2lustre.dat"); 
  for(int i=0; i<niter; i++) {
    MPI_File_open(MPI_COMM_WORLD, f1, MPI_MODE_WRONLY | MPI_MODE_CREATE, info, &handle);
    MPI_File_write_at_all(handle, rank*dim*sizeof(int), myarray, dim, MPI_INT, MPI_STATUS_IGNORE);
    MPI_File_close(&handle);
  }
  clock_t t1 = clock();
  if (rank==0) {
    cout << "\n--------------- Memory to Lustre (direct) -----" << endl; 
    cout << "Write Time: " << float(t1 - t0)/CLOCKS_PER_SEC/niter << " seconds" << endl;
    cout << "Write Rate: " << size/(float(t1 - t0)/CLOCKS_PER_SEC)/1024/1024*niter*nproc << " MB/sec" << endl;
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
  int fd = open(f, O_RDWR | O_CREAT | O_TRUNC, 0600); //6 = read+write for me!
  lseek(fd, size, SEEK_SET);
  write(fd, "A", 1);
  t0 = clock();
  void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == (void*) -1 ) QUIT;
  int *array = (int*) addr;
  for(int j=0; j<niter; j++) {
    clock_t t00=clock();
    for(int i=0; i<dim; i++)
      array[i] = i+j;
    clock_t t01 = clock();
    if (rank==0) cout << j <<  " - Write Time: " << float(t01 - t00)/CLOCKS_PER_SEC << endl;
  }
  t1 = clock();
  munmap(addr, size);
  if (rank==0) {
    cout << "\n--------------- Memory to node-local SSD -------" << endl; 
    cout << "Write Time: " << float(t1 - t0)/CLOCKS_PER_SEC/niter << " seconds" << endl;
    cout << "Write Rate: " << size/(float(t1 - t0)/CLOCKS_PER_SEC)/1024/1024*niter*nproc << " MB/sec" << endl;
    cout << "------------------------------------------------" << endl;
  }
#ifdef DEBUG
  printf("SSD files: %s (Rank-%d)", f, rank);
#endif
  addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  t0 = clock();
  char f2[100]; 
  strcpy(f2, lustre); 
  strcat(f2, "/file-ssd2lustre.dat"); 
  int *array2 = (int*) addr;
  MPI_Request request[niter]; 
  MPI_Status statusall[niter]; 
  for(int i=0; i<niter; i++) {
    MPI_File_open(MPI_COMM_WORLD, f2, MPI_MODE_WRONLY | MPI_MODE_CREATE, info, &handle);
    MPI_File_write_at_all(handle, rank*dim*sizeof(int), array2, dim, MPI_INT, MPI_STATUS_IGNORE);
    MPI_File_close(&handle);
  }
  t1 = clock();
  //MPI_Waitall(niter, request, statusall); 
  clock_t t2=clock();
  if (rank==0) {
    cout << "\n--------------- Node-local SSD to lustre -----" << endl; 
    //cout << "iwrite Time: " << float(t1 - t0)/CLOCKS_PER_SEC/niter << " seconds" << endl;
    //cout << "Wait Time: " << float(t2 - t1)/CLOCKS_PER_SEC/niter << " seconds" << endl;
    cout << "Write Rate: " << size/(float(t2 - t0)/CLOCKS_PER_SEC)/1024/1024*niter*nproc << " MB/sec" << endl;
    cout << "---------------------------------------------" << endl;
  }
  munmap(addr, size);
  MPI_Finalize();
  return 0;
}

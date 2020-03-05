/* 
   This is to test the idea of data loading using one sided communication. 
   Assuming that we have data on the storage device, we created a memory map
   for all the rank to the specific offset of the data. We then perform 
   MPI I/O using MPI_Get with data shuffling. 

   In reality, each rank is responsible for a specific portion of data
 */
#include "mpi.h"
#include "stdio.h"
#include <iostream>
#include <vector>
#include "assert.h"
#include "string.h"
#include "stdlib.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
using namespace std; 
int main(int argc, char **argv) {
  MPI_Win win;
  int i=0;
  int dim=1024;
  while (i < argc) {
    if (strcmp(argv[i], "--dim")==0) {
      dim = int(atof(argv[i+1]));i+=2;
    } else {i+=1;}
  }

  int rank, nproc;
  srand ( unsigned ( std::time(0) ) );
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  printf("I am rank %d of %d\n", rank, nproc);


  // This is to create the file which contains the dataset
  vector<int> lst;
  lst.resize(dim*nproc);
  for(int i=0; i<dim*nproc; i++) {
    lst[i] = i; 
  }
  char filename[255] = "test.dat";
  if (rank==0) {
    int fd = open(filename, O_WRONLY | O_CREAT | O_EXCL, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
    write(fd, (char*)&lst[0], dim*sizeof(int)*nproc);
    close(fd);
    fsync(fd);
  }
  // Create the memory mapped buffer and attach to a MPI Window
  int fd = open(filename, O_RDONLY);
  void *addr = mmap(NULL, dim*sizeof(int), PROT_READ, MAP_SHARED, fd, dim*sizeof(int)*rank);
  msync(addr, sizeof(int)*dim, MS_SYNC);
  MPI_Barrier(MPI_COMM_WORLD);
  int *data = (int*)addr;
  MPI_Win_create(data, sizeof(int)*dim, sizeof(int), MPI_INFO_NULL, MPI_COMM_WORLD, &win);
  // Reading data using MPI_Get. -- this is for random memory access. 
  random_shuffle(lst.begin(), lst.end());
  int *b = new int [dim];
  MPI_Win_fence(MPI_MODE_NOPUT, win);
  for(int i=0; i<dim; i++) {
    int dest = lst[rank*dim+i];
    int src = dest/dim;
    int disp = dest%dim;
    if (src==rank)
      MPI_Get(&b[i], 1, MPI_INT, src, disp, 1, MPI_INT, win);
    else
      b[i] = lst[rank*dim+i];
  }
  MPI_Win_fence(MPI_MODE_NOPUT, win);

  if (getenv("DEBUG")!= NULL and int(atof(getenv("DEBUG")))>0) {
    if (rank==0) {
      for(int i=0; i<min(dim, 256); i++) {
	printf("%5d: %5d(%5d)    ",i, b[i], lst[rank*dim+i]);
	if (i%5==4) printf("\n");
      }
      printf("\n");
    }
  }
  for(int i=0; i<dim; i++) {
    assert(b[i]==lst[rank*dim+i]); 
  }
  delete [] b;

  MPI_Win_free(&win);
  munmap(data, sizeof(int)*dim);
  MPI_Finalize();
  return 0;
}

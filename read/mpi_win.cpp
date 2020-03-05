/* 
   This is to test the idea of data loading using one sided communication. 
   Assuming that we have data on the storage device, we created a memory map
   for all the rank to the specific offset of the data. We then perform 
   MPI I/O using MPI_Get with data shuffling. 

   In reality, each rank is responsible for a specific portion of data. During 
   the shuffling process. One rank will read the data, and send the data around 
   to other rank. 
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
#include <algorithm>    // std::shuffle
#include <random>
#include "debug.h"

using namespace std; 
int main(int argc, char **argv) {
  MPI_Win win;
  int i=0;
  int dim=224*224*3;
  int num_images = 8192; 
  int epochs = 10;
  int num_batches = 16;
  int batch_size = 32; 
  int sz = 224; 
  while (i < argc) {
    if (strcmp(argv[i], "--sz")==0) {
      sz = int(atof(argv[i+1]));i+=2;
      dim = sz*sz*3;
    } else if (strcmp(argv[i], "--num_images")==0) {
      num_images = int(atof(argv[i+1]));i+=2;
    } else if (strcmp(argv[i], "--num_epochs")==0) {
      epochs = int(atof(argv[i+1]));i+=2;
    } else if (strcmp(argv[i], "--num_batches")==0) {
      num_batches = int(atof(argv[i+1])); i+=2;
    } else if (strcmp(argv[i], "--batch_size")==0) {
      batch_size = int(atof(argv[i+1])); i+=2; 
    } else {
      i+=1;
    }
  }

  int rank, nproc, nloc, start; 
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  num_images = num_images - num_images%nproc; 
  if (io_node()==rank) printf("* MPI setup\n"); 
  printf(" I am rank %d of %d\n", rank, nproc);
  MPI_Barrier(MPI_COMM_WORLD);
  nloc = num_images/nproc;
  if (io_node()==rank) {
    cout << " Number of proc: " << nproc << endl; 
    cout << " Number of images: " << num_images << endl;
    cout << " Batch size: " << batch_size << endl; 
    cout << " Number of batches: " << num_batches << endl;
    cout << " Number of epochs: " << epochs << endl; 
    cout << " Dimension of image: " << dim << endl;
    cout << " Local number of imgs: " << nloc << endl; 
  } 
  mt19937 g(100);
  // This is to create the file which contains the dataset
  vector<int> lst;
  lst.resize(num_images);
  for(int i=0; i<num_images; i++) lst[i]; 

  int *dataset = new int [nloc*dim];
  for (int i=0; i<nloc; i++) {
    for(int j=0; j<dim; j++) {
      dataset[i*dim + j] = i+rank*nloc;
    }
  }
  if (io_node()==rank) cout << "* Writing splitted dataset to the file system" << endl; 
  char filename[255] = "test.dat";
  strcat(filename, to_string(rank).c_str());
  int fd = open(filename, O_RDWR | O_CREAT | O_TRUNC, S_IRUSR | S_IWUSR | S_IRGRP | S_IROTH);
  
  ::pwrite(fd, dataset, nloc*dim*sizeof(int), 0);
  close(fd);
  fsync(fd);

  MPI_Barrier(MPI_COMM_WORLD);
  delete [] dataset;
  MPI_Finalize();
  return 0;
  // =====================================================================//
  // Create the memory mapped buffer and attach to a MPI Window
  if (io_node()==rank) cout << "* Creating memory map" << endl; 
  fd = open(filename, O_RDONLY);
  void *addr = mmap(NULL, nloc*dim*sizeof(int), PROT_READ, MAP_SHARED, fd, 0);
  msync(addr, sizeof(int)*dim*nloc, MS_SYNC);
  MPI_Barrier(MPI_COMM_WORLD);
  int *data = (int*)addr;
  MPI_Win_create(data, sizeof(int)*dim*nloc, sizeof(int), MPI_INFO_NULL, MPI_COMM_WORLD, &win);
  // Reading data using MPI_Get. -- this is for random memory access. 
  if (io_node()==rank) cout << "* Reading data batch by batch using MPI_Get" << endl; 
  int *bd = new int [dim*batch_size];
  for(int e=0; e<epochs; e++) {
    ::shuffle(lst.begin(), lst.end(), g);
    double t1 = 0.0; 
    for (int b = 0; b < num_batches; b++) {
      MPI_Win_fence(MPI_MODE_NOPUT, win);
      double t0 = MPI_Wtime();
      for(int i=0; i< batch_size; i++) {
	int dest = lst[rank*nloc+(b*batch_size + i)%nloc];
	int src = dest/nloc;
	int disp = dest%nloc*dim;
	if (src==rank)
	  memcpy(&bd[i*dim], &data[disp], dim*sizeof(int));
	else
	  MPI_Get(&bd[i*dim], dim, MPI_INT, src, disp, dim, MPI_INT, win);
      }
      MPI_Win_fence(MPI_MODE_NOPUT, win);
      t1 += MPI_Wtime() - t0; 
      for(int i=0; i<batch_size; i++) {
	assert(bd[i*dim]==lst[rank*nloc+(b*batch_size+i)%nloc]);
      }
      if (getenv("DEBUG")!= NULL and int(atof(getenv("DEBUG")))>0) {
	if (rank==io_node()) {
	  for(int i=0; i<batch_size; i++) {
	    printf("%5d: %5d(%5d)    ",i, bd[i*dim], lst[rank*nloc+(b*batch_size+i)%nloc]);
	    if (i%5==4) printf("\n");
	  }
	  printf("\n");
	}
      }
    }
    if (io_node()==rank) 
      printf("Epoch: %d     time: %6.4f    rate: %6.4f MB/sec\n", e, t1,num_images*dim*sizeof(int)/t1/1024/1024);
  }
    
  delete [] bd;
  MPI_Win_free(&win);
  munmap(data, sizeof(int)*dim*nloc);
  close(fd);
  remove(filename);
  MPI_Finalize();
  return 0;
}

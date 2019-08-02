#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <stdio.h>

using namespace std;
int main(int narg, char * argc[] ) {
  int dim = 1024576; 
  int niter = 1; 
  int i = 0; 
  int ierr, rank, nproc; 
  int flush = 1;
  // read command line arguments
  ierr = MPI_Init(&narg, &argc); 
  ierr = MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  ierr = MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
  MPI_Info info;
  int ssd = 0; 
  MPI_Info_create(&info);
  MPI_Info_set ( info , "direct_io" , "true" );
  char* filename; 
  while (i<narg) {
    if (strcmp(argc[i], "-dim")==0) {
      dim = atoi(argc[i+1]); i+=2;
    } else if (strcmp(argc[i], "-filename")==0) {
      filename = argc[i+1]; i+=2;
    }
  }
  if (rank == 0) {
    cout << " Buffer size: " << dim/1024./1024. << " MB" << endl; 
  } 
  char *buffer = new char[dim]; 
  MPI_Comm comm = MPI_COMM_WORLD; 
  MPI_File fh; 
  MPI_Status status; 
  double w_rate = 0.0; 
  double r_rate = 0.0;
  double r_open = 0.0; 
  double w_open = 0.0; 
  double r_close = 0.0; 
  double w_close = 0.0; 
  double trate=0.0; 
  double t0 = MPI_Wtime(); 
  MPI_File_open(comm, filename, MPI_MODE_RDWR,  info, &fh); 
  double t1 = MPI_Wtime(); 
  w_open += t1 - t0; 
  MPI_File_seek(fh, rank*dim, MPI_SEEK_SET); 
  double t2 = MPI_Wtime(); 
  MPI_File_read(fh, buffer, dim, MPI_CHAR, &status); 
  double t3 = MPI_Wtime(); 
  MPI_File_close(&fh); 
  double t4 = MPI_Wtime(); 
  w_close += t4 - t3; 
  double rate = dim/(t3-t2)/1024./1024.;
  MPI_Reduce(&rate, &trate, 1, MPI_DOUBLE, MPI_SUM, 0, comm); 
  if (rank==0) {
    cout << "======================" << endl; 
    cout << " Open (s): " << w_open << endl; 
    cout << " Read(MB/s): " << trate << endl; 
    cout << " Close (s): " << w_close << endl;     
  }
  ierr = MPI_Finalize(); 
  return ierr; 
}


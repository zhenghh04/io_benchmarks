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
  //  MPI_Info_set ( info , "ind_rd_buffer_size" , "0");
  //MPI_Info_set ( info , "ind_wd_buffer_size" , "0");
  //MPI_Info_set ( info, "romio_cb_read", "diable"); 
  //MPI_Info_set ( info, "romio_cb_write", "diable");
  while (i<narg) {
    if (strcmp(argc[i], "-dim")==0) {
      dim = atoi(argc[i+1]); i+=2;
    } else if (strcmp(argc[i], "-niter")==0) {
      niter = atoi(argc[i+1]); i+=2; 
    } else if (strcmp(argc[i], "-flush")==0) {
      flush = atoi(argc[i+1]); i+=2; 
    } else if (strcmp(argc[i], "-ssd")==0) {
      ssd = atoi(argc[i+1]); i+=2; 
    } else {
      i++; 
    }
  }
  if (rank == 0) {
    cout << " Number of ranks: " << nproc << endl; 
    cout << " Buffer size: " << dim/1024./1024. << " MB" << endl; 
    cout << " Number of Iterations: " << niter << endl; 
    cout << " Flush the cache: " << flush << endl; 
  } 

  string filename; 
  if (ssd>0)
    filename = "/local/scratch/testFile.dat"; 
  else
    filename = "testFile.dat"; 
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
  for (int it = 0; it < niter; it++) {
    double t0 = MPI_Wtime(); 
    MPI_File_open(comm, filename.c_str(), MPI_MODE_RDWR | MPI_MODE_CREATE, info, &fh); 
    double t1 = MPI_Wtime(); 
    w_open += t1 - t0; 
    MPI_File_seek(fh, rank*dim, MPI_SEEK_SET); 
    for (int j=0; j<dim; j++)
      buffer[j] = char((j+it)%26); 
    MPI_Barrier(comm);
    double t2 = MPI_Wtime(); 
    MPI_File_write(fh, buffer, dim, MPI_CHAR, &status); 
    double t3 = MPI_Wtime(); 
    MPI_File_close(&fh); 
    double t4 = MPI_Wtime(); 
    w_close += t4 - t3; 
    double rate = dim/(t3-t2)/1024./1024.;
    w_rate += rate; 
    if (flush>0) {
      delete buffer; 
      buffer = new char [dim]; 
    }
    MPI_Reduce(&rate, &trate, 1, MPI_DOUBLE, MPI_SUM, 0, comm); 
    if (rank==0) {
      cout << it << " Rate  MB/s: " << trate << "(W)    "; 
    }
    t0 = MPI_Wtime(); 
    MPI_File_open(comm, filename.c_str(), MPI_MODE_RDWR, info, &fh); 
    t1 = MPI_Wtime(); 
    r_open += t1 - t0; 
    MPI_File_seek(fh, rank*dim, MPI_SEEK_SET); 
    t2 = MPI_Wtime(); 
    MPI_File_read(fh, buffer, dim, MPI_CHAR, &status); 
    t3 = MPI_Wtime(); 
    MPI_File_close(&fh); 
    t4 = MPI_Wtime(); 
    r_close += t4 - t3; 
    rate = dim/(t3-t2)/1024./1024.;
    r_rate += rate; 
    MPI_Reduce(&rate, &trate, 1, MPI_DOUBLE, MPI_SUM, 0, comm); 
    MPI_Barrier(comm);
    if (rank==0) cout << trate << "(R)"<< endl; 
    if (flush>0) {
      delete buffer; 
      buffer = new char [dim]; 
    }
    MPI_File_delete(filename.c_str(), info);
    MPI_Barrier(comm);
  }
  r_open /= niter; 
  r_rate /= niter;
  r_close /= niter; 
  w_rate /=niter; 
  w_open /= niter; 
  w_close /= niter; 
  
  MPI_Reduce(&w_rate, &trate, 1, MPI_DOUBLE, MPI_SUM, 0, comm); 
  if (rank==0) {
    cout << "======================" << endl; 
    cout << " Open (s): " << w_open << endl; 
    cout << " Write (MB/s): " << trate << endl; 
    cout << " Close (s): " << w_close << endl;     
  }
  MPI_Reduce(&r_rate, &trate, 1, MPI_DOUBLE, MPI_SUM, 0, comm); 
  if (rank==0) {
    cout << "======================" << endl; 
    cout << " Open (s): " << r_open << endl; 
    cout << " Read (MB/s): " << trate << endl; 
    cout << " Close (s): " << r_close << endl;     
  }
  ierr = MPI_Finalize(); 
  return ierr; 
}


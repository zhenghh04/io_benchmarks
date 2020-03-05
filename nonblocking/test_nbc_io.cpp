#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <string.h>
#include <unistd.h>
#include <sstream>  
#include <iostream>

using namespace std; 
void simulate_compute(double n) {
  usleep(int(n*1000000));
};
int main(int argc, char * argv[])
{
  long nel, i;
  int32_t * buffer;
  double start_time, total_time;
  MPI_File handle;
  MPI_Info info = MPI_INFO_NULL;
  MPI_Request * request;
  MPI_Status status;
  int nproc, mype; 
  int nonblocking=0;
  int batch_size = 32;
  double n=1.0; 
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  MPI_Comm_rank(MPI_COMM_WORLD, &mype);

  for(int i=1; i<argc; i++) {
    if (strcmp(argv[i], "--batch_size")==0) {
      batch_size = int(atof(argv[i+1]));
      i++;
    } else if (strcmp(argv[i], "--nonblocking")==0) {
      nonblocking = int(atof(argv[i+1]));
      i++;
    } else if (strcmp(argv[i], "--compute_time")==0){
      n = atof(argv[i+1]);
      i++; 
    } else {
      cout << " I don't know option: " << argv[i] << endl; 
    }
  }
  
  if (mype==0) {
    cout << "Tesing prefetch: " << endl; 
    cout << "Batch size: " << batch_size << endl;
    cout << "Compute time: " << n << " seconds" << endl;
    cout << "Number of workers: " << nproc << endl; 
  }
  
  nel = batch_size * 224 * 224 * 3;
  buffer = new int32_t [nel];

  for (int i = 0; i < nel; i++) {
    buffer[i] = mype;
  }

  double io = 0.0;
  double compute = 0.0;
  double wait = 0.0;
  double close = 0.0;
  double open = 0.0; 
  double t0 = MPI_Wtime();
  int offset = nel*sizeof(int32_t)*mype;
  if (nonblocking>0) {
    if (mype==0) fprintf(stdout, "blocking read\n"); 
    MPI_File_open(MPI_COMM_WORLD, "file.dat", MPI_MODE_WRONLY, info, &handle);
    open += MPI_Wtime() - start_time;
    start_time = MPI_Wtime();
    MPI_File_iwrite_at_all(handle, offset, buffer, nel, MPI_INT32_T, request);
    io += MPI_Wtime() - start_time;
    start_time = MPI_Wtime();
    simulate_compute(n);
    compute += MPI_Wtime() - start_time;
    start_time = MPI_Wtime();
    MPI_Waitall(1, request, MPI_STATUS_IGNORE);
    wait += MPI_Wtime() - start_time;
    start_time = MPI_Wtime();
    MPI_File_close(&handle);
    close += MPI_Wtime() - start_time;
  } else {
    if (mype==0) fprintf(stdout, "blocking read\n"); 
    start_time = MPI_Wtime();
    MPI_File_open(MPI_COMM_WORLD, "file.dat", MPI_MODE_WRONLY, info, &handle);
    open += MPI_Wtime() - start_time;
    start_time = MPI_Wtime();
    MPI_File_write_at_all(handle, offset,  buffer, nel, MPI_INT32_T, MPI_STATUS_IGNORE);
    io += MPI_Wtime() - start_time;
    start_time = MPI_Wtime();
    simulate_compute(n);
    compute += MPI_Wtime() - start_time;
    start_time = MPI_Wtime();
    MPI_File_close(&handle);
    close += MPI_Wtime() - start_time;
  }
  double t1 = MPI_Wtime();
  if (mype==0) {
    cout << "Timing information " << endl; 
    fprintf(stdout, "open: %.10f \n", open);
    fprintf(stdout, "write: %.10f \n", io);
    fprintf(stdout, "close: %.10f \n", close);
    fprintf(stdout, "wait: %.10f \n", wait);
    fprintf(stdout, "compute: %.10f \n", compute);
    fprintf(stdout, "total time: %.10f \n", t1 - t0);
  }
  MPI_Finalize();
  return 0; 
}

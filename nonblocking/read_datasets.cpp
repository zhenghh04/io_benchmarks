#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <string.h>
#include <unistd.h>
#include<sstream>  
#include <iostream>
#define RANGE 256
using namespace std; 
string int2string(int k) {
  stringstream ss;  
  ss<<k;  
  string s;  
  ss>>s;  
  return s; 
};

char *string2char(string str) {
  int n = str.length();
  char *c = new char[n+1]; 
  strcpy(c, str.c_str());
  return c; 
}

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
  MPIO_Request* requests;
  MPI_Status* status;
  int nproc, mype; 
  
  int nonblocking=0;
  int batch_size = 32;
  int nbatch = 2048;

  
  double n=1.0; 
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  MPI_Comm_rank(MPI_COMM_WORLD, &mype);

  for(int i=1; i<argc; i++) {
    if (strcmp(argv[i], "--batch_size")==0) {
      batch_size = int(atof(argv[i+1]));
      i++;
    } else if (strcmp(argv[i], "--num_batches")==0) {
      nbatch = int(atof(argv[i+1]));
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
    cout << "        Batch size: " << batch_size << endl;
    cout << " Number of batches: " << nbatch << endl;
    cout << "      Compute time: " << n << " seconds" << endl;
    cout << " Number of workers: " << nproc << endl; 
  }
  requests = new MPIO_Request [batch_size];
  status = new MPI_Status [batch_size];
  nel = batch_size * 224*224*3*sizeof(int32_t);
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
  if (nonblocking>0) {
    if (mype==0) fprintf(stdout, "non-blocking read\n"); 
    for(int it = mype; it < nbatch; it+=nproc) {
      string lab="./datasets/batch_";
      lab.append(int2string(it));
      lab.append(".dat");
      char *labs = string2char(lab);
      start_time = MPI_Wtime();
      MPI_File_open(MPI_COMM_SELF, labs, MPI_MODE_RDONLY, info, &handle);
      open += MPI_Wtime() - start_time;
      start_time = MPI_Wtime();
      for(int b = 0; b < batch_size; b++)
	MPI_File_iread(handle, &buffer[b*224*224*3*sizeof(int32_t)], nel, MPI_INT32_T, &requests[b]);
      io += MPI_Wtime() - start_time;
      start_time = MPI_Wtime();
      simulate_compute(n);
      compute += MPI_Wtime() - start_time;
      start_time = MPI_Wtime();
      MPI_Waitall(batch_size, requests, status);
      wait += MPI_Wtime() - start_time;
      start_time = MPI_Wtime();
      MPI_File_close(&handle);
      close += MPI_Wtime() - start_time;
      start_time = MPI_Wtime();
      wait += MPI_Wtime() - start_time;
    }
  } else {
    if (mype==0) fprintf(stdout, "blocking read\n"); 
    for(int it=mype; it<nbatch; it+=nproc) {
      string lab="./datasets/batch_";
      lab.append(int2string(it));
      lab.append(".dat");
      char *labs = string2char(lab);
      start_time = MPI_Wtime();
      MPI_File_open(MPI_COMM_SELF, labs, MPI_MODE_RDONLY, info, &handle);
      open += MPI_Wtime() - start_time;
      start_time = MPI_Wtime();
      for(int b = 0; b < batch_size; b++)
	MPI_File_read(handle, &buffer[b*224*224*3*sizeof(int32_t)], nel, MPI_INT32_T, MPI_STATUS_IGNORE);
      io += MPI_Wtime() - start_time;
      start_time = MPI_Wtime();
      simulate_compute(n);
      compute += MPI_Wtime() - start_time;
      start_time = MPI_Wtime();
      MPI_File_close(&handle);
      close += MPI_Wtime() - start_time;
    }
  }
  double t1 = MPI_Wtime();
  if (mype==0) {
    cout << " Timing information (seconds): " << endl; 
    fprintf(stdout, "      open: %.10f \n", open);
    fprintf(stdout, "      read: %.10f \n", io);
    fprintf(stdout, "   compute: %.10f \n", compute);
    fprintf(stdout, "      wait: %.10f \n", wait);
    fprintf(stdout, "     close: %.10f \n", close);
    fprintf(stdout, "total time: %.10f \n", t1 - t0);
  }
  MPI_Finalize();
  return 0; 
}

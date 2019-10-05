#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <string.h>
#include <unistd.h>
#include<sstream>  

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
  MPI_Request request;
  MPI_Status status;
  
  nel = 147 * 1048576;
  buffer = (int32_t *) malloc(nel * sizeof(int32_t));
  for (i = 0; i < nel; i++) {
    buffer[i] = 0;
  }
  
  MPI_Init(&argc, &argv);
  
  int opt =atof(argv[1]);
  double n =atof(argv[2]);
  int nbatch = atof(argv[3]);
  double io = 0.0;
  double compute = 0.0;
  double wait = 0.0;
  double t0 = 0.0;
  double close = 0.0;
  double open = 0.0; 
  if (opt==0) {
    fprintf(stdout, "non-blocking read\n"); 
    for(int it = 0; it < nbatch; it++) {
      string lab="./datasets/batch";
      lab.append(int2string(it));
      char *labs = string2char(lab);
      start_time = MPI_Wtime();
      MPI_File_open(MPI_COMM_SELF, labs, MPI_MODE_RDONLY, info, &handle);
      open += MPI_Wtime() - start_time;
      start_time = MPI_Wtime();
      MPI_File_iread(handle, buffer, nel, MPI_INT32_T, &request);
      io += MPI_Wtime() - start_time;
      start_time = MPI_Wtime();
      simulate_compute(n);
      compute += MPI_Wtime() - start_time;
      start_time = MPI_Wtime();
      MPI_Wait(&request, &status);
      wait += MPI_Wtime() - start_time;
      start_time = MPI_Wtime();
      MPI_File_close(&handle);
      close += MPI_Wtime() - start_time;
    }
  } else {
    fprintf(stdout, "blocking read\n"); 
    for(int it=0; it<nbatch; it++) {
      string lab="./datasets/batch";

      lab.append(int2string(it));
      char *labs = string2char(lab);
      start_time = MPI_Wtime();
      MPI_File_open(MPI_COMM_SELF, labs, MPI_MODE_RDONLY, info, &handle);
      open += MPI_Wtime() - start_time;
      start_time = MPI_Wtime();
      MPI_File_read(handle, buffer, nel, MPI_INT32_T, MPI_STATUS_IGNORE);
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
  fprintf(stdout, "open: %.10f \n", open);
  fprintf(stdout, "io: %.10f \n", io);
  fprintf(stdout, "close: %.10f \n", close);
  fprintf(stdout, "wait: %.10f \n", wait);
  fprintf(stdout, "compute: %.10f \n", compute);
  fprintf(stdout, "total time: %.10f \n", t1 - t0);
  MPI_Finalize();
  return 0; 
}

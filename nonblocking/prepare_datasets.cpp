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
  int nproc, mype; 
  int nbatch = 2048;
  nel = 147 * 1048576;
  buffer = (int32_t *) malloc(nel * sizeof(int32_t));
  for (i = 0; i < nel; i++) {
    buffer[i] = 0;
  }
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  MPI_Comm_rank(MPI_COMM_WORLD, &mype); 
  for(int it = mype; it < nbatch; it+nproc) {
    string lab="./datasets/batch";
    lab.append(int2string(it));
    char *labs = string2char(lab);
    MPI_File_open(MPI_COMM_SELF, labs, MPI_MODE_WRONLY, info, &handle);
    MPI_File_write(handle, buffer, nel, MPI_INT32_T, &status);
    MPI_File_close(&handle);
  }
  MPI_Finalize();
  return 0; 
}

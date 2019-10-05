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
  char * buffer;
  double start_time, total_time;
  MPI_File handle;
  MPI_Info info = MPI_INFO_NULL;
  MPI_Request request;
  MPI_Status status;
  int nproc, mype; 
  int nbatch = 2048;
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  MPI_Comm_rank(MPI_COMM_WORLD, &mype);
  nel = 1 * 1048576;
  buffer = new char [nel];
  for (int i = 0; i < nel; i++) {
    buffer[i] = mype;
  }
  for(int it = mype; it < nbatch; it+=nproc) {
    string lab="./datasets/batch_";
    lab.append(int2string(it));
    lab.append(".dat");
    char *labs = string2char(lab);
    cout << labs << endl; 
    MPI_File_open(MPI_COMM_SELF, labs, MPI_MODE_WRONLY, info, &handle);
    MPI_File_write(handle, buffer, nel, MPI_CHAR, &status);
    MPI_File_close(&handle);
  }
  MPI_Finalize();
  return 0; 
}

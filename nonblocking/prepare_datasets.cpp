#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <string.h>
#include <unistd.h>
#include<sstream>  
#include <iostream>
#include <fcntl.h>
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
  int batch_size = 32;
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
    } else {
      cout << " I don't know option: " << argv[i] << endl; 
    }
  }

  
  nel = batch_size * 224*224*3*sizeof(int32_t);
  buffer = new int32_t [nel];

  for (int i = 0; i < nel; i++) {
    buffer[i] = mype;
  }
  if (mype==0) {
    cout << "Preparing datasets in datasets/batch_" << endl; 
    cout << "Batch size: " << batch_size << endl;
    cout << "Number of batches: " << nbatch << endl; 
    cout << "Number of elements: " << nel << endl; 
  }

  for(int it = mype; it < nbatch; it+=nproc) {
    string lab="./datasets/batch_";
    lab.append(int2string(it));
    lab.append(".dat");
    char *labs = string2char(lab);
    printf("The filename: %s\n", labs); 
    double t0 = MPI_Wtime();
    MPI_File_open(MPI_COMM_SELF, labs, MPI_MODE_WRONLY | MPI_MODE_CREATE, info, &handle);
    MPI_File_write(handle, buffer, nel, MPI_INT32_T, &status);
    MPI_File_close(&handle);
    double t1 = MPI_Wtime() - t0; 
  }
  
  MPI_Finalize();
  return 0; 
}

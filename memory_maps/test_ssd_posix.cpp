/* 
   This benchmark is to understand how much performance can we gain for collective I/O if we use 
   node-local SSD as a Cache to the lustre file system. 
   Huihuo Zheng <huihuo.zheng@anl.gov>
 */
#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include "time.h"
#include "mpi.h"
#include <string>
#include <sys/stat.h>
#include <unistd.h>
#include "timing.h"
#include <cmath>
using namespace std; 

#include <sstream>
struct IOT
{
  float open, close, raw, wait, sync;
  int rep;
};

template <typename T>
string itoa ( T Number)
{
  std::ostringstream ss;
  ss << Number;
  return ss.str(); 
}

int fail(char *filename, int linenumber) { 
  fprintf(stderr, "%s:%d %s\n", filename, linenumber, strerror(errno)); 
  exit(1);
  return 0; /*Make compiler happy */
}
#define QUIT fail(__FILE__, __LINE__ )
int main(int argc, char *argv[]) {
  int i=1;
  int dim=1024*1024*2;
  int niter=16;
  char *dir = "./scratch/"; 
  MPI_File handle;
  MPI_Info info = MPI_INFO_NULL;
  MPI_Status status;
  int rank, nproc, filePerProc, rankReorder;
  bool fsync=false; 
  bool keep = false; 
  bool read_test = false;
  bool write_test = true; 
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  Timing tt(rank==0);
  rankReorder = 0; 
  while (i<argc) {
    if (strcmp(argv[i], "--dim") == 0) {
      dim = atoi(argv[i+1]); 
      i+=2;
    } else if (strcmp(argv[i], "--niter") == 0) {
      niter = int(atoi(argv[i+1])); 
      i+=2;
    } else if (strcmp(argv[i], "--dir") == 0) {
      dir = argv[i+1];
      i+=2;
    } else if (strcmp(argv[i], "--fsync")==0) {
      fsync = true; i++;
    } else if (strcmp(argv[i], "--keep")==0) {
      keep = true; i++;
    } else if (strcmp(argv[i], "--R")==0) {
      read_test = true; i++;

    } else if (strcmp(argv[i], "--rankReorder")==0) {
      rankReorder=int(atoi(argv[i+1])); 
      i+=2;
    } else if (strcmp(argv[i], "--W")==0) {
      write_test = true; i++;
    } else {
      i++; 
    }
  }
  long size = sizeof(int) * dim;
  MPI_Comm comm; 
  MPI_Comm local_comm; 
  int local_rank, ppn; 
  if (rank==0)  {
    printf("----------- SSD Staging test---------------\n"); 
    printf(" *           Dimension: %d\n", dim); 
    cout <<" *   Local buffer size: " << size/1024./1024. << " MiB" << endl;
    printf(" *            location: %s\n", dir); 
    printf(" *     Number of iter.: %d\n", niter); 
    printf(" * Total num. of ranks: %d\n", nproc);
    printf(" *               fsync: %d\n", fsync);
    printf("-------------------------------------------\n"); 
  }
  char fn[100]; 
  strcpy(fn, dir);
  strcat(fn, "/file.dat"); 

  int *myarray = new int [dim];
  for(int i=0; i<dim; i++) {
    myarray[i] = i+rank; 
  }

  comm = MPI_COMM_SELF; 
  
  IOT W, R;
  int fd;
  double t0 = MPI_Wtime();

  if (write_test) {
    strcat(fn, itoa(rank).c_str()); 
    strcat(fn, "-iter"); 
    double write_rate[niter]; 
    double w2 = 0.0; 
    double w = 0.0; 
    for(int i=0; i<niter; i++) {
      MPI_Barrier(MPI_COMM_WORLD); 
      strcat(fn, itoa(i).c_str()); 
      tt.start_clock("w_open"); 
      fd = open(fn, O_WRONLY | O_CREAT); 
      tt.stop_clock("w_open");
      double t0 = MPI_Wtime();
      tt.start_clock("w_write"); 
      write(fd, (char *)myarray, size); 
      tt.stop_clock("w_write");
      tt.start_clock("w_sync"); 
      if (fsync) ::fsync(fd); 
      tt.stop_clock("w_sync");
      double t1 = MPI_Wtime(); 
      tt.start_clock("w_close"); 
      close(fd);
      tt.stop_clock("w_close"); 
      tt.start_clock("remove"); 
      if (not keep) remove(fn); 
      tt.stop_clock("remove"); 
      write_rate[i] = size/(t1-t0)/1024/1024*nproc;
      w += write_rate[i]; 
      w2 += write_rate[i]*write_rate[i];
      if (rank==0) printf("* write rate(%d): %f\n", i, write_rate[i]);
    }
    W.open = tt["w_open"].t;
    W.raw = tt["w_write"].t + tt["w_sync"].t; 
    W.close = tt["w_close"].t;
    W.rep = tt["w_open"].num_call; 
    if (rank==0) {
      cout << "\n----------------   I/O WRITE  ---------------" << endl; 
      cout << " Open time (s): " << W.open/W.rep << endl;
      cout << "Write time (s): " << W.raw/W.rep << endl;
      cout << "Close time (s): " << W.close/W.rep << endl;
      cout << "    Write rate: " << size/W.raw/1024/1024*W.rep*nproc << " +/- " << sqrt(w2/niter - w*w/niter/niter)<<" MiB/sec" << endl;
      cout << "-----------------------------------------------" << endl; 
    }
  }

  if (read_test) {
    strcat(fn, itoa((rank+rankReorder)%nproc).c_str()); 
    strcat(fn, "-iter"); 
    for(int i=0; i<niter; i++) {
      strcat(fn, itoa(i).c_str()); 
      tt.start_clock("r_open"); 
      fd = open(fn, O_RDONLY); 
      tt.stop_clock("r_open");
      tt.start_clock("r_read"); 
      read(fd, (char *)myarray, size); 
      tt.stop_clock("r_read");
      tt.start_clock("r_close"); 
      close(fd);
      tt.stop_clock("r_close"); 
    }
    R.open = tt["r_open"].t;
    R.raw = tt["r_read"].t;
    R.close = tt["m2s_close"].t;
    R.rep = tt["m2s_open"].num_call; 
    if (rank==0) {
      cout << "\n---------------   I/O READ   ----------------" << endl; 
      cout << "  Open time (s): " << R.open/R.rep << endl;
      cout << "  Read time (s): " << R.raw/R.rep << endl;
      cout << " Close time (s): " << R.close/R.rep << endl;
      cout << "      Read rate: " << size/R.raw/1024/1024*R.rep*nproc << " MiB/sec" << endl;
      cout << "-----------------------------------------------" << endl; 
    }
  }

  MPI_Finalize();
  return 0;
}

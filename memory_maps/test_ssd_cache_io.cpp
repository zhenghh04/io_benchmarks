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
#include "stat.h"
#include "ssd_cache_io.h"
using namespace std; 
Timing tt; 
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
inline void reduction_avg(double *t, int niter, double &w, double &std) {
  stat(t, niter, w, std, 'i');
  double w0 = w; 
  double std0 = std*std;
  MPI_Allreduce(&w0, &w, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  MPI_Allreduce(&std0, &std, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  std = sqrt(std); 
}

int fail(char *filename, int linenumber) { 
  fprintf(stderr, "%s:%d %s\n", filename, linenumber, strerror(errno)); 
  exit(1);
  return 0; /*Make compiler happy */
}
#define QUIT fail(__FILE__, __LINE__ )

int main(int argc, char *argv[]) {
  int i=0;
  int dim=1024*1024*2;
  int niter=1;
  char *ssd = "/local/scratch/";
  char *lustre="./scratch/"; 
  MPI_File handle;
  MPI_Info info = MPI_INFO_NULL;
  MPI_Status status;
  int rank, nproc;
  int fsync = 0; 
  int async = 0; 
  int filePerProc=0;
  int collective=0; 
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  for(int i=1; i<argc; i++) {
    if (strcmp(argv[i], "--dim") == 0) {
      dim = atoi(argv[i+1]); i+=1;
    } else if (strcmp(argv[i], "--niter") == 0) {
      niter = atoi(argv[i+1]); 
      i+=1;
    } else if (strcmp(argv[i], "--SSD") == 0) {
      ssd = argv[i+1];
      i+=1;
    } else if (strcmp(argv[i], "--lustre") == 0) {
      lustre = argv[i+1];
      i+=1;
    } else if (strcmp(argv[i], "--fsync")==0) {
      fsync = atoi(argv[i+1]);
      i+=1;
    } else if (strcmp(argv[i], "--async")==0) {
      async = atoi(argv[i+1]);
      i+=1;
    } else if (strcmp(argv[i], "--filePerProc")==0){ 
      filePerProc = atoi(argv[i+1]);
      i+=1;
    } else if (strcmp(argv[i], "--collective")==0){ 
      collective = atoi(argv[i+1]);
      i+=1;
    } else {
      if (rank==0) cout << argv[0] << "--dim DIM --niter NTER --SSD SSD --luster LUSTRE --fsync [0|1] --async [0|1] --filePerProc [0|1]" << endl;  
    }
  }
  long size = sizeof(int) * dim;
  // Get local rank and world rank for cross comm establishment.
  int local_rank, ppn, num_nodes;
  ppn = 1;
  num_nodes = nproc/ppn; 
  MPI_Barrier(MPI_COMM_WORLD);
  if (rank==0)  {
    printf("----------- SSD Staging test---------------\n"); 
    printf(" *           Dimension: %d\n", dim); 
    cout <<" *   Local buffer size: " << size/1024./1024. << " MB" << endl;
    printf(" *        SSD location: %s\n", ssd); 
    printf(" *     Lustre location: %s\n", lustre); 
    printf(" *     Number of iter.: %d\n", niter); 
    printf(" * Total num. of ranks: %d\n", nproc);
    printf(" *                 PPN: %d\n", ppn);
    printf(" *               fsync: %d\n", fsync);
    printf(" *               async: %d\n", async);
    printf(" *         filePerProc: %d\n", filePerProc);
    printf(" *          collective: %d\n", collective);
    printf("-------------------------------------------\n"); 
  }
  MPI_Barrier(MPI_COMM_WORLD);
  int *myarray = new int [dim];
  for(int i=0; i<dim; i++) {
    myarray[i] = i; 
  }
  MPI_Comm comm; 
  comm = MPI_COMM_WORLD; 
  IOT M2L, M2S, M2MMF, MMF2L, W;
  char f1[100]; 
  strcpy(f1, lustre);
  strcat(f1, "/file-mem2lustre.dat"); 
  strcat(f1, itoa(rank).c_str());
  tt.start_clock("m2l_open");
  MPI_File_open(comm, f1, MPI_MODE_WRONLY | MPI_MODE_CREATE | MPI_MODE_DELETE_ON_CLOSE, info, &handle);
  tt.stop_clock("m2l_open");
  for(int i=0; i<niter; i++) {
    char f1[100]; 
    strcpy(f1, lustre);
    strcat(f1, "/file-mem2lustre.dat"); 
    tt.start_clock("m2l_rate");
    tt.start_clock("m2l_write"); 
    if (collective==1) 
      MPI_File_write_at_all(handle, rank*dim*sizeof(int), myarray, dim, MPI_INT, MPI_STATUS_IGNORE);
    else
      MPI_File_write_at(handle, rank*dim*sizeof(int), myarray, dim, MPI_INT, MPI_STATUS_IGNORE);
    tt.stop_clock("m2l_write");
    tt.stop_clock("m2l_rate");
  }
  tt.start_clock("m2l_close"); 
  MPI_File_close(&handle);
  tt.stop_clock("m2l_close");
  
  M2L.open = tt["m2l_open"].t;
  M2L.raw = tt["m2l_write"].t  +tt["m2l_sync"].t;
  M2L.close = tt["m2l_close"].t;
  M2L.rep = tt["m2l_open"].num_call;
  double w, std; 
  reduction_avg(tt["m2l_rate"].t_iter, niter, w, std);
  w = w/1024/1024*size;
  std = std/1024/1024*size;

  if (rank==0) {
    cout << "\n--------------- Memory to Lustre (direct) -----" << endl; 
    cout << "     Open time (s): " << M2L.open/M2L.rep << endl; 
    cout << "    Write time (s): " << M2L.raw/M2L.rep << endl;
    cout << "    Close time (s): " << M2L.close/M2L.rep << endl;
    cout << "Write rate (MiB/s): " << w << " +/- " << std << endl; 
    cout << "-----------------------------------------------" << endl; 
  }

#ifdef DEBUG
  tt.PrintTiming(rank==0);
#endif
  MPI_Finalize();
  return 0;
}

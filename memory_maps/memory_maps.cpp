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
  char *ssd = getenv("SSD_CACHE_PATH");
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
  MPI_Comm local_comm; 
  MPI_Comm_split_type(MPI_COMM_WORLD, MPI_COMM_TYPE_SHARED, 0, MPI_INFO_NULL,
			&local_comm);
  
  // Get local rank and world rank for cross comm establishment.
  int local_rank, ppn, num_nodes; 
  MPI_Comm_rank(local_comm, &local_rank);
  MPI_Comm_size(local_comm, &ppn);
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
  if (filePerProc==1) 
    comm = MPI_COMM_SELF; 
  else
    comm = MPI_COMM_WORLD; 

  IOT M2L, M2S, M2MMF, MMF2L, W;

  MPI_Barrier(MPI_COMM_WORLD);
  int *myarrayssd = new int [dim];
  for(int i=0; i<dim; i++) {
    myarrayssd[i] = i; 
  }
    
// testing SSD write performance using multiple ranks posix. For this test, we will always using file per rank
  double write_rate[niter]; 
  double w = 0.0; 
  double std = 0.0;
  double w0 = 0.0;
  double std0 = 0.0; 
  for(int i=0; i<niter; i++) {
    MPI_Barrier(MPI_COMM_WORLD); 
    char fn[100]; 
    strcpy(fn, ssd);
    strcat(fn, "/file.dat"); 
    strcat(fn, itoa(i).c_str()); 
    strcat(fn, itoa(rank).c_str()); 
    tt.start_clock("w_open"); 
    int fd = open(fn, O_WRONLY | O_CREAT); // write only for 
    tt.stop_clock("w_open");
    double t0 = MPI_Wtime();
    tt.start_clock("w_rate");
    tt.start_clock("w_write"); 
    write(fd, (char *)myarray, size); 
    tt.stop_clock("w_write");
    tt.start_clock("w_sync"); 
    if (fsync) ::fsync(fd); 
    tt.stop_clock("w_sync");
    tt.stop_clock("w_rate");
    double t1 = MPI_Wtime(); 
    tt.start_clock("w_close"); 
    close(fd);
    tt.stop_clock("w_close"); 
    tt.start_clock("remove"); 
    remove(fn); 
    tt.stop_clock("remove"); 
  }
  stat(tt["w_rate"].t_iter, niter, w, std, 'i');
  w = w/1024/1024*size;
  std = std/1024/1024*size;
  // we do this only because we would like to find out the performance of each SSD ranks; 
  double wt[num_nodes];
  double wtt[num_nodes];
  for(int i=0; i<num_nodes; i++) wt[i]=0;
  MPI_Allreduce(&w, &wt[rank/ppn], 1, MPI_DOUBLE, MPI_SUM, local_comm);
  MPI_Allreduce(&wt, &wtt, num_nodes, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  for(int i=0; i<num_nodes; i++) wtt[i]=wtt[i]/ppn;

  double st[int(nproc/ppn)];
  double stt[int(nproc/ppn)];
  for(int i=0; i<nproc/ppn; i++) wt[i]=0;
  std = std*std; 
  MPI_Allreduce(&std, &st[rank/ppn], 1, MPI_DOUBLE, MPI_SUM, local_comm);
  MPI_Allreduce(&st, &stt, num_nodes, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  for(int i=0; i<num_nodes; i++) stt[i] = sqrt(stt[i]/ppn);

  if (rank==0) {
    cout << "SSD NODE PROP.: "; 
    for(int i=0; i<nproc/ppn; i++) {
      cout << wtt[i] << " +/- " << stt[i] << ", ";
    }
    cout << endl; 
  }
  
  W.open = tt["w_open"].t;
  W.raw = tt["w_write"].t + tt["w_sync"].t; 
  W.close = tt["w_close"].t;
  W.rep = tt["w_open"].num_call; 

  reduction_avg(tt["w_rate"].t_iter, niter, w, std);
  w = w/1024/1024*size;
  std = std/1024/1024*size;
  if (rank==0) {
    cout << "\n----------------  SSD I/O WRITE  ---------------" << endl; 
    cout << "     Open time (s): " << W.open/W.rep << endl;
    cout << "    Write time (s): " << W.raw/W.rep << endl;
    cout << "    Close time (s): " << W.close/W.rep << endl;
    cout << "Write rate (MiB/s): " << w << " +/- " << std << endl; 
    cout << "-----------------------------------------------" << endl; 
  }

  if (filePerProc==1) {
    for(int i=0; i<niter; i++) {
      char f1[100]; 
      strcpy(f1, lustre);
      strcat(f1, "/file-mem2lustre.dat"); 
      strcat(f1, itoa(rank).c_str()); 
      tt.start_clock("m2l_open");
      MPI_File_open(comm, f1, MPI_MODE_WRONLY | MPI_MODE_CREATE | MPI_MODE_DELETE_ON_CLOSE, info, &handle);
      tt.stop_clock("m2l_open");
      tt.start_clock("m2l_rate");
      tt.start_clock("m2l_write"); 
      MPI_File_write(handle, myarray, dim, MPI_INT, MPI_STATUS_IGNORE);
      tt.stop_clock("m2l_write"); 
      tt.start_clock("m2l_sync"); 
      if (fsync) MPI_File_sync(handle);
      tt.stop_clock("m2l_sync"); 
      tt.stop_clock("m2l_rate");
      tt.start_clock("m2l_close"); 
      MPI_File_close(&handle);
      tt.stop_clock("m2l_close"); 
    }
  } else {
    for(int i=0; i<niter; i++) {
      char f1[100]; 
      strcpy(f1, lustre);
      strcat(f1, "/file-mem2lustre.dat"); 
      tt.start_clock("m2l_open");
      MPI_File_open(comm, f1, MPI_MODE_WRONLY | MPI_MODE_CREATE | MPI_MODE_DELETE_ON_CLOSE, info, &handle);
      tt.stop_clock("m2l_open");
      tt.start_clock("m2l_rate");
      tt.start_clock("m2l_write"); 
      if (collective==1) 
	MPI_File_write_at_all(handle, rank*dim*sizeof(int), myarray, dim, MPI_INT, MPI_STATUS_IGNORE);
      else
	MPI_File_write_at(handle, rank*dim*sizeof(int), myarray, dim, MPI_INT, MPI_STATUS_IGNORE);
      tt.stop_clock("m2l_write"); 

      tt.start_clock("m2l_sync"); 
      if (fsync) MPI_File_sync(handle);
      tt.stop_clock("m2l_sync"); 
      tt.stop_clock("m2l_rate");
      
      tt.start_clock("m2l_close"); 
      MPI_File_close(&handle);
      tt.stop_clock("m2l_close"); 
    }
  }
  M2L.open = tt["m2l_open"].t;
  M2L.raw = tt["m2l_write"].t  +tt["m2l_sync"].t;
  M2L.close = tt["m2l_close"].t;
  M2L.rep = tt["m2l_open"].num_call;

  
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

  //staging time
  MPI_Barrier(MPI_COMM_WORLD);
  char *name; 
  int *resultlen; 

  ofstream myfile;
  int err;
  for(int j=0; j<niter; j++) {
    char f[100]; 
    strcpy(f, ssd); 
    strcat(f, "/file-"); 
    strcat(f, itoa(local_rank).c_str()); 
    strcat(f, ".dat-iter"); 
    strcat(f, itoa(j).c_str());
    int fd = open(f, O_RDWR | O_CREAT | O_TRUNC, 0600); //6 = read+write for me!
    lseek(fd, size, SEEK_SET);
    write(fd, "A", 1);
    void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    if (addr == (void*) -1 ) QUIT;
    int *array = (int*) addr;
    tt.start_clock("m2mmf_write");
    tt.start_clock("m2mmf_rate");
    for(int i=0; i<dim; i++)
      array[i] = i+j;
    tt.stop_clock("m2mmf_write");
    tt.start_clock("m2mmf_sync"); 
    msync(addr, size, MS_SYNC);
    if (fsync) ::fsync(fd);
    tt.stop_clock("m2mmf_sync");
    tt.stop_clock("m2mmf_rate");
    close(fd); 
    munmap(addr, size);
  }
  M2MMF.raw = tt["m2mmf_write"].t + tt["m2mmf_sync"].t;
  reduction_avg(tt["m2mmf_rate"].t_iter, niter, w, std);
  w = w/1024/1024*size;
  std = std/1024/1024*size;
  if (rank==0) {
    cout << "\n--------------- Memory to mmap file -------" << endl; 
    cout << "    Write time (s): " << M2MMF.raw/niter << endl; 
    cout << "Write rate (MiB/s): " << w << " +/- " << std << endl; 
    cout << "------------------------------------------------" << endl;
  }
#ifdef DEBUG
  printf("SSD files: %s (Rank-%d)", f, rank);
#endif

  /*
  MPI_Request request;
  
  if (filePerProc==1) {
    for (int i=0; i<niter; i++){
      char f2[100]; 
      strcpy(f2, lustre); 
      strcat(f2, "/file-mmf2lustre.dat");
      strcat(f2, itoa(rank).c_str()); 
      char f[100]; 
      strcpy(f, ssd); 
      strcat(f, "/file-"); 
      strcat(f, itoa(local_rank).c_str()); 
      strcat(f, ".dat-iter"); 
      strcat(f, itoa(i).c_str());
      int fd = open(f, O_RDONLY, 0600); //6 = read+write for me!
      void *addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
      msync(addr, size, MS_SYNC);
      int *array2 = (int*) addr;
      tt.start_clock("mmf2l_open"); 
      MPI_File_open(comm, f2, MPI_MODE_WRONLY | MPI_MODE_CREATE | MPI_MODE_DELETE_ON_CLOSE, info, &handle);
      tt.stop_clock("mmf2l_open");
      tt.start_clock("mmf2l_rate");
      if (async) {
	tt.start_clock("mmf2l_iwrite"); 
	MPI_File_iwrite(handle,array2, dim, MPI_INT, &request);
	tt.stop_clock("mmf2l_iwrite");
	
	tt.start_clock("mmf2l_Wait"); 
	MPI_Wait(&request, &status);
	tt.stop_clock("mmf2l_Wait");
      } else {
	tt.start_clock("mmf2l_iwrite"); 
	MPI_File_write(handle, array2, dim, MPI_INT, MPI_STATUS_IGNORE);
	tt.stop_clock("mmf2l_iwrite");
	tt.start_clock("mmf2l_Wait"); 
	tt.stop_clock("mmf2l_Wait");
      }
      tt.start_clock("mmf2l_sync"); 
      if (fsync) MPI_File_sync(handle);
      tt.stop_clock("mmf2l_sync"); 
      tt.stop_clock("mmf2l_rate");
      tt.start_clock("mmf2l_close"); 
      MPI_File_close(&handle);
      tt.stop_clock("mmf2l_close");
      munmap(addr, size);
    }
  } else {
    for (int i=0; i<niter; i++) {
      char f2[100]; 
      strcpy(f2, lustre); 
      strcat(f2, "/file-mmf2lustre.dat");
      char f[100]; 
      strcpy(f, ssd); 
      strcat(f, "/file-"); 
      strcat(f, itoa(local_rank).c_str()); 
      strcat(f, ".dat-iter"); 
      strcat(f, itoa(i).c_str()); 
      int fd = open(f, O_RDONLY, 0600); //6 = read+write for me!
      void *addr = mmap(NULL, size, PROT_READ, MAP_SHARED, fd, 0);
      int *array2 = (int*) addr;
      msync(addr, size, MS_SYNC);
      tt.start_clock("mmf2l_open"); 
      MPI_File_open(MPI_COMM_WORLD, f2, MPI_MODE_WRONLY | MPI_MODE_CREATE | MPI_MODE_DELETE_ON_CLOSE, info, &handle);
      tt.stop_clock("mmf2l_open");
      tt.start_clock("mmf2l_rate");
      if (async) {
	tt.start_clock("mmf2l_iwrite"); 
	if (collective)
	  MPI_File_iwrite_at_all(handle, rank*dim*sizeof(int), array2, dim, MPI_INT, &request);
	else
	  MPI_File_iwrite_at(handle, rank*dim*sizeof(int), array2, dim, MPI_INT, &request);
	tt.stop_clock("mmf2l_iwrite");
	tt.start_clock("mmf2l_Wait"); 
	MPI_Wait(&request, &status);
	tt.stop_clock("mmf2l_Wait");
      } else {
	tt.start_clock("mmf2l_iwrite"); 
	MPI_File_write_at_all(handle, rank*dim*sizeof(int), array2, dim, MPI_INT,  MPI_STATUS_IGNORE);
	tt.stop_clock("mmf2l_iwrite");
	tt.start_clock("mmf2l_Wait"); 
	tt.stop_clock("mmf2l_Wait");
      }
      tt.start_clock("mmf2l_sync"); 
      if (fsync) MPI_File_sync(handle);
      tt.stop_clock("mmf2l_sync"); 
      tt.stop_clock("mmf2l_rate");
      tt.start_clock("mmf2l_close"); 
      MPI_File_close(&handle);
      tt.stop_clock("mmf2l_close");
      munmap(addr, size);
    }
  }
  MMF2L.open = tt["mmf2l_open"].t;
  MMF2L.raw = (tt["mmf2l_iwrite"].t + tt["mmf2l_Wait"].t + tt["mmf2l_sync"].t);
  MMF2L.close = tt["mmf2l_close"].t;
  MMF2L.rep = niter;
  reduction_avg(tt["mmf2l_rate"].t_iter, niter, w, std);
  w = w/1024/1024*size;
  std = std/1024/1024*size;

  w0 = size/MMF2L.raw/1024/1024*niter; 
  MPI_Allreduce(&w0, &w, 1, MPI_DOUBLE, MPI_SUM, MPI_COMM_WORLD);
  
  if (rank==0) {
    cout << "\n--------------- memmap file to lustre -----" << endl; 
    cout << "     Open time (s): " << MMF2L.open/MMF2L.rep << endl; 
    cout << "    Write time (s): " << MMF2L.raw/MMF2L.rep << endl; 
    cout << "    Close time (s): " << MMF2L.close/MMF2L.rep << endl; 
    cout << "Write rate (MiB/s): " << w << " +/- " << std << endl; 
    cout << "---------------------------------------------" << endl;
  }
  */
  if (filePerProc==1) {
    for(int i=0; i<niter; i++) {
      char f1[100]; 
      strcpy(f1, lustre);
      strcat(f1, "/file-mem2ssd2lustre.dat"); 
      strcat(f1, itoa(rank).c_str()); 
      tt.start_clock("m2s2l_open");
      MPI_File_open_cache(comm, f1, MPI_MODE_WRONLY | MPI_MODE_CREATE | MPI_MODE_DELETE_ON_CLOSE, info, &handle);
      tt.stop_clock("m2s2l_open");
      tt.start_clock("m2s2l_rate");
      tt.start_clock("m2s2l_write"); 
      MPI_File_write_cache(handle, myarray, dim, MPI_INT, MPI_STATUS_IGNORE);
      tt.stop_clock("m2s2l_write"); 
      tt.start_clock("m2s2l_sync"); 
      //if (fsync) MPI_File_sync(handle);
      tt.stop_clock("m2s2l_sync"); 
      tt.stop_clock("m2s2l_rate");
      tt.start_clock("m2s2l_close"); 
      MPI_File_close_cache(&handle);
      tt.stop_clock("m2s2l_close"); 
    }
  } else {
    for(int i=0; i<niter; i++) {
      char f1[100]; 
      strcpy(f1, lustre);
      strcat(f1, "/file-mem2ssd2lustre.dat"); 
      tt.start_clock("m2s2l_open");
      MPI_File_open_cache(comm, f1, MPI_MODE_WRONLY | MPI_MODE_CREATE | MPI_MODE_DELETE_ON_CLOSE, info, &handle);
      tt.stop_clock("m2s2l_open");
      tt.start_clock("m2s2l_rate");
      tt.start_clock("m2s2l_write"); 
      if (collective==1) 
	MPI_File_write_at_all_cache(handle, rank*dim*sizeof(int), myarray, dim, MPI_INT, MPI_STATUS_IGNORE);
      else
	MPI_File_write_at_cache(handle, rank*dim*sizeof(int), myarray, dim, MPI_INT, MPI_STATUS_IGNORE);
      tt.stop_clock("m2s2l_write"); 

      tt.start_clock("m2s2l_sync"); 
      //if (fsync) MPI_File_sync(handle);
      tt.stop_clock("m2s2l_sync"); 
      tt.stop_clock("m2s2l_rate");
      tt.start_clock("m2s2l_close"); 
      MPI_File_close_cache(&handle);
      tt.stop_clock("m2s2l_close"); 
    }
  }
  M2L.open = tt["m2s2l_open"].t;
  M2L.raw = tt["m2s2l_write"].t  +tt["m2s2l_sync"].t;
  M2L.close = tt["m2s2l_close"].t;
  M2L.rep = tt["m2s2l_open"].num_call;

  
  reduction_avg(tt["m2s2l_rate"].t_iter, niter, w, std);
  w = w/1024/1024*size;
  std = std/1024/1024*size;

  if (rank==0) {
    cout << "\n--------------- Memory->SSD->Lustre -----" << endl; 
    cout << "     Open time (s): " << M2L.open/M2L.rep << endl; 
    cout << "    Write time (s): " << M2L.raw/M2L.rep << endl;
    cout << "    Close time (s): " << M2L.close/M2L.rep << endl;
    cout << "Write rate (MiB/s): " << w << " +/- " << std << endl; 
    cout << "-----------------------------------------------" << endl; 
  }
  MPI_Barrier(MPI_COMM_WORLD);  
  delete [] myarray;
  delete [] myarrayssd;
  
#ifdef DEBUG
  //  tt.PrintTiming(rank==0);
#endif
  MPI_Finalize();
  return 0;
}

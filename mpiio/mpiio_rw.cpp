#include <iostream>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <mpi.h>
#include <sstream>
#ifndef MPIAPI
#define MPIAPI                  /* defined as __stdcall on Windows */
#endif


using namespace std;
string itoa(int myInt) {
  stringstream ss;
  ss << myInt;
  string myString = ss.str();
  return myString; 
}
int main(int narg, char * argc[] ) {
  int dim = 1024576; 
  int niter = 1; 
  int i = 0; 
  int ierr, rank, nproc, nblock; 
  int amode; // access mode
  int filePerProc=0;
  int collective=0;
  char access='W'; 
  MPI_Comm comm; 
  nblock = 1; 
  int flush = 1;
  // read command line arguments
  ierr = MPI_Init(&narg, &argc); 
  ierr = MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  ierr = MPI_Comm_rank(MPI_COMM_WORLD, &rank); 
  MPI_Info info;
  int ssd = 0; 
  MPI_Info_create(&info);
  MPI_Info_set ( info , "direct_io" , "true" );
  char *filename;
  while (i<narg) {
    if (strcmp(argc[i], "-dim")==0) {
      dim = atoi(argc[i+1]); i+=2;
    } else if (strcmp(argc[i], "-nblock")==0) {
      nblock = atoi(argc[i+1]); i+=2;
    } else if (strcmp(argc[i], "-collective")==0) {
      collective = atoi(argc[i+1]); i+=2;
    } else if (strcmp(argc[i], "-filePerProc")==0) {
      filePerProc = atoi(argc[i+1]); i+=2;
    } else if (strcmp(argc[i], "-access")==0) {
      access = argc[i+1][0]; i+=2;
    } else if (strcmp(argc[i], "-filename")==0) {
      filename = argc[i+1]; i+=2; 
    } else {
      i+=1; 
    }
  }
  if (filePerProc>0) {
    comm = MPI_COMM_SELF; 
    const char *pe  = itoa(rank).c_str(); 
    strcat(filename, pe); 
  }



  MPI_File fh; 
  MPI_Status status; 
  double w_rate = 0.0; 
  double r_rate = 0.0;
  double r_open = 0.0; 
  double w_open = 0.0; 
  double r_close = 0.0; 
  double w_close = 0.0; 
  double trate=0.0; 
  int tt = int(dim/nblock);
  dim = tt*nblock; 
  char *buffer = new char[tt]; 
  double t0 = MPI_Wtime(); 
  comm = MPI_COMM_WORLD; 
  MPI_File_open(comm, filename, amode, info, &fh); 
  double t1 = MPI_Wtime(); 
  w_open += t1 - t0; 
  if (access=='W') 
    for (int j=0; j<tt; j++)
      buffer[j] = char(j%26); 
  MPI_Barrier(comm);
  double t2 = MPI_Wtime(); 
  if (access=='R') {
    if (filePerProc>0) {
      for(int j=0; j<nblock; j++) {
	MPI_File_seek(fh, j*tt, MPI_SEEK_SET); 
	MPI_File_read(fh, buffer, tt, MPI_CHAR, &status); 
      }
    } else {
      if (collective==0) {
	for(int j=0; j<nblock; j++) {
	  MPI_File_read_at(fh, rank*dim+j*tt, buffer, tt, MPI_CHAR, &status); 
	}
      } else {
	for(int j=0; j<nblock; j++) {
	  MPI_File_read_at_all(fh, rank*dim+j*tt, buffer, tt, MPI_CHAR, &status); 
	}
      }
    }
  } else {
    if (filePerProc>0) {
      for(int j=0; j<nblock; j++) {
	MPI_File_seek(fh, j*tt, MPI_SEEK_SET); 
	MPI_File_write(fh, buffer, tt, MPI_CHAR, &status); 
      }
    } else {
      if (collective==0) {
	for(int j=0; j<nblock; j++) {
	  MPI_File_write_at(fh, rank*dim+j*tt, buffer, tt, MPI_CHAR, &status); 
	}
      } else {
	for(int j=0; j<nblock; j++) {
	  MPI_File_write_at_all(fh, rank*dim+j*tt, buffer, tt, MPI_CHAR, &status); 
	}
      }
    }
  }
  double t3 = MPI_Wtime(); 
  MPI_File_close(&fh); 
  double t4 = MPI_Wtime(); 
  w_close += t4 - t3; 
  double rate = dim/(t3-t2)/1024./1024.;
  MPI_Reduce(&rate, &trate, 1, MPI_DOUBLE, MPI_SUM, 0, comm); 
  if (rank==0) cout << dim/1024./1024. << " " << w_open << " " << trate << " " << w_close << " " << endl;     
  ierr = MPI_Finalize(); 
  return ierr; 
}


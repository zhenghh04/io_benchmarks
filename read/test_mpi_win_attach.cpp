#include "mpi.h"
#include "stdio.h"
#include <iostream>
#include <vector>
#include "assert.h"
#include "string.h"
#include "stdlib.h"
#include <unistd.h>
#include <fcntl.h>
#include <sys/mman.h>
#include <algorithm>    // std::shuffle
#include <random>
#include <thread>         // std::this_thread::sleep_for
#include <chrono>

using namespace std;
int main(int argc, char **argv) {
  MPI_Win win;
  size_t dim_a, dim_b, dim;
  int rank, nproc;
  int ta, tb;
  dim_a = 1024; dim_b = 1024; dim = dim_a + dim_b;
  MPI_Init(&argc, &argv);
  MPI_Comm_size(MPI_COMM_WORLD, &nproc);
  MPI_Comm_rank(MPI_COMM_WORLD, &rank);
  int *a = new int [dim_a];
  int *b = new int [dim_b];
  for(int i=0; i<dim_a; i++) {
    a[i] = rank; 
  }
  for(int i=0; i<dim_b; i++) {
    b[i] = rank + nproc; 
  }
  MPI_Win_create_dynamic(MPI_INFO_NULL, MPI_COMM_WORLD, &win);
  MPI_Win_attach(win, a, sizeof(int)*dim_a);
  MPI_Win_attach(win, b, sizeof(int)*dim_b);
  MPI_Win_fence(MPI_MODE_NOPUT, win);
  if (nproc > 1) {
    MPI_Get(&ta, 1, MPI_INT, (rank+1)%nproc, 0, 1, MPI_INT, win);
    MPI_Get(&tb, 1, MPI_INT, (rank+1)%nproc, dim_a, 1, MPI_INT, win);
  } else {
    ta = a[0];
    tb = b[0];
  }
  MPI_Win_fence(MPI_MODE_NOPUT, win);
  for(int i=0; i<nproc; i++)  {
    if (rank==i) {
      cout << "rank " << rank << ":  ta - " << ta << endl;
      cout << "rank " << rank << ":  tb - " << tb << endl;
    }
    this_thread::sleep_for (chrono::seconds(1));
  }
  MPI_Win_detach(win, a); free(a);
  MPI_Win_detach(win, b); free(b);
  MPI_Finalize();
}

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

int MPI_File_open(); 
int MPI_File_write_at_all(MPI_File fh, MPI_Offset offset, ROMIO_CONST void *buf,
                      int count, MPI_Datatype datatype, MPI_Status *status);
    char *lbuf; 
    open()
int MPI_File_write_at();
int MPI_File_close();
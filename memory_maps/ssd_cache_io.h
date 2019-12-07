#ifndef SSD_CACHE_IO_H_
#define SSD_CACHE_IO_H_
int MPI_File_open(); 
int MPI_File_write_at_all(MPI_File fh, MPI_Offset offset, ROMIO_CONST void *buf,
                      int count, MPI_Datatype datatype, MPI_Status *status);

int MPI_File_write_at();
int MPI_File_close(MPI_File &fh) {
};
#endif
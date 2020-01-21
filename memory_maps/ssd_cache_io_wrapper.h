#ifndef SSD_CACHE_IO_WRAPPER_H_
#define SSD_CACHE_IO_WRAPPER_H_
#include "ssd_cache_io.h"
int MPI_File_open(MPI_Comm comm, const char *filename,
		  int amode, MPI_Info info,
		  MPI_File *fh) {
    return MPI_File_open_cache(comm, filename, amode, info, fh);
};
int MPI_File_write_at_all(MPI_File fh, MPI_Offset offset,
			  const void *buf, int count,
			  MPI_Datatype datatype,
			  MPI_Status *status) {
    return MPI_File_write_at_all_cache(fh, offset, buf, count, datatype, status);
};
int MPI_File_write_at(MPI_File fh, MPI_Offset offset,
		      const void *buf, int count,
		      MPI_Datatype datatype,
		      MPI_Status *status) {
    return MPI_File_write_at_all_cache(fh, offset, buf, count, datatype, status);
};
int MPI_File_write(MPI_File fh, const void *buf,
		   int count, MPI_Datatype datatype,
		   MPI_Status *status) {
    return MPI_File_write_cache(fh, buf, count, datatype, status);
};
int MPI_File_close(MPI_File *fh) {
    return MPI_File_close_cache(fh);
};
#endif

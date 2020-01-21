#ifndef SSD_CACHE_IO_H_
#define SSD_CACHE_IO_H_
int MPI_File_open_cache(MPI_Comm comm, const char *filename,
		  int amode, MPI_Info info,
		  MPI_File *fh);
int MPI_File_write_at_all_cache(MPI_File fh, MPI_Offset offset,
			  const void *buf, int count,
			  MPI_Datatype datatype,
			  MPI_Status *status);
int MPI_File_write_at_cache(MPI_File fh, MPI_Offset offset,
		      const void *buf, int count,
		      MPI_Datatype datatype,
		      MPI_Status *status);
int MPI_File_write_cache(MPI_File fh, const void *buf,
		   int count, MPI_Datatype datatype,
		   MPI_Status *status); 
int MPI_File_close_cache(MPI_File *fh);
#endif

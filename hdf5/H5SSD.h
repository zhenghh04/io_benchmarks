#ifndef H5FSSD_H_
#define H5FSSD_H_
#include "hdf5.h"
hid_t H5Fcreate_cache( const char *name, unsigned flags, 
		       hid_t fcpl_id, hid_t fapl_id );

herr_t H5Fclose_cache( hid_t file_id );
herr_t H5Dwrite_cache(hid_t dset_id, hid_t mem_type_id, 
		      hid_t mem_space_id, hid_t file_space_id, 
		      hid_t dxpl_id, const void *buf);
#endif

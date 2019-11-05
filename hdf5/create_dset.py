import h5py
d1 = 4
d2 = 6
f = h5py.File("file.hdf5", 'w')

dset = f.create_dataset("dset", (d1, d2), dtype='i')
dset.attrs['Units'] = "Images per second"
for i in range(d1):
    for j in range(d2):
        dset[i, j] = i*d2 + j + 1
f.close()


#!/usr/bin/env python
# I/O testing for incorparting node local storage
# -- Huihuo Zheng
import mpi4py
import h5py
import argparse
import numpy as np
from multiprocessing import Process
comm = mpi4py.MPI.COMM_WORLD
import os
from os import path
import glob
import mmap
from itertools import cycle
sz = 224
sz = 224
nc = 3
def read_image(fstr):
    fin = h5py.File(fstr, 'r')
    a = fin['images'][:, :, :]
    fin.close()
    return a

def get_one_image(fstr):
    f = os.environ['SSD_CACHE_PATH'] + "/" + fstr
    if path.exists(f):
        return read_image(f)
    else:
        os.system("cp %s %s"%(fstr, f))
        return read_image(f)

def parallel_dist(n, rank=0, nproc=1):
    nloc = n//nproc
    offset = nloc*rank
    if (rank < n%nproc):
        nloc = nloc + 1
        offset = offset + rank
    else:
        offset = offset + n%nproc
    return offset, nloc

class ImageGenerator:
    def __init__(self, directory="./", batch_size=32, shuffle=True, split=True):
        self.fname = []
        self.batch_size = batch_size
        self.directory = directory
        self.file_iterator = None
        self.shuffle=shuffle
        self.split = split
        self._get_file_iterator()

    def _get_file_iterator(self):
        self.fname = glob.glob("%s/*.h5"%self.directory)
        if (self.split):
            np.random.seed(100)
        else:
            np.random.seed(rank)
        if (self.shuffle):
            np.random.shuffle(self.fname)

        if (self.split):
            offset, nloc = parallel_dist(len(self.fname), rank=comm.rank, nproc = comm.size)
            self.file_iterator = cycle(self.fname[offset:offset+nloc])
        else:
            self.file_iterator = cycle(self.fname)
            
    def _shuffle(self):
        np.random.shuffle(self.fname)
        self.file_iterator = cycle(self.fname)

    def _get_batch_of_files(self):
        batch_fname = []
        for i in range(self.batch_size):
            batch_fname.append(next(self.file_iterator))
        return batch_fname

    def __next__(self):
        batch = np.zeros((self.batch_size, sz, sz, nc))
        i = 0
        for f in self._get_batch_of_files():
            batch[i] = read_image(f)
            i=i+1
        return batch

    def __iter__(self):
        return self

a = ImageGenerator("images/", shuffle=True, batch_size=32, split=True)

b = next(a)
print(b[:, 0, 0, 0])


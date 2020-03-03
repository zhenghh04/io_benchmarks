#!/usr/bin/env python
# I/O testing for incorparting node local storage in deep learning applications
#    
# -- Huihuo Zheng
import mpi4py
import h5py
import argparse
import numpy as np
from threading import Thread
from queue import Queue
import time
comm = mpi4py.MPI.COMM_WORLD
import os
from os import path
import glob
import mmap
from tqdm import tqdm
from itertools import cycle
parser = argparse.ArgumentParser(description='Process some integers.')
parser.add_argument('--batch_size', type=int, default=32, help="batch size")
parser.add_argument('--num_batches', type=int, default=10, help='Number of batches per epoch')
parser.add_argument("--epochs", type=int, default=4, help="Number of epochs")
parser.add_argument("--verbose", type=int, default=0, help='Verbose level')
parser.add_argument("--nimage", type=int, default=-1, help="Number of image")
parser.add_argument("--simulate_compute", type=np.float32, default=0.5, help="Time per training step")
parser.add_argument("--node_local_storage", type=str, default="SSD/")
parser.add_argument("--space", type=int, default=1024*1024*1024*128)
args = parser.parse_args()
sz = 224
sz = 224
nc = 3
verbose = args.verbose
node_local_storage=args.node_local_storage
space = args.space

print("Deep learning I/O test")
print("* batch size: ", args.batch_size)
print("* number of batches: ", args.num_batches)
print("* Number of epochs: ", args.epochs)
print("* nimage: ", args.nimage)
print("* node_local_storage: ", args.node_local_storage)
print("* time per step: ", args.simulate_compute)
print("--------------------------------------------------")
def read_image(fstr):
    ''' reading a single image'''
    fin = h5py.File(fstr, 'r')
    a = fin['images'][:, :, :]
    fin.close()
    return a

def get_remote_image(fstr, target=0):
    if (rank==target):
        a = read_image(fstr)
        MPI_Send()
    MPI_Recv()
    return a

def where_is_file(fstr):
    ''' checking where is the file '''
    pos = np.zeros(nproc)
    
def parallel_dist(n, rank=0, nproc=1):
    nloc = n//nproc
    offset = nloc*rank
    if (rank < n%nproc):
        nloc = nloc + 1
        offset = offset + rank
    else:
        offset = offset + n%nproc
    return offset, nloc

def data_migration_thread(queue, terminate, wait=0.01):
    fstr = queue.get()
    while(not terminate):
        a = read_image(fstr)
        f = node_local_storage + "/" + fstr
        if (verbose>2):
            print("Copying data %s to node local cache" %fstr)
        os.system("cp %s %s"%(fstr, f))
        try:
            fstr = queue.get(timeout=wait)
        except:
            break

class ImageGenerator:
    def __init__(self, directory="./", batch_size=32, shuffle=True, split=True, nimage=-1):
        self.batch_size = batch_size
        self.directory = directory
        self.fname = glob.glob("%s/*.h5"%self.directory)
        self.fname=self.fname[:nimage]
        self.index = np.arange(len(self.fname))
        self.file_iterator = None
        self.shuffle=shuffle
        self.split = split
        self._get_iterator()
        self.local_cache_list = []
        self.files_to_move = Queue()
        self.terminate=False
        self.thread = Thread(target=data_migration_thread, args=(self.files_to_move,self.terminate))
        self.start_thread = False
        self.local_storage_space_left = space

    def _get_iterator(self):
        if (self.split):
            np.random.seed(100)
        else:
            np.random.seed(rank)
        if (self.shuffle):
            np.random.shuffle(self.index)
            
        if (self.split):
            offset, nloc = parallel_dist(len(self.fname), rank=comm.rank, nproc = comm.size)
            self.iterator = cycle(self.index[offset:offset+nloc])
        else:
            self.iterator = cycle(self.index)
            
    def _get_batch_of_files(self):
        batch_index = []
        for i in range(self.batch_size):
            batch_index.append(next(self.iterator))
        return batch_index

    def __next__(self):
        batch = np.zeros((self.batch_size, sz, sz, nc))
        i = 0
        for n in self._get_batch_of_files():
            batch[i] = self._get_one_image(n)
            i=i+1
        return batch

    def __iter__(self):
        return self
    
    def _get_one_image(self, n):
        fstr = self.fname[n]
        try:
            place = self.local_cache_list.find(n)
            f = os.environ['SSD_CACHE_PATH'] + "/" + fstr
            return read_image(f)
        except:
            if (self.local_storage_space_left >  4*sz*sz*nc):
                self.local_cache_list.append(n)
                self.files_to_move.put(fstr)
                if (self.start_thread != True):
                    self.start_thread = True
                    self.thread.start()
                self.local_storage_space_left = self.local_storage_space_left - 4*sz*sz*nc
            return read_image(fstr)


    def _where_is_image(self, fstr):
        rank = -1
        return rank
    def terminate_thread(self):
        self.terminate=True
        if (self.start_thread):
            self.thread.join()

    def on_epoch_end(self):
        self._get_iterator()
        
def train(a, steps_per_epoch, epochs, t=5):
    for epoch in range(epochs):
        tt = 0
        print("Epoch - %s" %epoch)
        for i in tqdm(range(steps_per_epoch)):
            t0= time.time()
            b = next(a)
            t1 = time.time()
            tt += t1 - t0
            if (verbose>1):
                print("training on batch: ", b[:, 0, 0, 0])
            time.sleep(t)
        a.on_epoch_end()
        print("Total time on I/O: %s" %tt)
        print("I/O throughput: %s images/sec" %(args.num_batches*args.batch_size/tt))
    a.terminate_thread()
a = ImageGenerator("images/", shuffle=True, batch_size=args.batch_size, split=True, nimage=args.nimage)
train(a, steps_per_epoch=args.num_batches, epochs=args.epochs, t=args.simulate_compute)

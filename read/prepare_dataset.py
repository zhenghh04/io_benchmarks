#!/usr/bin/env python
# This is for preparing fake images for I/O tests
#  One can select 
from mpi4py import MPI
import h5py
import argparse
import numpy as np
import os
from tqdm import tqdm
comm = MPI.COMM_WORLD
parser = argparse.ArgumentParser(description="preparing data set")
parser.add_argument("--num_images", type=int, default=8192)
parser.add_argument("--sz", type=int, default=224)
parser.add_argument("--format", default='channel_last', type=str)
parser.add_argument("--output", default='images.h5', type=str)
parser.add_argument("--file_per_image", action="store_true")
args = parser.parse_args()
if args.format == "channel_last":
    data = np.zeros((args.num_images, args.sz, args.sz, 3))
else:
    data = np.zeros((args.num_images, 3, args.sz, args.sz))

for i in range(args.num_images):
    data[i] = np.ones(data[i].shape)*i

if (args.file_per_image):
    for i in tqdm(range(comm.rank, args.num_images, comm.size)):
        f = h5py.File("images/%d.h5"%i, 'w')
        f.create_dataset("images", data=data[i], dtype=np.float32)
        f.close()
else:
    f = h5py.File(args.output, 'w')
    dset = f.create_dataset("image", data=data, dtype=np.float32)
    dset.attrs["format"] = args.format
    f.close()

#!/usr/bin/env python
# This is for testing SSD cache for read
import mpi4py
import h5py
import argparse
import numpy as np
import os
comm = mpi4py.MPI.COMM_WORLD
parser = argparse.ArgumentParser(description="preparing data set")
parser.add_argument("--num_images", type=int, default=1024)
parser.add_argument("--sz", type=int, default=224)
parser.add_argument("--format", default='channel_last', type=str)
parser.add_argument("--output", default='images.h5', type=str)
args = parser.parse_args()
f = h5py.File(args.output, 'w')
if args.format == "channel_last":
    data = np.zeros((args.num_images, args.sz, args.sz, 3))
else:
    data = np.zeros((args.num_images, 3, args.sz, args.sz))

for i in range(args.num_images):
    data[i] = np.ones(data[i].shape)*i
    
dset = f.create_dataset("image", data=data, dtype=np.float32)
dset.attrs["format"] = args.format
f.close()

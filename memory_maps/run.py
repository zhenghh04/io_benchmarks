#!/usr/bin/env python
# This benchmark is for running memory maps files
# The benchmark is corresponding to memory_maps.x
# The basic idea is to use SSD as a cache
# ----- 
# Huihuo Zheng
import os
import argparse
import socket
import time
import numpy.random as rnd
hostname = socket.gethostname()
parser = argparse.ArgumentParser(description='Process some integers.')
parser.add_argument("--dim", type=int, default=2097152)
if hostname.find("theta")!=-1:
    root="/gpfs/mira-home/hzheng/io_benchmarks/memory_maps/"
    parser.add_argument("--SSD", default="/local/scratch/", help="Directory for node local storage")
    parser.add_argument("--num_nodes", default=int(os.environ['COBALT_JOBSIZE']), type=int)
    parser.add_argument("--ppn", default=16, type=int)
else:
    root="/Users/zhenghh/Documents/Research/ExaHDF5/io_benchmarks/memory_maps/"
    parser.add_argument("--num_nodes", default=1, type=int)
    parser.add_argument("--SSD", default="SSD")
    parser.add_argument("--ppn", default=2, type=int)
parser.add_argument("--lustre", default=root+"/scratch/")
parser.add_argument("--niter", default=16, type=int)
parser.add_argument("--lustreStripeSize", default='8m')
parser.add_argument("--lustreStripeCount", default=48, type=int)
parser.add_argument("--filePerProc", type=int, default=0)
parser.add_argument("--fsync",type=int, default=1)
parser.add_argument("--async",type=int, default=1)
parser.add_argument("--directory", default="run/")
parser.add_argument("--output", default="run.log")
parser.add_argument("--collective", default=1, type=int)
args = parser.parse_args()
exe=root+"memory_maps.x"
options = vars(args)
args.lustre=args.lustre + "/%s%s"%(time.time(), rnd.randint(10000)) + "/n%s.p%s.c%s.s%s/" %(args.num_nodes, args.ppn, args.lustreStripeCount, args.lustreStripeSize)
def cmkdir(d):
    if not os.path.exists(d):
        os.makedirs(d)
for d in root+args.directory, args.lustre:
    cmkdir(d)
if args.SSD!="/local/scratch/":
    cmkdir(args.SSD)
    if (args.SSD[0]!='/'):
        args.SSD = root+"/"+args.SSD
extra_opts=" --filePerProc %s --fsync %s --async %s --collective %s" %(args.filePerProc, args.fsync, args.async, args.collective)
if hostname.find("theta")!=-1:
    os.system("lfs setstripe -c %s -S %s %s"%(args.lustreStripeCount, args.lustreStripeSize, args.lustre))
    os.system("lfs getstripe %s"%args.lustre)
    print("module unload intel; module load intel-2019; module load mpich-3.3.1-intel-2019; cd %s; aprun -n %s -N %s %s --SSD %s --lustre %s --niter %s %s |& tee %s; cd - " %(args.directory, args.num_nodes*args.ppn, args.ppn, exe, args.SSD, args.lustre, args.niter, extra_opts, root + args.directory + "/"+args.output))
    os.system("cd %s; aprun -n %s -N %s %s --SSD %s --lustre %s --niter %s %s |& tee %s; cd - " %(args.directory, args.num_nodes*args.ppn, args.ppn, exe, args.SSD, args.lustre, args.niter, extra_opts, root + args.directory + "/"+args.output))
else:
    print("cd %s; mpirun -np %s %s --SSD %s --lustre %s --niter %s %s | tee %s; cd -" %(args.directory, args.ppn, exe, args.SSD, args.lustre, args.niter, extra_opts, args.output))
    os.system("cd %s; mpirun -np %s %s --SSD %s --lustre %s --niter %s %s | tee %s; cd -" %(args.directory, args.ppn, exe, args.SSD, args.lustre, args.niter, extra_opts, args.output))

def read_to_str(fin, string):
    '''                                                                                                                                           Read file until reaching at the specified string                                                                                              fin can be the file name or file stream                                                                                                       '''
    f = 0
    if type(fin) is str:
        try:
            f = open(fin, "r")
        except IOError:
            print("File %s does not exist in the document" %fin)
    else:
        f = fin
    line = f.readline()
    while line and line.find(string)==-1:
        try:
            line = f.readline()
        except:
            break
    return line
import json
import datetime
fin = open(root+args.directory+"/"+args.output, 'r')
run = {}
run['input'] = vars(args)
run['SSD'] = [ [float(d.split()[0]), float(d.split()[2])] for d in read_to_str(fin, "SSD NODE").split(":")[1].split(',')[:-1]]
run['mem->ssd'] = [float(d) for d in read_to_str(fin, "Write rate").split(":")[1].split()[0::2]]
run['mem->lustre'] = [float(d) for d in read_to_str(fin, "Write rate").split(":")[1].split()[0::2]]
run['mem->mmap'] =  [float(d) for d in read_to_str(fin, "Write rate").split(":")[1].split()[0::2]]
run['mmap->lustre'] = [float(d) for d in read_to_str(fin, "Write rate").split(":")[1].split()[0::2]]
run['mmap->ssd->lustre'] = [float(d) for d in read_to_str(fin, "Write rate").split(":")[1].split()[0::2]]
with open(root+args.directory+"/" + args.output.split('.')[0]+'.json', 'w') as f:
    json.dump(run, f)

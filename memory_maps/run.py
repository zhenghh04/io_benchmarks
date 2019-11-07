#!/usr/bin/env python
import os
import argparse
import socket
hostname = socket.gethostname()
parser = argparse.ArgumentParser(description='Process some integers.')
parser.add_argument("--dim", type=int, default=2097152)
if hostname.find("theta")!=-1:
    parser.add_argument("--SSD", default="/local/scratch/")
    parser.add_argument("--num_nodes", default=int(os.environ['COBALT_JOBSIZE']), type=int)
    parser.add_argument("--ppn", default=16, type=int)
else:
    parser.add_argument("--num_nodes", default=1, type=int)
    parser.add_argument("--SSD", default="SSD")
    parser.add_argument("--ppn", default=2, type=int)
parser.add_argument("--lustre", default="./scratch/")
parser.add_argument("--niter", default=16, type=int)
parser.add_argument("--lustreStripeSize", default='8m')
parser.add_argument("--lustreStripeCount", default=48, type=int)
parser.add_argument("--filePerProc", type=int, default=0)
parser.add_argument("--fsync",type=int, default=1)
parser.add_argument("--async",type=int, default=1)
args = parser.parse_args()
options = vars(args)
print(options)
ll = args.lustre.split("/")
if ll[0]=='.':
    lustre=ll[1]
else:
    lustre=ll[0]
try:
    os.mkdir(lustre)
    os.mkdir(args.SSD)
except:
    print("already exist")
extra_opts=" --filePerProc %s --fsync %s --async %s" %(args.filePerProc, args.fsync, args.async)
print(extra_opts)
if hostname.find("theta")!=-1:
    os.system("lfs setstripe -c %s -S %s %s"%(args.lustreStripeCount, args.lustreStripeSize, lustre))
    os.system("lfs getstripe %s"%lustre)
    os.system("aprun -n %s -N %s ./memory_maps.x --SSD %s --lustre %s --niter %s %s" %(args.num_nodes*args.ppn, args.ppn, args.SSD, args.lustre, args.niter, extra_opts))
else:
    os.system("mpirun -np %s ./memory_maps.x --SSD %s --lustre %s --niter %s %s" %(args.ppn, args.SSD, args.lustre, args.niter, extra_opts))

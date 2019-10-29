#!/usr/bin/env python
import os
import argparse
parser = argparse.ArgumentParser(description='Process some integers.')
parser.add_argument("--dim", type=int, default=2097152)
parser.add_argument("--SSD", default="/local/scratch/")
parser.add_argument("--lustre", default="./scratch/")
parser.add_argument("--niter", default=1, type=int)
parser.add_argument("--num_nodes", default=int(os.environ['COBALT_JOBSIZE']), type=int)
parser.add_argument("--ppn", default=16, type=int)
parser.add_argument("--lustreStripeSize", default='8m')
parser.add_argument("--lustreStripeCount", default=48)
args = parser.parse_args()
ll = args.lustre.split("/")
if ll[0]=='.':
    lustre=ll[1]
else:
    lustre=ll[0]
os.system("lfs setstripe -c %s -S %s %s"%(args.lustreStripeCount, args.lustreStripeSize, lustre))
os.system("lfs getstripe %s"%lustre)
os.system("aprun -n %s -N %s ./memory_maps.x --SSD %s --lustre %s --niter %s" %(args.num_nodes*args.ppn, args.ppn, args.SSD, args.lustre, args.niter))

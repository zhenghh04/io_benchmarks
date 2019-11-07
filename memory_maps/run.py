#!/usr/bin/env python
import os
import argparse
import socket
hostname = socket.gethostname()
parser = argparse.ArgumentParser(description='Process some integers.')
parser.add_argument("--dim", type=int, default=2097152)
if hostname.find("theta")!=-1:
    root="/gpfs/mira-home/hzheng/io_benchmarks/memory_maps/"
    parser.add_argument("--SSD", default="/local/scratch/")
    parser.add_argument("--num_nodes", default=int(os.environ['COBALT_JOBSIZE']), type=int)
    parser.add_argument("--ppn", default=16, type=int)
else:
    root="/Users/zhenghh/Documents/Research/ExaHDF5/io_benchmarks/memory_maps"
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
args = parser.parse_args()
exe=root+"memory_maps.x"
options = vars(args)
args.lustre=args.lustre + "/n%s.p%s.c%s.s%s/" %(args.num_nodes, args.ppn, args.lustreStripeCount, args.lustreStripeSize)
def cmkdir(d):
    if not os.path.exists(d):
        os.makedirs(d)
for d in root+args.directory, args.lustre:
    cmkdir(d)
if args.SSD!="/local/scratch/":
    cmkdir(args.SSD)
extra_opts=" --filePerProc %s --fsync %s --async %s" %(args.filePerProc, args.fsync, args.async)
if hostname.find("theta")!=-1:
    os.system("lfs setstripe -c %s -S %s %s"%(args.lustreStripeCount, args.lustreStripeSize, args.lustre))
    os.system("lfs getstripe %s"%args.lustre)
    os.system("cd %s; aprun -n %s -N %s %s --SSD %s --lustre %s --niter %s %s |& tee %s; cd - " %(args.directory, args.num_nodes*args.ppn, args.ppn, exe, args.SSD, args.lustre, args.niter, extra_opts, args.output))
else:
    os.system("cd %s; mpirun -np %s %s --SSD %s --lustre %s --niter %s %s |& tee %s; cd -" %(args.directory, args.ppn, exe, args.SSD, args.lustre, args.niter, extra_opts, args.output))
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
os.chdir(args.directory)
fin = open(args.output, 'w')
run = {}
run['input'] = vars(args)
run['SSD'] = [ float(d) for d in read_to_str(fin, "SSD NODE").split(":").split()]
run['mem->ssd'] = float(read_to_str(fin, "Write rate:").split()[2])
run['mem->lustre'] = float(read_to_str(fin, "Write rate:").split()[2])
run['mem->mmap'] = float(read_to_str(fin, "Write rate:").split()[2])
run['mmap->lustre'] = float(read_to_str(fin, "Write rate:").split()[2])

with open(args.output.split('.')[0]+'json', 'w') as f:
    json.dump(run, f)

#!/usr/bin/env python
# This is to perform the ior benchmarks
# Huihuo Zheng @ ANL
import os
import argparse
import sys
import datetime, time
import socket
d = datetime.datetime.now()
os.environ['LD_LIBRARY_PATH']='/opt/cray/pe/mpt/7.7.14/gni/mpich-intel-abi/16.0/lib:/home/hzheng/soft/hdf5/ccio-abi/lib:'+os.environ["LD_LIBRARY_PATH"]
sys.path.append("/home/hzheng/mylibs/python")
from utils import read_to_str
from numpy.random import randint
parser = argparse.ArgumentParser(description='IOR Benchmark test')
parser.add_argument('--api', default='MPIIO')
parser.add_argument('--blockSize', default='8m')
parser.add_argument('--filePerProc', default=0, type=int)
parser.add_argument('--collective', default=0, type=int)
parser.add_argument('--lustreStripeCount', default=48, type=int)
parser.add_argument('--lustreStripeSize', default='16m')
parser.add_argument('--transferSize', default='8m')
parser.add_argument('--keepFile', default=0, type=int)
parser.add_argument("--cb_nodes", type=int, default=48)
parser.add_argument("--cb_size", type=str, default='16m')
parser.add_argument("--fs_block_size", type=str, default='16m')
parser.add_argument("--fs_block_count", type=int, default=1)
if socket.gethostname()=='zion':
    parser.add_argument('--numNodes', default=1, type=int)
    parser.add_argument('--procPerNode', default=2, type=int)
else:
    parser.add_argument('--numNodes', default=int(os.environ['COBALT_JOBSIZE']), type=int)
    parser.add_argument('--procPerNode', default=32, type=int)
parser.add_argument('--repetitions', default=16, type=int)
parser.add_argument('--directory', default=None)
parser.add_argument('--useFileView', default=0, type=int)
parser.add_argument('--jobid', default=None, type=int)
parser.add_argument('--ssd', action='store_true')
parser.add_argument('--fsyncPerWrite', action='store_true')
parser.add_argument('--fsync', action='store_true')
parser.add_argument("--hpctw", action='store_true')
parser.add_argument("--darshan", action='store_true')
parser.add_argument("--intel", action='store_true')
parser.add_argument("--ccio", action='store_true')
args = parser.parse_args()
api=args.api
blockSize=args.blockSize
transferSize=args.transferSize

filePerProc=args.filePerProc
collective=args.collective
lustreStripeCount=args.lustreStripeCount
lustreStripeSize=args.lustreStripeSize
useFileView=args.useFileView
numNodes=args.numNodes


def bytes(string):
    if string.find('m')!=-1:
        return int(string[:-1])*1024*1024
    elif string.find('k')!=-1:
        return int(string[:-1])*1024
    elif string.find('g')!=-1:
        return int(string[:-1])*1024*1024*1024
    else:
        return int(string)
if bytes(blockSize) < bytes(transferSize):
    blockSize = transferSize
    print("Change blocksize to transfersize")
cb_size = bytes(args.cb_size)
if (bytes(args.cb_size)<bytes(args.fs_block_size)):
    args.fs_block_size = args.cb_size
fs_block_size = bytes(args.fs_block_size)


if (args.hpctw):
    os.environ['LD_PRELOAD']='/soft/perftools/hpctw/INTEL/libhpmprof.so'
    os.environ['HPM_PROFILE_THRESHOLD']='0'
if (args.intel):
    os.environ['LD_LIBRARY_PATH']='/home/hzheng/soft/intel//intel64/lib/release_mt:/home/hzheng/soft/intel//intel64/lib:/home/hzheng/soft/intel//intel64/libfabric-ugni/lib:'+os.environ['LD_LIBRARY_PATH']
    os.environ['I_MPI_PMI']="pmi2"
    os.environ['I_MPI_PMI_LIBRARY']="/opt/cray/pe/pmi/default/lib64/libpmi.so"

import numpy.random as rnd
if args.jobid==None:
    jobid="%s.%s"%(time.time(), rnd.randint(10000))
else:
    jobid=args.jobid

if args.directory == None:
    sdir = jobid
else:
    sdir = args.directory
def recMkdir(string):
    dd = os.environ['PWD']
    directories = string.split('/')
    for d in directories:
        os.system("[ -e %s ] || mkdir %s" %(d, d))
        os.chdir(d)
    os.chdir(dd)
if sdir[-1]=='/':
    sdir=sdir[:-1]
recMkdir(sdir)

if socket.gethostname().find("theta")!=-1:
    os.system("lfs setstripe --stripe-size %s --stripe-count  %s %s" %(lustreStripeSize, lustreStripeCount, sdir))
f = open('%s/%s.cfg'%(sdir, jobid), 'w')
f.write("IOR START\n")
f.write("   api=%s\n" %api)
f.write("   blockSize=%s\n"%blockSize)
f.write("   filePerProc=%s\n" %filePerProc)
f.write("   collective=%s\n"%collective)
if socket.gethostname().find("theta")!=-1:
    f.write("   lustreStripeCount=%s\n"%lustreStripeCount)
    f.write("   lustreStripeSize=%s\n"%lustreStripeSize)
    if args.ssd:
        f.write("   testFile=/local/scratch/%s.dat\n"%(jobid))
    else:
        f.write("   testFile=%s.dat\n"%(jobid))
f.write("   verbose=0\n")
f.write("   transferSize=%s\n" %transferSize)
f.write("   repetitions=%s\n"%args.repetitions)
f.write("   keepFile=%s\n"%args.keepFile)
f.write("   useFileView=%s\n"%args.useFileView)
f.write("   reorderTasksConstant=%s\n"%args.procPerNode)
f.write("   fsyncPerWrite=%s\n"%args.fsyncPerWrite)
f.write("   RUN\n")
f.write("IOR STOP\n")
f.close()
f = os.system('cat %s/%s.cfg'%(sdir, jobid))
if args.fsync:
    extra = '-e'
else:
    extra=""
j = max(int(args.procPerNode/64), 1)
if socket.gethostname()=='zion':
    RUN="mpirun -n %s $HOME/soft/hdf5/ccio-abi/bin/ior %s" %(args.procPerNode*args.numNodes, extra)
else:
    RUN="aprun -n %s -N %s -d %s -j %s -cc depth /home/hzheng/soft/hdf5/ccio-abi/bin/ior %s"%(args.procPerNode*args.numNodes, args.procPerNode, 64*j//args.procPerNode, j, extra)
cmd = 'dir=$PWD; cd %s; %s -f %s.cfg >& %s.log; tail %s.log; cd $PWD'%(sdir, RUN, jobid, jobid, jobid)
print(cmd)


if args.ccio:
    os.environ["HDF5_CCIO_ASYNC"]="yes"
    os.environ['HDF5_CCIO_FS_BLOCK_SIZE']=str(fs_block_size)
    os.environ['HDF5_CCIO_FS_BLOCK_COUNT']=str(args.fs_block_count)
    os.environ["HDF5_CCIO_WR"]="yes"
    os.environ["HDF5_CCIO_RD"]="yes"
    os.environ["HDF5_CCIO_DEBUG"]="yes"
    os.environ["HDF5_CCIO_CB_NODES"]=str(args.cb_nodes)
    os.environ["HDF5_CCIO_CB_SIZE"]=str(cb_size)


os.system(cmd)
          
from time import time
run = {}
run['id'] = '%s'%jobid
run['n'] = args.numNodes
run['ppn'] = args.procPerNode
run['api'] = api
run['blockSize'] = blockSize
run['filePerProc'] = filePerProc
run['transferSize'] = transferSize
run['collective'] = collective
run['lustreStripeCount'] = lustreStripeCount
run['lustreStripeSize'] = lustreStripeSize
run['useFileView'] = useFileView
run['directory']=sdir
run['date']="%s"%d
f = open("%s/%s.log" %(sdir, jobid), 'r')
read_to_str(f, 'Summary of all tests')

ma, mi, me, std = read_to_str(f, 'write').split()[1:5]
run['write'] = {'max':ma,
                'min':mi,
                'mean':me,
                'std':std}
ma, mi, me, std = read_to_str(f, 'read').split()[1:5]
run['read'] = {'max':ma,
               'min':mi,
               'mean':me,
               'std':std}
f.close()
import json
data = json.dumps(run)
f = open('%s/%s.json'%(sdir, jobid), 'w')
f.write(data)
f.close()

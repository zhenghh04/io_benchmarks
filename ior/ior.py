#!/usr/bin/env python
# This is to perform the ior benchmarks
# Huihuo Zheng @ ANL
import os
import argparse
import sys
import datetime, time
import socket
d = datetime.datetime.now()
sys.path.append("/home/hzheng/mylibs/python")
from common import *
from numpy.random import randint
parser = argparse.ArgumentParser(description='IOR Benchmark test')
parser.add_argument('--api', default='MPIIO')
parser.add_argument('--blockSize', default='8m')
parser.add_argument('--filePerProc', default=0, type=int)
parser.add_argument('--collective', default=0, type=int)
parser.add_argument('--lustreStripeCount', default=1, type=int)
parser.add_argument('--lustreStripeSize', default='1m')
parser.add_argument('--transferSize', default='8m')
parser.add_argument('--keepFile', default=0, type=int)
if socket.gethostname()=='zion':
    parser.add_argument('--numNodes', default=1, type=int)
    parser.add_argument('--procPerNode', default=2, type=int)
else:
    parser.add_argument('--numNodes', default=int(os.environ['COBALT_JOBSIZE']), type=int)
    parser.add_argument('--procPerNode', default=16, type=int)
parser.add_argument('--repetitions', default=16, type=int)
parser.add_argument('--directory', default=None)
parser.add_argument('--useFileView', default=0, type=int)
parser.add_argument('--jobid', default=None, type=int)
parser.add_argument('--ssd', action='store_true')
parser.add_argument('--fsyncPerWrite', action='store_true')
parser.add_argument('--fsync', action='store_true')
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
    directories = string.split('/')
    for d in directories:
        os.system("[ -e %s ] || mkdir %s" %(d, d))
        os.chdir(d)
    for d in directories:
        os.system("cd ../")
if sdir[-1]=='/':
    sdir=sdir[:-1]
recMkdir(sdir)
os.system("[ -e %s ] || mkdir %s " %(sdir, sdir))
if socket.gethostname().find("theta")!=-1:
    os.system("lfs setstripe --stripe-size %s --count  %s %s" %(lustreStripeSize, lustreStripeCount, sdir))
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
<<<<<<< HEAD

if socket.gethostname()=='zion':
    RUN="mpirun -n %s $HOME/opt/HPC-IOR/bin/ior" %(args.procPerNode*args.numNodes)
    redirect=">& $dir/%s/%s.log; tail $dir/%s/%s.log" %(sdir, jobid, sdir, jobid)
else:
    RUN="aprun -n %s -N %s -d %s -j 1 -cc depth /home/hzheng/ExaHDF5/HPC-IOR-prof-hdf5/install/bin/ior"%(args.procPerNode*args.numNodes, args.procPerNode, 64//args.procPerNode)
    redirect="|& tee $dir/%s/%s.log" %(sdir, jobid)
cmd = 'dir=$PWD; cd %s; %s -f $dir/%s/%s.cfg %s; cd $PWD'%(sdir, RUN, sdir, jobid, redirect)
=======
if args.fsync:
    extra = '-e'
else:
    extra=""
if socket.gethostname()=='zion':
    RUN="mpirun -n %s $HOME/opt/HPC-IOR/bin/ior %s" %(args.procPerNode*args.numNodes, extra)
else:
    RUN="aprun -n %s -N %s -d %s -j 1 -cc depth /home/hzheng/ExaHDF5/HPC-IOR-prof-hdf5/install/bin/ior %s"%(args.procPerNode*args.numNodes, args.procPerNode, 64//args.procPerNode, extra)
cmd = 'dir=$PWD; cd %s; %s -f $dir/%s/%s.cfg >& $dir/%s/%s.log; tail $dir/%s/%s.log; cd $PWD'%(sdir, RUN, sdir, jobid, sdir, jobid, sdir, jobid)
>>>>>>> 9078e20fac168712755750d9d75a05742759c340
print(cmd)

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

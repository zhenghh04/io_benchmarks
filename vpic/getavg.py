#!/usr/bin/env python
import os
import numpy as np
import sys

def getavg(fstr):
    os.system("grep 'just writing' `ls %s` > tmp.dat" %fstr)
    fin = open("tmp.dat").readlines()
    time = [ float(l.split()[0]) for l in fin ] 
    return np.mean(time), np.std(time), np.max(time), np.min(time)
res = getavg(sys.argv[1])
print("| %5.2f | %5.2f | %5.2f | %5.2f "%(res[0], res[1], res[2], res[3]))
if (len(sys.argv)>2):
    nproc = int(sys.argv[2])
    try:
        np = int(sys.argv[3])
    except:
        np=8
    print("Number of particles: %s*1024*1024"%np)
    print("  Number of process: %s"%nproc)
    print("          Bandwidth: %6.2f (%5.2f) MiB/s" %(np*32*nproc/res[0], np*32*nproc/res[0]*res[1]/res[0]))
#print("| %5.2f | %5.2f "%(res[0], res[1]))

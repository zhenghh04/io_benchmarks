#!/usr/bin/env python
import os
import numpy as np
import sys
def getavg(fstr):
    os.system("grep 'just writing' `ls %s*/results` > tmp.dat" %fstr)
    fin = open("tmp.dat").readlines()
    time = [ float(l.split()[0]) for l in fin ] 
    return np.mean(time), np.std(time), np.max(time), np.min(time)
res = getavg(sys.argv[1])
print("| %5.2f | %5.2f | %5.2f | %5.2f "%(res[0], res[1], res[2], res[3]))
#print("| %5.2f | %5.2f "%(res[0], res[1]))

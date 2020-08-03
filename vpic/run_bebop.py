#!/usr/bin/env python
import os
import argparse
os.environ['ROMIO_PRINT_HINTS']='1'
os.environ['LD_LIBRARY_PATH']='/opt/cray/pe/mpt/7.7.14/gni/mpich-intel-abi/16.0/lib:/home/hzheng/soft/hdf5/ccio-abi/lib:'+os.environ["LD_LIBRARY_PATH"]
os.environ['HPM_PROFILE_THRESHOLD']='1400000000'
parser = argparse.ArgumentParser(description='Process some integers.')
parser.add_argument("--num_nodes", type=int, default=1)
parser.add_argument("--ppn", type=int, default=16)
parser.add_argument("--ind", action='store_true')
parser.add_argument("--ccio", action='store_true')
parser.add_argument("--ntrials", type=int, default=1)
parser.add_argument("--cb_nodes", type=int, default=1)
parser.add_argument("--cb_size", type=str, default='16m')
parser.add_argument("--fs_block_size", type=str, default='16m')
parser.add_argument("--fs_block_count", type=int, default=1)
parser.add_argument("--num_particles", type=int, default=8)
parser.add_argument("--stdout", action='store_true')
parser.add_argument("--mpich", action="store_true")
parser.add_argument("--intel", action="store_true")
parser.add_argument("--hpctw", action="store_true")
parser.add_argument("--romio_cb_nodes", type=int, default=None)
parser.add_argument("--romio_cb_size", type=str, default=None)

args = parser.parse_args()
if args.mpich:
    os.environ['LD_LIBRARY_PATH']="/home/hzheng/soft/mpich/3.3.1-intel-2019/lib:"+os.environ['LD_LIBRARY_PATH']
if (args.intel):
    #os.environ['LD_LIBRARY_PATH']='/home/hzheng/soft/intel//intel64/lib/release_mt:/home/hzheng/soft/intel//intel64/lib:/home/hzheng/soft/intel//intel64/libfabric-ugni/lib:'+os.environ['LD_LIBRARY_PATH']
    #os.environ['I_MPI_PMI']="pmi2"
    #os.environ['I_MPI_PMI_LIBRARY']="/opt/cray/pe/pmi/default/lib64/libpmi.so"
    os.environ['I_MPI_DEBUG']='120'
def bytes(string):
    if string.find('m')!=-1:
        return int(string[:-1])*1024*1024
    elif string.find('k')!=-1:
        return int(string[:-1])*1024
    elif string.find('g')!=-1:
        return int(string[:-1])*1024*1024*1024
    else:
        return int(string)
cb_size = bytes(args.cb_size)
fs_block_size = bytes(args.fs_block_size)
if (bytes(args.cb_size)<bytes(args.fs_block_size)):
    args.fs_block_size = args.cb_size
if (args.hpctw):
    os.environ['LD_PRELOAD']='/home/huihuo.zheng/mylibs/libmpitrace.so'

if (args.ind):
    output="ind.n%s.p%s.np%s" %(args.num_nodes, args.ppn, args.num_particles)
else:
    output="n%s.p%s.np%s" %(args.num_nodes, args.ppn, args.num_particles)

if args.ccio:
    output=output+".cn%s.cs%s.fb%s.ccio"%(args.cb_nodes, args.cb_size, args.fs_block_size)
if not os.path.exists(output):
    os.mkdir(output)
execname = "/home/huihuo.zheng/soft/hdf5/ccio/bin/vpicio_uni_h5_"
if (args.ind):
    execname = execname + "ind"
else:
    execname = execname + "col"
if ((args.romio_cb_nodes!=None) or (args.romio_cb_size != None)):
    fin = open("romio_hints", 'w')
    fin.write("romio_cb_write enable\n")
    if (args.romio_cb_nodes!=None):
        fin.write("cb_nodes %s\n"%args.romio_cb_nodes)
        sp = ((args.num_nodes*args.ppn)) // args.romio_cb_nodes
        s = " "
        for i in range(args.romio_cb_nodes):
            s = s+ " %s " %(i*sp)
        fin.write("cb_config_list *:%s\n"%(args.romio_cb_nodes//args.num_nodes))
        fin.write("cb_aggregator_list %s\n"%s)
    else:
        args.romio_cb_nodes=args.num_nodes
    if (args.romio_cb_size!=None):
        fin.write("cb_buffer_size %s\n"%bytes(args.romio_cb_size))
    else:
        args.romio_cb_size='16m'
    fin.close()
    os.environ['ROMIO_HINTS']=os.environ['PWD']+"/romio_hints"
    output = output + ".rcn%s.rcs%s"%(args.romio_cb_nodes, args.romio_cb_size)

if args.ccio:
    os.environ["HDF5_CCIO_ASYNC"]="yes"
    os.environ['HDF5_CCIO_FS_BLOCK_SIZE']=str(fs_block_size)
    os.environ['HDF5_CCIO_FS_BLOCK_COUNT']=str(args.fs_block_count)
    os.environ["HDF5_CCIO_WR"]="yes"
    os.environ["HDF5_CCIO_RD"]="yes"
    os.environ["HDF5_CCIO_DEBUG"]="yes"
    os.environ["HDF5_CCIO_CB_NODES"]=str(args.cb_nodes)
    os.environ["HDF5_CCIO_CB_SIZE"]=str(cb_size)
os.environ['I_MPI_EXTRA_FILESYSTEM']='on'
os.environ['I_MPI_EXTRA_FILESYSTEM_LIST']='gpfs'
cmd = "srun -n %s --ntasks-per-node %s %s testFile %s " %(args.num_nodes*args.ppn, args.ppn, execname, args.num_particles)
print(cmd)
for i in range(args.ntrials):
    if (args.stdout):
        os.system(cmd)
    else:
        os.system(cmd+">> %s/results"%(output))
    os.system("rnf mpi_profile. mpi_profile_%s."%i)
    if (not args.stdout):
        os.system("mv mpi_profile_%s.* %s"%(i, output))
    os.system("rm -rf testFile")
if (not args.stdout):
    os.system("python /getavg.py %s/results"%(output))
print("GB: ", args.ppn*args.num_nodes*args.num_particles*4*8/1024.)

EXE=vpicio_uni_h5_ind

# Mac
HDF5_ROOT=/Users/rzamora/IO/CCIO
BLD_DIR=mpich
MPICC=mpicc

default: ${EXE}

HDF5_INSTALL_DIR=${HDF5_ROOT}/library/install/${BLD_DIR}

vpicio_uni_h5.o: vpicio_uni_h5.c
	${MPICC} -c -g -O3 -I${HDF5_INSTALL_DIR}/include vpicio_uni_h5.c -o vpicio_uni_h5.o

${EXE}: vpicio_uni_h5.o
	${MPICC} vpicio_uni_h5.o -o ${EXE} -L${HDF5_INSTALL_DIR}/lib -lhdf5 -lz

clean:
	rm -f vpicio_uni_h5.o
	rm -f ${EXE}

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <iostream>
#include <fstream>
#include "time.h"
using namespace std; 

int fail(char *filename, int linenumber) { 
  fprintf(stderr, "%s:%d %s\n", filename, linenumber, strerror(errno)); 
  exit(1);
  return 0; /*Make compiler happy */
}
#define QUIT fail(__FILE__, __LINE__ )

int main(int argc, char *argv[]) {
  int i=1;
  int dim=1024*1024*1024;
  char *ssd = "/local/scratch/data";
  while (i<argc) {
    if (strcmp(argv[i], "--dim") == 0) {
      dim = atoi(argv[i+1]); 
      i+=2;
      printf("Dimension is %d\n", dim); 
    } else if (strcmp(argv[i], "--SSD") == 0) {
      ssd = argv[i+1];
      printf("SSD location: %s\n", ssd); 
      i+=2;
    } else {
      i++; 
    }
  }

  long size = sizeof(int) * dim;
  cout << "Total amount of data: " << size/1024./1024. << " MB" << endl;

  clock_t t0 = clock(); 
  int *myarray = new int [dim];
  for(int i=0; i<dim; i++) {
    myarray[i] = i; 
  }
  t0 = clock();
  ofstream myfile;
  myfile.open("data_lustre2",  ofstream::binary);
  myfile.write((char*)myarray, size);
  myfile.close();
  clock_t t1 = clock();
  cout << "-----------------------------------------------" << endl; 
  cout << "Memory->Lustre Write Rate: " << float(t1 - t0)/CLOCKS_PER_SEC << " seconds" << endl;
  cout << "Memory->Lustre Write Rate: " << size/(float(t1 - t0)/CLOCKS_PER_SEC)/1024/1024 << " MB/sec" << endl;
  cout << "-----------------------------------------------" << endl; 
  //staging time
  t0 = clock(); 
  int fd = open(ssd, O_RDWR | O_CREAT | O_TRUNC, 0600); //6 = read+write for me!
  lseek(fd, size, SEEK_SET);
  write(fd, "A", 1);
  void *addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  if (addr == (void*) -1 ) QUIT;
  int *array = (int*) addr;
  for(int i=0; i<dim; i++)
    array[i] = i;
  t1 = clock();
  munmap(addr, size);

  cout << "Memory->SSD Write Time: " << float(t1 - t0)/CLOCKS_PER_SEC << " seconds" << endl;
  cout << "Memory->SSD Write Rate: " << size/(float(t1 - t0)/CLOCKS_PER_SEC)/1024/1024 << " MB/sec" << endl;
  cout << "---------------------------------------------" << endl;
  addr = mmap(NULL, size, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
  t0 = clock();
  myfile.open("data_lustre",  ofstream::binary);
  myfile.write((char*)array, size);
  myfile.close();
  t1 = clock();
  cout << "SSD->Lustre Write Time: " << float(t1 - t0)/CLOCKS_PER_SEC << " seconds" << endl;
  cout << "SSD->Lustre Write Rate: " << size/(float(t1 - t0)/CLOCKS_PER_SEC)/1024/1024 << " MB/sec" << endl;
  munmap(addr, size);
  cout << "---------------------------------------------" << endl;
  return 0;
  
}

#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include "timing.h"
#include <string.h>
//#include "pthread_barrier.h"

#define NUM_THREADS 2
using namespace std;
Timing tt; 
/* create thread argument struct for thr_func() */
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER; 
pthread_cond_t cond0 = PTHREAD_COND_INITIALIZER; 

// declaring mutex 
pthread_mutex_t count_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cond_lock = PTHREAD_MUTEX_INITIALIZER;
pthread_mutex_t cond0_lock = PTHREAD_MUTEX_INITIALIZER;

typedef struct _thread_data_t {
  int fd; 
  const void *buf;
  size_t nbyte;
  size_t offset; 
  struct _thread_data_t *next; 
} thread_data_t;


/* thread function */
thread_data_t thr_data;
thread_data_t *REQUEST_LIST=NULL, *REQUEST_HEAD=NULL;
int num_request = 0;

void *thr_func(void *arg) {
  pthread_mutex_lock(&count_lock);
  while(num_request >= 0) {
    if (num_request>0) {
      thread_data_t *data = REQUEST_HEAD;
      pwrite(data->fd, data->buf, data->nbyte, data->offset);
      fsync(data->fd);
      REQUEST_HEAD=REQUEST_HEAD->next; 
      num_request--;
    } 
    if (num_request==0) {
      pthread_cond_signal(&cond0);
      pthread_cond_wait(&cond1, &count_lock); 
    }
  }
  pthread_mutex_unlock(&count_lock);
  return NULL; 
}

pthread_t SSD_CACHE_PTHREAD;
int rc = pthread_create(&SSD_CACHE_PTHREAD, NULL, thr_func, NULL);

int pwrite_thread(int fd, const void *buf, size_t nbyte, size_t offset) {
  REQUEST_LIST->fd = fd;
  REQUEST_LIST->buf = buf; 
  REQUEST_LIST->nbyte = nbyte; 
  REQUEST_LIST->offset = offset; 
  REQUEST_LIST->next = (thread_data_t*) malloc(sizeof(thread_data_t)); 
  REQUEST_LIST = REQUEST_LIST->next; 
  sleep(1);
  pthread_mutex_lock(&count_lock);
  num_request++;
  rc = pthread_cond_signal(&cond1);
  pthread_mutex_unlock(&count_lock);

  return 0; 
}

int OPEN(const char *pathname, int flags, mode_t mode) {
    REQUEST_LIST = (thread_data_t*) malloc(sizeof(thread_data_t)); 
    pthread_mutex_lock(&count_lock);
    REQUEST_HEAD = REQUEST_LIST; 
    pthread_mutex_unlock(&count_lock);
    return open(pathname, flags, mode);  
}

int CLOSE(int fd) {
  pthread_mutex_lock(&count_lock);
  while (num_request>0) {
    rc = pthread_cond_signal(&cond1);
    pthread_cond_wait(&cond0, &count_lock); 
  }
  pthread_mutex_unlock(&count_lock);
  close(fd);
  return 0; 
}

int main(int argc, char **argv) {
  tt.start_clock("open");
  int fd1 = OPEN("test.dat", O_RDWR | O_CREAT, 0600);
  tt.stop_clock("open");
  int dim = 102457600;
  int *array = new int [dim];
  int niter = 10;
  for(int i=1; i<argc; i++) {
    if (strcmp(argv[i], "--dim") == 0) {
      dim = atoi(argv[i+1]); i+=1;
    } else if (strcmp(argv[i], "--niter") == 0) {
      niter = atoi(argv[i+1]); 
      i+=1;
    }
  }
  for (int i=0; i<dim; i++)
    array[i] = 1;
  for(int i=0; i<niter; i++) {
    tt.start_clock("WRITE");
    pwrite_thread(fd1, array, sizeof(int)*dim, i*sizeof(int)*dim);
    tt.stop_clock("WRITE");
  }
  tt.start_clock("close");
  CLOSE(fd1);
  fsync(fd1);
  delete [] array;
  tt.stop_clock("close");
  tt.PrintTiming();
  //CLOSE(fd); 
  /* create a thread_data_t argument array */
  return 0;
}

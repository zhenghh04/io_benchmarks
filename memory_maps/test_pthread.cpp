#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sys/stat.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <unistd.h>
#include <iostream>
#include "timing.h"
#include "pthread_barrier.h"


#define NUM_THREADS 2
using namespace std;
Timing tt; 
/* create thread argument struct for thr_func() */
pthread_cond_t cond1 = PTHREAD_COND_INITIALIZER; 
// declaring mutex 
pthread_mutex_t mylock = PTHREAD_MUTEX_INITIALIZER;
bool WAIT = false;

pthread_barrier_t mybarrier;
pthread_barrier_init(&mybarrier, NULL, 2);

typedef struct _thread_data_t {
  int fd; 
  const void *buf;
  size_t nbyte;
  size_t offset; 
} thread_data_t;

/* thread function */
int done = 1; 
void *thr_func(void *arg) {
  thread_data_t *data = (thread_data_t *)arg;
  pthread_mutex_lock(&mylock);
  if (done == 1) { 
    // let's wait on conition variable cond1 
    done = 2; 
    printf("Waiting on condition variable cond1\n"); 
    pthread_cond_wait(&cond1, &mylock); 
  } 
  else { 
    // Let's signal condition variable cond1 
    printf("Signaling condition variable cond1\n"); 
    pthread_cond_signal(&cond1); 
  } 
  pthread_mutex_unlock(&mylock);
  WAIT=true;
  pwrite(data->fd, data->buf, data->nbyte, data->offset);
  sleep(1);
  fsync(data->fd);
  WAIT=false;
  pthread_barrier_wait(&mybarrier);
  pthread_mutex_lock(&mylock);
  done = 1;
  return NULL; 
}

void *master_wait() {
  if (WAIT) { 
    // let's wait on conition variable cond1 
    done = 2; 
    printf("Waiting on condition variable cond1\n"); 
    pthread_cond_wait(&cond1, &mylock); 
  } 
  else { 
    // Let's signal condition variable cond1 
    printf("Signaling condition variable cond1\n"); 
    pthread_cond_signal(&cond1); 
  } 
  pthread_mutex_unlock(&mylock);
  done = 1;
}

pthread_t SSD_CACHE_PTHREAD;
thread_data_t thr_data;
int rc = pthread_create(&SSD_CACHE_PTHREAD, NULL, thr_func, &thr_data);

int pwrite_thread(int fd, const void *buf, size_t nbyte, size_t offset) {
  thr_data.fd = fd; 
  thr_data.buf = buf;
  thr_data.nbyte = nbyte;
  thr_data.offset = offset; 
  done = 2;

  rc = pthread_cond_broadcast(&cond1);
  return 0; 
}
int CLOSE(int fd) {
  pthread_join(SSD_CACHE_PTHREAD, NULL);
  close(fd);
  return 0; 
}

int main(int argc, char **argv) {
  int fd1 = open("test.dat", O_RDWR | O_CREAT, 0600);
  int array[1024];
  int dim = 102457600; 
  array[0] = 1;

  for(int i=0; i<4; i++) {
    tt.start_clock("WRITE");
    pwrite_thread(fd1, array, sizeof(int)*dim, i*sizeof(int)*dim);
    pthread_barrier_wait(&mybarrier);
    tt.stop_clock("WRITE");
  }

  tt.start_clock("close");
  CLOSE(fd1);
  fsync(fd1);
  tt.stop_clock("close");
  tt.PrintTiming();
  //CLOSE(fd); 
  /* create a thread_data_t argument array */
  return 0;
}

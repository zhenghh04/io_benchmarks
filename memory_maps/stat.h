#ifndef STAT_H_
#define STAT_H_
#include <stdio>
#include <cmath>
template <typename T>
void stat ( T *array, int n, T &avg, T &std, type='n')
{
  T x, xx;
  x=0;
  xx=0; 
  if (type=='n') {
      for(int i=0; i<n; i++) {
        x += array[i];
        xx += array[i]*array[i];
      }
  } else {
      for(int i=0; i<n; i++) {
        x += 1.0/array[i];
        xx += 1.0/array[i]*array[i];
      }
  }
  avg = x/n;
  std = sqrt(xx/n - avg*avg);
  return 0;
}

#endif
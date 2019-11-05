#ifndef TIMING_H__
#define TIMING_H__
#include <iostream>
#include <time.h>
#include <vector>
#include <string>
#include <stdio.h>
using namespace std; 
double get_time_diff_secs (struct timeval& start, struct timeval& end)
{
  return (double)(end.tv_sec - start.tv_sec) + (double) (end.tv_usec - start.tv_usec)/1000000;
}

struct Timer {
public:
  string name; 
  double t; 
  struct timeval start_t,  end_t; 
  int num_call; 
  bool open; 
}; 

class Timing {
 public:
  vector<Timer> T; 
  void start_clock(string str) {
    int ind =FindIndice(str); 
    if (ind==T.size()) {
      Timer t1; 
      t1.name = str; 
      t1.t = 0.0; 
      t1.num_call = 1; 
      t1.open = true; 
      gettimeofday(&t1.start_t, 0); 
      T.push_back(t1); 
    } else if (T[ind].open)
      cout << "Timer " + str +" is already open." << endl; 
    else {
      T[ind].num_call++; 
      gettimeofday(&T[ind].start_t,0); 
      T[ind].open = true; 
    }
  }
  void stop_clock(string str) {
    int ind = FindIndice(str); 
    if (ind == T.size()) {
      cout << "No timer named " + str + " has been started." << endl; 
      return; 
    } else {
      if (T[ind].open) {
	gettimeofday(&T[ind].end_t,0); 
	T[ind].t += get_time_diff_secs(T[ind].start_t, T[ind].end_t);
	T[ind].open = false; 
	return; 
      } else {
	cout << "Timer " + str + " has already been closed." << endl; 
	return; 
      }
    }
  }
  Timer& operator[] (string str) {
    int ind= FindIndice(str);
    return T[ind]; 
  }
  int FindIndice(string str) {
    bool find = false; 
    int i = 0; 
    while (i < T.size()) {
      if (T[i].name == str) {
	return i; 
      } else {
	i++; 
      }
    }
    return i; 
  }
  int PrintTiming(bool master=true) {
    if (master) {
      cout << "\n***************** Timing Information *********************" << endl; 
      for(int i = 0; i<T.size(); i++) {
	printf("*   %-10s:       %4.8f sec        ( %-3d call  )\n", T[i].name.c_str(), T[i].t, T[i].num_call); 
      }
      cout << "**********************************************************\n" << endl; 
    }
    return 0; 
  }
}; 

#endif

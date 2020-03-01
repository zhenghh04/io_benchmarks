#ifndef DEBUG_H__
#define DEBUG_H__
#include "stdlib.h"
#include "stdio.h"
int debug_level() {
  if (getenv("DEBUG")!=NULL) return int(atof(getenv("DEBUG")));
  else return 0; 
}

bool io_node() {
  if (getenv("IO_NODE")!=NULL) return int(atof(getenv("IO_NODE")));
  else return 0; 
}
#endif

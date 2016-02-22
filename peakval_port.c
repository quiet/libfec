/* Portable C version of peakval
 * Copyright 2004 Phil Karn, KA9Q
 */
#include <stdlib.h>
#include "fec.h"
int peakval_port(signed short *b,int len){
  int peak = 0;
  int a,i;

  for(i=0;i<len;i++){
    a = abs(b[i]);
    if(a > peak)
      peak = a;
  }
  return peak;
}

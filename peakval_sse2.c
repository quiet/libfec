/* Portable C version of peakval
 * Copyright 2004 Phil Karn, KA9Q
 */
#include <stdlib.h>
#include "fec.h"

int peakval_sse2_assist(signed short *,int);

int peakval_sse2(signed short *b,int cnt){
  int peak = 0;
  int a;

  while(((int)b & 15) != 0 && cnt != 0){
    a = abs(*b);
    if(a > peak)
      peak = a;
    b++;
    cnt--;
  }
  a = peakval_sse2_assist(b,cnt);
  if(a > peak)
    peak = a;
  b += cnt & ~7;
  cnt &= 7;

  while(cnt != 0){
    a = abs(*b);
    if(a > peak)
      peak = a;
    b++;
    cnt--;
  }
  return peak;
}  

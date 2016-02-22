/* Wrapper for the MMX version of peakval
 * Copyright 2004 Phil Karn, KA9Q
 */

#include <stdlib.h>

int peakval_mmx_assist(signed short *,int);

int peakval_mmx(signed short *b,int cnt){
  int peak = 0;
  int a;

  while(((int)b & 7) != 0 && cnt != 0){
    a = abs(*b);
    if(a > peak)
      peak = a;
    b++;
    cnt--;
  }
  a = peakval_mmx_assist(b,cnt);
  if(a > peak)
    peak = a;
  b += cnt & ~3;
  cnt &= 3;

  while(cnt != 0){
    a = abs(*b);
    if(a > peak)
      peak = a;
    b++;
    cnt--;
  }
  return peak;
}  

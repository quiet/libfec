/* Return the largest absolute value of a vector of signed shorts

 * This is the Altivec SIMD version.

 * Copyright 2004 Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */

#include "fec.h"

signed short peakval_av(signed short *in,int cnt){
  vector signed short x;
  int pad;
  union { vector signed char cv; vector signed short hv; signed short s[8]; signed char c[16];} s;
  vector signed short smallest,largest;

  smallest = (vector signed short)(0);
  largest = (vector signed short)(0);
  if((pad = (int)in & 15)!=0){
    /* Load unaligned leading word */
    x = vec_perm(vec_ld(0,in),(vector signed short)(0),vec_lvsl(0,in));
    if(cnt < 8){ /* Shift right to chop stuff beyond end of short block */
      s.c[15] = (8-cnt)<<4;
      x = vec_sro(x,s.cv);
    }
    smallest = vec_min(smallest,x);
    largest = vec_max(largest,x);
    in += 8-pad/2;
    cnt -= 8-pad/2;
  }
  /* Everything is now aligned, rip through most of the block */
  while(cnt >= 8){
    x = vec_ld(0,in);
    smallest = vec_min(smallest,x);
    largest = vec_max(largest,x);
    in += 8;
    cnt -= 8;
  }
  /* Handle trailing fragment, if any */
  if(cnt > 0){
    x = vec_ld(0,in);
    s.c[15] = (8-cnt)<<4;
    x = vec_sro(x,s.cv);
    smallest = vec_min(smallest,x);
    largest = vec_max(largest,x);
  }
  /* Combine and extract result */
  largest = vec_max(largest,vec_abs(smallest));

  s.c[15] = 64; /* Shift right four 16-bit words */
  largest = vec_max(largest,vec_sro(largest,s.cv));

  s.c[15] = 32; /* Shift right two 16-bit words */
  largest = vec_max(largest,vec_sro(largest,s.cv));

  s.c[15] = 16; /* Shift right one 16-bit word */
  largest = vec_max(largest,vec_sro(largest,s.cv));

  s.hv = largest;
  return s.s[7];
}

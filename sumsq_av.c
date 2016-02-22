/* Compute the sum of the squares of a vector of signed shorts

 * This is the Altivec SIMD version. It's a little hairy because Altivec
 * does not do 64-bit operations directly, so we have to accumulate separate
 * 32-bit sums and carries

 * Copyright 2004 Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */

#include "fec.h"

unsigned long long sumsq_av(signed short *in,int cnt){
  long long sum;
  vector signed short x;
  vector unsigned int sums,carries,s1,s2;
  int pad;
  union { vector unsigned char cv; vector unsigned int iv; unsigned int w[4]; unsigned char c[16];} s;

  carries = sums = (vector unsigned int)(0);
  if((pad = (int)in & 15)!=0){
    /* Load unaligned leading word */
    x = vec_perm(vec_ld(0,in),(vector signed short)(0),vec_lvsl(0,in));
    if(cnt < 8){ /* Shift right to chop stuff beyond end of short block */
      s.c[15] = (8-cnt)<<4;
      x = vec_sro(x,s.cv);
    }
    sums = (vector unsigned int)vec_msum(x,x,(vector signed int)(0));
    in += 8-pad/2;
    cnt -= 8-pad/2;
  }
  /* Everything is now aligned, rip through most of the block */
  while(cnt >= 8){
    x = vec_ld(0,in);
    /* A single vec_msum cannot overflow, but we have to sum it with
     * the earlier terms separately to handle the carries
     * The cast to unsigned is OK because squares are always positive
     */
    s1 = (vector unsigned int)vec_msum(x,x,(vector signed int)(0));
    carries = vec_add(carries,vec_addc(sums,s1));
    sums = vec_add(sums,s1);
    in += 8;
    cnt -= 8;
  }
  /* Handle trailing fragment, if any */
  if(cnt > 0){
    x = vec_ld(0,in);
    s.c[15] = (8-cnt)<<4;
    x = vec_sro(x,s.cv);
    s1 = (vector unsigned int)vec_msum(x,x,(vector signed int)(0));
    carries = vec_add(carries,vec_addc(sums,s1));
    sums = vec_add(sums,s1);
  }
  /* Combine 4 sub-sums and carries */
  s.c[15] = 64; /* Shift right two 32-bit words */
  s1 = vec_sro(sums,s.cv);
  s2 = vec_sro(carries,s.cv);
  carries = vec_add(carries,vec_addc(sums,s1));
  sums = vec_add(sums,s1);
  carries = vec_add(carries,s2);

  s.c[15] = 32; /* Shift right one 32-bit word */
  s1 = vec_sro(sums,s.cv);
  s2 = vec_sro(carries,s.cv);
  carries = vec_add(carries,vec_addc(sums,s1));
  sums = vec_add(sums,s1);
  carries = vec_add(carries,s2);

  /* Extract sum and carries from right-hand words and combine into result */
  s.iv = sums;
  sum = s.w[3];

  s.iv = carries;
  sum += (long long)s.w[3] << 32;

  return sum;
}


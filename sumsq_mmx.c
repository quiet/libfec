/* Compute the sum of the squares of a vector of signed shorts

 *  MMX-assisted version (also used on SSE)

 * The SSE2 and MMX assist routines both operate on multiples of
 * 8 words; they differ only in their alignment requirements (8 bytes
 * for MMX, 16 bytes for SSE2)

 * Copyright 2004 Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser Public License (LGPL)
 */

long long sumsq_mmx_assist(signed short *,int);

long long sumsq_mmx(signed short *in,int cnt){
  long long sum = 0;

  /* Handle stuff before the next 8-byte boundary */
  while(((int)in & 7) != 0 && cnt != 0){
    sum += (long)in[0] * in[0];
    in++;
    cnt--;
  }
  sum += sumsq_mmx_assist(in,cnt);
  in += cnt & ~7;
  cnt &= 7;

  /* Handle up to 7 words at end */
  while(cnt != 0){
    sum += (long)in[0] * in[0];
    in++;
    cnt--;
  }
  return sum;
}

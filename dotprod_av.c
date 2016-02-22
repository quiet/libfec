/* 16-bit signed integer dot product
 * Altivec-assisted version
 * Copyright 2004 Phil Karn
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <stdlib.h>
#include "fec.h"

struct dotprod {
  int len; /* Number of coefficients */

  /* On an Altivec machine, these hold 8 copies of the coefficients,
   * preshifted by 0,1,..7 words to meet all possible input data
   */
  signed short *coeffs[8];
};

/* Create and return a descriptor for use with the dot product function */
void *initdp_av(signed short coeffs[],int len){
  struct dotprod *dp;
  int i,j;

  if(len == 0)
    return NULL;

  dp = (struct dotprod *)calloc(1,sizeof(struct dotprod));
  dp->len = len;

  /* Make 8 copies of coefficients, one for each data alignment,
   * each aligned to 16-byte boundary
   */
  for(i=0;i<8;i++){
    dp->coeffs[i] = calloc(1+(len+i-1)/8,sizeof(vector signed short));
    for(j=0;j<len;j++)
      dp->coeffs[i][j+i] = coeffs[j];
  }
  return (void *)dp;
}


/* Free a dot product descriptor created earlier */
void freedp_av(void *p){
  struct dotprod *dp = (struct dotprod *)p;
  int i;

  for(i=0;i<8;i++)
    if(dp->coeffs[i] != NULL)
      free(dp->coeffs[i]);
  free(dp);
}

/* Compute a dot product given a descriptor and an input array
 * The length is taken from the descriptor
 */
long dotprod_av(void *p,signed short a[]){
  struct dotprod *dp = (struct dotprod *)p;
  int al;
  vector signed short *ar,*d;
  vector signed int sums0,sums1,sums2,sums3;
  union { vector signed int v; signed int w[4];} s;
  int nblocks;
    
  /* round ar down to beginning of 16-byte block containing 0th element of
   * input buffer. Then set d to one of 8 sets of shifted coefficients
   */
  ar = (vector signed short *)((int)a & ~15);
  al = ((int)a & 15)/sizeof(signed short);
  d = (vector signed short *)dp->coeffs[al];
  
  nblocks = (dp->len+al-1)/8+1;
  
  /* Sum into four vectors each holding four 32-bit partial sums */
  sums3 = sums2 = sums1 = sums0 = (vector signed int)(0);
  while(nblocks >= 4){
    sums0 = vec_msums(ar[nblocks-1],d[nblocks-1],sums0);
    sums1 = vec_msums(ar[nblocks-2],d[nblocks-2],sums1);
    sums2 = vec_msums(ar[nblocks-3],d[nblocks-3],sums2);
    sums3 = vec_msums(ar[nblocks-4],d[nblocks-4],sums3);
    nblocks -= 4;
  }
  sums0 = vec_adds(sums0,sums1);
  sums2 = vec_adds(sums2,sums3);
  sums0 = vec_adds(sums0,sums2);
  while(nblocks-- > 0){
    sums0 = vec_msums(ar[nblocks],d[nblocks],sums0);
  }
  /* Sum 4 partial sums into final result */
  s.v = vec_sums(sums0,(vector signed int)(0));
  
  return s.w[3];
}



/* 16-bit signed integer dot product
 * SSE2 version
 * Copyright 2004 Phil Karn
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#define _XOPEN_SOURCE 600
#include <stdlib.h>
#include <memory.h>
#include "fec.h"

struct dotprod {
  int len; /* Number of coefficients */

  /* On a SSE2 machine, these hold 8 copies of the coefficients,
   * preshifted by 0,1,..7 words to meet all possible input data
   * alignments (see Intel ap559 on MMX dot products).
   */
  signed short *coeffs[8];
};

long dotprod_sse2_assist(signed short *a,signed short *b,int cnt);

/* Create and return a descriptor for use with the dot product function */
void *initdp_sse2(signed short coeffs[],int len){
  struct dotprod *dp;
  int i,j,blksize;

  if(len == 0)
    return NULL;

  dp = (struct dotprod *)calloc(1,sizeof(struct dotprod));
  dp->len = len;

  /* Make 8 copies of coefficients, one for each data alignment,
   * each aligned to 16-byte boundary
   */
  for(i=0;i<8;i++){
    blksize = (1+(len+i-1)/8) * 8*sizeof(signed short);
    posix_memalign((void **)&dp->coeffs[i],16,blksize);
    memset(dp->coeffs[i],0,blksize);
    for(j=0;j<len;j++)
      dp->coeffs[i][j+i] = coeffs[j];
  }
  return (void *)dp;
}


/* Free a dot product descriptor created earlier */
void freedp_sse2(void *p){
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
long dotprod_sse2(void *p,signed short a[]){
  struct dotprod *dp = (struct dotprod *)p;
  int al;
  signed short *ar;
  
  ar = (signed short *)((int)a & ~15);
  al = a - ar;
  
  /* Call assembler routine to do the work, passing number of 8-word blocks */
  return dotprod_sse2_assist(ar,dp->coeffs[al],(dp->len+al-1)/8+1);
}

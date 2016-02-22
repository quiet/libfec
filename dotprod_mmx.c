/* 16-bit signed integer dot product
 * MMX assisted version; also for SSE
 *
 * Copyright 2004 Phil Karn
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <stdlib.h>
#include "fec.h"

struct dotprod {
  int len; /* Number of coefficients */

  /* On a MMX or SSE machine, these hold 4 copies of the coefficients,
   * preshifted by 0,1,2,3 words to meet all possible input data
   * alignments (see Intel ap559 on MMX dot products).
   */
  signed short *coeffs[4];
};
long dotprod_mmx_assist(signed short *a,signed short *b,int cnt);

/* Create and return a descriptor for use with the dot product function */
void *initdp_mmx(signed short coeffs[],int len){
  struct dotprod *dp;
  int i,j;


  if(len == 0)
    return NULL;

  dp = (struct dotprod *)calloc(1,sizeof(struct dotprod));
  dp->len = len;

  /* Make 4 copies of coefficients, one for each data alignment */
  for(i=0;i<4;i++){
    dp->coeffs[i] = (signed short *)calloc(1+(len+i-1)/4,
					   4*sizeof(signed short));
    for(j=0;j<len;j++)
      dp->coeffs[i][j+i] = coeffs[j];
  }
  return (void *)dp;
}


/* Free a dot product descriptor created earlier */
void freedp_mmx(void *p){
  struct dotprod *dp = (struct dotprod *)p;
  int i;

  for(i=0;i<4;i++)
    if(dp->coeffs[i] != NULL)
      free(dp->coeffs[i]);
  free(dp);
}

/* Compute a dot product given a descriptor and an input array
 * The length is taken from the descriptor
 */
long dotprod_mmx(void *p,signed short a[]){
  struct dotprod *dp = (struct dotprod *)p;
  int al;
  signed short *ar;
      
  /* Round input data address down to 8 byte boundary
   * NB: depending on the alignment of a[], memory
   * before a[] will be accessed. The contents don't matter since they'll
   * be multiplied by zero coefficients. I can't conceive of any
   * situation where this could cause a segfault since memory protection
   * in the x86 machines is done on much larger boundaries
   */
  ar = (signed short *)((int)a & ~7);
  
  /* Choose one of 4 sets of pre-shifted coefficients. al is both the
   * index into dp->coeffs[] and the number of 0 words padded onto
   * that coefficients array for alignment purposes
   */
  al = a - ar;
  
  /* Call assembler routine to do the work, passing number of 4-word blocks */
  return dotprod_mmx_assist(ar,dp->coeffs[al],(dp->len+al-1)/4+1);
}


/* 16-bit signed integer dot product
 * Portable C version
 * Copyright 2004 Phil Karn
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <stdlib.h>
#include "fec.h"

struct dotprod {
  int len; /* Number of coefficients */

  signed short *coeffs;
};

/* Create and return a descriptor for use with the dot product function */
void *initdp_port(signed short coeffs[],int len){
  struct dotprod *dp;
  int j;

  if(len == 0)
    return NULL;

  dp = (struct dotprod *)calloc(1,sizeof(struct dotprod));
  dp->len = len;

  /* Just one copy of the coefficients for the C version */
  dp->coeffs = (signed short *)calloc(len,sizeof(signed short));
  for(j=0;j<len;j++)
    dp->coeffs[j] = coeffs[j];
  return (void *)dp;
}


/* Free a dot product descriptor created earlier */
void freedp_port(void *p){
  struct dotprod *dp = (struct dotprod *)p;

  if(dp->coeffs != NULL)
      free(dp->coeffs);
  free(dp);
}

/* Compute a dot product given a descriptor and an input array
 * The length is taken from the descriptor
 */
long dotprod_port(void *p,signed short a[]){
  struct dotprod *dp = (struct dotprod *)p;
  long corr;
  int i;

  corr = 0;
  for(i=0;i<dp->len;i++){
    corr += (long)a[i] * dp->coeffs[i];
  }
  return corr;
}



/* K=9 r=1/2 Viterbi decoder for SSE
 * Copyright Feb 2004, Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <xmmintrin.h>
#include "fec.h"

typedef union { unsigned char w[256]; __m64 v[32];} metric_t;
typedef union { unsigned long w[8]; unsigned char c[32]; __m64 v[4];} decision_t;

union branchtab29 { unsigned char c[128]; } Branchtab29_sse[2];
static int Init = 0;

/* State info for instance of Viterbi decoder
 * Don't change this without also changing references in [mmx|sse|sse2]bfly29.s!
 */
struct v29 {
  metric_t metrics1; /* path metric buffer 1 */
  metric_t metrics2; /* path metric buffer 2 */
  decision_t *dp;          /* Pointer to current decision */
  metric_t *old_metrics,*new_metrics; /* Pointers to path metrics, swapped on every bit */
  decision_t *decisions;   /* Beginning of decisions for block */
};

/* Create a new instance of a Viterbi decoder */
void *create_viterbi29_sse(int len){
  struct v29 *vp;

  if(!Init){
    int polys[2] = { V29POLYA,V29POLYB };

    set_viterbi29_polynomial_sse(polys);
  }
  if((vp = (struct v29 *)malloc(sizeof(struct v29))) == NULL)
    return NULL;
  if((vp->decisions = (decision_t *)malloc((len+8)*sizeof(decision_t))) == NULL){
    free(vp);
    return NULL;
  }
  init_viterbi29(vp,0);
  return vp;
}

void set_viterbi29_polynomial_sse(int polys[2]){
  int state;

  for(state=0;state < 128;state++){
    Branchtab29_sse[0].c[state] = (polys[0] < 0) ^ parity((2*state) & abs(polys[0])) ? 255 : 0;
    Branchtab29_sse[1].c[state] = (polys[1] < 0) ^ parity((2*state) & abs(polys[1])) ? 255 : 0;
  }
  Init++;
}

/* Initialize Viterbi decoder for start of new frame */
int init_viterbi29_sse(void *p,int starting_state){
  struct v29 *vp = p;
  int i;

  if(p == NULL)
    return -1;
  for(i=0;i<256;i++)
    vp->metrics1.w[i] = 200;

  vp->old_metrics = &vp->metrics1;
  vp->new_metrics = &vp->metrics2;
  vp->dp = vp->decisions;
  vp->old_metrics->w[starting_state & 255] = 0; /* Bias known start state */
  return 0;
}

/* Viterbi chainback */
int chainback_viterbi29_sse(
      void *p,
      unsigned char *data, /* Decoded output data */
      unsigned int nbits, /* Number of data bits */
      unsigned int endstate){ /* Terminal encoder state */
  struct v29 *vp = p;
  decision_t *d;

  if(p == NULL)
    return -1;
  d = vp->decisions;
  /* Make room beyond the end of the encoder register so we can
   * accumulate a full byte of decoded data
   */
  endstate %= 256;

  /* The store into data[] only needs to be done every 8 bits.
   * But this avoids a conditional branch, and the writes will
   * combine in the cache anyway
   */
  d += 8; /* Look past tail */
  while(nbits-- != 0){
    int k;
    
    k = (d[nbits].c[endstate/8] >> (endstate%8)) & 1;
    data[nbits>>3] = endstate = (endstate >> 1) | (k << 7);
  }
  return 0;
}


/* Delete instance of a Viterbi decoder */
void delete_viterbi29_sse(void *p){
  struct v29 *vp = p;

  if(vp != NULL){
    free(vp->decisions);
    free(vp);
  }
}

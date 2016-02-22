/* K=7 r=1/2 Viterbi decoder for SSE
 * Feb 2004, Phil Karn, KA9Q
 */
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <xmmintrin.h>
#include "fec.h"

typedef union { unsigned char c[64]; } metric_t;
typedef union { unsigned long w[2]; unsigned char c[8]; __m64 v[1];} decision_t;
union branchtab27 { unsigned char c[32]; __m64 v[4];} Branchtab27_sse[2];
static int Init = 0;

/* State info for instance of Viterbi decoder
 * Don't change this without also changing references in ssebfly27.s!
 */
struct v27 {
  metric_t metrics1; /* path metric buffer 1 */
  metric_t metrics2; /* path metric buffer 2 */
  decision_t *dp;          /* Pointer to current decision */
  metric_t *old_metrics,*new_metrics; /* Pointers to path metrics, swapped on every bit */
  decision_t *decisions;   /* Beginning of decisions for block */
};

/* Create a new instance of a Viterbi decoder */
void *create_viterbi27_sse(int len){
  struct v27 *vp;

  if(!Init){
    int polys[2] = { V27POLYA, V27POLYB };

    set_viterbi27_polynomial_sse(polys);
  }
  if((vp = malloc(sizeof(struct v27))) == NULL)
    return NULL;
  if((vp->decisions = malloc((len+6)*sizeof(decision_t))) == NULL){
    free(vp);
    return NULL;
  }
  init_viterbi27(vp,0);
  return vp;
}

void set_viterbi27_polynomial_sse(int polys[2]){
  int state;

  for(state=0;state < 32;state++){
    Branchtab27_sse[0].c[state] = (polys[0] < 0) ^ parity((2*state) & abs(polys[0])) ? 255 : 0;
    Branchtab27_sse[1].c[state] = (polys[1] < 0) ^ parity((2*state) & abs(polys[1])) ? 255 : 0;
  }
  Init++;
}

/* Initialize Viterbi decoder for start of new frame */
int init_viterbi27_sse(void *p,int starting_state){
  struct v27 *vp = p;
  int i;

  if(p == NULL)
    return -1;
  for(i=0;i<64;i++)
    vp->metrics1.c[i] = 63;

  vp->old_metrics = &vp->metrics1;
  vp->new_metrics = &vp->metrics2;
  vp->dp = vp->decisions;
  vp->old_metrics->c[starting_state & 63] = 0; /* Bias known start state */
  return 0;
}

/* Viterbi chainback */
int chainback_viterbi27_sse(
      void *p,
      unsigned char *data, /* Decoded output data */
      unsigned int nbits, /* Number of data bits */
      unsigned int endstate){ /* Terminal encoder state */
  struct v27 *vp = p;
  decision_t *d;

  if(p == NULL)
    return -1;

  d = vp->decisions;
  /* Make room beyond the end of the encoder register so we can
   * accumulate a full byte of decoded data
   */
  endstate %= 64;
  endstate <<= 2;

  /* The store into data[] only needs to be done every 8 bits.
   * But this avoids a conditional branch, and the writes will
   * combine in the cache anyway
   */
  d += 6; /* Look past tail */
  while(nbits-- != 0){
    int k;

    k = (d[nbits].c[(endstate>>2)/8] >> ((endstate>>2)%8)) & 1;
    data[nbits>>3] = endstate = (endstate >> 1) | (k << 7);
  }
  return 0;
}

/* Delete instance of a Viterbi decoder */
void delete_viterbi27_sse(void *p){
  struct v27 *vp = p;

  if(vp != NULL){
    free(vp->decisions);
    free(vp);
  }
}

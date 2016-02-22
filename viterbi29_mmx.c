/* K=9 r=1/2 Viterbi decoder for MMX
 * Copyright Feb 2004, Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <mmintrin.h>
#include "fec.h"

typedef union { char c[256]; __m64 v[32];} decision_t;
typedef union { unsigned char c[256]; __m64 v[32];} metric_t;

unsigned char Mettab29_1[256][128] __attribute__ ((aligned(8)));
unsigned char Mettab29_2[256][128] __attribute__ ((aligned(8)));
static int Init = 0;

/* State info for instance of Viterbi decoder
 * Don't change this without also changing references in mmxbfly29.s!
 */
struct v29 {
  metric_t metrics1; /* path metric buffer 1 */
  metric_t metrics2; /* path metric buffer 2 */
  decision_t *dp;          /* Pointer to current decision */
  metric_t *old_metrics,*new_metrics; /* Pointers to path metrics, swapped on every bit */
  decision_t *decisions;   /* Beginning of decisions for block */
};

/* Create a new instance of a Viterbi decoder */
void *create_viterbi29_mmx(int len){
  struct v29 *vp;

  if(Init == 0){
    int polys[2] = {V29POLYA,V29POLYB};

    set_viterbi29_polynomial_mmx(polys);
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

void set_viterbi29_polynomial_mmx(int polys[2]){
  int state;

  for(state=0;state < 128;state++){
    int symbol;

    for(symbol = 0;symbol < 256;symbol++){
      int sym;

      sym = parity((2*state) & abs(polys[0])) ^ (polys[0] < 0);
      Mettab29_1[symbol][state] = (sym ? (255-symbol):symbol) / 16;

      sym = parity((2*state) & abs(polys[1])) ^ (polys[1] < 0);
      Mettab29_2[symbol][state] = (sym ? (255-symbol):symbol) / 16;
    }
  }
  Init++;
}

/* Initialize Viterbi decoder for start of new frame */
int init_viterbi29_mmx(void *p,int starting_state){
  struct v29 *vp = p;
  int i;

  if(p == NULL)
    return -1;
  for(i=0;i<256;i++)
    vp->metrics1.c[i] = 63;

  vp->old_metrics = &vp->metrics1;
  vp->new_metrics = &vp->metrics2;
  vp->dp = vp->decisions;
  vp->old_metrics->c[starting_state & 255] = 0; /* Bias known start state */
  return 0;
}

/* Viterbi chainback */
int chainback_viterbi29_mmx(
      void *p,
      unsigned char *data, /* Decoded output data */
      unsigned int nbits, /* Number of data bits */
      unsigned int endstate){ /* Terminal encoder state */

  struct v29 *vp = (struct v29 *)p;
  decision_t *d;

  if(p == NULL)
    return -1;

  d = (decision_t *)vp->decisions;
  endstate &= 255;
  d += 8; /* Look past tail */
  while(nbits-- != 0){
    int k;

    k = d[nbits].c[endstate] & 1;
    data[nbits>>3] = endstate = (endstate >> 1) | (k << 7);
  }
  return 0;
}

/* Delete instance of a Viterbi decoder */
void delete_viterbi29_mmx(void *p){
  struct v29 *vp = p;

  if(vp != NULL){
    free(vp->decisions);
    free(vp);
  }
}

/* K=15 r=1/6 Viterbi decoder in portable C
 * Copyright Mar 2004, Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <limits.h>
#include "fec.h"

typedef union { unsigned long w[512]; unsigned char c[2048];} decision_t;
typedef union { unsigned long w[16384]; } metric_t;

static union branchtab615 { unsigned long w[8192]; } Branchtab615[6] __attribute__ ((aligned(16)));
static int Init = 0;

/* State info for instance of Viterbi decoder */
struct v615 {
  metric_t metrics1; /* path metric buffer 1 */
  metric_t metrics2; /* path metric buffer 2 */
  decision_t *dp;          /* Pointer to current decision */
  metric_t *old_metrics,*new_metrics; /* Pointers to path metrics, swapped on every bit */
  decision_t *decisions;   /* Beginning of decisions for block */
};

/* Create a new instance of a Viterbi decoder */
void *create_viterbi615_port(int len){
  struct v615 *vp;

  if(!Init){
    int polys[6] = { V615POLYA,V615POLYB,V615POLYC,V615POLYD,V615POLYE,V615POLYF };
    set_viterbi615_polynomial_port(polys);
  }
  if((vp = (struct v615 *)malloc(sizeof(struct v615))) == NULL)
    return NULL;
  if((vp->decisions = malloc((len+14)*sizeof(decision_t))) == NULL){
    free(vp);
    return NULL;
  }
  init_viterbi615(vp,0);
  return vp;
}

void set_viterbi615_polynomial_port(int polys[6]){
  int state;
  int i;

  for(state=0;state < 8192;state++){
    for(i=0;i<6;i++)
      Branchtab615[i].w[state] = (polys[i] < 0) ^ parity((2*state) & abs(polys[i])) ? 255 : 0;
  }
  Init++;
}

/* Initialize Viterbi decoder for start of new frame */
int init_viterbi615_port(void *p,int starting_state){
  struct v615 *vp = p;
  int i;

  if(p == NULL)
    return -1;
  for(i=0;i<16384;i++)
    vp->metrics1.w[i] = 1000;

  vp->old_metrics = &vp->metrics1;
  vp->new_metrics = &vp->metrics2;
  vp->dp = vp->decisions;
  vp->old_metrics->w[starting_state & 16383] = 0; /* Bias known start state */
  return 0;
}

/* Viterbi chainback */
int chainback_viterbi615_port(
      void *p,
      unsigned char *data, /* Decoded output data */
      unsigned int nbits, /* Number of data bits */
      unsigned int endstate){ /* Terminal encoder state */
  struct v615 *vp = p;
  decision_t *d;

  if(p == NULL)
    return -1;
  d = (decision_t *)vp->decisions;  
  endstate %= 16384;

  /* The store into data[] only needs to be done every 8 bits.
   * But this avoids a conditional branch, and the writes will
   * combine in the cache anyway
   */
  d += 14; /* Look past tail */
  while(nbits-- != 0){
    int k;

    k = (d[nbits].c[endstate/8] >> (endstate%8)) & 1;
    endstate = (k << 13) | (endstate >> 1);
    data[nbits>>3] = endstate >> 6;
  }
  return 0;
}

/* Delete instance of a Viterbi decoder */
void delete_viterbi615_port(void *p){
  struct v615 *vp = p;

  if(vp != NULL){
    free(vp->decisions);
    free(vp);
  }
}

/* C-language butterfly */
#define BFLY(i) {\
unsigned long metric,m0,m1,m2,m3,decision0,decision1;\
    metric = ((Branchtab615[0].w[i] ^ syms[0]) + (Branchtab615[1].w[i] ^ syms[1])\
	      +(Branchtab615[2].w[i] ^ syms[2]) + (Branchtab615[3].w[i] ^ syms[3])\
	      +(Branchtab615[4].w[i] ^ syms[4]) + (Branchtab615[5].w[i] ^ syms[5]));\
    m0 = vp->old_metrics->w[i] + metric;\
    m1 = vp->old_metrics->w[i+8192] + (1530 - metric);\
    m2 = vp->old_metrics->w[i] + (1530-metric);\
    m3 = vp->old_metrics->w[i+8192] + metric;\
    decision0 = (signed long)(m0-m1) >= 0;\
    decision1 = (signed long)(m2-m3) >= 0;\
    vp->new_metrics->w[2*i] = decision0 ? m1 : m0;\
    vp->new_metrics->w[2*i+1] = decision1 ? m3 : m2;\
    d->c[i/4] |= ((decision0|(decision1<<1)) << ((2*i)&7));\
}
/* Update decoder with a block of demodulated symbols
 * Note that nbits is the number of decoded data bits, not the number
 * of symbols!
 */

int update_viterbi615_blk_port(void *p,unsigned char *syms,int nbits){
  struct v615 *vp = p;
  void *tmp;
  decision_t *d;
  int i;

  if(p == NULL)
    return -1;
  d = (decision_t *)vp->dp;
  while(nbits--){
    memset(d,0,sizeof(decision_t));
    for(i=0;i<8192;i++)
      BFLY(i);

    syms += 6;
    d++;
    /* Swap pointers to old and new metrics */
    tmp = vp->old_metrics;
    vp->old_metrics = vp->new_metrics;
    vp->new_metrics = tmp;
  }    
  vp->dp = d;
  return 0;
}


/* K=15 r=1/6 Viterbi decoder for x86 MMX
 * Mar 2004, Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <mmintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include "fec.h"

typedef union { unsigned char c[16384]; __m64 v[2048];} decision_t;
typedef union { unsigned short s[16384]; __m64 v[4096];} metric_t;

static union branchtab615 { unsigned short s[8192]; __m64 v[2048];} Branchtab615[6];
static int Init = 0;

/* State info for instance of Viterbi decoder */
struct v615 {
  metric_t metrics1; /* path metric buffer 1 */
  metric_t metrics2; /* path metric buffer 2 */
  void *dp;          /* Pointer to current decision */
  metric_t *old_metrics,*new_metrics; /* Pointers to path metrics, swapped on every bit */
  void *decisions;   /* Beginning of decisions for block */
};

/* Initialize Viterbi decoder for start of new frame */
int init_viterbi615_mmx(void *p,int starting_state){
  struct v615 *vp = p;
  int i;

  if(p == NULL)
    return -1;
  for(i=0;i<16384;i++)
    vp->metrics1.s[i] = 5000;

  vp->old_metrics = &vp->metrics1;
  vp->new_metrics = &vp->metrics2;
  vp->dp = vp->decisions;
  vp->old_metrics->s[starting_state & 16383] = 0; /* Bias known start state */
  return 0;
}

/* Create a new instance of a Viterbi decoder */
void *create_viterbi615_mmx(int len){
  struct v615 *vp;

  if(!Init){
    int polys[6] = { V615POLYA,V615POLYB,V615POLYC,V615POLYD,V615POLYE,V615POLYF };
    set_viterbi615_polynomial_mmx(polys);
  }

  if((vp = (struct v615 *)malloc(sizeof(struct v615))) == NULL)
    return NULL;
  if((vp->decisions = malloc((len+14)*sizeof(decision_t))) == NULL){
    free(vp);
    return NULL;
  }
  init_viterbi615_mmx(vp,0);
  return vp;
}

void set_viterbi615_polynomial_mmx(int polys[6]){
  int state;
  int i;

  for(state=0;state < 8192;state++){
    for(i=0;i<6;i++)
      Branchtab615[i].s[state] = (polys[i] < 0) ^ parity((2*state) & abs(polys[i])) ? 255 : 0;
  }
  Init++;
}

/* Viterbi chainback */
int chainback_viterbi615_mmx(
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

    k = d[nbits].c[endstate] & 1;
    endstate = (k << 13) | (endstate >> 1);
    data[nbits>>3] = endstate >> 6;
  }
  return 0;
}

/* Delete instance of a Viterbi decoder */
void delete_viterbi615_mmx(void *p){
  struct v615 *vp = p;

  if(vp != NULL){
    free(vp->decisions);
    free(vp);
  }
}


int update_viterbi615_blk_mmx(void *p,unsigned char *syms,int nbits){
  struct v615 *vp = p;
  decision_t *d;

  if(p == NULL)
    return -1;

  d = (decision_t *)vp->dp;
  
  while(nbits--){
    __m64 sym0v,sym1v,sym2v,sym3v,sym4v,sym5v;
    void *tmp;
    int i;
    
    /* Splat the 0th symbol across sym0v, the 1st symbol across sym1v, etc */
    sym0v = _mm_set1_pi16(syms[0]);
    sym1v = _mm_set1_pi16(syms[1]);
    sym2v = _mm_set1_pi16(syms[2]);
    sym3v = _mm_set1_pi16(syms[3]);
    sym4v = _mm_set1_pi16(syms[4]);
    sym5v = _mm_set1_pi16(syms[5]);
    syms += 6;

    for(i=0;i<2048;i++){
      __m64 decision0,decision1,metric,m_metric,m0,m1,m2,m3,survivor0,survivor1;

      /* Form branch metrics
       * Because Branchtab takes on values 0 and 255, and the values of sym?v are offset binary in the range 0-255,
       * the XOR operations constitute conditional negation.
       * metric and m_metric (-metric) are in the range 0-1530
       */
      m0 = _mm_add_pi16(_mm_xor_si64(Branchtab615[0].v[i],sym0v),_mm_xor_si64(Branchtab615[1].v[i],sym1v));
      m1 = _mm_add_pi16(_mm_xor_si64(Branchtab615[2].v[i],sym2v),_mm_xor_si64(Branchtab615[3].v[i],sym3v));
      m2 = _mm_add_pi16(_mm_xor_si64(Branchtab615[4].v[i],sym4v),_mm_xor_si64(Branchtab615[5].v[i],sym5v));
      metric = _mm_add_pi16(m0,_mm_add_pi16(m1,m2));
      m_metric = _mm_sub_pi16(_mm_set1_pi16(1530),metric);
    
      /* Add branch metrics to path metrics */
      m0 = _mm_add_pi16(vp->old_metrics->v[i],metric);
      m3 = _mm_add_pi16(vp->old_metrics->v[2048+i],metric);
      m1 = _mm_add_pi16(vp->old_metrics->v[2048+i],m_metric);
      m2 = _mm_add_pi16(vp->old_metrics->v[i],m_metric);
    
      /* Compare and select
       * There's no packed min instruction in MMX, so we use modulo arithmetic
       * to form the decisions and then do the select the hard way
       */
      decision0 = _mm_cmpgt_pi16(_mm_sub_pi16(m0,m1),_mm_setzero_si64());
      decision1 = _mm_cmpgt_pi16(_mm_sub_pi16(m2,m3),_mm_setzero_si64());
      survivor0 = _mm_or_si64(_mm_and_si64(decision0,m1),_mm_andnot_si64(decision0,m0));
      survivor1 = _mm_or_si64(_mm_and_si64(decision1,m3),_mm_andnot_si64(decision1,m2));
 
      /* Merge decisions and store as bytes */
      d->v[i] = _mm_unpacklo_pi8(_mm_packs_pi16(decision0,_mm_setzero_si64()),_mm_packs_pi16(decision1,_mm_setzero_si64()));

      /* Store surviving metrics */
      vp->new_metrics->v[2*i] = _mm_unpacklo_pi16(survivor0,survivor1);
      vp->new_metrics->v[2*i+1] = _mm_unpackhi_pi16(survivor0,survivor1);
    }
    d++;
    /* Swap pointers to old and new metrics */
    tmp = vp->old_metrics;
    vp->old_metrics = vp->new_metrics;
    vp->new_metrics = tmp;
  }
  vp->dp = d;
  _mm_empty();
  return 0;
}

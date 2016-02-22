/* K=15 r=1/6 Viterbi decoder for x86 SSE2
 * Copyright Mar 2004, Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <emmintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <limits.h>
#include "fec.h"

typedef union { unsigned long w[512]; unsigned short s[1024];} decision_t;
typedef union { signed short s[16384]; __m128i v[2048];} metric_t;

static union branchtab615 { unsigned short s[8192]; __m128i v[1024];} Branchtab615[6];
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
int init_viterbi615_sse2(void *p,int starting_state){
  struct v615 *vp = p;
  int i;

  if(p == NULL)
    return -1;
  for(i=0;i<16384;i++)
    vp->metrics1.s[i] = (SHRT_MIN+5000);

  vp->old_metrics = &vp->metrics1;
  vp->new_metrics = &vp->metrics2;
  vp->dp = vp->decisions;
  vp->old_metrics->s[starting_state & 16383] = SHRT_MIN; /* Bias known start state */
  return 0;
}

/* Create a new instance of a Viterbi decoder */
void *create_viterbi615_sse2(int len){
  void *p;
  struct v615 *vp;

  if(!Init){
    int polys[6] = { V615POLYA,V615POLYB,V615POLYC,V615POLYD,V615POLYE,V615POLYF };
    set_viterbi615_polynomial_sse2(polys);
  }

  /* Ordinary malloc() only returns 8-byte alignment, we need 16 */
  if(posix_memalign(&p, sizeof(__m128i),sizeof(struct v615)))
    return NULL;

  vp = (struct v615 *)p;
  if((p = malloc((len+14)*sizeof(decision_t))) == NULL){
    free(vp);
    return NULL;
  }
  vp->decisions = (decision_t *)p;
  init_viterbi615_sse2(vp,0);
  return vp;
}

void set_viterbi615_polynomial_sse2(int polys[6]){
  int state;
  int i;

  for(state=0;state < 8192;state++){
    for(i=0;i<6;i++)
      Branchtab615[i].s[state] = (polys[i] < 0) ^ parity((2*state) & abs(polys[i])) ? 255 : 0;
  }
  Init++;
}

/* Viterbi chainback */
int chainback_viterbi615_sse2(
      void *p,
      unsigned char *data, /* Decoded output data */
      unsigned int nbits, /* Number of data bits */
      unsigned int endstate){ /* Terminal encoder state */
  struct v615 *vp = p;
  decision_t *d = (decision_t *)vp->decisions;

  endstate %= 16384;

  /* The store into data[] only needs to be done every 8 bits.
   * But this avoids a conditional branch, and the writes will
   * combine in the cache anyway
   */
  d += 14; /* Look past tail */
  while(nbits-- != 0){
    int k;

    k = (d[nbits].w[endstate/32] >> (endstate%32)) & 1;
    endstate = (k << 13) | (endstate >> 1);
    data[nbits>>3] = endstate >> 6;
  }
  return 0;
}

/* Delete instance of a Viterbi decoder */
void delete_viterbi615_sse2(void *p){
  struct v615 *vp = p;

  if(vp != NULL){
    free(vp->decisions);
    free(vp);
  }
}


int update_viterbi615_blk_sse2(void *p,unsigned char *syms,int nbits){
  struct v615 *vp = p;
  decision_t *d = (decision_t *)vp->dp;

  while(nbits--){
    __m128i sym0v,sym1v,sym2v,sym3v,sym4v,sym5v;
    void *tmp;
    int i;

    /* Splat the 0th symbol across sym0v, the 1st symbol across sym1v, etc */
    sym0v = _mm_set1_epi16(syms[0]);
    sym1v = _mm_set1_epi16(syms[1]);
    sym2v = _mm_set1_epi16(syms[2]);
    sym3v = _mm_set1_epi16(syms[3]);
    sym4v = _mm_set1_epi16(syms[4]);
    sym5v = _mm_set1_epi16(syms[5]);
    syms += 6;

    /* SSE2 doesn't support saturated adds on unsigned shorts, so we have to use signed shorts */
    for(i=0;i<1024;i++){
      __m128i decision0,decision1,metric,m_metric,m0,m1,m2,m3,survivor0,survivor1;

      /* Form branch metrics
       * Because Branchtab takes on values 0 and 255, and the values of sym?v are offset binary in the range 0-255,
       * the XOR operations constitute conditional negation.
       * metric and m_metric (-metric) are in the range 0-1530
       */
      m0 = _mm_add_epi16(_mm_xor_si128(Branchtab615[0].v[i],sym0v),_mm_xor_si128(Branchtab615[1].v[i],sym1v));
      m1 = _mm_add_epi16(_mm_xor_si128(Branchtab615[2].v[i],sym2v),_mm_xor_si128(Branchtab615[3].v[i],sym3v));
      m2 = _mm_add_epi16(_mm_xor_si128(Branchtab615[4].v[i],sym4v),_mm_xor_si128(Branchtab615[5].v[i],sym5v));
      metric = _mm_add_epi16(m0,_mm_add_epi16(m1,m2));
      m_metric = _mm_sub_epi16(_mm_set1_epi16(1530),metric);
    
      /* Add branch metrics to path metrics */
      m0 = _mm_adds_epi16(vp->old_metrics->v[i],metric);
      m3 = _mm_adds_epi16(vp->old_metrics->v[1024+i],metric);
      m1 = _mm_adds_epi16(vp->old_metrics->v[1024+i],m_metric);
      m2 = _mm_adds_epi16(vp->old_metrics->v[i],m_metric);
    
      /* Compare and select */
      survivor0 = _mm_min_epi16(m0,m1);
      survivor1 = _mm_min_epi16(m2,m3);
      decision0 = _mm_cmpeq_epi16(survivor0,m1);
      decision1 = _mm_cmpeq_epi16(survivor1,m3);
 
      /* Pack each set of decisions into 8 8-bit bytes, then interleave them and compress into 16 bits */
      d->s[i] = _mm_movemask_epi8(_mm_unpacklo_epi8(_mm_packs_epi16(decision0,_mm_setzero_si128()),_mm_packs_epi16(decision1,_mm_setzero_si128())));

      /* Store surviving metrics */
      vp->new_metrics->v[2*i] = _mm_unpacklo_epi16(survivor0,survivor1);
      vp->new_metrics->v[2*i+1] = _mm_unpackhi_epi16(survivor0,survivor1);
    }
    /* See if we need to renormalize
     * Max metric spread for this code with 0-90 branch metrics is 405
     */
    if(vp->new_metrics->s[0] >= SHRT_MAX-12750){
      int i,adjust;
      __m128i adjustv;
      union { __m128i v; signed short w[8]; } t;
      
      /* Find smallest metric and set adjustv to bring it down to SHRT_MIN */
      adjustv = vp->new_metrics->v[0];
      for(i=1;i<2048;i++)
	adjustv = _mm_min_epi16(adjustv,vp->new_metrics->v[i]);

      adjustv = _mm_min_epi16(adjustv,_mm_srli_si128(adjustv,8));
      adjustv = _mm_min_epi16(adjustv,_mm_srli_si128(adjustv,4));
      adjustv = _mm_min_epi16(adjustv,_mm_srli_si128(adjustv,2));
      t.v = adjustv;
      adjust = t.w[0] - SHRT_MIN;
      adjustv = _mm_set1_epi16(adjust);

      /* We cannot use a saturated subtract, because we often have to adjust by more than SHRT_MAX
       * This is okay since it can't overflow anyway
       */
      for(i=0;i<2048;i++)
	vp->new_metrics->v[i] = _mm_sub_epi16(vp->new_metrics->v[i],adjustv);
    }
    d++;
    /* Swap pointers to old and new metrics */
    tmp = vp->old_metrics;
    vp->old_metrics = vp->new_metrics;
    vp->new_metrics = tmp;
  }
  vp->dp = d;
  return 0;
}



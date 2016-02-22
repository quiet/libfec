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

typedef union { unsigned long w[8]; unsigned short s[16];} decision_t;
typedef union { signed short s[256]; __m128i v[32];} metric_t;

static union branchtab39 { unsigned short s[128]; __m128i v[16];} Branchtab39[3];
static int Init = 0;

/* State info for instance of Viterbi decoder */
struct v39 {
  metric_t metrics1; /* path metric buffer 1 */
  metric_t metrics2; /* path metric buffer 2 */
  void *dp;          /* Pointer to current decision */
  metric_t *old_metrics,*new_metrics; /* Pointers to path metrics, swapped on every bit */
  void *decisions;   /* Beginning of decisions for block */
};

/* Initialize Viterbi decoder for start of new frame */
int init_viterbi39_sse2(void *p,int starting_state){
  struct v39 *vp = p;
  int i;

  for(i=0;i<256;i++)
    vp->metrics1.s[i] = (SHRT_MIN+1000);

  vp->old_metrics = &vp->metrics1;
  vp->new_metrics = &vp->metrics2;
  vp->dp = vp->decisions;
  vp->old_metrics->s[starting_state & 255] = SHRT_MIN; /* Bias known start state */
  return 0;
}

/* Create a new instance of a Viterbi decoder */
void *create_viterbi39_sse2(int len){
  void *p;
  struct v39 *vp;

  if(!Init){
    int polys[3] = { V39POLYA, V39POLYB, V39POLYC };

    set_viterbi39_polynomial_sse2(polys);
  }
  /* Ordinary malloc() only returns 8-byte alignment, we need 16 */
  if(posix_memalign(&p, sizeof(__m128i),sizeof(struct v39)))
    return NULL;

  vp = (struct v39 *)p;
  if((p = malloc((len+8)*sizeof(decision_t))) == NULL){
    free(vp);
    return NULL;
  }
  vp->decisions = (decision_t *)p;
  init_viterbi39_sse2(vp,0);
  return vp;
}

void set_viterbi39_polynomial_sse2(int polys[3]){
  int state;

  for(state=0;state < 128;state++){
    Branchtab39[0].s[state] = (polys[0] < 0) ^ parity((2*state) & polys[0]) ? 255:0;
    Branchtab39[1].s[state] = (polys[1] < 0) ^ parity((2*state) & polys[1]) ? 255:0;
    Branchtab39[2].s[state] = (polys[2] < 0) ^ parity((2*state) & polys[2]) ? 255:0;
  }
  Init++;
}

/* Viterbi chainback */
int chainback_viterbi39_sse2(
      void *p,
      unsigned char *data, /* Decoded output data */
      unsigned int nbits, /* Number of data bits */
      unsigned int endstate){ /* Terminal encoder state */
  struct v39 *vp = p;
  decision_t *d = (decision_t *)vp->decisions;
  int path_metric;

  endstate %= 256;

  path_metric = vp->old_metrics->s[endstate];

  /* The store into data[] only needs to be done every 8 bits.
   * But this avoids a conditional branch, and the writes will
   * combine in the cache anyway
   */
  d += 8; /* Look past tail */
  while(nbits-- != 0){
    int k;

    k = (d[nbits].w[endstate/32] >> (endstate%32)) & 1;
    endstate = (k << 7) | (endstate >> 1);
    data[nbits>>3] = endstate;
  }
  return path_metric;
}

/* Delete instance of a Viterbi decoder */
void delete_viterbi39_sse2(void *p){
  struct v39 *vp = p;

  if(vp != NULL){
    free(vp->decisions);
    free(vp);
  }
}


int update_viterbi39_blk_sse2(void *p,unsigned char *syms,int nbits){
  struct v39 *vp = p;
  decision_t *d = (decision_t *)vp->dp;
  int path_metric = 0;

  while(nbits--){
    __m128i sym0v,sym1v,sym2v;
    void *tmp;
    int i;

    /* Splat the 0th symbol across sym0v, the 1st symbol across sym1v, etc */
    sym0v = _mm_set1_epi16(syms[0]);
    sym1v = _mm_set1_epi16(syms[1]);
    sym2v = _mm_set1_epi16(syms[2]);
    syms += 3;

    /* SSE2 doesn't support saturated adds on unsigned shorts, so we have to use signed shorts */
    for(i=0;i<16;i++){
      __m128i decision0,decision1,metric,m_metric,m0,m1,m2,m3,survivor0,survivor1;

      /* Form branch metrics
       * Because Branchtab takes on values 0 and 255, and the values of sym?v are offset binary in the range 0-255,
       * the XOR operations constitute conditional negation.
       * metric and m_metric (-metric) are in the range 0-765
       */
      m0 = _mm_add_epi16(_mm_xor_si128(Branchtab39[0].v[i],sym0v),_mm_xor_si128(Branchtab39[1].v[i],sym1v));
      metric = _mm_add_epi16(_mm_xor_si128(Branchtab39[2].v[i],sym2v),m0);
      m_metric = _mm_sub_epi16(_mm_set1_epi16(765),metric);
    
      /* Add branch metrics to path metrics */
      m0 = _mm_adds_epi16(vp->old_metrics->v[i],metric);
      m3 = _mm_adds_epi16(vp->old_metrics->v[16+i],metric);
      m1 = _mm_adds_epi16(vp->old_metrics->v[16+i],m_metric);
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
    /* See if we need to renormalize */
    if(vp->new_metrics->s[0] >= SHRT_MAX-5000){
      int i,adjust;
      __m128i adjustv;
      union { __m128i v; signed short w[8]; } t;
      
      /* Find smallest metric and set adjustv to bring it down to SHRT_MIN */
      adjustv = vp->new_metrics->v[0];
      for(i=1;i<32;i++)
	adjustv = _mm_min_epi16(adjustv,vp->new_metrics->v[i]);

      adjustv = _mm_min_epi16(adjustv,_mm_srli_si128(adjustv,8));
      adjustv = _mm_min_epi16(adjustv,_mm_srli_si128(adjustv,4));
      adjustv = _mm_min_epi16(adjustv,_mm_srli_si128(adjustv,2));
      t.v = adjustv;
      adjust = t.w[0] - SHRT_MIN;
      path_metric += adjust;
      adjustv = _mm_set1_epi16(adjust);

      /* We cannot use a saturated subtract, because we often have to adjust by more than SHRT_MAX
       * This is okay since it can't overflow anyway
       */
      for(i=0;i<32;i++)
	vp->new_metrics->v[i] = _mm_sub_epi16(vp->new_metrics->v[i],adjustv);
    }
    d++;
    /* Swap pointers to old and new metrics */
    tmp = vp->old_metrics;
    vp->old_metrics = vp->new_metrics;
    vp->new_metrics = tmp;
  }
  vp->dp = d;
  return path_metric;
}



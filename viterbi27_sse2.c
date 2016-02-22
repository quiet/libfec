/* K=7 r=1/2 Viterbi decoder for SSE2
 * Feb 2004, Phil Karn, KA9Q
 */
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <xmmintrin.h>
#include "fec.h"

typedef union { unsigned char c[64]; __m128i v[4]; } metric_t;
typedef union { unsigned long w[2]; unsigned char c[8]; unsigned short s[4]; __m64 v[1];} decision_t;
union branchtab27 { unsigned char c[32]; __m128i v[2];} Branchtab27_sse2[2];
static int Init = 0;

/* State info for instance of Viterbi decoder
 * Don't change this without also changing references in sse2bfly27.s!
 */
struct v27 {
  metric_t metrics1; /* path metric buffer 1 */
  metric_t metrics2; /* path metric buffer 2 */
  decision_t *dp;          /* Pointer to current decision */
  metric_t *old_metrics,*new_metrics; /* Pointers to path metrics, swapped on every bit */
  decision_t *decisions;   /* Beginning of decisions for block */
};

/* Initialize Viterbi decoder for start of new frame */
int init_viterbi27_sse2(void *p,int starting_state){
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

void set_viterbi27_polynomial_sse2(int polys[2]){
  int state;

  for(state=0;state < 32;state++){
    Branchtab27_sse2[0].c[state] = (polys[0] < 0) ^ parity((2*state) & abs(polys[0])) ? 255 : 0;
    Branchtab27_sse2[1].c[state] = (polys[1] < 0) ^ parity((2*state) & abs(polys[1])) ? 255 : 0;
  }
  Init++;
}


/* Create a new instance of a Viterbi decoder */
void *create_viterbi27_sse2(int len){
  void *p;
  struct v27 *vp;

  if(!Init){
    int polys[2] = { V27POLYA, V27POLYB };
    set_viterbi27_polynomial_sse2(polys);
  }
  /* Ordinary malloc() only returns 8-byte alignment, we need 16 */
  if(posix_memalign(&p, sizeof(__m128i),sizeof(struct v27)))
    return NULL;
  vp = (struct v27 *)p;

  if((p = malloc((len+6)*sizeof(decision_t))) == NULL){
    free(vp);
    return NULL;
  }
  vp->decisions = (decision_t *)p;
  init_viterbi27_sse2(vp,0);

  return vp;
}

/* Viterbi chainback */
int chainback_viterbi27_sse2(
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
void delete_viterbi27_sse2(void *p){
  struct v27 *vp = p;

  if(vp != NULL){
    free(vp->decisions);
    free(vp);
  }
}


#if 0
/* This code is turned off because it's slower than my hand-crafted assembler in sse2bfly27.s. But it does work. */
void update_viterbi27_blk_sse2(void *p,unsigned char *syms,int nbits){
  struct v27 *vp = p;
  decision_t *d;

  if(p == NULL)
    return;
  d = (decision_t *)vp->dp;
  while(nbits--){
    __m128i sym0v,sym1v;
    void *tmp;
    int i;
    
    /* Splat the 0th symbol across sym0v, the 1st symbol across sym1v, etc */
    sym0v = _mm_set1_epi8(syms[0]);
    sym1v = _mm_set1_epi8(syms[1]);
    syms += 2;

    for(i=0;i<2;i++){
      __m128i decision0,decision1,metric,m_metric,m0,m1,m2,m3,survivor0,survivor1;

      /* Form branch metrics */
      metric = _mm_avg_epu8(_mm_xor_si128(Branchtab27_sse2[0].v[i],sym0v),_mm_xor_si128(Branchtab27_sse2[1].v[i],sym1v));
      /* There's no packed bytes right shift in SSE2, so we use the word version and mask
       * (I'm *really* starting to like Altivec...)
       */
      metric = _mm_srli_epi16(metric,3);
      metric = _mm_and_si128(metric,_mm_set1_epi8(31));
      m_metric = _mm_sub_epi8(_mm_set1_epi8(31),metric);
    
      /* Add branch metrics to path metrics */
      m0 = _mm_add_epi8(vp->old_metrics->v[i],metric);
      m3 = _mm_add_epi8(vp->old_metrics->v[2+i],metric);
      m1 = _mm_add_epi8(vp->old_metrics->v[2+i],m_metric);
      m2 = _mm_add_epi8(vp->old_metrics->v[i],m_metric);
    
      /* Compare and select, using modulo arithmetic */
      decision0 = _mm_cmpgt_epi8(_mm_sub_epi8(m0,m1),_mm_setzero_si128());
      decision1 = _mm_cmpgt_epi8(_mm_sub_epi8(m2,m3),_mm_setzero_si128());
      survivor0 = _mm_or_si128(_mm_and_si128(decision0,m1),_mm_andnot_si128(decision0,m0));
      survivor1 = _mm_or_si128(_mm_and_si128(decision1,m3),_mm_andnot_si128(decision1,m2));
 
      /* Pack each set of decisions into 16 bits */
      d->s[2*i] = _mm_movemask_epi8(_mm_unpacklo_epi8(decision0,decision1));
      d->s[2*i+1] = _mm_movemask_epi8(_mm_unpackhi_epi8(decision0,decision1));

      /* Store surviving metrics */
      vp->new_metrics->v[2*i] = _mm_unpacklo_epi8(survivor0,survivor1);
      vp->new_metrics->v[2*i+1] = _mm_unpackhi_epi8(survivor0,survivor1);
    }
    d++;
    /* Swap pointers to old and new metrics */
    tmp = vp->old_metrics;
    vp->old_metrics = vp->new_metrics;
    vp->new_metrics = tmp;
  }
  vp->dp = d;
}
#endif

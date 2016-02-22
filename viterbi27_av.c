/* K=7 r=1/2 Viterbi decoder for PowerPC G4/G5 Altivec instructions
 * Feb 2004, Phil Karn, KA9Q
 */
#include <stdio.h>
#include <memory.h>
#include <stdlib.h>
#include "fec.h"

typedef union { long long p; unsigned char c[64]; vector bool char v[4]; } decision_t;
typedef union { long long p; unsigned char c[64]; vector unsigned char v[4]; } metric_t;

static union branchtab27 { unsigned char c[32]; vector unsigned char v[2];} Branchtab27[2];
static int Init = 0;

/* State info for instance of Viterbi decoder
 * Don't change this without also changing references in [mmx|sse|sse2]bfly29.s!
 */
struct v27 {
  metric_t metrics1; /* path metric buffer 1 */
  metric_t metrics2; /* path metric buffer 2 */
  decision_t *dp;          /* Pointer to current decision */
  metric_t *old_metrics,*new_metrics; /* Pointers to path metrics, swapped on every bit */
  decision_t *decisions;   /* Beginning of decisions for block */
};

/* Initialize Viterbi decoder for start of new frame */
int init_viterbi27_av(void *p,int starting_state){
  struct v27 *vp = p;
  int i;

  if(p == NULL)
    return -1;
  for(i=0;i<4;i++)
    vp->metrics1.v[i] = (vector unsigned char)(63);
  vp->old_metrics = &vp->metrics1;
  vp->new_metrics = &vp->metrics2;
  vp->dp = vp->decisions;
  vp->old_metrics->c[starting_state & 63] = 0; /* Bias known start state */
  return 0;
}

void set_viterbi27_polynomial_av(int polys[2]){
  int state;

  for(state=0;state < 32;state++){
    Branchtab27[0].c[state] = (polys[0] < 0) ^ parity((2*state) & abs(polys[0])) ? 255 : 0;
    Branchtab27[1].c[state] = (polys[1] < 0) ^ parity((2*state) & abs(polys[1])) ? 255 : 0;
  }
  Init++;
}

/* Create a new instance of a Viterbi decoder */
void *create_viterbi27_av(int len){
  struct v27 *vp;

  if(!Init){
    int polys[2] = { V27POLYA,V27POLYB };
    set_viterbi27_polynomial_av(polys);
  }
  if((vp = (struct v27 *)malloc(sizeof(struct v27))) == NULL)
    return NULL;
  if((vp->decisions = (decision_t *)malloc((len+6)*sizeof(decision_t))) == NULL){
    free(vp);
    return NULL;
  }
  init_viterbi27_av(vp,0);
  return vp;
}

/* Viterbi chainback */
int chainback_viterbi27_av(
      void *p,
      unsigned char *data, /* Decoded output data */
      unsigned int nbits, /* Number of data bits */
      unsigned int endstate){ /* Terminal encoder state */
  struct v27 *vp = p;
  decision_t *d = (decision_t *)vp->decisions;

  if(p == NULL)
    return -1;

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
    
    k = d[nbits].c[endstate>>2] & 1;
    data[nbits>>3] = endstate = (endstate >> 1) | (k << 7);
  }
  return 0;
}

/* Delete instance of a Viterbi decoder */
void delete_viterbi27_av(void *p){
  struct v27 *vp = p;

  if(vp != NULL){
    free(vp->decisions);
    free(vp);
  }
}

/* Process received symbols */
int update_viterbi27_blk_av(void *p,unsigned char *syms,int nbits){
  struct v27 *vp = p;
  decision_t *d;

  if(p == NULL)
    return -1;
  d = (decision_t *)vp->dp;
  while(nbits--){
    vector unsigned char survivor0,survivor1,sym0v,sym1v;
    vector bool char decision0,decision1;
    vector unsigned char metric,m_metric,m0,m1,m2,m3;
    void *tmp;

    /* sym0v.0 = syms[0]; sym0v.1 = syms[1] */
    sym0v = vec_perm(vec_ld(0,syms),vec_ld(1,syms),vec_lvsl(0,syms));

    sym1v = vec_splat(sym0v,1); /* Splat syms[1] across sym1v */
    sym0v = vec_splat(sym0v,0); /* Splat syms[0] across sym0v */
    syms += 2;

    /* Do the 32 butterflies as two interleaved groups of 16 each to keep the pipes full */

    /* Form first set of 16 branch metrics */
    metric = vec_avg(vec_xor(Branchtab27[0].v[0],sym0v),vec_xor(Branchtab27[1].v[0],sym1v));
    metric = vec_sr(metric,(vector unsigned char)(3));
    m_metric = vec_sub((vector unsigned char)(31),metric);
    
    /* Form first set of path metrics */
    m0 = vec_adds(vp->old_metrics->v[0],metric);
    m3 = vec_adds(vp->old_metrics->v[2],metric);
    m1 = vec_adds(vp->old_metrics->v[2],m_metric);
    m2 = vec_adds(vp->old_metrics->v[0],m_metric);
    
    /* Form second set of 16 branch metrics */
    metric = vec_avg(vec_xor(Branchtab27[0].v[1],sym0v),vec_xor(Branchtab27[1].v[1],sym1v));
    metric = vec_sr(metric,(vector unsigned char)(3));
    m_metric = vec_sub((vector unsigned char)(31),metric);

    /* Compare and select first set */
    decision0 = vec_cmpgt(m0,m1);
    decision1 = vec_cmpgt(m2,m3);
    survivor0 = vec_min(m0,m1);
    survivor1 = vec_min(m2,m3);
    
    /* Compute second set of path metrics */
    m0 = vec_adds(vp->old_metrics->v[1],metric);
    m3 = vec_adds(vp->old_metrics->v[3],metric);
    m1 = vec_adds(vp->old_metrics->v[3],m_metric);
    m2 = vec_adds(vp->old_metrics->v[1],m_metric);

    /* Interleave and store first decisions and survivors */
    d->v[0] = vec_mergeh(decision0,decision1);
    d->v[1] = vec_mergel(decision0,decision1);
    vp->new_metrics->v[0] = vec_mergeh(survivor0,survivor1);
    vp->new_metrics->v[1] = vec_mergel(survivor0,survivor1);
    
    /* Compare and select second set */
    decision0 = vec_cmpgt(m0,m1);
    decision1 = vec_cmpgt(m2,m3);
    survivor0 = vec_min(m0,m1);
    survivor1 = vec_min(m2,m3);

    /* Interleave and store second set of decisions and survivors */
    d->v[2] = vec_mergeh(decision0,decision1);
    d->v[3] = vec_mergel(decision0,decision1);
    vp->new_metrics->v[2] = vec_mergeh(survivor0,survivor1);
    vp->new_metrics->v[3] = vec_mergel(survivor0,survivor1);
   
    /* renormalize if necessary */
    if(vp->new_metrics->c[0] >= 105){
      vector unsigned char scale0,scale1;

      /* Find smallest metric and splat */
      scale0 = vec_min(vp->new_metrics->v[0],vp->new_metrics->v[1]);
      scale1 = vec_min(vp->new_metrics->v[2],vp->new_metrics->v[3]);
      scale0 = vec_min(scale0,scale1);
      scale0 = vec_min(scale0,vec_sld(scale0,scale0,8));
      scale0 = vec_min(scale0,vec_sld(scale0,scale0,4));
      scale0 = vec_min(scale0,vec_sld(scale0,scale0,2));
      scale0 = vec_min(scale0,vec_sld(scale0,scale0,1));

      /* Now subtract from all metrics */
      vp->new_metrics->v[0] = vec_subs(vp->new_metrics->v[0],scale0);
      vp->new_metrics->v[1] = vec_subs(vp->new_metrics->v[1],scale0);
      vp->new_metrics->v[2] = vec_subs(vp->new_metrics->v[2],scale0);
      vp->new_metrics->v[3] = vec_subs(vp->new_metrics->v[3],scale0);
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


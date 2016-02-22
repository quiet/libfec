/* K=9 r=1/2 Viterbi decoder for PowerPC G4/G5 Altivec
 * Copyright Feb 2004, Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <sys/sysctl.h>
#include "fec.h"

typedef union { unsigned char c[256]; vector bool char v[16]; } decision_t;
typedef union { unsigned char c[256]; vector unsigned char v[16]; } metric_t;

static union branchtab29 { unsigned char c[128]; vector unsigned char v[8]; } Branchtab29[2];
static int Init = 0;

/* State info for instance of Viterbi decoder */
struct v29 {
  metric_t metrics1; /* path metric buffer 1 */
  metric_t metrics2; /* path metric buffer 2 */
  decision_t *dp;          /* Pointer to current decision */
  metric_t *old_metrics,*new_metrics; /* Pointers to path metrics, swapped on every bit */
  decision_t *decisions;   /* Beginning of decisions for block */
};

/* Initialize Viterbi decoder for start of new frame */
int init_viterbi29_av(void *p,int starting_state){
  struct v29 *vp = p;
  int i;

  if(p == NULL)
    return -1;
  for(i=0;i<16;i++)
    vp->metrics1.v[i] = (vector unsigned char)(63);

  vp->old_metrics = &vp->metrics1;
  vp->new_metrics = &vp->metrics2;
  vp->dp = vp->decisions;
  vp->old_metrics->c[starting_state & 255] = 0; /* Bias known start state */
  return 0;
}

void set_viterbi29_polynomial_av(int polys[2]){
  int state;

  for(state=0;state < 128;state++){
    Branchtab29[0].c[state] = (polys[0] < 0) ^ parity((2*state) & abs(polys[0])) ? 255 : 0;
    Branchtab29[1].c[state] = (polys[1] < 0) ^ parity((2*state) & abs(polys[1])) ? 255 : 0;
  }
  Init++;
}

/* Create a new instance of a Viterbi decoder */
void *create_viterbi29_av(int len){
  struct v29 *vp;

  if(!Init){
    int polys[2] = { V29POLYA,V29POLYB };
    set_viterbi29_polynomial_av(polys);
  }
  if((vp = (struct v29 *)malloc(sizeof(struct v29))) == NULL)
    return NULL;
  if((vp->decisions = (decision_t *)malloc((len+8)*sizeof(decision_t))) == NULL){
    free(vp);
    return NULL;
  }
  init_viterbi29_av(vp,0);
  return vp;
}

/* Viterbi chainback */
int chainback_viterbi29_av(
      void *p,
      unsigned char *data, /* Decoded output data */
      unsigned int nbits, /* Number of data bits */
      unsigned int endstate){ /* Terminal encoder state */
  struct v29 *vp = p;
  decision_t *d;

  if(p == NULL)
    return -1;
  d = (decision_t *)vp->decisions;  
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
    
    k = d[nbits].c[endstate] & 1;
    data[nbits>>3] = endstate = (endstate >> 1) | (k << 7);
  }
  return 0;
}


/* Delete instance of a Viterbi decoder */
void delete_viterbi29_av(void *p){
  struct v29 *vp = p;

  if(vp != NULL){
    free(vp->decisions);
    free(vp);
  }
}


int update_viterbi29_blk_av(void *p,unsigned char *syms,int nbits){
  struct v29 *vp = p;
  decision_t *d;
  int i;

  if(p == NULL)
    return -1;
  d = (decision_t *)vp->dp;

  while(nbits--){
    vector unsigned char sym1v,sym2v;
    void *tmp;
    
    /* All this seems necessary just to load a byte into all elements of a vector! */
    sym1v = vec_perm(vec_ld(0,syms),vec_ld(1,syms),vec_lvsl(0,syms)); /* sym1v.0 = syms[0]; sym1v.1 = syms[1] */
    sym2v = vec_splat(sym1v,1); /* Splat syms[1] across sym2v */
    sym1v = vec_splat(sym1v,0); /* Splat syms[0] across sym1v */
    syms += 2;
    
    for(i=0;i<8;i++){
      vector bool char decision0,decision1;
      vector unsigned char metric,m_metric,m0,m1,m2,m3,survivor0,survivor1;

      /* Form branch metrics */
      metric = vec_avg(vec_xor(Branchtab29[0].v[i],sym1v),vec_xor(Branchtab29[1].v[i],sym2v));
      metric = vec_sr(metric,(vector unsigned char)(3));
      m_metric = (vector unsigned char)(31) - metric;
    
      /* Add branch metrics to path metrics */
      m0 = vec_adds(vp->old_metrics->v[i],metric);
      m3 = vec_adds(vp->old_metrics->v[8+i],metric);
      m1 = vec_adds(vp->old_metrics->v[8+i],m_metric);
      m2 = vec_adds(vp->old_metrics->v[i],m_metric);
    
      /* Compare and select first set */
      decision0 = vec_cmpgt(m0,m1);
      decision1 = vec_cmpgt(m2,m3);
      survivor0 = vec_min(m0,m1);
      survivor1 = vec_min(m2,m3);

      /* Interleave and store decisions and survivors */
      d->v[2*i] = vec_mergeh(decision0,decision1);
      d->v[2*i+1] = vec_mergel(decision0,decision1);
      vp->new_metrics->v[2*i] = vec_mergeh(survivor0,survivor1);
      vp->new_metrics->v[2*i+1] = vec_mergel(survivor0,survivor1);
    }
    d++;
    /* renormalize if necessary */
    if(vp->new_metrics->c[0] >= 50){
      int i;
      vector unsigned char scale0,scale1;

      /* Find smallest metric and splat */
      scale0 = vp->new_metrics->v[0];
      scale1 = vp->new_metrics->v[1];
      for(i=2;i<16;i+=2){
	scale0 = vec_min(scale0,vp->new_metrics->v[i]);
	scale1 = vec_min(scale1,vp->new_metrics->v[i+1]);
      }
      scale0 = vec_min(scale0,scale1);
      scale0 = vec_min(scale0,vec_sld(scale0,scale0,8));
      scale0 = vec_min(scale0,vec_sld(scale0,scale0,4));
      scale0 = vec_min(scale0,vec_sld(scale0,scale0,2));
      scale0 = vec_min(scale0,vec_sld(scale0,scale0,1));

      /* Now subtract from all metrics */
      for(i=0;i<16;i++)
	vp->new_metrics->v[i] = vec_subs(vp->new_metrics->v[i],scale0);
    }
    /* Swap pointers to old and new metrics */
    tmp = vp->old_metrics;
    vp->old_metrics = vp->new_metrics;
    vp->new_metrics = tmp;
  }
  vp->dp = d;
  return 0;
}

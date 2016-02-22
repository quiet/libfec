/* K=9 r=1/3 Viterbi decoder for PowerPC G4/G5 Altivec vector instructions
 * 8-bit offset-binary soft decision samples
 * Copyright Aug 2006, Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <limits.h>
#include "fec.h"

typedef union { unsigned char c[2][16]; vector unsigned char v[2]; } decision_t;
typedef union { unsigned short s[256]; vector unsigned short v[32]; } metric_t;

static union branchtab39 { unsigned short s[128]; vector unsigned short v[16];} Branchtab39[3];
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
int init_viterbi39_av(void *p,int starting_state){
  struct v39 *vp = p;
  int i;

  for(i=0;i<32;i++)
    vp->metrics1.v[i] = (vector unsigned short)(1000);

  vp->old_metrics = &vp->metrics1;
  vp->new_metrics = &vp->metrics2;
  vp->dp = vp->decisions;
  vp->old_metrics->s[starting_state & 255] = 0; /* Bias known start state */
  return 0;
}

void set_viterbi39_polynomial_av(int polys[3]){
  int state;

  for(state=0;state < 128;state++){
    Branchtab39[0].s[state] = (polys[0] < 0) ^ parity((2*state) & abs(polys[0])) ? 255 : 0;
    Branchtab39[1].s[state] = (polys[1] < 0) ^ parity((2*state) & abs(polys[1])) ? 255 : 0;
    Branchtab39[2].s[state] = (polys[2] < 0) ^ parity((2*state) & abs(polys[2])) ? 255 : 0;
  }
  Init++;
}

/* Create a new instance of a Viterbi decoder */
void *create_viterbi39_av(int len){
  struct v39 *vp;

  if(!Init){
    int polys[3] = { V39POLYA, V39POLYB, V39POLYC };

    set_viterbi39_polynomial_av(polys);
  }
  vp = (struct v39 *)malloc(sizeof(struct v39));
  vp->decisions = malloc(sizeof(decision_t)*(len+8));
  init_viterbi39_av(vp,0);
  return vp;
}

/* Viterbi chainback */
int chainback_viterbi39_av(
      void *p,
      unsigned char *data, /* Decoded output data */
      unsigned int nbits, /* Number of data bits */
      unsigned int endstate){ /* Terminal encoder state */
  struct v39 *vp = p;
  decision_t *d = (decision_t *)vp->decisions;
  int path_metric;

  /* Make room beyond the end of the encoder register so we can
   * accumulate a full byte of decoded data
   */
  endstate %= 256;

  path_metric = vp->old_metrics->s[endstate];

  /* The store into data[] only needs to be done every 8 bits.
   * But this avoids a conditional branch, and the writes will
   * combine in the cache anyway
   */
  d += 8; /* Look past tail */
  while(nbits-- != 0){
    int k;
    
    k = (d[nbits].c[endstate >> 7][endstate & 15] & (0x80 >> ((endstate>>4)&7)) ) ? 1 : 0;
    endstate = (k << 7) | (endstate >> 1);
    data[nbits>>3] = endstate;
  }
  return path_metric;
}

/* Delete instance of a Viterbi decoder */
void delete_viterbi39_av(void *p){
  struct v39 *vp = p;

  if(vp != NULL){
    free(vp->decisions);
    free(vp);
  }
}

int update_viterbi39_blk_av(void *p,unsigned char *syms,int nbits){
  struct v39 *vp = p;
  decision_t *d = (decision_t *)vp->dp;
  int path_metric = 0;
  vector unsigned char decisions = (vector unsigned char)(0);

  while(nbits--){
    vector unsigned short symv,sym0v,sym1v,sym2v;
    vector unsigned char s;
    void *tmp;
    int i;
    
    /* Splat the 0th symbol across sym0v, the 1st symbol across sym1v, etc */
    s = (vector unsigned char)vec_perm(vec_ld(0,syms),vec_ld(5,syms),vec_lvsl(0,syms));

    symv = (vector unsigned short)vec_mergeh((vector unsigned char)(0),s);    /* Unsigned byte->word unpack */ 
    sym0v = vec_splat(symv,0);
    sym1v = vec_splat(symv,1);
    sym2v = vec_splat(symv,2);
    syms += 3;
    
    for(i=0;i<16;i++){
      vector bool short decision0,decision1;
      vector unsigned short metric,m_metric,m0,m1,m2,m3,survivor0,survivor1;

      /* Form branch metrics
       * Because Branchtab takes on values 0 and 255, and the values of sym?v are offset binary in the range 0-255,
       * the XOR operations constitute conditional negation.
       * the metrics are in the range 0-765
       */
      m0 = vec_add(vec_xor(Branchtab39[0].v[i],sym0v),vec_xor(Branchtab39[1].v[i],sym1v));
      m1 = vec_xor(Branchtab39[2].v[i],sym2v);
      metric = vec_add(m0,m1);
      m_metric = vec_sub((vector unsigned short)(765),metric);
    
      /* Add branch metrics to path metrics */
      m0 = vec_adds(vp->old_metrics->v[i],metric);
      m3 = vec_adds(vp->old_metrics->v[16+i],metric);
      m1 = vec_adds(vp->old_metrics->v[16+i],m_metric);
      m2 = vec_adds(vp->old_metrics->v[i],m_metric);
    
      /* Compare and select */
      decision0 = vec_cmpgt(m0,m1);
      decision1 = vec_cmpgt(m2,m3);
      survivor0 = vec_min(m0,m1);
      survivor1 = vec_min(m2,m3);
    
      /* Store decisions and survivors.
       * To save space without SSE2's handy PMOVMSKB instruction, we pack and store them in
       * a funny interleaved fashion that we undo in the chainback function.
       */
      decisions = vec_add(decisions,decisions); /* Shift each byte 1 bit to the left */

      /* Booleans are either 0xff or 0x00. Subtracting 0x00 leaves the lsb zero; subtracting
       * 0xff is equivalent to adding 1, which sets the lsb.
       */
      decisions = vec_sub(decisions,(vector unsigned char)vec_pack(vec_mergeh(decision0,decision1),vec_mergel(decision0,decision1)));

      vp->new_metrics->v[2*i] = vec_mergeh(survivor0,survivor1);
      vp->new_metrics->v[2*i+1] = vec_mergel(survivor0,survivor1);

      if((i % 8) == 7){
	/* We've accumulated a total of 128 decisions, stash and start again */
	d->v[i>>3] = decisions; /* No need to clear, the new bits will replace the old */
      }
    }
#if 0
    /* Experimentally determine metric spread
     * The results are fixed for a given code and input symbol size
     */
    {
      int i;
      vector unsigned short min_metric;
      vector unsigned short max_metric;
      union { vector unsigned short v; unsigned short s[8];} t;
      int minimum,maximum;
      static int max_spread = 0;

      min_metric = max_metric = vp->new_metrics->v[0];
      for(i=1;i<32;i++){
	min_metric = vec_min(min_metric,vp->new_metrics->v[i]);
	max_metric = vec_max(max_metric,vp->new_metrics->v[i]);
      }
      min_metric = vec_min(min_metric,vec_sld(min_metric,min_metric,8));
      max_metric = vec_max(max_metric,vec_sld(max_metric,max_metric,8));
      min_metric = vec_min(min_metric,vec_sld(min_metric,min_metric,4));
      max_metric = vec_max(max_metric,vec_sld(max_metric,max_metric,4));
      min_metric = vec_min(min_metric,vec_sld(min_metric,min_metric,2));
      max_metric = vec_max(max_metric,vec_sld(max_metric,max_metric,2));

      t.v = min_metric;
      minimum = t.s[0];
      t.v = max_metric;
      maximum = t.s[0];
      if(maximum-minimum > max_spread){
	max_spread = maximum-minimum;
	printf("metric spread = %d\n",max_spread);
      }
    }
#endif

    /* Renormalize if necessary. This deserves some explanation.
     * The maximum possible spread, found by experiment, for 8 bit symbols is about 3825
     * So by looking at one arbitrary metric we can tell if any of them have possibly saturated.
     * However, this is very conservative. Large spreads occur only at very high Eb/No, where
     * saturating a bad path metric doesn't do much to increase its chances of being erroneously chosen as a survivor.

     * At more interesting (low) Eb/No ratios, the spreads are much smaller so our chances of saturating a metric
     * by not not normalizing when we should are extremely low. So either way, the risk to performance is small.

     * All this is borne out by experiment.
     */
    if(vp->new_metrics->s[0] >= USHRT_MAX-5000){
      vector unsigned short scale;
      union { vector unsigned short v; unsigned short s[8];} t;
      
      /* Find smallest metric and splat */
      scale = vp->new_metrics->v[0];
      for(i=1;i<32;i++)
	scale = vec_min(scale,vp->new_metrics->v[i]);

      scale = vec_min(scale,vec_sld(scale,scale,8));
      scale = vec_min(scale,vec_sld(scale,scale,4));
      scale = vec_min(scale,vec_sld(scale,scale,2));

      /* Subtract it from all metrics
       * Work backwards to try to improve the cache hit ratio, assuming LRU
       */
      for(i=31;i>=0;i--)
	vp->new_metrics->v[i] = vec_subs(vp->new_metrics->v[i],scale);
      t.v = scale;
      path_metric += t.s[0];
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

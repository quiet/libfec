/* K=15 r=1/6 Viterbi decoder for PowerPC G4/G5 Altivec vector instructions
 * 8-bit offset-binary soft decision samples
 * Copyright Mar 2004, Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <limits.h>
#include "fec.h"

typedef union { unsigned char c[128][16]; vector unsigned char v[128]; } decision_t;
typedef union { unsigned short s[16384]; vector unsigned short v[2048]; } metric_t;

static union branchtab615 { unsigned short s[8192]; vector unsigned short v[1024];} Branchtab615[6];
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
int init_viterbi615_av(void *p,int starting_state){
  struct v615 *vp = p;
  int i;

  if(p == NULL)
    return -1;

  for(i=0;i<2048;i++)
    vp->metrics1.v[i] = (vector unsigned short)(5000);

  vp->old_metrics = &vp->metrics1;
  vp->new_metrics = &vp->metrics2;
  vp->dp = vp->decisions;
  vp->old_metrics->s[starting_state & 16383] = 0; /* Bias known start state */
  return 0;
}

/* Create a new instance of a Viterbi decoder */
void *create_viterbi615_av(int len){
  struct v615 *vp;

  if(!Init){
    int polys[6] = { V615POLYA,V615POLYB,V615POLYC,V615POLYD,V615POLYE,V615POLYF };
    set_viterbi615_polynomial_av(polys);
  }
  vp = (struct v615 *)malloc(sizeof(struct v615));
  vp->decisions = malloc(sizeof(decision_t)*(len+14));
  init_viterbi615_av(vp,0);
  return vp;
}

void set_viterbi615_polynomial_av(int polys[6]){
  int state;
  int i;

  for(state=0;state < 8192;state++){
    for(i=0;i<6;i++)
      Branchtab615[i].s[state] = (polys[i] < 0) ^ parity((2*state) & abs(polys[i])) ? 255 : 0;
  }
  Init++;
}


/* Viterbi chainback */
int chainback_viterbi615_av(
      void *p,
      unsigned char *data, /* Decoded output data */
      unsigned int nbits, /* Number of data bits */
      unsigned int endstate){ /* Terminal encoder state */
  struct v615 *vp = p;
  decision_t *d = (decision_t *)vp->decisions;
  int path_metric;

  endstate %= 16384;

  path_metric = vp->old_metrics->s[endstate];

  /* The store into data[] only needs to be done every 8 bits.
   * But this avoids a conditional branch, and the writes will
   * combine in the cache anyway
   */
  d += 14; /* Look past tail */
  while(nbits-- != 0){
    int k;
    
    k = (d[nbits].c[endstate >> 7][endstate & 15] & (0x80 >> ((endstate>>4)&7)) ) ? 1 : 0;
    endstate = (k << 13) | (endstate >> 1);
    data[nbits>>3] = endstate >> 6;
  }
  return path_metric;
}

/* Delete instance of a Viterbi decoder */
void delete_viterbi615_av(void *p){
  struct v615 *vp = p;

  if(vp != NULL){
    free(vp->decisions);
    free(vp);
  }
}

int update_viterbi615_blk_av(void *p,unsigned char *syms,int nbits){
  struct v615 *vp = p;
  decision_t *d = (decision_t *)vp->dp;
  int path_metric = 0;
  vector unsigned char decisions = (vector unsigned char)(0);

  while(nbits--){
    vector unsigned short symv,sym0v,sym1v,sym2v,sym3v,sym4v,sym5v;
    vector unsigned char s;
    void *tmp;
    int i;
    
    /* Splat the 0th symbol across sym0v, the 1st symbol across sym1v, etc */
    s = (vector unsigned char)vec_perm(vec_ld(0,syms),vec_ld(5,syms),vec_lvsl(0,syms));

    symv = (vector unsigned short)vec_mergeh((vector unsigned char)(0),s);    /* Unsigned byte->word unpack */ 
    sym0v = vec_splat(symv,0);
    sym1v = vec_splat(symv,1);
    sym2v = vec_splat(symv,2);
    sym3v = vec_splat(symv,3);
    sym4v = vec_splat(symv,4);
    sym5v = vec_splat(symv,5);
    syms += 6;
    
    for(i=0;i<1024;i++){
      vector bool short decision0,decision1;
      vector unsigned short metric,m_metric,m0,m1,m2,m3,survivor0,survivor1;

      /* Form branch metrics
       * Because Branchtab takes on values 0 and 255, and the values of sym?v are offset binary in the range 0-255,
       * the XOR operations constitute conditional negation.
       * metric and m_metric (-metric) are in the range 0-1530
       */
      m0 = vec_add(vec_xor(Branchtab615[0].v[i],sym0v),vec_xor(Branchtab615[1].v[i],sym1v));
      m1 = vec_add(vec_xor(Branchtab615[2].v[i],sym2v),vec_xor(Branchtab615[3].v[i],sym3v));
      m2 = vec_add(vec_xor(Branchtab615[4].v[i],sym4v),vec_xor(Branchtab615[5].v[i],sym5v));
      metric = vec_add(m0,m1);
      metric = vec_add(metric,m2);
      m_metric = vec_sub((vector unsigned short)(1530),metric);
    
      /* Add branch metrics to path metrics */
      m0 = vec_adds(vp->old_metrics->v[i],metric);
      m3 = vec_adds(vp->old_metrics->v[1024+i],metric);
      m1 = vec_adds(vp->old_metrics->v[1024+i],m_metric);
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
      for(i=1;i<2048;i++){
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

     * The maximum possible spread, found by experiment, for 4-bit symbols is 405; for 8 bit symbols, it's 12750.
     * So by looking at one arbitrary metric we can tell if any of them have possibly saturated.
     * However, this is very conservative. Large spreads occur only at very high Eb/No, where
     * saturating a bad path metric doesn't do much to increase its chances of being erroneously chosen as a survivor.

     * At more interesting (low) Eb/No ratios, the spreads are much smaller so our chances of saturating a metric
     * by not not normalizing when we should are extremely low. So either way, the risk to performance is small.

     * All this is borne out by experiment.
     */
    if(vp->new_metrics->s[0] >= USHRT_MAX-12750){
      vector unsigned short scale;
      union { vector unsigned short v; unsigned short s[8];} t;
      
      /* Find smallest metric and splat */
      scale = vp->new_metrics->v[0];
      for(i=1;i<2048;i++)
	scale = vec_min(scale,vp->new_metrics->v[i]);

      scale = vec_min(scale,vec_sld(scale,scale,8));
      scale = vec_min(scale,vec_sld(scale,scale,4));
      scale = vec_min(scale,vec_sld(scale,scale,2));

      /* Subtract it from all metrics
       * Work backwards to try to improve the cache hit ratio, assuming LRU
       */
      for(i=2047;i>=0;i--)
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

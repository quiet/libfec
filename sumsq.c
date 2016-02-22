/* Compute the sum of the squares of a vector of signed shorts

 * Copyright 2004 Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */

#include <stdlib.h>
#include "fec.h"

unsigned long long sumsq_port(signed short *,int);

#ifdef __i386__
unsigned long long sumsq_mmx(signed short *,int);
unsigned long long sumsq_sse(signed short *,int);
unsigned long long sumsq_sse2(signed short *,int);
#endif

#ifdef __VEC__
unsigned long long sumsq_av(signed short *,int);
#endif

unsigned long long sumsq(signed short *in,int cnt){
  switch(Cpu_mode){
  case PORT:
  default:
    return sumsq_port(in,cnt);
#ifdef __i386__
  case SSE:
  case MMX:
    return sumsq_mmx(in,cnt);
  case SSE2:
    return sumsq_sse2(in,cnt);
#endif

#ifdef __VEC__
  case ALTIVEC:
    return sumsq_av(in,cnt);
#endif
  }
}

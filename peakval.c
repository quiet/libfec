/* Switch to appropriate version of peakval routine
 * Copyright 2004, Phil Karn, KA9Q
 */

#include <stdlib.h>
#include "fec.h"

int peakval_port(signed short *b,int cnt);
#ifdef __i386__
int peakval_mmx(signed short *b,int cnt);
int peakval_sse(signed short *b,int cnt);
int peakval_sse2(signed short *b,int cnt);
#endif

#ifdef __VEC__
int peakval_av(signed short *b,int cnt);
#endif

int peakval(signed short *b,int cnt){
  find_cpu_mode();

  switch(Cpu_mode){
  case PORT:
  default:
    return peakval_port(b,cnt);
#ifdef __i386__
  case MMX:
    return peakval_mmx(b,cnt);
  case SSE:
    return peakval_sse(b,cnt);
  case SSE2:
    return peakval_sse2(b,cnt);
#endif
#ifdef __VEC__
  case ALTIVEC:
    return peakval_av(b,cnt);
#endif
  }
}

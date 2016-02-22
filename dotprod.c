/* 16-bit signed integer dot product
 * Switch to appropriate versions
 * Copyright 2004 Phil Karn
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <stdlib.h>
#include "fec.h"

void *initdp_port(signed short coeffs[],int len);
long dotprod_port(void *p,signed short *b);
void freedp_port(void *p);

#ifdef __i386__
void *initdp_mmx(signed short coeffs[],int len);
void *initdp_sse2(signed short coeffs[],int len);
long dotprod_mmx(void *p,signed short *b);
long dotprod_sse2(void *p,signed short *b);
void freedp_mmx(void *p);
void freedp_sse2(void *p);
#endif

#ifdef __VEC__
void *initdp_av(signed short coeffs[],int len);
long dotprod_av(void *p,signed short *b);
void freedp_av(void *p);
#endif

/* Create and return a descriptor for use with the dot product function */
void *initdp(signed short coeffs[],int len){
  find_cpu_mode();

  switch(Cpu_mode){
  case PORT:
  default:
    return initdp_port(coeffs,len);
#ifdef __i386__
  case MMX:
  case SSE:
    return initdp_mmx(coeffs,len);
  case SSE2:
    return initdp_sse2(coeffs,len);
#endif

#ifdef __VEC__
  case ALTIVEC:
    return initdp_av(coeffs,len);
#endif
  }
}


/* Free a dot product descriptor created earlier */
void freedp(void *p){
  switch(Cpu_mode){
  case PORT:
  default:
#ifdef __i386__
  case MMX:
  case SSE:
    return freedp_mmx(p);
  case SSE2:
    return freedp_sse2(p);
#endif
#ifdef __VEC__
  case ALTIVEC:
    return freedp_av(p);
#endif
  }
}

/* Compute a dot product given a descriptor and an input array
 * The length is taken from the descriptor
 */
long dotprod(void *p,signed short a[]){
  switch(Cpu_mode){
  case PORT:
  default:
    return dotprod_port(p,a);
#ifdef __i386__
  case MMX:
  case SSE:
    return dotprod_mmx(p,a);
  case SSE2:
    return dotprod_sse2(p,a);
#endif

#ifdef __VEC__
  case ALTIVEC:
    return dotprod_av(p,a);
#endif
  }
}



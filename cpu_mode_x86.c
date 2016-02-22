/* Determine CPU support for SIMD
 * Copyright 2004 Phil Karn, KA9Q
 */
#include <stdio.h>
#include "fec.h"

/* Various SIMD instruction set names */
char *Cpu_modes[] = {"Unknown","Portable C","x86 Multi Media Extensions (MMX)",
		   "x86 Streaming SIMD Extensions (SSE)",
		   "x86 Streaming SIMD Extensions 2 (SSE2)",
		   "PowerPC G4/G5 Altivec/Velocity Engine"};

enum cpu_mode Cpu_mode;

void find_cpu_mode(void){

  int f;
  if(Cpu_mode != UNKNOWN)
    return;

  /* Figure out what kind of CPU we have */
  f = cpu_features();
  if(f & (1<<26)){ /* SSE2 is present */
    Cpu_mode = SSE2;
  } else if(f & (1<<25)){ /* SSE is present */
    Cpu_mode = SSE;
  } else if(f & (1<<23)){ /* MMX is present */
    Cpu_mode = MMX;
  } else { /* No SIMD at all */
    Cpu_mode = PORT;
  }
  fprintf(stderr,"SIMD CPU detect: %s\n",Cpu_modes[Cpu_mode]);
}

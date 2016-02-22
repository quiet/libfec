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

  Cpu_mode = PORT;
}

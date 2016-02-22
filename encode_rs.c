/* Reed-Solomon encoder
 * Copyright 2002, Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <string.h>

#ifdef FIXED
#include "fixed.h"
#elif defined(BIGSYM)
#include "int.h"
#else
#include "char.h"
#endif

void ENCODE_RS(
#ifdef FIXED
data_t *data, data_t *bb,int pad){
#else
void *p,data_t *data, data_t *bb){
  struct rs *rs = (struct rs *)p;
#endif
  int i, j;
  data_t feedback;

#ifdef FIXED
  /* Check pad parameter for validity */
  if(pad < 0 || pad >= NN)
    return;
#endif

  memset(bb,0,NROOTS*sizeof(data_t));

  for(i=0;i<NN-NROOTS-PAD;i++){
    feedback = INDEX_OF[data[i] ^ bb[0]];
    if(feedback != A0){      /* feedback term is non-zero */
#ifdef UNNORMALIZED
      /* This line is unnecessary when GENPOLY[NROOTS] is unity, as it must
       * always be for the polynomials constructed by init_rs()
       */
      feedback = MODNN(NN - GENPOLY[NROOTS] + feedback);
#endif
      for(j=1;j<NROOTS;j++)
	bb[j] ^= ALPHA_TO[MODNN(feedback + GENPOLY[NROOTS-j])];
    }
    /* Shift */
    memmove(&bb[0],&bb[1],sizeof(data_t)*(NROOTS-1));
    if(feedback != A0)
      bb[NROOTS-1] = ALPHA_TO[MODNN(feedback + GENPOLY[0])];
    else
      bb[NROOTS-1] = 0;
  }
}

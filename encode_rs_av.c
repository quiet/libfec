/* Fast Reed-Solomon encoder for (255,223) CCSDS code on PowerPC G4/G5 using Altivec instructions
 * Copyright 2004, Phil Karn KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */
#include <stdio.h>
#include <string.h>
#include "fixed.h"

/* Lookup table for feedback multiplications
 * These are the low half of the coefficients. Since the generator polynomial is
 * palindromic, we form it by reversing these on the fly
 */
static union { vector unsigned char v; unsigned char c[16]; } table[256];

static vector unsigned char reverse = (vector unsigned char)(0,15,14,13,12,11,10,9,8,7,6,5,4,3,2,1);
static vector unsigned char shift_right = (vector unsigned char)(15,16,17,18,19,20,21,22,23,24,25,26,27,28,29,30);

extern data_t CCSDS_alpha_to[];
extern data_t CCSDS_index_of[];
extern data_t CCSDS_poly[];

void rs_init_av(){
  int i,j;

  /* The PowerPC is big-endian, so the low-order byte of each vector contains the highest order term in the polynomial */
  for(j=0;j<16;j++){
    table[0].c[j] = 0;
    for(i=1;i<256;i++){
      table[i].c[16-j-1] = CCSDS_alpha_to[MODNN(CCSDS_poly[j+1] + CCSDS_index_of[i])];
    }
  }
#if 0
  for(i=0;i<256;i++){
    printf("table[%3d] = %3vu\n",i,table[i].v);
  }
#endif
}

void encode_rs_av(unsigned char *data,unsigned char *parity,int pad){
  union { vector unsigned char v[2]; unsigned char c[32]; } shift_register;
  int i;

  shift_register.v[0] = (vector unsigned char)(0);
  shift_register.v[1] = (vector unsigned char)(0);
  
  for(i=0;i<NN-NROOTS-pad;i++){
    vector unsigned char feedback0,feedback1;
    unsigned char f;

    f = data[i] ^ shift_register.c[31];
    feedback1 = table[f].v;
    feedback0 = vec_perm(feedback1,feedback1,reverse);

    /* Shift right one byte */
    shift_register.v[1] = vec_perm(shift_register.v[0],shift_register.v[1],shift_right) ^ feedback1;
    shift_register.v[0] = vec_sro(shift_register.v[0],(vector unsigned char)(8)) ^ feedback0;
    shift_register.c[0] = f;
  }
  for(i=0;i<NROOTS;i++)
    parity[NROOTS-i-1] = shift_register.c[i];
}

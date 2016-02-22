/* Test dot-product function */

#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <math.h>
#include "config.h"
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include "fec.h"

#if HAVE_GETOPT_LONG
struct option Options[] = {
  {"force-altivec",0,NULL,'a'},
  {"force-port",0,NULL,'p'},
  {"force-mmx",0,NULL,'m'},
  {"force-sse",0,NULL,'s'},
  {"force-sse2",0,NULL,'t'},
  {"trials",0,NULL,'n'},
  {NULL},
};
#endif

int main(int argc,char *argv[]){
  short coeffs[512];
  short input[2048];
  int trials=1000,d;
  int errors = 0;

#if HAVE_GETOPT_LONG
  while((d = getopt_long(argc,argv,"apmstn:",Options,NULL)) != EOF){
#else
  while((d = getopt(argc,argv,"apmstn:")) != EOF){
#endif
    switch(d){
    case 'a':
      Cpu_mode = ALTIVEC;
      break;
    case 'p':
      Cpu_mode = PORT;
      break;
    case 'm':
      Cpu_mode = MMX;
      break;
    case 's':
      Cpu_mode = SSE;
      break;
    case 't':
      Cpu_mode = SSE2;
      break;
    case 'n':
      trials = atoi(optarg);
      break;
    }
  }

  while(trials--){
    long port_result;
    long simd_result;
    int ntaps;
    int i;
    int csum = 0;
    int offset;
    void *dp_simd,*dp_port;

    /* Generate set of coefficients
     * limit sum of absolute values to 32767 to avoid overflow
     */
    memset(coeffs,0,sizeof(coeffs));
    for(i=0;i<512;i++){
      double gv;

      gv = normal_rand(0.,100.);
      if(csum + fabs(gv) > 32767)
	break;
      coeffs[i] = gv;
      csum += fabs(gv);
    }
    ntaps = i;

    /* Compare results to portable C version for a bunch of random data buffers and offsets */
    dp_simd = initdp(coeffs,ntaps);
    dp_port = initdp_port(coeffs,ntaps);
    
    for(i=0;i<2048;i++)
      input[i] = random();
    
    offset = random() & 511;

    simd_result = dotprod(dp_simd,input+offset);
    port_result = dotprod_port(dp_port,input+offset);
    if(simd_result != port_result){
      errors++;
    }
  }
  printf("dtest: %d errors\n",errors);
  exit(0);
}

/* Test viterbi decoder speeds */
#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <time.h>
#include <math.h>
#include <memory.h>
#include <sys/time.h>
#include <sys/resource.h>
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include "fec.h"

#if HAVE_GETOPT_LONG
struct option Options[] = {
  {"frame-length",1,NULL,'l'},
  {"frame-count",1,NULL,'n'},
  {"ebn0",1,NULL,'e'},
  {"gain",1,NULL,'g'},
  {"verbose",0,NULL,'v'},
  {"force-altivec",0,NULL,'a'},
  {"force-port",0,NULL,'p'},
  {"force-mmx",0,NULL,'m'},
  {"force-sse",0,NULL,'s'},
  {"force-sse2",0,NULL,'t'},
  {NULL},
};
#endif

#define RATE (1./3.)
#define MAXBYTES 10000

double Gain = 32.0;
int Verbose = 0;

int main(int argc,char *argv[]){
  int i,d,tr;
  int sr=0,trials = 10000,errcnt,framebits=2048;
  long long tot_errs=0;
  unsigned char bits[MAXBYTES];
  unsigned char data[MAXBYTES];
  unsigned char xordata[MAXBYTES];
  unsigned char symbols[8*3*(MAXBYTES+8)];
  void *vp;
  extern char *optarg;
  struct rusage start,finish;
  double extime;
  double gain,esn0,ebn0;
  time_t t;
  int badframes=0;

  time(&t);
  srandom(t);
  ebn0 = -100;
#if HAVE_GETOPT_LONG
  while((d = getopt_long(argc,argv,"l:n:te:g:vapmst",Options,NULL)) != EOF){
#else
  while((d = getopt(argc,argv,"l:n:te:g:vapmst")) != EOF){
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
    case 'l':
      framebits = atoi(optarg);
      break;
    case 'n':
      trials = atoi(optarg);
      break;
    case 'e':
      ebn0 = atof(optarg);
      break;
    case 'g':
      Gain = atof(optarg);
      break;
    case 'v':
      Verbose++;
      break;
    }
  }
  if(framebits > 8*MAXBYTES){
    fprintf(stderr,"Frame limited to %d bits\n",MAXBYTES*8);
    framebits = MAXBYTES*8;
  }
  if((vp = create_viterbi39(framebits)) == NULL){
    printf("create_viterbi39 failed\n");
    exit(1);
  }
  if(ebn0 != -100){
    esn0 = ebn0 + 10*log10((double)RATE); /* Es/No in dB */
    /* Compute noise voltage. The 0.5 factor accounts for BPSK seeing
     * only half the noise power, and the sqrt() converts power to
     * voltage.
     */
    gain = 1./sqrt(0.5/pow(10.,esn0/10.));
    
    printf("nframes = %d framesize = %d ebn0 = %.2f dB gain = %g\n",trials,framebits,ebn0,Gain);
    
    for(tr=0;tr<trials;tr++){
      /* Encode a frame of random data */
      for(i=0;i<framebits+8;i++){
	int bit = (i < framebits) ? (random() & 1) : 0;
	
	sr = (sr << 1) | bit;
	bits[i/8] = sr & 0xff;
	symbols[3*i+0] = addnoise(parity(sr & V39POLYA),gain,Gain,127.5,255);
	symbols[3*i+1] = addnoise(parity(sr & V39POLYB),gain,Gain,127.5,255);
	symbols[3*i+2] = addnoise(parity(sr & V39POLYC),gain,Gain,127.5,255);
      }
      /* Decode it and make sure we get the right answer */
      /* Initialize Viterbi decoder */
      init_viterbi39(vp,0);
      
      /* Decode block */
      update_viterbi39_blk(vp,symbols,framebits+8);
      
      /* Do Viterbi chainback */
      chainback_viterbi39(vp,data,framebits,0);
      errcnt = 0;
      for(i=0;i<framebits/8;i++){
	int e = Bitcnt[xordata[i] = data[i] ^ bits[i]];
	errcnt += e;
	tot_errs += e;
      }
      if(errcnt != 0)
	badframes++;
      if(Verbose > 1 && errcnt != 0){
	printf("frame %d, %d errors: ",tr,errcnt);
	for(i=0;i<framebits/8;i++){
	  printf("%02x",xordata[i]);
	}
	printf("\n");
      }
      if(Verbose)
	printf("BER %lld/%lld (%10.3g) FER %d/%d (%10.3g)\r",
	       tot_errs,(long long)framebits*(tr+1),tot_errs/((double)framebits*(tr+1)),
	       badframes,tr+1,(double)badframes/(tr+1));
      fflush(stdout);
    }
    if(Verbose > 1)
      printf("nframes = %d framesize = %d ebn0 = %.2f dB gain = %g\n",trials,framebits,ebn0,Gain);
    else if(Verbose == 0)
      printf("BER %lld/%lld (%.3g) FER %d/%d (%.3g)\n",
	     tot_errs,(long long)framebits*trials,tot_errs/((double)framebits*trials),
	     badframes,tr+1,(double)badframes/(tr+1));
    else
      printf("\n");
  } else {
    /* Do time trials */
    memset(symbols,127,sizeof(symbols));
    printf("Starting time trials\n");
    getrusage(RUSAGE_SELF,&start);
    for(tr=0;tr < trials;tr++){
      /* Initialize Viterbi decoder */
      init_viterbi39(vp,0);
      
      /* Decode block */
      update_viterbi39_blk(vp,symbols,framebits);
      
      /* Do Viterbi chainback */
      chainback_viterbi39(vp,data,framebits,0);
    }
    getrusage(RUSAGE_SELF,&finish);
    extime = finish.ru_utime.tv_sec - start.ru_utime.tv_sec + 1e-6*(finish.ru_utime.tv_usec - start.ru_utime.tv_usec);
    printf("Execution time for %d %d-bit frames: %.2f sec\n",trials,
	   framebits,extime);
    printf("decoder speed: %g bits/s\n",trials*framebits/extime);
  }
  exit(0);
}



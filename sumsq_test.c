#include <stdio.h>
#include <stdlib.h>
#include <memory.h>
#include <time.h>
#include "config.h"
#ifdef HAVE_GETOPT_H
#include <getopt.h>
#endif
#include "fec.h"

#if HAVE_GETOPT_LONG
struct option Options[] = {
  {"frame-length",1,NULL,'l'},
  {"frame-count",1,NULL,'n'},
  {"verbose",0,NULL,'v'},
  {"force-altivec",0,NULL,'a'},
  {"force-port",0,NULL,'p'},
  {"force-mmx",0,NULL,'m'},
  {"force-sse",0,NULL,'s'},
  {"force-sse2",0,NULL,'t'},
  {NULL},
};
#endif

int Verbose = 0;

int main(int argc,char *argv[]){
  signed short *buf;
  int i,d,trial,trials=10000;
  int bufsize = 2048;
  long long port_sum,simd_sum;
  time_t t;
  int timetrials=0;

  find_cpu_mode();
  time(&t);
  srandom(t);

#if HAVE_GETOPT_LONG
  while((d = getopt_long(argc,argv,"vapmstl:n:T",Options,NULL)) != EOF){
#else
  while((d = getopt(argc,argv,"vapmstl:n:T")) != EOF){
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
      bufsize = atoi(optarg);
      break;
    case 'n':
      trials = atoi(optarg);
      break;
    case 'v':
      Verbose++;
      break;
    case 'T':
      timetrials++;
      break;
    }
  }

  buf = (signed short *)calloc(bufsize,sizeof(signed short));
  if(timetrials){
    for(trial=0;trial<trials;trial++){
      (void)sumsq(buf,bufsize);
    }
  } else {
    for(trial=0;trial<trials;trial++){
      int length,offset;

      offset = random() & 7;
      length = (random() % bufsize) - offset;
      if(length <= 0)
	continue;
      for(i=0;i<bufsize;i++)
	buf[i] = random();
      
      port_sum = sumsq_port(buf+offset,length);
      simd_sum = sumsq(buf+offset,length);
      if(port_sum != simd_sum){
	printf("offset %d len %d port_sum = %lld simd_sum = %lld ",offset,length,port_sum,simd_sum);
	
	printf("ERROR! diff = %lld\n",simd_sum-port_sum);
      }
    }
  }
  exit(0);
}

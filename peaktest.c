/* Verify correctness of the peak routine
 * Copyright 2004 Phil Karn, KA9Q
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* These values should trigger leading/trailing array fragment handling */
#define NSAMP 200002
#define OFFSET 1

int peakval(signed short *,int);
int peakval_port(signed short *,int);

int main(){
  int i,s;
  int result,rresult;
  signed short samples[NSAMP];

  srandom(time(NULL));

  for(i=0;i<NSAMP;i++){
    do {
      s = random() & 0x0fff;
    } while(s == 0x8000);
    samples[i] = s;
  }
  samples[5] = 25000;

  rresult = peakval_port(&samples[OFFSET],NSAMP-OFFSET);
  result = peakval(&samples[OFFSET],NSAMP-OFFSET);
  if(result == rresult){
    printf("OK\n");
  } else {
    printf("peak mismatch: %d != %d\n",result,rresult);
  }
  exit(0);
}

/* Verify correctness of the sum-of-square routines */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

/* These values should trigger leading/trailing array fragment handling */
#define NSAMP 200002
#define OFFSET 1

long long sumsq_wq(signed short *in,int cnt);
long long sumsq_wq_ref(signed short *in,int cnt);

int main(){
  int i;
  long long result,rresult;
  signed short samples[NSAMP];

  srandom(time(NULL));

  for(i=0;i<NSAMP;i++)
    samples[i] = random() & 0xffff;

  rresult = sumsq_wq(&samples[OFFSET],NSAMP-OFFSET);
  result = sumsq_wq(&samples[OFFSET],NSAMP-OFFSET);
  if(result == rresult){
    printf("OK\n");
  } else {
    printf("sum mismatch: %lld != %lld\n",result,rresult);
  }
  exit(0);
}

long long sumsq_wq_ref(signed short *in,int cnt){
  long long sum = 0;
  int i;

  for(i=0;i<cnt;i++){
    sum += (long)in[i] * in[i];
  }
  return sum;
}


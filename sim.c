#include <math.h>
#include <stdlib.h>
#include "fec.h"

#define	MAX_RANDOM	0x7fffffff

/* Generate gaussian random double with specified mean and std_dev */
double normal_rand(double mean, double std_dev)
{
  double fac,rsq,v1,v2;
  static double gset;
  static int iset;

  if(iset){
    /* Already got one */
    iset = 0;
    return mean + std_dev*gset;
  }
  /* Generate two evenly distributed numbers between -1 and +1
   * that are inside the unit circle
   */
  do {
    v1 = 2.0 * (double)random() / MAX_RANDOM - 1;
    v2 = 2.0 * (double)random() / MAX_RANDOM - 1;
    rsq = v1*v1 + v2*v2;
  } while(rsq >= 1.0 || rsq == 0.0);
  fac = sqrt(-2.0*log(rsq)/rsq);
  gset = v1*fac;
  iset++;
  return mean + std_dev*v2*fac;
}

unsigned char addnoise(int sym,double amp,double gain,double offset,int clip){
  int sample;
    
  sample = offset + gain*normal_rand(sym?amp:-amp,1.0);
  /* Clip to 8-bit offset range */
  if(sample < 0)
    sample = 0;
  else if(sample > clip)
    sample = clip;
  return sample;
}

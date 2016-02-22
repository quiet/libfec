/* Compute the sum of the squares of a vector of signed shorts

 *  Portable C version
 * Copyright 2004 Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */

unsigned long long sumsq_port(signed short *in,int cnt){
  long long sum = 0;
  int i;

  for(i=0;i<cnt;i++){
    sum += (int)in[i] * (int)in[i];
  }
  return sum;
}

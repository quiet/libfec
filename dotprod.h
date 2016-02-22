/* Internal definitions for dotproduct function */

struct dotprod {
  int len; /* Number of coefficients */

  /* On a MMX or SSE machine, these hold 4 copies of the coefficients,
   * preshifted by 0,1,2,3 words to meet all possible input data
   * alignments (see Intel ap559 on MMX dot products).
   *
   * SSE2 is similar, but with 8 words at a time
   *
   * On a non-MMX machine, only one copy is present
   */
  signed short *coeffs[8];
};

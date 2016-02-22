/* General purpose Reed-Solomon decoder
 * Copyright 2003 Phil Karn, KA9Q
 * May be used under the terms of the GNU Lesser General Public License (LGPL)
 */

#ifdef DEBUG
#include <stdio.h>
#endif

#include <string.h>

#include "int.h"
#include "rs-common.h"

int decode_rs_int(void *p, data_t *data, int *eras_pos, int no_eras){
  int retval;
  struct rs *rs = (struct rs *)p;
 
#include "decode_rs.h"
  
  return retval;
}

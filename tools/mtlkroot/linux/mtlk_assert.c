/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "mtlk_assert.h"

//NOTICE function is duplicated?

void __MTLK_IFUNC
__mtlk_assert (mtlk_slid_t slid, const char * xxx , const char * yyy,int line )
{
  printk("Assertion Failed " MTLK_SLID_FMT "\n%s\n%i\n%s\n",
         MTLK_SLID_ARGS(slid),
          yyy, line,xxx);
  BUG();
}

EXPORT_SYMBOL(__mtlk_assert);

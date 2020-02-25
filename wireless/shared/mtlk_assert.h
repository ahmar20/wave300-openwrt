/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MTLK_ASSERT_H__
#define __MTLK_ASSERT_H__

#ifdef MTCFG_DEBUG

//better assert function, otherwise use SLID code and .scd files
void __MTLK_IFUNC
__mtlk_assert(mtlk_slid_t slid, const char * xxx, const char * yyy, int line);


#define __MTLK_ASSERT(expr, slid)  \
  do {                             \
    if (__UNLIKELY(!(expr))) {     \
      __mtlk_assert(slid, #expr, __FILE__, __LINE__);   \
    }                              \
  } while (0)

#else
#define __MTLK_ASSERT(expr, slid)
#endif

#define ASSERT(expr)      __MTLK_ASSERT(expr, MTLK_SLID)
#define MTLK_ASSERT(expr) __MTLK_ASSERT(expr, MTLK_SLID)

#endif /* __MTLK_ASSERT_H__ */


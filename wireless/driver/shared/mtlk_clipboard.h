/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
 * 
 *
 * Clipboard between Core and DF
 *
 * Originally written by Andrii Tseglytskyi
 *
 */

#ifndef _MTLK_CLIPBOARD_H_
#define _MTLK_CLIPBOARD_H_

/* "clipboard" between DF UI and Core */
typedef struct _mtlk_clpb_t mtlk_clpb_t;

mtlk_clpb_t* __MTLK_IFUNC
mtlk_clpb_create(void);

void __MTLK_IFUNC
mtlk_clpb_delete(mtlk_clpb_t *clpb);

void __MTLK_IFUNC
mtlk_clpb_purge(mtlk_clpb_t *clpb);

int __MTLK_IFUNC
mtlk_clpb_push_ex(mtlk_clpb_t *clpb,
                  const void *data_src,
                  void *data_preallocated,
                  uint32 size);

#define mtlk_clpb_push(clpb, data, size) \
  mtlk_clpb_push_ex(clpb, data, NULL, size)

#define mtlk_clpb_push_nocopy(clpb, data_preallocated, size) \
  mtlk_clpb_push_ex(clpb, NULL, data_preallocated, size)

void __MTLK_IFUNC
mtlk_clpb_enum_rewind(mtlk_clpb_t *clpb);

void* __MTLK_IFUNC
mtlk_clpb_enum_get_next(mtlk_clpb_t *clpb, uint32* size);

uint32 __MTLK_IFUNC
mtlk_clpb_get_num_of_elements(mtlk_clpb_t *clpb);

#endif /* _MTLK_CLIPBOARD_H_ */

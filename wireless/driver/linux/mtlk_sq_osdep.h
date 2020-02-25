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
 * Linux dependant send queue parts.
 */

#ifndef __MTLK_SQ_OSDEP_H__
#define __MTLK_SQ_OSDEP_H__

/* functions called from shared SendQueue code */
int  _mtlk_sq_send_to_hw(mtlk_nbuf_t *nbuf, uint16 prio);

#endif /* __MTLK_SQ_OSDEP_H__ */

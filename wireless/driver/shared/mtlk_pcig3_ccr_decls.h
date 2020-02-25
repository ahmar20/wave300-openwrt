/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/* $Id$ */

#if !defined(SAFE_PLACE_TO_INCLUDE_MTLK_PCIG3_CCR_DECLS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_PCIG3_CCR_DECLS */
#undef SAFE_PLACE_TO_INCLUDE_MTLK_PCIG3_CCR_DECLS

#include "shram_ex.h"
#include "pcishram_ex.h"
#include "g3shram_ex.h"

typedef struct
{
  struct g3_pas_map     *pas;
  struct pci_hrc_regs   *hrc;
  void                  *dev;
  mtlk_mmb_drv_t        *mmb_drv;
  mtlk_hw_t             *hw;
} _mtlk_pcig3_ccr_t;

#define G3_BIST_DONE_MASK             (1 << 3)
#define G3_BIST_RED_MASK              (1 << 2)
#define G3_BIST_SINGLE_MASK           (1 << 1)

#define G3_BIST_SUCCESS_MASK          ( G3_BIST_DONE_MASK  | \
                                        G3_BIST_RED_MASK   | \
                                        G3_BIST_SINGLE_MASK )

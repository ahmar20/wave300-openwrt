/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/* $Id$ */

#if !defined(SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DECLS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DECLS */
#undef SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DECLS

#include "shram_ex.h"
#include "g3shram_ex.h"

typedef struct
{
  struct g3_pas_map     *pas;
  void                  *dev;
  mtlk_mmb_drv_t        *mmb_drv;
  mtlk_hw_t             *hw;
  uint8                 current_ucpu_state;
  uint8                 current_lcpu_state;
  uint8                 next_boot_mode;

  volatile BOOL         irqs_enabled;
  volatile BOOL         irq_pending;
  mtlk_osal_spinlock_t  irq_lock;

  MTLK_DECLARE_INIT_STATUS;
} _mtlk_pcie_ccr_t;

#define G3PCIE_CPU_Control_BIST_Passed    ((1 << 31) | (1 << 15)) 

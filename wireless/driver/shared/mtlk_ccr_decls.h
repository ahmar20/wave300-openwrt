/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/* $Id$ */
#if !defined (SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DECLS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DECLS */

#undef SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DECLS

#define SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DECLS
#include "mtlk_pcie_ccr_decls.h"

#define SAFE_PLACE_TO_INCLUDE_MTLK_PCIG3_CCR_DECLS
#include "mtlk_pcig3_ccr_decls.h"

#define SAFE_PLACE_TO_INCLUDE_MTLK_G35_CCR_DECLS
#include "mtlk_g35_ccr_decls.h"

typedef struct _mtlk_ccr_t
{
  union
  {
    _mtlk_pcig3_ccr_t  pcig3;
    _mtlk_pcie_ccr_t   pcie;
    _mtlk_g35_ccr_t    g35;
  } mem;

  mtlk_card_type_t     hw_type;
  mtlk_chip_info_t const *chip_info;

  mtlk_osal_mutex_t    eeprom_mutex;

#ifdef MTCFG_USE_INTERRUPT_POLLING
  mtlk_osal_timer_t    poll_interrupts;
#endif

  MTLK_DECLARE_INIT_STATUS;
} mtlk_ccr_t;

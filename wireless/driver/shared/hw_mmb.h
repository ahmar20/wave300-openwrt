/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __HW_MMB_H__
#define __HW_MMB_H__

#define MTLK_UPPER_CPU       (1 << 0)
#define MTLK_LOWER_CPU       (1 << 1)

#include "mtlkdfdefs.h"
#include "mtlkirbd.h"
#include "mtlk_wss.h"

#include "mtlk_card_types.h"

#include "mtlk_mmb_drv.h"

#define SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DECLS
#include "mtlk_ccr_decls.h"

#include "mtlk_vap_manager.h"

#include "shram_ex.h"

#include "mtlk_osal.h"
#include "txmm.h"

#include "eeprom.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/**************************************************************
 * Auxiliary OS/bus abstractions required for MMB
 * NOTE: must be defined within hw_<platfrorm>.h
 **************************************************************/

/**************************************************************/

/**************************************************************
 * MMB Interface
 **************************************************************/

typedef struct
{
  uint8  bist_check_permitted;
  uint32 no_pll_write_delay_us;
  uint32 man_msg_size;
  uint32 dbg_msg_size;
} __MTLK_IDATA mtlk_hw_mmb_cfg_t;

typedef struct
{
  mtlk_hw_mmb_cfg_t    cfg;
  mtlk_hw_t           *cards[MTLK_MAX_HW_ADAPTERS_SUPPORTED];
  uint32               nof_cards;
  mtlk_osal_spinlock_t lock;
  uint32               bist_passed;

  MTLK_DECLARE_INIT_STATUS;
} __MTLK_IDATA mtlk_hw_mmb_t;

extern mtlk_hw_mmb_t mtlk_mmb_obj;

/**************************************************************
* FW recovery related functions
**************************************************************/
void __MTLK_IFUNC
mtlk_hw_mmb_isolate(mtlk_hw_t *hw);

int __MTLK_IFUNC
mtlk_hw_mmb_restore(mtlk_hw_t *hw);

void __MTLK_IFUNC
mtlk_hw_mmb_dis_interrupts(mtlk_hw_t *hw);

/**************************************************************
 * Init/cleanup functions - must be called on driver's
 * loading/unloading
 **************************************************************/
int __MTLK_IFUNC 
mtlk_hw_mmb_init(mtlk_hw_mmb_t *mmb, const mtlk_hw_mmb_cfg_t *cfg);
void __MTLK_IFUNC
mtlk_hw_mmb_cleanup(mtlk_hw_mmb_t *mmb);
/**************************************************************/

int __MTLK_IFUNC
mtlk_mmb_txmm_init(mtlk_hw_t *hw, mtlk_vap_handle_t master_vap);
int __MTLK_IFUNC
mtlk_mmb_txdm_init(mtlk_hw_t *hw, mtlk_vap_handle_t master_vap);

/**************************************************************
 * Auxiliary MMB interface - for BUS module usage
 **************************************************************/
uint32 __MTLK_IFUNC
mtlk_hw_mmb_get_cards_no(mtlk_hw_mmb_t *mmb);

mtlk_txmm_base_t *__MTLK_IFUNC
mtlk_hw_mmb_get_txmm(mtlk_hw_t *card);

mtlk_txmm_base_t *__MTLK_IFUNC
mtlk_hw_mmb_get_txdm(mtlk_hw_t *card);

uint8 __MTLK_IFUNC
mtlk_hw_mmb_get_card_idx(mtlk_hw_t *card);

/* Stops all the MAC-initiated events (INDs), sending to MAC still working */
void __MTLK_IFUNC
mtlk_hw_mmb_stop_mac_events(mtlk_hw_t *card);
/**************************************************************/

/**************************************************************
 * Add/remove card - must be called on device addition/removal
 **************************************************************/
mtlk_hw_api_t * __MTLK_IFUNC 
mtlk_hw_mmb_add_card (mtlk_hw_mmb_t *mmb);

void __MTLK_IFUNC 
mtlk_hw_mmb_remove_card(mtlk_hw_mmb_t *mmb,
                        mtlk_hw_api_t *hw_api);

void __MTLK_IFUNC
mtlk_hw_mmb_global_version_printout (mtlk_hw_mmb_t *mmb,
                                     char *buffer,
                                     uint32 size);

/**************************************************************
 * Init/cleanup card - must be called on device init/cleanup
 **************************************************************/
int __MTLK_IFUNC 
mtlk_hw_mmb_init_card(mtlk_hw_t *hw, mtlk_ccr_t *ccr, unsigned char *mmb_pas,
                      mtlk_vap_manager_t *vap_manager);

void __MTLK_IFUNC 
mtlk_hw_mmb_cleanup_card(mtlk_hw_t *card);

int __MTLK_IFUNC 
mtlk_hw_mmb_start_card(mtlk_hw_t   *hw);

void __MTLK_IFUNC 
mtlk_hw_mmb_stop_card(mtlk_hw_t *card);

/**************************************************************
 * Send Debug TPC value to FW
 **************************************************************/
int __MTLK_IFUNC
mtlk_mmb_send_debug_tpc(mtlk_hw_t *hw, uint8 value);

/**************************************************************
 * Send logger severity configuration to FW
 **************************************************************/
int __MTLK_IFUNC 
_mtlk_mmb_send_fw_log_severity(mtlk_hw_t *hw,
                               uint32 newLevel,
                               uint32 targetCPU);

/**************************************************************
 * Send Aggregation Rate Limit configuration to FW
 **************************************************************/
int __MTLK_IFUNC
mtlk_mmb_send_agg_rate_limit(mtlk_hw_t *hw,
                             uint8  mode,
                             uint16 maxRate);

/**************************************************************
 * Send RX High Reception Threshold configuration
 **************************************************************/
int __MTLK_IFUNC
mtlk_mmb_send_rx_high_threshold(mtlk_hw_t *hw,
                                int8 value);

/**************************************************************
 * Send RX Duty Cycle configuration
 **************************************************************/
int __MTLK_IFUNC
mtlk_mmb_send_rx_duty_cycle(mtlk_hw_t *hw,
                            uint32 onTime,
                            uint32 offTime);

/**************************************************************
 * Send LNA Gain configuration
 **************************************************************/
int __MTLK_IFUNC
mtlk_mmb_send_lna_gains(mtlk_hw_t *hw,
                              int8 lna_gain_bypass,
                              int8 lna_gain_high);

/**************************************************************
 * Send CCA Threshold configuration
 **************************************************************/
int __MTLK_IFUNC
mtlk_mmb_send_cca_threshold(mtlk_hw_t *hw,
                                int8 value);

/**************************************************************/

/**************************************************************
 * Card's ISR - must be called on interrupt handler
 * Return values:
 *   MTLK_ERR_OK      - do nothing
 *   MTLK_ERR_UNKNOWN - not an our interrupt
 *   MTLK_ERR_PENDING - order bottom half routine (DPC, tasklet etc.)
 **************************************************************/
int __MTLK_IFUNC 
mtlk_hw_mmb_interrupt_handler(mtlk_hw_t *card);
/**************************************************************/

/**************************************************************
 * Card's bottom half of irq handling (DPC, tasklet etc.)
 **************************************************************/
void __MTLK_IFUNC 
mtlk_hw_mmb_deferred_handler(mtlk_hw_t *card);
/**************************************************************/
/**************************************************************/

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#include "mmb_ops.h"

#define SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DEFS
#include "mtlk_ccr_defs.h"

#endif /* __HW_MMB_H__ */

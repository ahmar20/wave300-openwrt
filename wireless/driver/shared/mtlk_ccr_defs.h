/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/* $Id$ */

#if !defined (SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DEFS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DEFS */

#undef SAFE_PLACE_TO_INCLUDE_MTLK_CCR_DEFS

#include "mtlk_card_selector.h"

#define LOG_LOCAL_GID   GID_MTLK_CCR_DEFS
#define LOG_LOCAL_FID   0

/* Clear interrupts (unconditionally) by handling received msg */
#define MTLK_CCR_CLEAR_INT_UNCONDIT

#ifdef MTCFG_CCR_DEBUG
/* Helper BUS read/write macros */
#define __ccr_writel(val, addr) for(;;) { \
    mtlk_osal_emergency_print("write shram: 0x%X to address (0x%p)", (val), (addr)); \
    mtlk_writel((val), (addr)); \
    break; \
  }
#define __ccr_readl(addr, val) for(;;) { \
    (val) = mtlk_readl((addr)); \
    mtlk_osal_emergency_print("read shram: 0x%X from address (0x%p)", (val), (addr)); \
    break; \
  }
#else
#define __ccr_writel(val, addr) for(;;) { \
    mtlk_writel((val), (addr)); \
    break; \
  }
#define __ccr_readl(addr, val) for(;;) { \
    (val) = mtlk_readl((addr)); \
    break; \
  }
#endif

#define __ccr_setl(addr, flag) for(;;) { \
    uint32 val; \
    __ccr_readl((addr), val); \
    __ccr_writel(val | (flag), (addr)); \
    break; \
  }

#define __ccr_resetl(addr, flag) for(;;) { \
    uint32 val; \
    __ccr_readl((addr), val); \
    __ccr_writel((val) & ~(flag), (addr)); \
    break; \
  }

#define __ccr_issetl(addr, flag, res) for(;;) { \
    uint32 val; \
    __ccr_readl((addr), val); \
    (res) = ( (val & (flag)) == (flag) ); \
    break; \
  }

#define FW_UPPER_NAME_FMT             "%s_upper_%s.bin"
#define FW_UPPER_NAME_ARGS(pre, post) (pre), (post)

#define FW_AP_NAME_PREFFIX   "ap"
#define FW_STA_NAME_PREFFIX  "sta"

#define FW_LOWER_NAME                 "contr_lm.bin"

#ifdef MTCFG_BUS_PCI_PCIE

  #define SAFE_PLACE_TO_INCLUDE_MTLK_G3_CCR_DEFS
  #include "mtlk_g3_ccr_defs.h"

  #define SAFE_PLACE_TO_INCLUDE_MTLK_PCIG3_CCR_DEFS
  #include "mtlk_pcig3_ccr_defs.h"

  #undef LOG_LOCAL_GID
  #undef LOG_LOCAL_FID

  #define SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DEFS
  #include "mtlk_pcie_ccr_defs.h"

  #define LOG_LOCAL_GID   GID_MTLK_CCR_DEFS
  #define LOG_LOCAL_FID   0

#endif /* MTCFG_BUS_PCI_PCIE */

#ifdef MTCFG_BUS_AHB

  #define SAFE_PLACE_TO_INCLUDE_MTLK_G35_CCR_DEFS
  #include "mtlk_g35_ccr_defs.h"

#endif /* MTCFG_BUS_AHB */

static __INLINE int
_mtlk_sub_ccr_init(mtlk_ccr_t *ccr, mtlk_mmb_drv_t *mmb_drv, mtlk_hw_t *hw, void *sys_dev)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( return mtlk_mmb_pcig3_ccr_init(&ccr->mem.pcig3, mmb_drv, hw, sys_dev) );
    IF_CARD_PCIE  ( return mtlk_mmb_pcie_ccr_init(&ccr->mem.pcie, mmb_drv, hw, sys_dev)   );
    IF_CARD_AHBG35( return mtlk_mmb_g35_ccr_init(&ccr->mem.g35, mmb_drv, hw, sys_dev)     );
  CARD_SELECTOR_END();

  return MTLK_ERR_PARAMS;
}

static __INLINE void
_mtlk_sub_ccr_cleanup(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( mtlk_mmb_pcig3_ccr_cleanup(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( mtlk_mmb_pcie_ccr_cleanup(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( mtlk_mmb_g35_ccr_cleanup(&ccr->mem.g35)     );
  CARD_SELECTOR_END();
}

#ifdef MTCFG_USE_INTERRUPT_POLLING

#define _MTLK_CCR_INT_POLL_PERIOD_MS (100)

static uint32 __MTLK_IFUNC
_mtlk_ccr_poll_interrupt (mtlk_osal_timer_t *timer, 
                          mtlk_handle_t      clb_usr_data)
{
  mtlk_ccr_t *ccr = (mtlk_ccr_t *)clb_usr_data;

  MTLK_UNREFERENCED_PARAM(timer);

  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( _mtlk_pcig3_poll_interrupt(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_poll_interrupt(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( _mtlk_g35_poll_interrupt(&ccr->mem.g35)     );
  CARD_SELECTOR_END();

  return _MTLK_CCR_INT_POLL_PERIOD_MS;
}

#endif

MTLK_INIT_STEPS_LIST_BEGIN(ccr)
#ifdef MTCFG_USE_INTERRUPT_POLLING
  MTLK_INIT_STEPS_LIST_ENTRY(ccr, INT_POLL_TIMER)
#endif
  MTLK_INIT_STEPS_LIST_ENTRY(ccr, EEPROM_MUTEX)
  MTLK_INIT_STEPS_LIST_ENTRY(ccr, SUB_CCR)
  MTLK_INIT_STEPS_LIST_ENTRY(ccr, CHIP_ID)
MTLK_INIT_INNER_STEPS_BEGIN(ccr)
MTLK_INIT_STEPS_LIST_END(ccr);

static __INLINE void
mtlk_ccr_cleanup(mtlk_ccr_t *ccr)
{
  MTLK_CLEANUP_BEGIN(ccr, MTLK_OBJ_PTR(ccr))
    MTLK_CLEANUP_STEP(ccr, CHIP_ID, MTLK_OBJ_PTR(ccr),
                      MTLK_NOACTION, ())
    MTLK_CLEANUP_STEP(ccr, SUB_CCR, MTLK_OBJ_PTR(ccr),
                      _mtlk_sub_ccr_cleanup, (ccr))
    MTLK_CLEANUP_STEP(ccr, EEPROM_MUTEX, MTLK_OBJ_PTR(ccr),
                      mtlk_osal_mutex_cleanup, (&ccr->eeprom_mutex));
#ifdef MTCFG_USE_INTERRUPT_POLLING
    MTLK_CLEANUP_STEP(ccr, INT_POLL_TIMER, MTLK_OBJ_PTR(ccr),
                      mtlk_osal_timer_cleanup, (&ccr->poll_interrupts))
#endif
  MTLK_CLEANUP_END(ccr, MTLK_OBJ_PTR(ccr));
}

static __INLINE uint32
_mtlk_ccr_read_chip_id(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( return _mtlk_pcig3_ccr_read_chip_id(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( return _mtlk_pcie_ccr_read_chip_id(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( return _mtlk_g35_ccr_read_chip_id(&ccr->mem.g35)     );
  CARD_SELECTOR_END();
}

static __INLINE int
mtlk_ccr_init(mtlk_ccr_t *ccr, mtlk_card_type_t hw_type,
              mtlk_mmb_drv_t *mmb_drv, mtlk_hw_t *hw, void *sys_dev)
{
  MTLK_INIT_TRY(ccr, MTLK_OBJ_PTR(ccr))
    ccr->hw_type = hw_type;

#ifdef MTCFG_USE_INTERRUPT_POLLING
    MTLK_INIT_STEP(ccr, INT_POLL_TIMER, MTLK_OBJ_PTR(ccr),
                   mtlk_osal_timer_init, 
                   (&ccr->poll_interrupts, _mtlk_ccr_poll_interrupt, HANDLE_T(ccr)))
#endif
    MTLK_INIT_STEP(ccr, EEPROM_MUTEX, MTLK_OBJ_PTR(ccr),
                   mtlk_osal_mutex_init, (&ccr->eeprom_mutex));
    MTLK_INIT_STEP(ccr, SUB_CCR, MTLK_OBJ_PTR(ccr),
                   _mtlk_sub_ccr_init, (ccr, mmb_drv, hw, sys_dev))

    MTLK_INIT_STEP_EX(ccr, CHIP_ID, MTLK_OBJ_PTR(ccr),
                      mtlk_chip_info_get, (_mtlk_ccr_read_chip_id(ccr)),
                      ccr->chip_info, NULL != ccr->chip_info,
                      MTLK_ERR_NOT_SUPPORTED);
  MTLK_INIT_FINALLY(ccr, MTLK_OBJ_PTR(ccr))
  MTLK_INIT_RETURN(ccr, MTLK_OBJ_PTR(ccr), mtlk_ccr_cleanup, (ccr));
}

static __INLINE mtlk_mmb_drv_t*
mtlk_ccr_get_bus_drv(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( return _mtlk_pcig3_ccr_get_bus_drv(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( return _mtlk_pcie_ccr_get_bus_drv(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( return _mtlk_g35_ccr_get_bus_drv(&ccr->mem.g35)     );
  CARD_SELECTOR_END();
}

static __INLINE int
mtlk_ccr_get_fw_info(mtlk_ccr_t *ccr, uint8 cpu,
                     mtlk_work_mode_e mode, mtlk_fw_info_t *fw_info)
{
  MTLK_ASSERT(NULL != fw_info);
  MTLK_ASSERT(NULL != ccr->chip_info);

  memset(fw_info, 0, sizeof (*fw_info));

  /* prepare common information */
  if (CHI_CPU_NUM_UM == cpu)
  {
    snprintf(fw_info->fname, sizeof(fw_info->fname), FW_UPPER_NAME_FMT,
             FW_UPPER_NAME_ARGS(
               (MTLK_MODE_STA == mode) ? FW_STA_NAME_PREFFIX : FW_AP_NAME_PREFFIX,
               ccr->chip_info->name));
  }
  else
  {
    snprintf(fw_info->fname, sizeof(fw_info->fname), FW_LOWER_NAME);
  }

  /* prepare card dependent information */
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( return _mtlk_pcig3_ccr_get_fw_info(&ccr->mem.pcig3, cpu, fw_info)  );
    IF_CARD_PCIE  ( return _mtlk_pcie_ccr_get_fw_info(&ccr->mem.pcie, cpu, fw_info)    );
    IF_CARD_AHBG35( return _mtlk_g35_ccr_get_fw_info(&ccr->mem.g35, cpu, fw_info)      );
  CARD_SELECTOR_END();
}

#ifdef MTCFG_TSF_TIMER_ACCESS_ENABLED
static __INLINE void
  mtlk_ccr_read_hw_timestamp(mtlk_ccr_t *ccr, uint32 *low, uint32 *high)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( _mtlk_pcig3_read_hw_timestamp(&ccr->mem.pcig3, low, high) );
    IF_CARD_PCIE  ( _mtlk_pcie_read_hw_timestamp(&ccr->mem.pcie, low, high)   );
    IF_CARD_AHBG35( _mtlk_g35_read_hw_timestamp(&ccr->mem.g35, low, high)     );
  CARD_SELECTOR_END();
}
#endif /* MTCFG_TSF_TIMER_ACCESS_ENABLED */

static __INLINE int
mtlk_ccr_on_interrupt(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( return _mtlk_pcig3_on_interrupt(&ccr->mem.pcig3)  );
    IF_CARD_PCIE  ( return _mtlk_pcie_on_interrupt(&ccr->mem.pcie)    );
    IF_CARD_AHBG35( return _mtlk_g35_on_interrupt(&ccr->mem.g35)      );
  CARD_SELECTOR_END();
}

static __INLINE int
mtlk_ccr_postpone_irq_handler(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( return _mtlk_pcig3_postpone_irq_handler(&ccr->mem.pcig3)  );
    IF_CARD_PCIE  ( return _mtlk_pcie_postpone_irq_handler(&ccr->mem.pcie)    );
    IF_CARD_AHBG35( return _mtlk_g35_postpone_irq_handler(&ccr->mem.g35)      );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_enable_interrupts(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( _mtlk_pcig3_enable_interrupts(&ccr->mem.pcig3));
    IF_CARD_PCIE  ( _mtlk_pcie_enable_interrupts(&ccr->mem.pcie)  );
    IF_CARD_AHBG35( _mtlk_g35_enable_interrupts(&ccr->mem.g35)    );
  CARD_SELECTOR_END();

#ifdef MTCFG_USE_INTERRUPT_POLLING
  mtlk_osal_timer_set(&ccr->poll_interrupts, 
                      _MTLK_CCR_INT_POLL_PERIOD_MS);
#endif
}

static __INLINE void
mtlk_ccr_disable_interrupts(mtlk_ccr_t *ccr)
{
#ifdef MTCFG_USE_INTERRUPT_POLLING
  mtlk_osal_timer_cancel(&ccr->poll_interrupts);
#endif

  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( _mtlk_pcig3_disable_interrupts(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_disable_interrupts(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( _mtlk_g35_disable_interrupts(&ccr->mem.g35)     );
  CARD_SELECTOR_END();
}

static __INLINE BOOL
mtlk_ccr_clear_interrupts_if_pending(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( return _mtlk_pcig3_clear_interrupts_if_pending(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( return _mtlk_pcie_clear_interrupts_if_pending(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( return _mtlk_g35_clear_interrupts_if_pending(&ccr->mem.g35)     );
  CARD_SELECTOR_END();

  MTLK_ASSERT(!"Should never be here");
  return 0;
}

/* Clear interrupts unconditionally (is not affected for the AHB) */
#ifdef MTLK_CCR_CLEAR_INT_UNCONDIT

static __INLINE BOOL
mtlk_ccr_clear_interrupts(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( return _mtlk_pcig3_clear_interrupts(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( return _mtlk_pcie_clear_interrupts(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( return _mtlk_g35_clear_interrupts_if_pending(&ccr->mem.g35));

  CARD_SELECTOR_END();

  MTLK_ASSERT(!"Should never be here");
  return 0;
}
#endif /* MTLK_CCR_CLEAR_INT_UNCONDIT */

static __INLINE BOOL
mtlk_ccr_disable_interrupts_if_pending(mtlk_ccr_t *ccr)
{
  BOOL res = FALSE;

  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( res = _mtlk_pcig3_disable_interrupts_if_pending(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( res = _mtlk_pcie_disable_interrupts_if_pending(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( res = _mtlk_g35_disable_interrupts_if_pending(&ccr->mem.g35)     );
  CARD_SELECTOR_END();

#ifdef MTCFG_USE_INTERRUPT_POLLING
  if(res)
    mtlk_osal_timer_cancel(&ccr->poll_interrupts);
#endif

  return res;
}

static __INLINE void
mtlk_ccr_initiate_doorbell_inerrupt(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( _mtlk_pcig3_initiate_doorbell_inerrupt(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_initiate_doorbell_inerrupt(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( _mtlk_g35_initiate_doorbell_inerrupt(&ccr->mem.g35)     );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_release_ctl_from_reset(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( _mtlk_pcig3_release_ctl_from_reset(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_release_ctl_from_reset(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( _mtlk_g35_release_ctl_from_reset(&ccr->mem.g35)     );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_put_ctl_to_reset(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( _mtlk_g3_put_ctl_to_reset(ccr->mem.pcig3.pas) );
    IF_CARD_PCIE  ( _mtlk_g3_put_ctl_to_reset(ccr->mem.pcie.pas)  );
    IF_CARD_AHBG35( _mtlk_g35_put_ctl_to_reset(&ccr->mem.g35)     );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_boot_from_bus(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( _mtlk_pcig3_boot_from_bus(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_boot_from_bus(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( _mtlk_g35_boot_from_bus(&ccr->mem.g35)     );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_clear_boot_from_bus(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( _mtlk_pcig3_clear_boot_from_bus(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_clear_boot_from_bus(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( _mtlk_g35_clear_boot_from_bus(&ccr->mem.g35)     );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_switch_to_iram_boot(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( _mtlk_pcig3_switch_to_iram_boot(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_switch_to_iram_boot(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( _mtlk_g35_switch_to_iram_boot(&ccr->mem.g35)     );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_exit_debug_mode(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( _mtlk_pcig3_exit_debug_mode(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_exit_debug_mode(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( _mtlk_g35_exit_debug_mode(&ccr->mem.g35)     );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_put_cpus_to_reset(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( _mtlk_pcig3_put_cpus_to_reset(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_put_cpus_to_reset(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( _mtlk_g35_put_cpus_to_reset(&ccr->mem.g35)     );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_power_on_cpus(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( _mtlk_pcig3_power_on_cpus(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_power_on_cpus(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( _mtlk_g35_power_on_cpus(&ccr->mem.g35)     );
  CARD_SELECTOR_END();
}

static __INLINE void
mtlk_ccr_release_cpus_reset(mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( _mtlk_pcig3_release_cpus_reset(&ccr->mem.pcig3) );
    IF_CARD_PCIE  ( _mtlk_pcie_release_cpus_reset(&ccr->mem.pcie)   );
    IF_CARD_AHBG35( _mtlk_g35_release_cpus_reset(&ccr->mem.g35)     );
  CARD_SELECTOR_END();
}

static __INLINE BOOL
mtlk_ccr_check_bist(mtlk_ccr_t *ccr, uint32 *bist_result)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( return _mtlk_pcig3_check_bist(&ccr->mem.pcig3, bist_result) );
    IF_CARD_PCIE  ( return _mtlk_pcie_check_bist(&ccr->mem.pcie, bist_result)   );
    IF_CARD_AHBG35( return _mtlk_g35_check_bist(&ccr->mem.g35, bist_result)     );
  CARD_SELECTOR_END();

  MTLK_ASSERT(!"Should never be here");
  return MTLK_ERR_PARAMS;
}

static __INLINE void
mtlk_ccr_eeprom_protect (mtlk_ccr_t *ccr)
{
  mtlk_osal_mutex_acquire(&ccr->eeprom_mutex);
}

static __INLINE void
mtlk_ccr_eeprom_release (mtlk_ccr_t *ccr)
{
  mtlk_osal_mutex_release(&ccr->eeprom_mutex);
}

static __INLINE int
mtlk_ccr_eeprom_override_write (mtlk_ccr_t *ccr, uint32 data, uint32 mask)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( return _mtlk_pcig3_eeprom_override_write(&ccr->mem.pcig3, data, mask) );
    IF_CARD_PCIE  ( return _mtlk_pcie_eeprom_override_write(&ccr->mem.pcie, data, mask)   );
    IF_CARD_AHBG35( return _mtlk_g35_eeprom_override_write(&ccr->mem.g35, data, mask)     );
  CARD_SELECTOR_END();
}

static __INLINE uint32
mtlk_ccr_eeprom_override_read (mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( return _mtlk_pcig3_eeprom_override_read(&ccr->mem.pcig3)  );
    IF_CARD_PCIE  ( return _mtlk_pcie_eeprom_override_read(&ccr->mem.pcie)    );
    IF_CARD_AHBG35( return _mtlk_g35_eeprom_override_read(&ccr->mem.g35)      );
  CARD_SELECTOR_END();
}

static __INLINE uint32
mtlk_ccr_get_mips_freq (mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( return _mtlk_pcig3_ccr_get_mips_freq(&ccr->mem.pcig3)  );
    IF_CARD_PCIE  ( return _mtlk_pcie_ccr_get_mips_freq(&ccr->mem.pcie)    );
    IF_CARD_AHBG35( return _mtlk_g35_ccr_get_mips_freq(&ccr->mem.g35)      );
  CARD_SELECTOR_END();
}

static __INLINE uint32
mtlk_ccr_get_afe_value (mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( return 0 );
    IF_CARD_PCIE  ( return 0 );
    IF_CARD_AHBG35( return _mtlk_g35_ccr_get_afe_value());
  CARD_SELECTOR_END();
}

static __INLINE uint32
mtlk_ccr_get_progmodel_version (mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( return _mtlk_pcig3_ccr_get_progmodel_version(&ccr->mem.pcig3)  );
    IF_CARD_PCIE  ( return _mtlk_pcie_ccr_get_progmodel_version(&ccr->mem.pcie)    );
    IF_CARD_AHBG35( return _mtlk_g35_ccr_get_progmodel_version(&ccr->mem.g35)      );
  CARD_SELECTOR_END();
}

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

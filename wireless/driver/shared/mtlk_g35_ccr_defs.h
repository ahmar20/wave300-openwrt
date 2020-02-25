/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/* $Id$ */

#if !defined(SAFE_PLACE_TO_INCLUDE_MTLK_G35_CCR_DEFS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_G35_CCR_DEFS */
#undef SAFE_PLACE_TO_INCLUDE_MTLK_G35_CCR_DEFS

#define G35_MIPS_FREQ_DEFAULT  250

void __MTLK_IFUNC
mtlk_mmb_g35_ccr_cleanup(_mtlk_g35_ccr_t *g35_mem);

int __MTLK_IFUNC
mtlk_mmb_g35_ccr_init(
  _mtlk_g35_ccr_t *g35_mem, 
  mtlk_mmb_drv_t *obj, 
  mtlk_hw_t *hw,
  void *sys_dev);

static __INLINE uint32
_mtlk_g35_ccr_read_chip_id(_mtlk_g35_ccr_t *g35_mem)
{
  uint32 system_info;

  __ccr_readl(&g35_mem->pas->CPU_SYS_IF.system_info, system_info);

  system_info &= G33_CPU_SYS_IF_SYSTEM_INFO_CHIPID_MASK;
  system_info >>= G33_CPU_SYS_IF_SYSTEM_INFO_CHIPID_OFFSET;

  return system_info;
}

static __INLINE mtlk_mmb_drv_t*
_mtlk_g35_ccr_get_bus_drv(_mtlk_g35_ccr_t *g35_mem)
{
  return g35_mem->mmb_drv;
}

static __INLINE int
_mtlk_g35_ccr_get_fw_info(_mtlk_g35_ccr_t *g35_mem, uint8 cpu,
                          mtlk_fw_info_t *fw_info)
{
  MTLK_ASSERT(ARRAY_SIZE(fw_info->mem) >= 3);

  switch (cpu)
  {
    case CHI_CPU_NUM_UM:
      /* initialize memory chunks for upper-mac CPU */
      fw_info->mem[0].start   = ((unsigned char*)g35_mem->pas) + MTLK_G35_CPU_IRAM_START;
      fw_info->mem[0].length  = MTLK_G35_CPU_IRAM_SIZE;
      fw_info->mem[0].swap    = FALSE;
      fw_info->mem[1].start   = ((unsigned char*)g35_mem->cpu_ddr) + MTLK_G35_CPU_DDR_OFFSET;
      fw_info->mem[1].length  = MTLK_G35_CPU_DDR_SIZE;
#if defined(MTCFG_BUS_AHB_BYTESWAP)
      fw_info->mem[1].swap    = TRUE;
#else
      fw_info->mem[1].swap    = FALSE;
#endif
      fw_info->mem[2].start   = 0;
      fw_info->mem[2].length  = 0;
      fw_info->mem[2].swap    = FALSE;
      break;
    default:
      return MTLK_ERR_NOT_SUPPORTED;
  }

  return MTLK_ERR_OK;
}

#ifdef MTCFG_TSF_TIMER_ACCESS_ENABLED
static __INLINE void
_mtlk_g35_read_hw_timestamp(_mtlk_g35_ccr_t *g35_mem, uint32 *low, uint32 *high)
{
  MTLK_ASSERT(NULL != g35_mem);

  __ccr_readl(&g35_mem->pas->tsf_timer_low, *low);
  __ccr_readl(&g35_mem->pas->tsf_timer_high, *high);
}
#endif /* MTCFG_TSF_TIMER_ACCESS_ENABLED */

static __INLINE int
_mtlk_g35_on_interrupt(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_UNREFERENCED_PARAM(g35_mem);

  return MTLK_ERR_OK;
}

static __INLINE int
_mtlk_g35_postpone_irq_handler(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);
  return mtlk_mmb_drv_postpone_irq_handler(g35_mem->mmb_drv);
}

static __INLINE  void
_mtlk_g35_enable_interrupts(_mtlk_g35_ccr_t *g35_mem)
{
#define MTLK_GEN35_ENABLE_INTERRUPT     (0x1)

  uint32 reg;

  MTLK_ASSERT(NULL != g35_mem);

  __ccr_writel(MTLK_GEN35_ENABLE_INTERRUPT,
               &g35_mem->pas->HTEXT.host_irq_mask);

  __ccr_readl(&g35_mem->pas->RAB.enable_phi_interrupt, reg);
  __ccr_writel(reg | MTLK_GEN35_ENABLE_INTERRUPT,
               &g35_mem->pas->RAB.enable_phi_interrupt);
}

static __INLINE  void
_mtlk_g35_disable_interrupts(_mtlk_g35_ccr_t *g35_mem)
{
#define MTLK_GEN35_DISABLE_INTERRUPT     (0x0)

  MTLK_ASSERT(NULL != g35_mem);

  __ccr_writel(MTLK_GEN35_DISABLE_INTERRUPT,
               &g35_mem->pas->HTEXT.host_irq_mask);
}

/* WARNING                                                      */
/* Currently we do not utilize (and have no plans to utilize)   */ 
/* interrupt sharing on Gen 3.5 platform. However, Gen 3.5      */
/* hardware supports this ability.                              */
/* For now, in all *_if_pending functions we assume there is no */
/* interrupt sharing, so we may not check whether arrived       */
/* interrupt is our. This save us CPU cycles and code lines.    */
/* In case interrupt sharing will be used, additional checks    */
/* for interrupt appurtenance to be added into these functions. */

static __INLINE BOOL
_mtlk_g35_clear_interrupts_if_pending(_mtlk_g35_ccr_t *g35_mem)
{
#define MTLK_GEN35_CLEAR_INTERRUPT       (0x1)

  MTLK_ASSERT(NULL != g35_mem);

  __ccr_writel(MTLK_GEN35_CLEAR_INTERRUPT, 
               &g35_mem->pas->RAB.phi_interrupt_clear);
  return TRUE;
}

static __INLINE BOOL
_mtlk_g35_disable_interrupts_if_pending(_mtlk_g35_ccr_t *g35_mem)
{
  _mtlk_g35_disable_interrupts(g35_mem);
  return TRUE;
}

#ifdef MTCFG_USE_INTERRUPT_POLLING

static __INLINE void
_mtlk_g35_poll_interrupt(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);

  (void)mtlk_hw_mmb_interrupt_handler(g35_mem->hw);
}

#endif /* MTCFG_USE_INTERRUPT_POLLING */

static __INLINE  void
_mtlk_g35_initiate_doorbell_inerrupt(_mtlk_g35_ccr_t *g35_mem)
{
#define MTLK_GEN35_GENERATE_DOOR_BELL    (0x1)

  MTLK_ASSERT(NULL != g35_mem);
  __ccr_writel(MTLK_GEN35_GENERATE_DOOR_BELL, 
               &g35_mem->pas->RAB.upi_interrupt);
}

static __INLINE  void
_mtlk_g35_boot_from_bus(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);
  /* No boot from bus on G35 */
  MTLK_UNREFERENCED_PARAM(g35_mem);
}

static __INLINE  void
_mtlk_g35_clear_boot_from_bus(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);
  /* No boot from bus on G35 */
  MTLK_UNREFERENCED_PARAM(g35_mem);
}

static __INLINE  void
_mtlk_g35_switch_to_iram_boot(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);

  g35_mem->next_boot_mode = G3_CPU_RAB_IRAM;
}

static __INLINE  void
_mtlk_g35_exit_debug_mode(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);
  /* Not needed on G35 */
  MTLK_UNREFERENCED_PARAM(g35_mem);
}

static __INLINE void
__g35_open_secure_write_register(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);

  __ccr_writel(0xAAAA,
               &g35_mem->pas->RAB.secure_write_register);
  __ccr_writel(0x5555,
               &g35_mem->pas->RAB.secure_write_register);
}

static __INLINE  void
_mtlk_g35_put_cpus_to_reset(_mtlk_g35_ccr_t *g35_mem)
{
  uint8 new_cpu_boot_mode = G3_CPU_RAB_Override |
                            G3_CPU_RAB_IRAM;

  MTLK_ASSERT(NULL != g35_mem);

  /* Dynamic clocks of BBCPU should be disabled before put it to reset.
     In other case reset can be not done. The dynamics clocks are set by
     FW. This is work around for hw issue for wave300 and AR10.
  */
  __ccr_writel(0, &g35_mem->pas->CLC.dynamic);


  g35_mem->current_ucpu_state =
    G3_CPU_RAB_IRAM;

  __g35_open_secure_write_register(g35_mem);
  __ccr_writel(new_cpu_boot_mode,
               &g35_mem->pas->RAB.cpu_control_register);

  /* CPU requires time to go to  reset, so we       */
  /* MUST wait here before writing something else   */
  /* to CPU control register. In other case this    */
  /* may lead to unpredictable results.             */
  mtlk_osal_msleep(20);
}

static __INLINE  void
_mtlk_g35_put_ctl_to_reset(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);

  /* Disable RX */
  __ccr_resetl(&g35_mem->pas->PAC.rx_control, 
               G3_MASK_RX_ENABLED);
}

static __INLINE  void
_mtlk_g35_power_on_cpus(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);
  /* Not needed on G35 */
  MTLK_UNREFERENCED_PARAM(g35_mem);
}

static __INLINE  void
_mtlk_g35_release_cpus_reset(_mtlk_g35_ccr_t *g35_mem)
{
  uint32 new_cpu_state;

  MTLK_ASSERT(NULL != g35_mem);
  MTLK_ASSERT(G3_CPU_RAB_IRAM == g35_mem->next_boot_mode);
  /************************************************************************/
  /* The BBCPU after first startup always running in debug mode and to be */
  /* sure that release from reset will take affect we need to put it to   */
  /* reset at first. Issue with CHI_MAGIC value=0x00000000.               */
  _mtlk_g35_put_cpus_to_reset(g35_mem);
  /************************************************************************/

  g35_mem->current_ucpu_state = 
    G3_CPU_RAB_Active | g35_mem->next_boot_mode;

  new_cpu_state = ( g35_mem->current_ucpu_state | G3_CPU_RAB_Override );

  __g35_open_secure_write_register(g35_mem);

  __ccr_writel(new_cpu_state,
               &g35_mem->pas->RAB.cpu_control_register);

  /* CPU requires time to change its state, so we   */
  /* MUST wait here before writing something else   */
  /* to CPU control register. In other case this    */
  /* may lead to unpredictable results.             */
  mtlk_osal_msleep(10);
}

static __INLINE BOOL
_mtlk_g35_check_bist(_mtlk_g35_ccr_t *g35_mem, uint32 *bist_result)
{
  MTLK_ASSERT(NULL != g35_mem);
  /* Not needed on G35 */
  MTLK_UNREFERENCED_PARAM(g35_mem);

  *bist_result = 0;
  return TRUE;
}

#ifndef MTLK_G35_NPU
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,5,0)
#include <asm/ifx/ifx_regs.h>
#else
#include <asm/mach-ltqcpe/ltq_regs.h>
#endif
#else
#include <linux/interrupt.h>
#define INT_NUM_IM0_IRL18 (INT_NUM_IM0_IRL0 + 18)
#endif
#endif

static __INLINE  void
_mtlk_g35_release_ctl_from_reset(_mtlk_g35_ccr_t *g35_mem)
{
  extern int bb_cpu_ddr_mb_number;
#ifdef MTLK_G35_NPU
  MTLK_ASSERT(NULL != g35_mem);
  __ccr_writel(bb_cpu_ddr_mb_number,
               &g35_mem->pas->SH_REG_BLOCK.bb_ddr_offset_mb);
#else
#define BBCPU_PAGE_REG_PAD  MTLK_BFIELD_INFO(0, 4)
#define BBCPU_PAGE_REG_VAL  MTLK_BFIELD_INFO(4, 12)
#define BBCPU_MASK_REG_PAD  MTLK_BFIELD_INFO(16, 4)
#define BBCPU_MASK_REG_VAL  MTLK_BFIELD_INFO(20, 12)

#define BBCPU_MASK -1

  uint32 bbcpu_reg_val = 
    MTLK_BFIELD_VALUE(BBCPU_PAGE_REG_VAL, bb_cpu_ddr_mb_number, uint32) |
    MTLK_BFIELD_VALUE(BBCPU_MASK_REG_VAL, BBCPU_MASK, uint32);

  mtlk_osal_emergency_print("BBCPU Reg: offs=0x%04x (%p) val=0x%08x (offset_mb=%d)",
      MTLK_OFFSET_OF(struct g35_pas_map, HTEXT.ahb_arb_bbcpu_page_reg) - MTLK_OFFSET_OF(struct g35_pas_map, HTEXT),
      &g35_mem->pas->HTEXT.ahb_arb_bbcpu_page_reg,
      bbcpu_reg_val, bb_cpu_ddr_mb_number);

  __ccr_writel(bbcpu_reg_val,
               &g35_mem->pas->HTEXT.ahb_arb_bbcpu_page_reg);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
  mtlk_osal_emergency_print("Enabling interrupt (%p)", IFX_ICU_IM0_IER);
  (*IFX_ICU_IM0_IER) |= 0x00040000;
#else
  mtlk_osal_emergency_print("Enabling interrupt (%p)", (void*) INT_NUM_IM0_IRL18);
// Interrupt for new kernel will be enabled while request_irq? (see mtlk_mmb_drv.c)
#endif

  mtlk_osal_emergency_print("Interrupt enabled");
#endif

}

static __INLINE int
_mtlk_g35_eeprom_override_write (_mtlk_g35_ccr_t *g35_mem, uint32 data, uint32 mask)
{
  MTLK_ASSERT(NULL != g35_mem);
  MTLK_UNREFERENCED_PARAM(g35_mem);

  return MTLK_ERR_NOT_SUPPORTED;
}

static __INLINE uint32
_mtlk_g35_eeprom_override_read(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);
  MTLK_UNREFERENCED_PARAM(g35_mem);

  /* MTLK_ERR_NOT_SUPPORTED */
  return (uint32)(-1);
}

static __INLINE uint32
_mtlk_g35_ccr_get_mips_freq(_mtlk_g35_ccr_t *g35_mem)
{
  uint32 freq;
  
  freq = mtlk_mmb_drv_get_cpu_freq(g35_mem->mmb_drv);
  if (freq == CPU_FREQ_RESERVED)
  {
    freq = G35_MIPS_FREQ_DEFAULT;   /* Return default value if not specified */
  }

  return freq;
}

static __INLINE void
_mtlk_g35_ccr_iram_cache_fix_vec (_mtlk_g35_ccr_t *g35_mem, uint32 fix_vec_sel, uint32 fix_vec)
{
  __ccr_writel(fix_vec_sel, &g35_mem->pas->CPU_SYS_IF.bist_red_fix_select);
  __ccr_writel(fix_vec, &g35_mem->pas->CPU_SYS_IF.red_fix_load_value);
}

static __INLINE void
_mtlk_g35_ccr_shram_fix_vec (_mtlk_g35_ccr_t *g35_mem, uint32 fix_vec_sel, uint32 fix_vec)
{
  __ccr_writel(fix_vec_sel, &g35_mem->pas->HTEXT.shram_fix_vector_sel);
  __ccr_writel(fix_vec, &g35_mem->pas->HTEXT.red_fix_load_value);
}

static __INLINE uint32
_mtlk_g35_ccr_get_afe_value (void)
{
  uint32 afe_value;

  /* Read value from register */
  afe_value = *(uint32 *) MTLK_G35_MPS_AFE_CTRL;

  /* Check WLAN AFE Trimming Indication */
  if (0 == (MTLK_G35_MPS_AFE_CTRL_WLAN_TRIM & afe_value)) {
      afe_value = 0; /* Not done */
  }

  return afe_value;
}

static __INLINE uint32
_mtlk_g35_ccr_get_progmodel_version(_mtlk_g35_ccr_t *g35_mem)
{
  uint32 val;

  MTLK_ASSERT(NULL != g35_mem);

  __ccr_readl(&g35_mem->pas->TD.phy_progmodel_ver, val);

  return val;
}


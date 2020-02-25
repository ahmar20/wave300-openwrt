/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/* $Id$ */

#if !defined(SAFE_PLACE_TO_INCLUDE_MTLK_PCIG3_CCR_DEFS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_PCIG3_CCR_DEFS */
#undef SAFE_PLACE_TO_INCLUDE_MTLK_PCIG3_CCR_DEFS

#define PCIG3_MIPS_FREQ_DEFAULT  240

void __MTLK_IFUNC 
mtlk_mmb_pcig3_ccr_cleanup(_mtlk_pcig3_ccr_t *pcig3_mem);

int __MTLK_IFUNC
mtlk_mmb_pcig3_ccr_init(
  _mtlk_pcig3_ccr_t *pcig3_mem, 
  mtlk_mmb_drv_t *obj, 
  mtlk_hw_t *hw,
  void *sys_dev);

static __INLINE uint32
_mtlk_pcig3_ccr_read_chip_id(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  uint32 system_info;

  __ccr_readl(&pcig3_mem->pas->UPPER_SYS_IF.system_info, system_info);

  system_info &= G3_UPPER_SYS_IF_SYSTEM_INFO_CHIPID_MASK;
  system_info >>= G3_UPPER_SYS_IF_SYSTEM_INFO_CHIPID_OFFSET;

  return system_info;
}

static __INLINE mtlk_mmb_drv_t*
_mtlk_pcig3_ccr_get_bus_drv(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  return pcig3_mem->mmb_drv;
}

static __INLINE int
_mtlk_pcig3_ccr_get_fw_info(_mtlk_pcig3_ccr_t *pcig3_mem, uint8 cpu,
                            mtlk_fw_info_t *fw_info)
{
  MTLK_ASSERT(ARRAY_SIZE(fw_info->mem) >= 3);

  switch (cpu)
  {
    case CHI_CPU_NUM_UM:
      /* initialize memory chunks for upper-mac CPU */
      fw_info->mem[0].start   = ((unsigned char*)pcig3_mem->pas) + UCPU_INTERNAL_MEMORY_START;
      fw_info->mem[0].length  = UCPU_INTERNAL_MEMORY_SIZE;
      fw_info->mem[0].swap    = FALSE;
      fw_info->mem[1].start   = ((unsigned char*)pcig3_mem->pas) + UCPU_EXTERNAL_MEMORY_START;
      fw_info->mem[1].length  = UCPU_EXTERNAL_MEMORY_SIZE;
      fw_info->mem[1].swap    = FALSE;
      fw_info->mem[2].start   = 0;
      fw_info->mem[2].length  = 0;
      fw_info->mem[2].swap    = FALSE;
      break;
    case CHI_CPU_NUM_LM:
      /* initialize memory chunks for lower-mac CPU */
      fw_info->mem[0].start   = ((unsigned char*)pcig3_mem->pas) + LCPU_INTERNAL_MEMORY_START;
      fw_info->mem[0].length  = LCPU_INTERNAL_MEMORY_SIZE;
      fw_info->mem[0].swap    = FALSE;
      fw_info->mem[1].start   = ((unsigned char*)pcig3_mem->pas) + LCPU_EXTERNAL_MEMORY_START;
      fw_info->mem[1].length  = LCPU_EXTERNAL_MEMORY_SIZE;
      fw_info->mem[1].swap    = FALSE;
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
  _mtlk_pcig3_read_hw_timestamp(_mtlk_pcig3_ccr_t *pcig3_mem, uint32 *low, uint32 *high)
{
  MTLK_ASSERT(NULL != pcig3_mem);

  __ccr_readl(&pcig3_mem->pas->tsf_timer_low, *low);
  __ccr_readl(&pcig3_mem->pas->tsf_timer_high, *high);
}
#endif /* MTCFG_TSF_TIMER_ACCESS_ENABLED */

static __INLINE int
_mtlk_pcig3_on_interrupt(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  MTLK_UNREFERENCED_PARAM(pcig3_mem);

  return MTLK_ERR_OK;
}

static __INLINE int
_mtlk_pcig3_postpone_irq_handler(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  MTLK_ASSERT(NULL != pcig3_mem);
  return mtlk_mmb_drv_postpone_irq_handler(pcig3_mem->mmb_drv);
}

static __INLINE  void
_mtlk_pcig3_enable_interrupts(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  MTLK_ASSERT(NULL != pcig3_mem);
  __ccr_writel((HWI_MASK_local_hrt_to_host << HWI_OFFSET_local_hrt_to_host) |
               (HWI_MASK_global << HWI_OFFSET_global),
               &pcig3_mem->hrc->HWI_ADDR_host_interrupt_enable);
}

static __INLINE  void
_mtlk_pcig3_disable_interrupts(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  MTLK_ASSERT(NULL != pcig3_mem);
  __ccr_writel(0,&pcig3_mem->hrc->HWI_ADDR_host_interrupt_enable);
}

static __INLINE BOOL
_mtlk_pcig3_clear_interrupts_if_pending(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  uint32 v;
  MTLK_ASSERT(NULL != pcig3_mem);
  
  __ccr_readl(&pcig3_mem->hrc->HWI_ADDR_host_interrupt_active, v);
  v &= HWI_MASK_local_hrt_to_host << HWI_OFFSET_local_hrt_to_host;
  __ccr_writel(v, &pcig3_mem->hrc->HWI_ADDR_host_interrupt_status);

  return v != 0;
}

/* Clear interrupts unconditionally */
#ifdef MTLK_CCR_CLEAR_INT_UNCONDIT

static __INLINE BOOL
_mtlk_pcig3_clear_interrupts(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  uint32 v;
  MTLK_ASSERT(NULL != pcig3_mem);

  v = HWI_MASK_local_hrt_to_host << HWI_OFFSET_local_hrt_to_host;
  __ccr_writel(v, &pcig3_mem->hrc->HWI_ADDR_host_interrupt_status);

  return v != 0;
}
#endif /* MTLK_CCR_CLEAR_INT_UNCONDIT */

static __INLINE BOOL
_mtlk_pcig3_disable_interrupts_if_pending(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  uint32 v;
  MTLK_ASSERT(NULL != pcig3_mem);


  __ccr_readl(&pcig3_mem->hrc->HWI_ADDR_host_interrupt_active, v);
  if ((v & (HWI_MASK_global << HWI_OFFSET_global)) == 0)
    return FALSE;

  __ccr_writel(v & ~(HWI_MASK_global << HWI_OFFSET_global), 
               &pcig3_mem->hrc->HWI_ADDR_host_interrupt_enable);
  return TRUE;
}

#ifdef MTCFG_USE_INTERRUPT_POLLING

static __INLINE void 
_mtlk_pcig3_poll_interrupt(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  MTLK_ASSERT(NULL != pcig3_mem);

  (void)mtlk_hw_mmb_interrupt_handler(pcig3_mem->hw);
}

#endif /* MTCFG_USE_INTERRUPT_POLLING */

static __INLINE  void
_mtlk_pcig3_initiate_doorbell_inerrupt(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  MTLK_ASSERT(NULL != pcig3_mem);
  __ccr_writel(1, &pcig3_mem->hrc->HWI_ADDR_host_to_local_doorbell_interrupt);
}

static __INLINE  void
_mtlk_pcig3_boot_from_bus(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  MTLK_ASSERT(NULL != pcig3_mem);

  __ccr_setl(&pcig3_mem->hrc->HWI_ADDR_general_purpose_control,
             HWI_MASK_cpu_control_pci_cpu_mode);
}

static __INLINE  void
_mtlk_pcig3_clear_boot_from_bus(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  MTLK_ASSERT(NULL != pcig3_mem);

  __ccr_resetl(&pcig3_mem->hrc->HWI_ADDR_general_purpose_control,
               HWI_MASK_cpu_control_pci_cpu_mode);
}

static __INLINE  void
_mtlk_pcig3_switch_to_iram_boot(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  MTLK_ASSERT(NULL != pcig3_mem);

  /* According to spec this operation requires system to be in PCI boot mode */
#ifdef MTLK_DEBUG
  {
    int res;
    __ccr_issetl(&pcig3_mem->hrc->HWI_ADDR_general_purpose_control,
                 HWI_MASK_cpu_control_pci_cpu_mode, res);
    MTLK_ASSERT(res);

  }
#endif

  __ccr_writel(0, &pcig3_mem->hrc->HWI_ADDR_cpu_control);
}

static __INLINE  void
_mtlk_pcig3_exit_debug_mode(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  MTLK_ASSERT(NULL != pcig3_mem);

  /* According to spec this operation requires system to be in PCI boot mode */
#ifdef MTLK_DEBUG
  {
    int res;
    __ccr_issetl(&pcig3_mem->hrc->HWI_ADDR_general_purpose_control,
                 HWI_MASK_cpu_control_pci_cpu_mode, res);
    MTLK_ASSERT(res);

  }
#endif

  __ccr_writel(0, &pcig3_mem->hrc->HWI_ADDR_cpu_control);
}

/*
* HWI_ADDR_cpu_control:
* Bit [0] = CPU power on reset (default = 0x1). If boot mode = PCI, this
* bit is used to override the boot mode and change it to ?Boot from IRAM?.
* Bit [1] = Upper CPU reset (default = 0x0). When this bit is set, and
* boot mode is PCI (regardless of the state of bit[0] in this register),
* It will release the Upper CPU reset. Default value = 0x0 = CPU in reset.
* Bit [2] = Lower CPU reset (default = 0x0). Operation mode of this bit is
* the same as the previous one, with one change ? it controls the Lower
* CPU.
*/

static __INLINE  void
_mtlk_pcig3_put_cpus_to_reset(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  MTLK_ASSERT(NULL != pcig3_mem);

  /* Dynamic clocks of BBCPU should be disabled before put it to reset.
     In other case reset can be not done. The dynamics clocks are set by
     FW. This is work around for hw issue for wave300 and AR10.
  */
  __ccr_writel(0, &pcig3_mem->pas->CLC.dynamic);

  __ccr_writel(0, &pcig3_mem->hrc->HWI_ADDR_cpu_control);

  /* CPU requires time to go to  reset, so we       */
  /* MUST wait here before writing something else   */
  /* to CPU control register. In other case this    */
  /* may lead to unpredictable results.             */
  mtlk_osal_msleep(20);
}

static __INLINE  void
_mtlk_pcig3_power_on_cpus(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  MTLK_ASSERT(NULL != pcig3_mem);

  __ccr_writel(HWI_MASK_cpu_power_on_reset << HWI_OFFSET_cpu_power_on_reset,
               &pcig3_mem->hrc->HWI_ADDR_cpu_control);
}

static __INLINE  void
_mtlk_pcig3_release_cpus_reset(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  MTLK_ASSERT(NULL != pcig3_mem);

  __mtlk_g3_set_ucpu_32k_blocks(pcig3_mem->pas);

  /* CPU requires time to exit from reset, so we   */
  /* MUST wait after CPU control register changed. */
  /* In other case this may lead to unpredictable  */
  /* results.                                      */

  __ccr_setl(&pcig3_mem->hrc->HWI_ADDR_cpu_control, 
             HWI_MASK_upper_cpu_reset << HWI_OFFSET_upper_cpu_reset);
  mtlk_osal_msleep(10);

  __ccr_setl(&pcig3_mem->hrc->HWI_ADDR_cpu_control, 
            HWI_MASK_lower_cpu_reset << HWI_OFFSET_lower_cpu_reset);
  mtlk_osal_msleep(10);
}

static __INLINE BOOL
_mtlk_pcig3_check_bist(_mtlk_pcig3_ccr_t *pcig3_mem, uint32 *bist_result)
{
  MTLK_ASSERT(NULL != pcig3_mem);

  __ccr_writel(G3_VAL_START_BIST, &pcig3_mem->pas->HTEXT.start_bist);
  mtlk_osal_msleep(20);

  __ccr_readl(&pcig3_mem->hrc->HWI_ADDR_general_purpose_status, *bist_result);

  return ((*bist_result & G3_BIST_SUCCESS_MASK) == G3_BIST_SUCCESS_MASK);
}

static __INLINE void
_mtlk_pcig3_release_ctl_from_reset(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  MTLK_ASSERT(NULL != pcig3_mem);

  __mtlk_g3_release_ctl_from_reset_phase1(pcig3_mem->pas);
  __mtlk_g3_release_ctl_from_reset_phase2(pcig3_mem->pas);
}

static __INLINE int
_mtlk_pcig3_eeprom_override_write (_mtlk_pcig3_ccr_t *pcig3_mem, uint32 data, uint32 mask)
{
  uint32 val;

  MTLK_ASSERT(NULL != pcig3_mem);

  __ccr_readl(&pcig3_mem->hrc->HWI_ADDR_eeprom_override, val);

  val = (val & (~mask))| data;

  __ccr_writel(val, &pcig3_mem->hrc->HWI_ADDR_eeprom_override);

  return MTLK_ERR_OK;
}

static __INLINE uint32
_mtlk_pcig3_eeprom_override_read (_mtlk_pcig3_ccr_t *pcig3_mem)
{
  uint32 val;

  MTLK_ASSERT(NULL != pcig3_mem);

  __ccr_readl(&pcig3_mem->hrc->HWI_ADDR_eeprom_override, val);

  return val;
}

static __INLINE uint32
_mtlk_pcig3_ccr_get_mips_freq(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  return PCIG3_MIPS_FREQ_DEFAULT;
}

static __INLINE uint32
_mtlk_pcig3_ccr_get_progmodel_version(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  uint32 val;

  MTLK_ASSERT(NULL != pcig3_mem);

  __ccr_readl(&pcig3_mem->pas->TD.phy_progmodel_ver, val);

  return val;
}


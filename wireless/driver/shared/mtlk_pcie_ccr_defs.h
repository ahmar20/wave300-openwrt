/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/* $Id$ */

#if !defined(SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DEFS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DEFS */
#undef SAFE_PLACE_TO_INCLUDE_MTLK_PCIE_CCR_DEFS

#define LOG_LOCAL_GID   GID_PCIE_CCR_DEFS
#define LOG_LOCAL_FID   0

#define PCIE_MIPS_FREQ_DEFAULT  240

void __MTLK_IFUNC
mtlk_mmb_pcie_ccr_cleanup(_mtlk_pcie_ccr_t *pcie_mem);

int __MTLK_IFUNC
mtlk_mmb_pcie_ccr_init(
  _mtlk_pcie_ccr_t *pcie_mem, 
  mtlk_mmb_drv_t *obj, 
  mtlk_hw_t *hw,
  void *sys_dev);

static __INLINE uint32
_mtlk_pcie_ccr_read_chip_id(_mtlk_pcie_ccr_t *pcie_mem)
{
  uint32 system_info;

  __ccr_readl(&pcie_mem->pas->UPPER_SYS_IF.system_info, system_info);

  system_info &= G3_UPPER_SYS_IF_SYSTEM_INFO_CHIPID_MASK;
  system_info >>= G3_UPPER_SYS_IF_SYSTEM_INFO_CHIPID_OFFSET;

  return system_info;
}

static __INLINE mtlk_mmb_drv_t*
_mtlk_pcie_ccr_get_bus_drv(_mtlk_pcie_ccr_t *pcie_mem)
{
  return pcie_mem->mmb_drv;
}

void __MTLK_IFUNC
mtlk_mmb_pcie_ccr_enable_interrupts(_mtlk_pcie_ccr_t *pcie_mem);

void __MTLK_IFUNC
mtlk_mmb_pcie_ccr_disable_interrupts(_mtlk_pcie_ccr_t *pcie_mem);

static __INLINE int
_mtlk_pcie_ccr_get_fw_info(_mtlk_pcie_ccr_t *pcie_mem, uint8 cpu,
                           mtlk_fw_info_t *fw_info)
{
  MTLK_ASSERT(ARRAY_SIZE(fw_info->mem) >= 3);

  switch (cpu)
  {
    case CHI_CPU_NUM_UM:
      /* initialize memory chunks for upper-mac CPU */
      fw_info->mem[0].start   = ((unsigned char*)pcie_mem->pas) + UCPU_INTERNAL_MEMORY_START;
      fw_info->mem[0].length  = UCPU_INTERNAL_MEMORY_SIZE;
      fw_info->mem[0].swap    = FALSE;
      fw_info->mem[1].start   = ((unsigned char*)pcie_mem->pas) + UCPU_EXTERNAL_MEMORY_START;
      fw_info->mem[1].length  = UCPU_EXTERNAL_MEMORY_SIZE;
      fw_info->mem[1].swap    = FALSE;
      fw_info->mem[2].start   = 0;
      fw_info->mem[2].length  = 0;
      fw_info->mem[2].swap    = FALSE;
      break;
    case CHI_CPU_NUM_LM:
      /* initialize memory chunks for lower-mac CPU */
      fw_info->mem[0].start   = ((unsigned char*)pcie_mem->pas) + LCPU_INTERNAL_MEMORY_START;
      fw_info->mem[0].length  = LCPU_INTERNAL_MEMORY_SIZE;
      fw_info->mem[0].swap    = FALSE;
      fw_info->mem[1].start   = ((unsigned char*)pcie_mem->pas) + LCPU_EXTERNAL_MEMORY_START;
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
  _mtlk_pcie_read_hw_timestamp(_mtlk_pcie_ccr_t *pcie_mem, uint32 *low, uint32 *high)
{
  MTLK_ASSERT(NULL != pcie_mem);

  __ccr_readl(&pcie_mem->pas->tsf_timer_low, *low);
  __ccr_readl(&pcie_mem->pas->tsf_timer_high, *high);
}
#endif /* MTCFG_TSF_TIMER_ACCESS_ENABLED */

static __INLINE int
_mtlk_pcie_on_interrupt(_mtlk_pcie_ccr_t *pcie_mem)
{
  mtlk_handle_t lock;
  int ret = MTLK_ERR_OK;

  lock = mtlk_osal_lock_acquire_irq(&pcie_mem->irq_lock);

  if (FALSE == pcie_mem->irqs_enabled)
  {
    pcie_mem->irq_pending = TRUE;
    ret = MTLK_ERR_PENDING;
  }

  mtlk_osal_lock_release_irq(&pcie_mem->irq_lock, lock);

  return ret;
}

static __INLINE int
_mtlk_pcie_postpone_irq_handler(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);
  return mtlk_mmb_drv_postpone_irq_handler(pcie_mem->mmb_drv);
}

static __INLINE void
_mtlk_pcie_enable_interrupts(_mtlk_pcie_ccr_t *pcie_mem)
{
  mtlk_handle_t lock;
  BOOL irq_pending = FALSE;

  lock = mtlk_osal_lock_acquire_irq(&pcie_mem->irq_lock);

  if(!pcie_mem->irqs_enabled)
  {
    pcie_mem->irqs_enabled = TRUE;
    irq_pending = pcie_mem->irq_pending;
  }

  mtlk_osal_lock_release_irq(&pcie_mem->irq_lock, lock);

  if(TRUE == irq_pending)
    (void)mtlk_hw_mmb_interrupt_handler(pcie_mem->hw);
}

static __INLINE void
_mtlk_pcie_disable_interrupts(_mtlk_pcie_ccr_t *pcie_mem)
{
  mtlk_handle_t lock;

  MTLK_ASSERT(NULL != pcie_mem);

  lock = mtlk_osal_lock_acquire_irq(&pcie_mem->irq_lock);

  if(pcie_mem->irqs_enabled)
  {
    pcie_mem->irq_pending = FALSE;
    pcie_mem->irqs_enabled = FALSE;
  }

  mtlk_osal_lock_release_irq(&pcie_mem->irq_lock, lock);
}


static __INLINE BOOL
_mtlk_pcie_clear_interrupts_if_pending(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);
  /* There is no "clear" operation for MSI interrupts  */
  /* and handler is called in case of interrupt only,  */ 
  /* i.e. there is no spurious interrupts or interrupt */
  /* line sharing.                                     */
  return TRUE;
}

/* Clear interrupts unconditionally */
#ifdef MTLK_CCR_CLEAR_INT_UNCONDIT

static __INLINE BOOL
_mtlk_pcie_clear_interrupts(_mtlk_pcie_ccr_t *pcie_mem)
{
  mtlk_handle_t lock;

  MTLK_ASSERT(NULL != pcie_mem);

  lock = mtlk_osal_lock_acquire_irq(&pcie_mem->irq_lock);

  /* unconditionally */
  /* if(pcie_mem->irqs_enabled) */
  {
    pcie_mem->irq_pending = FALSE;
  }

  mtlk_osal_lock_release_irq(&pcie_mem->irq_lock, lock);

  return TRUE;
}
#endif /* MTLK_CCR_CLEAR_INT_UNCONDIT */

static __INLINE BOOL
_mtlk_pcie_disable_interrupts_if_pending(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);
  /* For MSI interrupts handler is called in case  */
  /* of interrupt only, i.e. there is no spurious  */
  /* interrupts or interrupt line sharing.         */
  _mtlk_pcie_disable_interrupts(pcie_mem);
  return TRUE;
}

#ifdef MTCFG_USE_INTERRUPT_POLLING

static __INLINE void
_mtlk_pcie_poll_interrupt(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);

  (void)mtlk_hw_mmb_interrupt_handler(pcie_mem->hw);
}

#endif /* MTCFG_USE_INTERRUPT_POLLING */

static __INLINE  void
_mtlk_pcie_initiate_doorbell_inerrupt(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);
  __ccr_writel(1, &pcie_mem->pas->HTEXT.door_bell);
}

static __INLINE  void
_mtlk_pcie_boot_from_bus(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);

  pcie_mem->next_boot_mode = G3_CPU_RAB_SHRAM;
}

static __INLINE  void
_mtlk_pcie_switch_to_iram_boot(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);

  pcie_mem->next_boot_mode = G3_CPU_RAB_IRAM;
}

static __INLINE  void
_mtlk_pcie_exit_debug_mode(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);
  /* Not needed on PCIE */
  MTLK_UNREFERENCED_PARAM(pcie_mem);
}

static __INLINE  void
_mtlk_pcie_clear_boot_from_bus(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);

  pcie_mem->next_boot_mode = G3_CPU_RAB_DEBUG;
}

static __INLINE  void
_mtlk_pcie_put_cpus_to_reset(_mtlk_pcie_ccr_t *pcie_mem)
{
  uint8 new_cpu_boot_mode = G3_CPU_RAB_Override |
                            G3_CPU_RAB_IRAM;

  MTLK_ASSERT(NULL != pcie_mem);
  MTLK_ASSERT(NULL != pcie_mem->pas);

  /* Dynamic clocks of BBCPU should be disabled before put it to reset.
     In other case reset can be not done. The dynamics clocks are set by
     FW. This is work around for hw issue for wave300 and AR10.
  */
  __ccr_writel(0, &pcie_mem->pas->CLC.dynamic);

  pcie_mem->current_lcpu_state =
    pcie_mem->current_ucpu_state =
      G3_CPU_RAB_IRAM;

  _mtlk_g3_open_secure_write_register(pcie_mem->pas, RAB);
  __ccr_writel(new_cpu_boot_mode | (new_cpu_boot_mode << 8), 
               &pcie_mem->pas->RAB.cpu_control_register);

  /* CPU requires time to go to  reset, so we       */
  /* MUST wait here before writing something else   */
  /* to CPU control register. In other case this    */
  /* may lead to unpredictable results.             */
  mtlk_osal_msleep(20);
}

static __INLINE  void
_mtlk_pcie_power_on_cpus(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);
  /* Not needed on PCIE */
  MTLK_UNREFERENCED_PARAM(pcie_mem);
}

static __INLINE  void
_mtlk_pcie_release_cpus_reset(_mtlk_pcie_ccr_t *pcie_mem)
{
  uint32 new_cpus_state;

  //////////////////////////////////////////////////////////////////////////
  //This is a fix for bug WLS-2621 (G3 PCIE hardware).
  //Even so there is no visible reason, this code must stay.
  _mtlk_pcie_put_cpus_to_reset(pcie_mem);
  mtlk_osal_msleep(50);
  //////////////////////////////////////////////////////////////////////////

  __mtlk_g3_set_ucpu_32k_blocks(pcie_mem->pas);

  pcie_mem->current_ucpu_state = 
    pcie_mem->current_lcpu_state = 
      G3_CPU_RAB_Active | pcie_mem->next_boot_mode;

  new_cpus_state =   ( pcie_mem->current_ucpu_state | G3_CPU_RAB_Override ) |
                   ( ( pcie_mem->current_lcpu_state | G3_CPU_RAB_Override ) << 8 );

  _mtlk_g3_open_secure_write_register(pcie_mem->pas, RAB);

  __ccr_writel(new_cpus_state,
               &pcie_mem->pas->RAB.cpu_control_register);

  /* CPU requires time to change its state, so we   */
  /* MUST wait here before writing something else   */
  /* to CPU control register. In other case this    */
  /* may lead to unpredictable results.             */
  mtlk_osal_msleep(10);
}

static __INLINE BOOL
_mtlk_pcie_check_bist(_mtlk_pcie_ccr_t *pcie_mem, uint32 *bist_result)
{
  MTLK_ASSERT(NULL != pcie_mem);
  MTLK_ASSERT(NULL != bist_result);

   __ccr_writel(G3_VAL_START_BIST, &pcie_mem->pas->HTEXT.start_bist);
   mtlk_osal_msleep(20);

  __ccr_readl(&pcie_mem->pas->HTEXT.bist_result, *bist_result);

  return (*bist_result & G3PCIE_CPU_Control_BIST_Passed) == 
    G3PCIE_CPU_Control_BIST_Passed;
}

/* 
  PHY indirect control access, bit definitions
  ---------------------------------------------
  Control address, offset = 0x1D4:
  Bits[15:0] = write data
  Bit[16] = Capture address
  Bit[17] = capture data
  Bit[18] = CR read
  Bit[19] = CR write

  Status address, offset = 0x1D8
  Bits[15:0] = read data
  Bit[16] = ack.
*/

#define __MTLK_PCIE_ACK_WAIT_ITERATIONS (300)

static __INLINE void
__mtlk_pcie_wait_ack_assertion(_mtlk_pcie_ccr_t *pcie_mem, char* operation_id)
{
  uint32 phy_status, ct = 0;

#ifdef MTCFG_SILENT
  MTLK_UNREFERENCED_PARAM(operation_id);
#endif

  for(ct = 0; ct < __MTLK_PCIE_ACK_WAIT_ITERATIONS; ct++)
  {
    __ccr_readl(&pcie_mem->pas->HTEXT.phy_data, phy_status);
    if(phy_status & (1<<16)) return;
  }

  ILOG0_SD("Ack assertion %s wait failed (phy_status = 0x%08x)\n", operation_id, phy_status);
  MTLK_ASSERT(FALSE);
}

static __INLINE void
__mtlk_pcie_wait_ack_deassertion(_mtlk_pcie_ccr_t *pcie_mem, char* operation_id)
{
  uint32 phy_status, ct = 0;

#ifdef MTCFG_SILENT
  MTLK_UNREFERENCED_PARAM(operation_id);
#endif

  for(ct = 0; ct < __MTLK_PCIE_ACK_WAIT_ITERATIONS; ct++)
  {
    __ccr_readl(&pcie_mem->pas->HTEXT.phy_data, phy_status);
    if(!(phy_status & (1<<16))) return;
  }

  ILOG0_SD("Ack deassertion %s wait failed (phy_status = 0x%08x)\n", operation_id, phy_status);
  MTLK_ASSERT(FALSE);
}

static __INLINE void 
__mtlk_pcie_open_reg(_mtlk_pcie_ccr_t *pcie_mem, uint32 reg)
{
  /* 1.Set address bus */
  /* 2.Assert capture address bit */
  __ccr_writel((reg | (1<<16)), &pcie_mem->pas->HTEXT.phy_ctl);

  /* 3.Wait for CR_ack assertion */
  __mtlk_pcie_wait_ack_assertion(pcie_mem, "o1");

  /* 4.De-assert capture address bit */
  __ccr_writel(0x00000000, &pcie_mem->pas->HTEXT.phy_ctl);

  /* 5.Wait for CR_ack de-assertion */
  __mtlk_pcie_wait_ack_deassertion(pcie_mem, "o2");
}

static __INLINE void 
__mtlk_pcie_write_reg(_mtlk_pcie_ccr_t *pcie_mem, uint32 reg, uint16 val)
{
  /* 1-5. Open the register */
  __mtlk_pcie_open_reg(pcie_mem, reg);

  /* 6.Set write data */
  /* 7.Assert capture data bit (while keeping the write data) */
  __ccr_writel(((1<<17)|val), &pcie_mem->pas->HTEXT.phy_ctl);
  
  /* 8.Wait for CR_ack assertion */
  __mtlk_pcie_wait_ack_assertion(pcie_mem, "w3");

  /* 9.De-assert capture data bit */
  __ccr_writel(0x00000000, &pcie_mem->pas->HTEXT.phy_ctl);

  /* 10.Wait for CR_ack de-assertion */
  __mtlk_pcie_wait_ack_deassertion(pcie_mem, "w4");

  /* 11.Set CR write bit */
  __ccr_writel((1<<19), &pcie_mem->pas->HTEXT.phy_ctl);

  /* 12.Wait for CR ack assertion */
  __mtlk_pcie_wait_ack_assertion(pcie_mem, "w5");

  /* 13.De-assert CR write bit */
  __ccr_writel(0x00000000, &pcie_mem->pas->HTEXT.phy_ctl);

  /* 14.Wait for CR ack de-assertion */
  __mtlk_pcie_wait_ack_deassertion(pcie_mem, "w6");
}

static __INLINE uint32 
__mtlk_pcie_read_reg(_mtlk_pcie_ccr_t *pcie_mem, uint32 reg) 
{
    uint32 val;

    /* 1-5. Open the register */
    __mtlk_pcie_open_reg(pcie_mem, reg);

    /* 6.Set CR-read data bit */
    __ccr_writel((1<<18), &pcie_mem->pas->HTEXT.phy_ctl);

    /* 7.Wait for CR ack assertion */ 
    __mtlk_pcie_wait_ack_assertion(pcie_mem, "r3");

    /* 8.Read the <read data> from status bus */
    __ccr_readl(&pcie_mem->pas->HTEXT.phy_data, val);

    /* 9.De-assert CR-read bit */
    __ccr_writel(0x00000000, &pcie_mem->pas->HTEXT.phy_ctl);

    /* 10.Wait for CR_ack de-assertion */
    __mtlk_pcie_wait_ack_deassertion(pcie_mem, "r4");

    return val & 0x0000ffff;
}

extern int tx_ovr_and_mask;
extern int tx_ovr_or_mask;
extern int lvl_ovr_and_mask;
extern int lvl_ovr_or_mask;

static __INLINE void
_mtlk_pcie_release_ctl_from_reset(_mtlk_pcie_ccr_t *pcie_mem)
{
  uint32 tx_ovr_before, lvl_ovr_before;

#if (defined MTCFG_PCIE_TUNING) && (RTLOG_MAX_DLEVEL >= 1)
  uint32 tx_ovr_after, lvl_ovr_after;
#endif

  uint16 dataval;

  MTLK_ASSERT(NULL != pcie_mem);

  __mtlk_g3_release_ctl_from_reset_phase1(pcie_mem->pas);

  tx_ovr_before = __mtlk_pcie_read_reg(pcie_mem ,0x2004);

  /*                                            EN         Boost        Atten        Edge   */
  dataval = (uint16)((tx_ovr_before & tx_ovr_and_mask) | tx_ovr_or_mask);
  __mtlk_pcie_write_reg(pcie_mem, 0x2004, dataval);

#if (defined MTCFG_PCIE_TUNING) && (RTLOG_MAX_DLEVEL >= 1)
  tx_ovr_after = __mtlk_pcie_read_reg(pcie_mem ,0x2004);
#endif

  lvl_ovr_before = __mtlk_pcie_read_reg(pcie_mem ,0x14);
  /*								                          Transmit Lvl      EN  */
  dataval = (uint16)((lvl_ovr_before & lvl_ovr_and_mask) | lvl_ovr_or_mask);
  __mtlk_pcie_write_reg(pcie_mem, 0x14, dataval);

#if (defined MTCFG_PCIE_TUNING) && (RTLOG_MAX_DLEVEL >= 1)
  lvl_ovr_after = __mtlk_pcie_read_reg(pcie_mem ,0x14);
#endif

#ifdef MTCFG_PCIE_TUNING
  ILOG1_DD("de-emphasis: TX input override modification: 0x%08x --> 0x%08x", tx_ovr_before, tx_ovr_after);
  ILOG1_DD("de-emphasis: level override modification: 0x%08x --> 0x%08x", lvl_ovr_before, lvl_ovr_after);
#endif

  __mtlk_g3_release_ctl_from_reset_phase2(pcie_mem->pas);
}

static __INLINE int
_mtlk_pcie_eeprom_override_write (_mtlk_pcie_ccr_t *pcie_mem, uint32 data, uint32 mask)
{
  uint32 val;

  MTLK_ASSERT(NULL != pcie_mem);

  __ccr_readl(&pcie_mem->pas->RAB.eeprom_override, val);

  val = (val & (~mask))| data;

  __ccr_writel(val, &pcie_mem->pas->RAB.eeprom_override);

  return MTLK_ERR_OK;
}

static __INLINE uint32
_mtlk_pcie_eeprom_override_read (_mtlk_pcie_ccr_t *pcie_mem)
{
  uint32 val;

  MTLK_ASSERT(NULL != pcie_mem);

  __ccr_readl(&pcie_mem->pas->RAB.eeprom_override, val);

  return val;
}

static __INLINE uint32
_mtlk_pcie_ccr_get_mips_freq(_mtlk_pcie_ccr_t *pcie_mem)
{
  return PCIE_MIPS_FREQ_DEFAULT;
}

static __INLINE uint32
_mtlk_pcie_ccr_get_progmodel_version (_mtlk_pcie_ccr_t *pcie_mem)
{
  uint32 val;

  MTLK_ASSERT(NULL != pcie_mem);

  __ccr_readl(&pcie_mem->pas->TD.phy_progmodel_ver, val);

  return val;
}

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID


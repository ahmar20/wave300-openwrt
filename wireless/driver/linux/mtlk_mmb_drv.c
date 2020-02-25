/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "hw_mmb.h"
#include "drvver.h"
#include "mtlk_fast_mem.h"
#include "mtlk_vap_manager.h"
#include "mtlk_dfg.h"
#include "mtlkhal.h"

#include <linux/module.h>
#include <linux/init.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/pci.h>
#include <linux/cpufreq.h>	//TODO may not be required??
#include <lantiq_soc.h>

#ifdef MTCFG_PLATFORM_GEN35FPGA
  #include <mtlk_interrupt.h>
#endif

#ifdef MTCFG_BENCHMARK_TOOLS
  #include "mtlk_dbg.h"
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
  #include <asm/ifx/common_routines.h>
#endif

#define LOG_LOCAL_GID   GID_MMBDRV
#define LOG_LOCAL_FID   1

#define MTLK_MEM_BAR0_INDEX  0
#define MTLK_MEM_BAR1_INDEX  1
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,10,0)
#define MTLK_MEM_BAR1_INDEX_AHB  0
#endif

#ifdef MTCFG_BUS_PCI_PCIE
  /* we only support 32-bit addresses */
  #define PCI_SUPPORTED_DMA_MASK       0xffffffff
#endif /* MTCFG_BUS_PCI_PCIE */

MODULE_DESCRIPTION(DRV_DESCRIPTION);
MODULE_AUTHOR(MTLK_COPYRIGHT);
MODULE_LICENSE("GPL");

static int ap[MTLK_MAX_HW_ADAPTERS_SUPPORTED] = {0};
static int dut[MTLK_MAX_HW_ADAPTERS_SUPPORTED] = {0};
static int debug = 0;

#define MTLK_G35_DEFAULT_CPU_MB_NUMBER (31) /* Default number of megabyte of  */
                                            /* DDR that is mapped into BB CPU */
                                            /* internal memory region         */
int bb_cpu_ddr_mb_number = MTLK_G35_DEFAULT_CPU_MB_NUMBER;

#ifdef MTLK_DEBUG
extern int step_to_fail;
#endif
static int cpu_freq = CPU_FREQ_RESERVED;
static int no_more_ifaces = FALSE;  /* A flag used to mark if any card startup failed    */
                                    /* In this case we do not bring up any subsequent    */
                                    /* interfaces to avoid wrong wlanX addressing issues */

module_param_array(ap, int, NULL, 0);
module_param_array(dut, int, NULL, 0);
module_param(debug, int, 0);
module_param(bb_cpu_ddr_mb_number, int, 0);
#ifdef MTLK_DEBUG
module_param(step_to_fail, int, 0);
#endif
module_param(cpu_freq, int, 0);

MODULE_PARM_DESC(ap, "Make an access point");
MODULE_PARM_DESC(dut, "Start driver in device under test (DUT) mode");
MODULE_PARM_DESC(debug, "Debug level");
MODULE_PARM_DESC(bb_cpu_ddr_mb_number, "Offset of DDR memory region allocated for wireless card CPU (in megabytes)");
#ifdef MTLK_DEBUG
MODULE_PARM_DESC(step_to_fail, "Init step to simulate fail");
#endif
MODULE_PARM_DESC(cpu_freq, "CPU working frequency in Hz");

struct _mtlk_mmb_drv_t {
  mtlk_vap_manager_t    *vap_manager;
  mtlk_hw_api_t         *hw_api;
  mtlk_ccr_t            ccr;
  int                   irq;
  struct tasklet_struct irq_tasklet;

  MTLK_DECLARE_INIT_STATUS;
};

/**************************************************************
 *  MMB driver external interface                             *
 **************************************************************/

/**
 * Prepare string representation of device
 */
void __MTLK_IFUNC
mtlk_mmb_drv_get_name(mtlk_mmb_drv_t *obj, char *buf, int buf_len)
{
  MTLK_ASSERT(NULL != obj);
  MTLK_ASSERT(NULL != buf);
  MTLK_ASSERT(buf_len > 0);

  buf[0] = '\0';

  CARD_SELECTOR_START(obj->ccr.hw_type)
    IF_CARD_PCIG3 ( snprintf(buf, buf_len - 1, "PCI-%s",
                             pci_name(obj->ccr.mem.pcig3.dev)));
    IF_CARD_PCIE  ( snprintf(buf, buf_len - 1, "PCIe-%s",
                             pci_name(obj->ccr.mem.pcie.dev)));
    IF_CARD_AHBG35( snprintf(buf, buf_len - 1, "AHB-%s",
                             dev_name(&((struct platform_device*)obj->ccr.mem.g35.pdev)->dev)));
  CARD_SELECTOR_END();
}

/**
 * Retrieve system device handle
 */
struct device * __MTLK_IFUNC
mtlk_mmb_drv_get_device (mtlk_mmb_drv_t *obj)
{
  MTLK_ASSERT(NULL != obj);

  CARD_SELECTOR_START(obj->ccr.hw_type)
    IF_CARD_PCIG3 ( return &((struct pci_dev*)obj->ccr.mem.pcig3.dev)->dev);
    IF_CARD_PCIE  ( return &((struct pci_dev*)obj->ccr.mem.pcie.dev)->dev);
    IF_CARD_AHBG35( return &((struct platform_device*)obj->ccr.mem.g35.pdev)->dev);
  CARD_SELECTOR_END();
}

/**************************************************************
 *  Basic device initialization initialization                *
 **************************************************************/

static __INLINE unsigned char*
_mmb_mem_pas_get(mtlk_mmb_drv_t *obj)
{
  MTLK_ASSERT(NULL != obj);

  CARD_SELECTOR_START(obj->ccr.hw_type)
    IF_CARD_PCIG3 ( return (unsigned char*)obj->ccr.mem.pcig3.pas );
    IF_CARD_PCIE  ( return (unsigned char*)obj->ccr.mem.pcie.pas  );
    IF_CARD_AHBG35( return (unsigned char*)obj->ccr.mem.g35.pas   );
  CARD_SELECTOR_END();
}

#ifdef MTCFG_BUS_PCI_PCIE
/** 
 * Common PCI device cleanup
 */
static __INLINE void
_pci_stop(struct pci_dev *dev)
{
  pci_disable_device(dev);
}

/**
 * Common PCI device initialization 
 */
static __INLINE int
_pci_start(struct pci_dev *dev)
{
  if (0 != pci_enable_device(dev))
    return MTLK_ERR_UNKNOWN;

  pci_set_master(dev);

  if (0 != pci_set_dma_mask(dev, PCI_SUPPORTED_DMA_MASK))
    return MTLK_ERR_UNKNOWN;

  return MTLK_ERR_OK;
}

/** 
 * Map physical memory of PCI bus
 */
static void*
_pci_mem_get (struct pci_dev *dev, int res_id)
{
  void __iomem *mem = NULL;

  MTLK_ASSERT(NULL != dev);

  mem = ioremap(pci_resource_start(dev, res_id), pci_resource_len(dev, res_id));

  ILOG2_DPPD("PCI Memory block %i: PA: 0x%p, VA: 0x%p, Len=0x%x", res_id,
            (void*) pci_resource_start(dev, res_id), mem, 
            (uint32) pci_resource_len(dev, res_id));

  return mem;
}

/** 
 * Release physical memory of pci bus
 */
static __INLINE void
_pci_mem_put (void* mem)
{
  if (NULL != mem)
    iounmap(mem);
}

#ifdef MTCFG_LINDRV_HW_PCIG3
/**
 * PCI device cleanup
 */
void __MTLK_IFUNC 
mtlk_mmb_pcig3_ccr_cleanup(_mtlk_pcig3_ccr_t *pcig3_mem)
{
  MTLK_ASSERT(NULL != pcig3_mem);

  /* release device memory */
  _pci_mem_put(pcig3_mem->pas);
  _pci_mem_put(pcig3_mem->hrc);

  /* remove link for resources */
  pci_set_drvdata(pcig3_mem->dev, NULL);

  /* stop PCI device */
  _pci_stop(pcig3_mem->dev);

  /* cleanup all values at once */
  memset(pcig3_mem, 0, sizeof(*pcig3_mem));
}

/** 
 * PCI device initialization 
 */
int __MTLK_IFUNC
mtlk_mmb_pcig3_ccr_init(_mtlk_pcig3_ccr_t *pcig3_mem, mtlk_mmb_drv_t *obj,
                        mtlk_hw_t *hw, void *sys_dev)
{
  struct pci_dev *dev = sys_dev;

  MTLK_ASSERT(NULL != pcig3_mem);
  MTLK_ASSERT(NULL != sys_dev);

#if 0  
  /* NOTE: redundant step. The object is a *static* part of owner 
     and have to be already cleaned */
  memset(pcig3_mem, 0, sizeof(*pcig3_mem));
#endif /* 0 */

  /* prepare bidirectional link for resources */
  pci_set_drvdata(dev, obj);
  pcig3_mem->dev = dev;
  pcig3_mem->mmb_drv = obj;
  pcig3_mem->hw= hw;

  /* start PCI device */
  if (MTLK_ERR_OK != _pci_start(dev))
    return MTLK_ERR_UNKNOWN;

  /* request device memory for BAR0 */
  pcig3_mem->hrc = _pci_mem_get(dev, MTLK_MEM_BAR0_INDEX);
  if (NULL == pcig3_mem->hrc)
    return MTLK_ERR_UNKNOWN;

  /* request device memory for BAR1 */
  pcig3_mem->pas = _pci_mem_get(dev, MTLK_MEM_BAR1_INDEX);
  if (NULL == pcig3_mem->pas)
    return MTLK_ERR_UNKNOWN;

#ifndef MTCFG_USE_INTERRUPT_POLLING
  obj->irq = dev->irq;
#endif /* not MTCFG_USE_INTERRUPT_POLLING */

  return MTLK_ERR_OK;
}
#endif /* MTCFG_LINDRV_HW_PCIG3 */

#ifdef MTCFG_LINDRV_HW_PCIE
/** 
 * PCI Express device cleanup
 */
void __MTLK_IFUNC
mtlk_mmb_pcie_ccr_cleanup(_mtlk_pcie_ccr_t *pcie_mem)
{
  MTLK_ASSERT(NULL != pcie_mem);

  mtlk_osal_lock_cleanup(&pcie_mem->irq_lock);

  pci_disable_msi(pcie_mem->dev);

  /* release device memory */
  _pci_mem_put(pcie_mem->pas);

  /* remove link for resources */
  pci_set_drvdata(pcie_mem->dev, NULL);

  /* stop PCI device */
  _pci_stop(pcie_mem->dev);

  /* cleanup all values at once */
  memset(pcie_mem, 0, sizeof(*pcie_mem));
}

/** 
 * PCI Express device initialization 
 */
int __MTLK_IFUNC
mtlk_mmb_pcie_ccr_init(_mtlk_pcie_ccr_t *pcie_mem, mtlk_mmb_drv_t *obj,
                       mtlk_hw_t *hw, void *sys_dev)
{
  struct pci_dev *dev = sys_dev;

  MTLK_ASSERT(NULL != pcie_mem);
  MTLK_ASSERT(NULL != sys_dev);

#if 0  
  /* NOTE: redundant step. The object is a *static* part of owner 
     and have to be already cleaned */
  memset(pcie_mem, 0, sizeof(*pcie_mem));
#endif /* 0 */

  /* prepare bidirectional link for resources */
  pci_set_drvdata(dev, obj);
  pcie_mem->dev = dev;
  pcie_mem->mmb_drv = obj;
  pcie_mem->hw = hw;

  /* start PCI device */
  if (MTLK_ERR_OK != _pci_start(dev))
    return MTLK_ERR_UNKNOWN;

  /* request device memory */
  pcie_mem->pas = _pci_mem_get(dev, MTLK_MEM_BAR1_INDEX);
  if (NULL == pcie_mem->pas)
    return MTLK_ERR_UNKNOWN;

  /* allocate IRQs for a device with the MSI capability */
  if (0 != pci_enable_msi(dev))
  {
    ELOG_V("Failed to enable MSI interrupts for the device");
    return MTLK_ERR_UNKNOWN;
  }

#ifndef MTCFG_USE_INTERRUPT_POLLING
  obj->irq = dev->irq;
#endif /* not MTCFG_USE_INTERRUPT_POLLING */

  /* This is a state of cpu on power-on */
  pcie_mem->current_ucpu_state = 
    pcie_mem->current_lcpu_state = 
      G3_CPU_RAB_Active | G3_CPU_RAB_DEBUG;

  pcie_mem->next_boot_mode = G3_CPU_RAB_DEBUG;

  pcie_mem->irqs_enabled = TRUE;
  pcie_mem->irq_pending = FALSE;

  return mtlk_osal_lock_init(&pcie_mem->irq_lock);
}
#endif /* MTCFG_LINDRV_HW_PCIE */

#endif /* MTCFG_BUS_PCI_PCIE */

#ifdef MTCFG_BUS_AHB

/** 
 * Map physical memory of AHB bus
 */
static void*
_ahb_mem_get (struct platform_device *pdev, int res_id)
{
  struct resource* res = NULL; 
  void __iomem *mem = NULL;

  MTLK_ASSERT(NULL != pdev);

  res = platform_get_resource(pdev, IORESOURCE_MEM, res_id);
  if(NULL == res)
      return NULL;

  if(NULL == request_mem_region(res->start, resource_size(res), dev_name(&pdev->dev)))
    return NULL;

  mem = ioremap(res->start, resource_size(res));

  ILOG2_DPPD("AHB Memory block %i: PA: 0x%p, VA: 0x%p, Len=0x%x", res_id,
            (void*) res->start, mem, (uint32) resource_size(res));

  return mem;
}

/** 
 * Release physical memory of AHB bus
 */
static void
_ahb_mem_put (void* mem, struct platform_device *pdev, int res_id)
{
  struct resource* res = NULL; 

  res = platform_get_resource(pdev, IORESOURCE_MEM, res_id);
  if(NULL == res)
      return;

  release_mem_region(res->start, resource_size(res));

  if (NULL != mem)
    iounmap(mem);
}

#ifndef MTCFG_USE_INTERRUPT_POLLING
/**
 * Retrieve IRQ number for AHB device
 */
static int
_ahb_irq_get(struct platform_device *pdev)
{

#ifdef MTLK_G35_NPU
  return MTLK_WIRELESS_IRQ_OUT_INDEX;
#else
  int i;

  /* find IRQ from resource list */
  for(i = 0; i< pdev->num_resources; i++) {
    if(pdev->resource[i].flags & IORESOURCE_IRQ) {
      ELOG_D("Found device IRQ: %d", pdev->resource[i].start);
      return pdev->resource[i].start;
    }
  }

  ELOG_V("Failed to find interrupt for the AHB device");

  return -1;
#endif
}
#endif /* not MTCFG_USE_INTERRUPT_POLLING */

#ifdef MTCFG_LINDRV_HW_AHBG35
/** 
 * AHB device cleanup
 */
void __MTLK_IFUNC
mtlk_mmb_g35_ccr_cleanup(_mtlk_g35_ccr_t *g35_mem)
{
  MTLK_ASSERT(NULL != g35_mem);

  /* release device memory */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
  _ahb_mem_put(g35_mem->cpu_ddr, g35_mem->pdev, MTLK_MEM_BAR0_INDEX);
  _ahb_mem_put(g35_mem->pas, g35_mem->pdev, MTLK_MEM_BAR1_INDEX);
#else
  _ahb_mem_put(g35_mem->pas, g35_mem->pdev, MTLK_MEM_BAR1_INDEX_AHB);
#endif

  /* remove link for resources */
  platform_set_drvdata((struct platform_device*)g35_mem->pdev, NULL);

  /* cleanup all values at once */
  memset(g35_mem, 0, sizeof(*g35_mem));
}

#if defined(MTCFG_BUS_AHB_BYTESWAP)
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
#include <asm/ifx/ar10/ar10.h>  /* required for AHB configuration */
#else
#include <ar10/ar10.h>
#endif
#else
#include <asm/mach-lantiq/xway/lantiq_soc.h>
#define RCU_AHB_ENDIAN_OFFSET 0x004C
#define IFX_RCU_BE_AHB1S 0x00000800
#endif
#endif

/** 
 * AHB device initialization 
 */
int __MTLK_IFUNC
mtlk_mmb_g35_ccr_init(_mtlk_g35_ccr_t *g35_mem, mtlk_mmb_drv_t *obj,
                      mtlk_hw_t *hw, void *sys_dev)
{
  struct platform_device *pdev = sys_dev;

  MTLK_ASSERT(NULL != g35_mem);
  MTLK_ASSERT(NULL != sys_dev);

#if 0  
  /* NOTE: redundant step. The object is a *static* part of owner 
     and have to be already cleaned */
  memset(g35_mem, 0, sizeof(*g35_mem));
#endif /* 0 */

  /* prepare bidirectional link for resources */
  platform_set_drvdata(pdev, obj);
  g35_mem->pdev = pdev;
  g35_mem->mmb_drv = obj;
  g35_mem->hw = hw;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
  /* request device memory for BAR0 */
  g35_mem->cpu_ddr = _ahb_mem_get(pdev, MTLK_MEM_BAR0_INDEX);
  if (NULL == g35_mem->cpu_ddr)
    return MTLK_ERR_UNKNOWN;

  /* request device memory for BAR1 */
  g35_mem->pas = _ahb_mem_get(pdev, MTLK_MEM_BAR1_INDEX);
  if (NULL == g35_mem->pas)
    return MTLK_ERR_UNKNOWN;
#else
  /* request device memory for BAR0 */
  g35_mem->cpu_ddr = (unsigned char*)(((unsigned int)bb_cpu_ddr_mb_number) * 0x100000u + 0xa0000000u);

  /* request device memory for BAR1 */
  g35_mem->pas = _ahb_mem_get(pdev, MTLK_MEM_BAR1_INDEX_AHB);
  if (NULL == g35_mem->pas)
    return MTLK_ERR_UNKNOWN;
#endif

#ifndef MTCFG_USE_INTERRUPT_POLLING
  {
    int irq = _ahb_irq_get(pdev);
    if(-1 == irq)
      return MTLK_ERR_UNKNOWN;

    obj->irq = irq;
  }
#endif /* not MTCFG_USE_INTERRUPT_POLLING */

  /* This is a state of cpu on power-on */
  g35_mem->current_ucpu_state = 
    G3_CPU_RAB_Active | G3_CPU_RAB_DEBUG;

  g35_mem->next_boot_mode = G3_CPU_RAB_DEBUG;

#if defined(MTCFG_BUS_AHB_BYTESWAP)
  /*  Configure AHB #1 module in big endian */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
  (*IFX_RCU_AHB_ENDIAN) |= IFX_RCU_BE_AHB1S;
#else
  ltq_rcu_w32(ltq_rcu_r32(RCU_AHB_ENDIAN_OFFSET) | IFX_RCU_BE_AHB1S, RCU_AHB_ENDIAN_OFFSET);
#endif

  __ccr_writel((G33_CPU_HTEXT_SCD_AHB_MASTER_WRITE_FULL_BYTE_SWAP | G33_CPU_HTEXT_SCD_AHB_MASTER_READ_FULL_BYTESWAP | \
      G33_CPU_HTEXT_DMA_AHB_MASTER_WRITE_FULL_BYTE_SWAP | G33_CPU_HTEXT_DMA_AHB_MASTER_READ_FULL_BYTESWAP) , \
      &g35_mem->pas->HTEXT.endian_swap_control);
#endif

  return MTLK_ERR_OK;
}
#endif /* MTCFG_LINDRV_HW_AHBG35 */

#endif /* MTCFG_BUS_AHB */

/**************************************************************
 *  MMB driver interrupt handlers                             *
 **************************************************************/

/**
 * Bottom half of MMB driver IRQ handler
 */
static void
_on_irq_tasklet(unsigned long param)
{
  MTLK_ASSERT(param);

  mtlk_hw_mmb_deferred_handler((mtlk_hw_t *)param);
}

/**
 * Postpone upper half IRQ handler of MMB driver
 */
int __MTLK_IFUNC
mtlk_mmb_drv_postpone_irq_handler(mtlk_mmb_drv_t *obj)
{
  MTLK_ASSERT(obj);

      /* postpone IRQ handler */
      tasklet_schedule(&obj->irq_tasklet);

  return MTLK_ERR_OK;
}


/**
 * retrieve CPU working frequency passed as parameter on insmod
 * Input in Hz, output in MHz.
 */
uint32 __MTLK_IFUNC
mtlk_mmb_drv_get_cpu_freq(mtlk_mmb_drv_t *obj)
{
  uint32 freq;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,10,0)
  freq = ifx_get_fpi_hz() / 1000000;
#else
  struct clk *fpi_clk = clk_get_fpi();
  freq = clk_get_rate(fpi_clk) / 1000000;
#endif

  ILOG0_D("FPI bus frequency: %d MHz", freq);
  return freq;
}

#ifndef MTCFG_USE_INTERRUPT_POLLING

/**
 * Entry point for MMB driver interrupt handler
 */
static irqreturn_t
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
_on_interrupt(int irq, void *hw, struct pt_regs *regs)
#else
_on_interrupt(int irq, void *hw)
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19) */
{
  MTLK_UNREFERENCED_PARAM(irq);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
  MTLK_UNREFERENCED_PARAM(regs);
#endif /* LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19) */

  MTLK_ASSERT(hw);

  if(MTLK_ERR_OK == mtlk_hw_mmb_interrupt_handler(hw))
    return IRQ_HANDLED;
  
  return IRQ_NONE;
}

/**
 * Register IRQ handler
 */
static int 
_mmb_drv_irq_start(mtlk_mmb_drv_t *obj)
{
  ILOG2_SD("%s IRQ 0x%x", DRV_NAME, obj->irq);

  MTLK_ASSERT(obj);
  MTLK_ASSERT(obj->hw_api);
  MTLK_ASSERT(obj->hw_api->hw);

#if defined(MTCFG_BUS_AHB) && defined(MTLK_G35_NPU)
  if(MTLK_CARD_AHBG35 == obj->hw_type)
  {
    mtlk_register_irq(MTLK_WIRELESS_IRQ_IN_INDEX, obj->irq, _on_interrupt,
                      IRQF_DISABLED, DRV_NAME, obj->hw_api->hw, metalink_wls_irq);
  }
  else
#endif /* defined(MTCFG_BUS_AHB) && defined(MTLK_G35_NPU) */
  {
    int err = request_irq(obj->irq, _on_interrupt, IRQF_SHARED, DRV_NAME, obj->hw_api->hw);

    if (err < 0)
    {
      ELOG_DD("Failed to allocate interrupt %d, error code: %d", obj->irq, err);
      return MTLK_ERR_UNKNOWN;
    }
  }

  return MTLK_ERR_OK;
}

/**
 * Unregister IRW handler
 */
static void 
_mmb_drv_irq_stop(mtlk_mmb_drv_t *obj)
{
  MTLK_ASSERT(obj);
  MTLK_ASSERT(obj->hw_api);
  MTLK_ASSERT(obj->hw_api->hw);

#if defined(MTCFG_BUS_AHB) && defined(MTLK_G35_NPU)
  if(MTLK_CARD_AHBG35 == obj->hw_type)
  {
    mtlk_unregister_irq(MTLK_WIRELESS_IRQ_IN_INDEX, obj->irq, obj->hw_api->hw);
  }
  else
#endif /* defined(MTCFG_BUS_AHB) && defined(MTLK_G35_NPU) */
  {
    free_irq(obj->irq, obj->hw_api->hw);
  }
}

#endif /* not MTCFG_USE_INTERRUPT_POLLING */

/**************************************************************
 *  Main MMB driver initialization                            *
 **************************************************************/

/**
 * MMB driver initialization steps
 */
MTLK_INIT_STEPS_LIST_BEGIN(mmb_drv)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_drv, MMB_HW_CARD_CREATE)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_drv, MMB_IRQ_TASKLET_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_drv, MMB_CCR_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_drv, MMB_VAP_MANAGER_CREATE)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_drv, MMB_HW_CARD_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_drv, MMB_MASTER_VAP_CREATE)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_drv, MMB_TXMM_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_drv, MMB_TXDM_INIT)
#ifndef MTCFG_USE_INTERRUPT_POLLING
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_drv, MMB_IRQ_START)
#endif /* not MTCFG_USE_INTERRUPT_POLLING */
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_drv, MMB_CARD_START)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_drv, MMB_VAP_MANAGER_PREPARE_VAPS)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_drv, MMB_VAP_MANAGER_PREPARE_START)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_drv, MMB_MAC_EVENTS_START)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_drv, MMB_VAP_START)
  MTLK_INIT_STEPS_LIST_ENTRY(mmb_drv, MMB_CORE_PREPARE_STOP)
MTLK_INIT_INNER_STEPS_BEGIN(mmb_drv)
MTLK_INIT_STEPS_LIST_END(mmb_drv);

/**
 * retrieve working mode for latest added HW card 
 */
static __INLINE mtlk_work_mode_e 
_work_mode_get(mtlk_hw_t *hw)
{

//NOTICE this is one of functions to determine if driver is in AP or STA mode

  return dut[mtlk_hw_mmb_get_card_idx(hw)] ? MTLK_MODE_DUT :
    (ap[mtlk_hw_mmb_get_card_idx(hw)] ? MTLK_MODE_AP : MTLK_MODE_STA);
}

/**
 * MMB device deletion procedure
 */
static void
_mmb_drv_cleanup(mtlk_mmb_drv_t *obj)
{
  MTLK_CLEANUP_BEGIN(mmb_drv, MTLK_OBJ_PTR(obj))
    MTLK_CLEANUP_STEP(mmb_drv, MMB_CORE_PREPARE_STOP, MTLK_OBJ_PTR(obj),
                      mtlk_vap_manager_prepare_stop, (obj->vap_manager));

    MTLK_CLEANUP_STEP(mmb_drv, MMB_VAP_START, MTLK_OBJ_PTR(obj),
                      mtlk_vap_manager_stop_all_vaps, 
                      (obj->vap_manager, MTLK_VAP_MASTER_INTERFACE));

    MTLK_CLEANUP_STEP(mmb_drv, MMB_MAC_EVENTS_START, MTLK_OBJ_PTR(obj),
                      mtlk_hw_mmb_stop_mac_events, (obj->hw_api->hw));

    MTLK_CLEANUP_STEP(mmb_drv, MMB_VAP_MANAGER_PREPARE_START, MTLK_OBJ_PTR(obj),
                      mtlk_vap_manager_unprepare, (obj->vap_manager));

    MTLK_CLEANUP_STEP(mmb_drv, MMB_VAP_MANAGER_PREPARE_VAPS, MTLK_OBJ_PTR(obj),
                      mtlk_vap_manager_deallocate_vaps, (obj->vap_manager));

    MTLK_CLEANUP_STEP(mmb_drv, MMB_CARD_START, MTLK_OBJ_PTR(obj),
                      mtlk_hw_mmb_stop_card, (obj->hw_api->hw));

#ifndef MTCFG_USE_INTERRUPT_POLLING
    MTLK_CLEANUP_STEP(mmb_drv, MMB_IRQ_START, MTLK_OBJ_PTR(obj),
                      _mmb_drv_irq_stop, (obj));
#endif /* not MTCFG_USE_INTERRUPT_POLLING */

    MTLK_CLEANUP_STEP(mmb_drv, MMB_TXDM_INIT, MTLK_OBJ_PTR(obj),
                      mtlk_txmm_cleanup, (mtlk_hw_mmb_get_txdm(obj->hw_api->hw)));

    MTLK_CLEANUP_STEP(mmb_drv, MMB_TXMM_INIT, MTLK_OBJ_PTR(obj),
                      mtlk_txmm_cleanup, (mtlk_hw_mmb_get_txmm(obj->hw_api->hw)));

    MTLK_CLEANUP_STEP(mmb_drv, MMB_MASTER_VAP_CREATE, MTLK_OBJ_PTR(obj),
                      mtlk_vap_manager_delete_master_vap, (obj->vap_manager));

    MTLK_CLEANUP_STEP(mmb_drv, MMB_HW_CARD_INIT, MTLK_OBJ_PTR(obj),
                      mtlk_hw_mmb_cleanup_card, (obj->hw_api->hw));

    MTLK_CLEANUP_STEP(mmb_drv, MMB_VAP_MANAGER_CREATE, MTLK_OBJ_PTR(obj),
                      mtlk_vap_manager_delete, (obj->vap_manager));

    MTLK_CLEANUP_STEP(mmb_drv, MMB_CCR_INIT, MTLK_OBJ_PTR(obj),
                      mtlk_ccr_cleanup, (&obj->ccr));

    MTLK_CLEANUP_STEP(mmb_drv, MMB_IRQ_TASKLET_INIT, MTLK_OBJ_PTR(obj),
                      tasklet_kill, (&obj->irq_tasklet));

    MTLK_CLEANUP_STEP(mmb_drv, MMB_HW_CARD_CREATE, MTLK_OBJ_PTR(obj),
                      mtlk_hw_mmb_remove_card, (&mtlk_mmb_obj, obj->hw_api));

  MTLK_CLEANUP_END(mmb_drv, MTLK_OBJ_PTR(obj));
}

/**
 * MMB device registration procedure
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
static int __init
#else
static int
#endif
_mmb_drv_init(mtlk_mmb_drv_t *obj, mtlk_card_type_t hw_type, void *sys_dev)
{
  mtlk_hw_t *hw = NULL;
  mtlk_vap_handle_t master_vap_handle;

  MTLK_ASSERT(_known_card_type(hw_type));

  MTLK_INIT_TRY(mmb_drv, MTLK_OBJ_PTR(obj))

    MTLK_INIT_STEP_EX(mmb_drv, MMB_HW_CARD_CREATE, MTLK_OBJ_PTR(obj),
                      mtlk_hw_mmb_add_card, (&mtlk_mmb_obj),
                      obj->hw_api, NULL != obj->hw_api,
                      MTLK_ERR_UNKNOWN);

    MTLK_ASSERT(NULL != obj->hw_api->vft->get_msg_to_send);
    MTLK_ASSERT(NULL != obj->hw_api->vft->send_data);
    MTLK_ASSERT(NULL != obj->hw_api->vft->release_msg_to_send);
    MTLK_ASSERT(NULL != obj->hw_api->vft->set_prop);
    MTLK_ASSERT(NULL != obj->hw_api->vft->get_prop);
    MTLK_ASSERT(NULL != obj->hw_api->vft->get_device);

    /* get short HW handle accessor */
    hw = obj->hw_api->hw;

    MTLK_INIT_STEP_VOID(mmb_drv, MMB_IRQ_TASKLET_INIT, MTLK_OBJ_PTR(obj),
                        tasklet_init,
                        (&obj->irq_tasklet, _on_irq_tasklet, (unsigned long)hw));

    MTLK_INIT_STEP(mmb_drv, MMB_CCR_INIT, MTLK_OBJ_PTR(obj),
                   mtlk_ccr_init, (&obj->ccr, hw_type, obj, hw, sys_dev));

    MTLK_INIT_STEP_EX(mmb_drv, MMB_VAP_MANAGER_CREATE, MTLK_OBJ_PTR(obj),
                      mtlk_vap_manager_create,
                        (obj->hw_api, _work_mode_get(hw)),
                      obj->vap_manager, NULL != obj->vap_manager,
                      MTLK_ERR_UNKNOWN);

    MTLK_INIT_STEP(mmb_drv, MMB_HW_CARD_INIT, MTLK_OBJ_PTR(obj),
                   mtlk_hw_mmb_init_card, (hw, &obj->ccr, _mmb_mem_pas_get(obj),
                                           obj->vap_manager));

    MTLK_INIT_STEP(mmb_drv, MMB_MASTER_VAP_CREATE, MTLK_OBJ_PTR(obj),
                   mtlk_vap_manager_create_vap,
                     (obj->vap_manager, &master_vap_handle, hw_type, MTLK_MASTER_VAP_ID));

    MTLK_INIT_STEP(mmb_drv, MMB_TXMM_INIT, MTLK_OBJ_PTR(obj),
                   mtlk_mmb_txmm_init, (hw, master_vap_handle));

    MTLK_INIT_STEP(mmb_drv, MMB_TXDM_INIT, MTLK_OBJ_PTR(obj),
                   mtlk_mmb_txdm_init, (hw, master_vap_handle));

#ifndef MTCFG_USE_INTERRUPT_POLLING
    MTLK_INIT_STEP(mmb_drv, MMB_IRQ_START, MTLK_OBJ_PTR(obj),
                   _mmb_drv_irq_start, (obj));
#else /* MTCFG_USE_INTERRUPT_POLLING */   
    ILOG2_S("%s IRQ polling mode", DRV_NAME);
#endif /* not MTCFG_USE_INTERRUPT_POLLING */

    MTLK_INIT_STEP(mmb_drv, MMB_CARD_START, MTLK_OBJ_PTR(obj),
                   mtlk_hw_mmb_start_card, (hw));

    MTLK_INIT_STEP(mmb_drv, MMB_VAP_MANAGER_PREPARE_VAPS, MTLK_OBJ_PTR(obj),
                   mtlk_vap_manager_prepare_vaps, (obj->vap_manager));

    MTLK_INIT_STEP(mmb_drv, MMB_VAP_MANAGER_PREPARE_START, MTLK_OBJ_PTR(obj),
                   mtlk_vap_manager_prepare_start,
                   (obj->vap_manager,
                    HANDLE_T(mtlk_hw_mmb_get_txmm(hw)),
                    HANDLE_T(mtlk_hw_mmb_get_txdm(hw))));

    MTLK_INIT_STEP_VOID(mmb_drv, MMB_MAC_EVENTS_START, MTLK_OBJ_PTR(obj),
                        MTLK_NOACTION, ());

    MTLK_INIT_STEP(mmb_drv, MMB_VAP_START, MTLK_OBJ_PTR(obj),
                   mtlk_vap_start, (master_vap_handle, MTLK_VAP_MASTER_INTERFACE));

    MTLK_INIT_STEP_VOID(mmb_drv, MMB_CORE_PREPARE_STOP, MTLK_OBJ_PTR(obj),
                        MTLK_NOACTION, ());

  MTLK_INIT_FINALLY(mmb_drv, MTLK_OBJ_PTR(obj))    
  MTLK_INIT_RETURN(mmb_drv, MTLK_OBJ_PTR(obj), _mmb_drv_cleanup, (obj))
}

#ifdef MTCFG_BUS_PCI_PCIE

/**
 * Entry point for PCI device registration procedure
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
static int __init
#else
static int
#endif
_pci_probe(struct pci_dev *dev, const struct pci_device_id *ent)
{
  mtlk_mmb_drv_t *obj = NULL;

  mtlk_fast_mem_print_info();

  ILOG2_S("PCI %s Initialization", pci_name(dev));

  if (no_more_ifaces) {
    ELOG_S("Skipping PCI %s initialization - previous interface startup failed", pci_name(dev));
    return -ENODEV;
  }

  obj = mtlk_fast_mem_alloc(MTLK_FM_USER_MMBDRV, sizeof(*obj));
  if (NULL == obj)
    return -ENOMEM;

  memset(obj, 0, sizeof(*obj));

  if(MTLK_ERR_OK != _mmb_drv_init(obj, (mtlk_card_type_t)ent->driver_data, dev)) 
  {
    no_more_ifaces = TRUE;
    mtlk_fast_mem_free(obj);
    return -ENODEV;
  }

  ILOG2_S("PCI %s Initialization finished", pci_name(dev));

  return 0;
}

/**
 * Entry point for PCI device deletion procedure
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
static void __devexit
#else
static void
#endif
_pci_remove(struct pci_dev *dev)
{
  mtlk_mmb_drv_t *obj = pci_get_drvdata(dev);

  if (NULL == obj)
    return;

  ILOG2_S("PCI %s CleanUp", pci_name(dev));

  _mmb_drv_cleanup(obj);

  mtlk_fast_mem_free(obj);

  ILOG2_S("PCI %s CleanUp finished", pci_name(dev));
}

#endif /* MTCFG_BUS_PCI_PCIE */

#ifdef MTCFG_BUS_AHB

/**
 * Entry point for AHB device registration procedure
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
static int __init
#else
static int
#endif
_ahb_probe(struct platform_device *pdev)
{
  mtlk_mmb_drv_t *obj = NULL;

  ILOG2_S("AHB %s Initialization", dev_name(&pdev->dev));

  if (no_more_ifaces) {
    ELOG_S("Skipping AHB %s initialization - previous interface startup failed", dev_name(&pdev->dev));
    return -ENODEV;
  }

  obj = mtlk_fast_mem_alloc(MTLK_FM_USER_MMBDRV, sizeof(*obj));
  if (NULL == obj)
    return -ENOMEM;

  memset(obj, 0, sizeof(*obj));

  if(MTLK_ERR_OK != _mmb_drv_init(obj, MTLK_CARD_AHBG35, pdev)) 
  {
    no_more_ifaces = TRUE;
    mtlk_fast_mem_free(obj);
    return -ENODEV;
  }

  ILOG2_S("AHB %s Initialization finished", dev_name(&pdev->dev));

  return 0;
}

/**
 * Entry point for AHB device deletion procedure
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
static int __devexit
#else
static int
#endif
_ahb_remove(struct platform_device *pdev)
{
  mtlk_mmb_drv_t *obj = platform_get_drvdata(pdev);

  if (NULL == obj)
    return 0;

  ILOG2_S("AHB %s CleanUp", dev_name(&pdev->dev));

  _mmb_drv_cleanup(obj);

  mtlk_fast_mem_free(obj);

  ILOG2_S("AHB %s CleanUp finished", dev_name(&pdev->dev));

  return 0;
}

#endif /* MTCFG_BUS_AHB */

/**************************************************************
 *  Main MMB driver registration                              *
 **************************************************************/

#ifdef MTCFG_BUS_PCI_PCIE

static struct pci_device_id mmb_pci_tbl[] = {
#ifdef MTCFG_LINDRV_HW_PCIE
  { MTLK_VENDOR_ID,     WAVE_III_PCIE_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, MTLK_CARD_PCIE},
#endif

#ifdef MTCFG_LINDRV_HW_PCIG3
  { MTLK_VENDOR_ID,     WAVE_III_PCI_DEVICE_ID, PCI_ANY_ID, PCI_ANY_ID, 0, 0, MTLK_CARD_PCIG3},
#endif
  { 0,}
};

MODULE_DEVICE_TABLE(pci, mmb_pci_tbl);

static struct pci_driver mmb_pci_driver = {
  .name     = DRV_NAME,
  .id_table = mmb_pci_tbl,
  .probe    = _pci_probe,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
  .remove   = __devexit_p(_pci_remove),
#else
  .remove   = _pci_remove,
#endif
};
#endif /* MTCFG_BUS_PCI_PCIE */

#ifdef MTCFG_BUS_AHB
static struct platform_driver mmb_ahb_driver = {
  .probe    = _ahb_probe,
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
  .remove   = __devexit_p(_ahb_remove),
#else
  .remove   = _ahb_remove,
#endif
  .driver   = {
    .name   = DRV_NAME,
  },
};
#endif /* MTCFG_BUS_AHB */

struct mtlk_drv_state
{
  int os_res;
  int init_res;
  int osal_init_res;
  MTLK_DECLARE_INIT_STATUS;
};

static struct mtlk_drv_state drv_state = {0};

MTLK_INIT_STEPS_LIST_BEGIN(bus_drv_mod)
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv_mod, DRV_DFG_INIT)
#ifdef MTCFG_BUS_AHB
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv_mod, DRV_AHB_REGISTER)
#endif /* MTCFG_BUS_AHB */
#ifdef MTCFG_BUS_PCI_PCIE
  MTLK_INIT_STEPS_LIST_ENTRY(bus_drv_mod, DRV_PCI_REGISTER)
#endif /* MTCFG_BUS_PCI_PCIE */
MTLK_INIT_INNER_STEPS_BEGIN(bus_drv_mod)
MTLK_INIT_STEPS_LIST_END(bus_drv_mod);

static void
__exit_module (void)
{
  MTLK_CLEANUP_BEGIN(bus_drv_mod, MTLK_OBJ_PTR(&drv_state))
#ifdef MTCFG_BUS_PCI_PCIE
    MTLK_CLEANUP_STEP(bus_drv_mod, DRV_PCI_REGISTER, MTLK_OBJ_PTR(&drv_state), 
                      pci_unregister_driver, (&mmb_pci_driver))
#endif /* MTCFG_BUS_PCI_PCIE */
#ifdef MTCFG_BUS_AHB
    MTLK_CLEANUP_STEP(bus_drv_mod, DRV_AHB_REGISTER, MTLK_OBJ_PTR(&drv_state), 
                      platform_driver_unregister, (&mmb_ahb_driver));
#endif /* MTCFG_BUS_AHB */
    MTLK_CLEANUP_STEP(bus_drv_mod, DRV_DFG_INIT, MTLK_OBJ_PTR(&drv_state),
                      mtlk_dfg_cleanup, ());
  MTLK_CLEANUP_END(bus_drv_mod, MTLK_OBJ_PTR(&drv_state))
}

static int __init
__init_module (void)
{
  log_osdep_reset_levels(debug);

  MTLK_INIT_TRY(bus_drv_mod, MTLK_OBJ_PTR(&drv_state))
    MTLK_INIT_STEP(bus_drv_mod, DRV_DFG_INIT, MTLK_OBJ_PTR(&drv_state),
                   mtlk_dfg_init, ());
#ifdef MTCFG_BUS_AHB
    MTLK_INIT_STEP_EX(bus_drv_mod, DRV_AHB_REGISTER, MTLK_OBJ_PTR(&drv_state),
                      platform_driver_register, (&mmb_ahb_driver),
                      drv_state.os_res, 0 == drv_state.os_res, MTLK_ERR_NO_RESOURCES);
#endif /* MTCFG_BUS_AHB */
#ifdef MTCFG_BUS_PCI_PCIE
  #if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22) 
    #define pci_register_driver pci_module_init
  #endif
    MTLK_INIT_STEP_EX(bus_drv_mod, DRV_PCI_REGISTER, MTLK_OBJ_PTR(&drv_state), 
                      pci_register_driver, (&mmb_pci_driver),
                      drv_state.os_res, 0 == drv_state.os_res, MTLK_ERR_NO_RESOURCES);
#endif /* MTCFG_BUS_PCI_PCIE */
  MTLK_INIT_FINALLY(bus_drv_mod, MTLK_OBJ_PTR(&drv_state))
  MTLK_INIT_RETURN(bus_drv_mod, MTLK_OBJ_PTR(&drv_state), __exit_module, ())
}

/**
 * Main entry point for MMB driver
 */
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,8,0)
static int __init
#else
static int
#endif
_init_module (void)
{
  drv_state.osal_init_res = mtlk_osal_init();
  if (MTLK_ERR_OK != drv_state.osal_init_res) 
    return -ENODEV;

  drv_state.init_res = __init_module();
  return (drv_state.init_res == MTLK_ERR_OK) ? 0 : drv_state.os_res;
}

/**
 * Main termination point for MMB driver
 */
static void __exit
_exit_module (void)
{
  ILOG2_V("Cleanup");
  if (MTLK_ERR_OK == drv_state.init_res) {
    /* Call cleanup only if init succeeds, 
     * otherwise it will be called by macros on init itself 
     */
    __exit_module();
  }

  if (MTLK_ERR_OK == drv_state.osal_init_res) 
  {
    mtlk_osal_cleanup();
  }
}

/* Register entry and termination pints for MMB driver */
module_init(_init_module);
module_exit(_exit_module);


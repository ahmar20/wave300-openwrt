/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/* $Id$ */

#if !defined(SAFE_PLACE_TO_INCLUDE_MTLK_G35_CCR_DECLS)
#error "You shouldn't include this file directly!"
#endif /* SAFE_PLACE_TO_INCLUDE_MTLK_G35_CCR_DECLS */
#undef SAFE_PLACE_TO_INCLUDE_MTLK_G35_CCR_DECLS

#if defined(MTCFG_PLATFORM_UGW_WAVE400) || defined (MTCFG_PLATFORM_GEN35FPGA)
#define MTLK_G35_NPU
#endif

#include "shram_ex.h"
#include "g3shram_ex.h"
#ifndef MTLK_G35_NPU
#include "pcishram_ex.h"
#endif

struct g35_pas_map {
  unsigned char filler1[0x00200000];          /* 0x0 */
  struct {                                    
    unsigned char   pac_filler1[0x300];       /* 0x0,200,000 */
    volatile uint32 rx_control;               /* 0x0,200,300 */
  } PAC;

  unsigned char filler2[0x434];               /* 0x200,304 */
  volatile uint32 tsf_timer_low;              /* 0x200,738 */
  volatile uint32 tsf_timer_high;             /* 0x200,73C */
  unsigned char filler3[0x1F8C0];             /* 0x200,740 */

  struct {                                    
    unsigned char   rab_filler1[0x08];        /* 0x0,220,000 */
    volatile uint32 upi_interrupt;            /* 0x0,220,008 */
    volatile uint32 upi_interrupt_clear;      /* 0x0,220,00C */
    volatile uint32 lpi_interrupt;            /* 0x0,220,010 */
    volatile uint32 lpi_interrupt_clear;      /* 0x0,220,014 */
    volatile uint32 phi_interrupt;            /* 0x0,220,018 */
    volatile uint32 phi_interrupt_clear;      /* 0x0,220,01C */
    unsigned char   rab_filler2[0x24];        /* 0x0,220,020 */
    volatile uint32 secure_write_register;    /* 0x0,220,044 */
    unsigned char   rab_filler3[0xC];         /* 0x0,220,048 */
    volatile uint32 enable_phi_interrupt;     /* 0x0,220,054 */
    volatile uint32 cpu_control_register;     /* 0x0,220,058 */
  } RAB;
  unsigned char filler4[0xFFA4];              /* 0x0,220,05C */
  struct {
    volatile uint32 dynamic;                  /* 0x0,230,000 */
    volatile uint32 stop;                     /* 0x0,230,004 */
    volatile uint32 extend_regclk;            /* 0x0,230,008 */
    volatile uint32 extend_smcclk;            /* 0x0,230,00C */
    volatile uint32 follow_regclk;            /* 0x0,230,010 */
  } CLC;
  unsigned char filler5[0x4FFEC];             /* 0x0,230,014 */
  struct {
    unsigned char   sys_if_filler1[0x60];     /* 0x0,280,000 */
    volatile uint32 system_info;              /* 0x0,280,060 */
    unsigned char   sys_if_filler2[0x70];     /* 0x0,280,064 */
    volatile uint32 m4k_rams_rm;              /* 0x0,280,0D4 */
    volatile uint32 iram_rm;                  /* 0x0,280,0D8 */
    volatile uint32 eram_rm;                  /* 0x0,280,0DC */
    volatile uint32 bist_red_fix_select;      /* 0x0,280,0E0 */
    unsigned char   sys_if_filler3[0x8];      /* 0x0,280,0E4 */
    volatile uint32 red_fix_load_value;       /* 0x0,280,0EC */
  } CPU_SYS_IF;
  unsigned char filler6[0x6FF10];             /* 0x0,280,0F0 */
  struct {
    unsigned char   htext_filler1[0x14];      /* 0x0,2F0,000 */
    volatile uint32 bist_result;              /* 0x0,2F0,014 */
    volatile uint32 door_bell;                /* 0x0,2F0,018 */
    unsigned char   htext_filler2[0x0DC];     /* 0x0,2F0,01C */
    volatile uint32 htext_offset_f8;          /* 0x0,2F0,0F8 */
    unsigned char   htext_filler3[0x064];     /* 0x0,2F0,0FC */
    volatile uint32 start_bist;               /* 0x0,2F0,160 */
    unsigned char   htext_filler4[0x8];       /* 0x0,2F0,164 */
    volatile uint32 endian_swap_control;      /* 0x0,2F0,16C */
    unsigned char   htext_filler5[0x8];       /* 0x0,2F0,170 */
    volatile uint32 red_fix_load_value;       /* 0x0,2F0,178 */
    unsigned char   htext_filler6[0x20];      /* 0x0,2F0,17C */
    volatile uint32 shram_fix_vector_sel;     /* 0x0,2F0,19C */
    unsigned char   htext_filler7[0x20];      /* 0x0,2F0,1A0 */
    volatile uint32 ahb_arb_bbcpu_page_reg;   /* 0x0,2F0,1C0 */
    unsigned char   htext_filler8[0x00C];     /* 0x0,2F0,1C4 */
    volatile uint32 host_irq_status;          /* 0x0,2F0,1D0 */
    volatile uint32 host_irq_mask;            /* 0x0,2F0,1D4 */
    unsigned char   htext_filler9[0x08];      /* 0x0,2F0,1D8 */
    volatile uint32 host_irq;                 /* 0x0,2F0,1E0 */
  } HTEXT;

  unsigned char filler7[0xFE1C];              /* 0x0,2F0,1E4 */

  struct {
    unsigned char td_filler1[0x8040];         /* 0x0,300,000 */
    volatile uint32 phy_progmodel_ver;        /* 0x0,308,040 */
  } TD;

#ifdef MTLK_G35_NPU
  unsigned char filler8[0xEB7FBC];            /* 0x0,308,044 */
  struct {
    unsigned char   shb_filler1[0x10];        /* 0x1,1C0,000 */
    volatile uint32 door_bell;                /* 0x1,1C0,010 */
    unsigned char   shb_filler2[0x04];        /* 0x1,1C0,014 */
    volatile uint32 interrupt_clear;          /* 0x1,1C0,018 */
    volatile uint32 interrupt_enable;         /* 0x1,1C0,01C */
    unsigned char   shb_filler3[0x54];        /* 0x1,1C0,020 */
    volatile uint32 bb_ddr_offset_mb;         /* 0x1,1C0,074 */
  } SH_REG_BLOCK;
#endif
};

typedef struct
{
  struct g35_pas_map    *pas;
  unsigned char         *cpu_ddr;
  void                  *pdev;
  mtlk_mmb_drv_t        *mmb_drv;
  mtlk_hw_t             *hw;
  uint8                 current_ucpu_state;
  uint8                 next_boot_mode;
} _mtlk_g35_ccr_t;

/* endian_swap_control bits */
#define G33_CPU_HTEXT_SCD_AHB_MASTER_WRITE_SWAP             (1 << 0)
#define G33_CPU_HTEXT_SCD_AHB_MASTER_WRITE_FULL_BYTE_SWAP   (1 << 1)
#define G33_CPU_HTEXT_SCD_AHB_MASTER_READ_SWAP              (1 << 2)
#define G33_CPU_HTEXT_SCD_AHB_MASTER_READ_FULL_BYTESWAP     (1 << 3)
#define G33_CPU_HTEXT_DMA_AHB_MASTER_WRITE_SWAP             (1 << 4)
#define G33_CPU_HTEXT_DMA_AHB_MASTER_WRITE_FULL_BYTE_SWAP   (1 << 5)
#define G33_CPU_HTEXT_DMA_AHB_MASTER_READ_SWAP              (1 << 6)
#define G33_CPU_HTEXT_DMA_AHB_MASTER_READ_FULL_BYTESWAP     (1 << 7)
#define G33_CPU_HTEXT_SHRAM_AHB_SLAVE_WRITE_SWAP            (1 << 8)
#define G33_CPU_HTEXT_SHRAM_AHB_SLAVE_WRITE_FULL_BYTE_SWAP  (1 << 9)
#define G33_CPU_HTEXT_SHRAM_AHB_SLAVE_READ_SWAP             (1 << 10)
#define G33_CPU_HTEXT_SHRAM_AHB_SLAVE_READ_FULL_BYTESWAP    (1 << 11)
#define G33_CPU_HTEXT_PCIH_AHB_SLAVE_WRITE_SWAP             (1 << 12)
#define G33_CPU_HTEXT_PCIH_AHB_SLAVE_WRITE_FULL_BYTE_SWAP   (1 << 13)
#define G33_CPU_HTEXT_PCIH_AHB_SLAVE_READ_SWAP              (1 << 14)
#define G33_CPU_HTEXT_PCIH_AHB_SLAVE_READ_FULL_BYTESWAP     (1 << 15)

#define G33_CPU_SYS_IF_SYSTEM_INFO_CHIPID_OFFSET            (0x00000000)
#define G33_CPU_SYS_IF_SYSTEM_INFO_CHIPID_MASK              (0x0000FFFF)

#define MTLK_G35_CPU_IRAM_START (0x240000)
#define MTLK_G35_CPU_IRAM_SIZE  (256*1024)

#define MTLK_G35_CPU_DDR_OFFSET  MTLK_G35_CPU_IRAM_SIZE
#define MTLK_G35_CPU_DDR_SIZE    (1024*1024 - MTLK_G35_CPU_IRAM_SIZE)

#define MTLK_G35_TOTAL_MEMORY_SIZE (MTLK_G35_CPU_IRAM_SIZE + \
                                    MTLK_G35_CPU_DDR_SIZE)

/* Multi Processor System */
#define MTLK_G35_MPS_BASE_ADDR          (0xBF107000)                        /* Base Address */
#define MTLK_G35_MPS_AFE_CTRL           (MTLK_G35_MPS_BASE_ADDR + 0x0360)   /* AFE Fuse Control Register */
#define MTLK_G35_MPS_AFE_CTRL_WLAN_TRIM (1 << 29)                           /* WLAN AFE Trimming Indication, 1: Done */

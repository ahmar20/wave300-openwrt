/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MTLK_MMB_DRV_H_INCLUDED__ 
#define __MTLK_MMB_DRV_H_INCLUDED__

/**************************************************************
 *  Export global data types                                  *
 **************************************************************/

typedef struct
{
  void* start;
  int length;
  BOOL swap;
} mtlk_cpu_mem_t;

#define FW_NAME_MAX_LEN 0x20
#define CHI_CPU_MEM_CNT 3

#define CPU_FREQ_RESERVED MAX_UINT32

typedef struct
{
  char fname[FW_NAME_MAX_LEN];
  mtlk_cpu_mem_t mem[CHI_CPU_MEM_CNT];
} mtlk_fw_info_t;

typedef struct _mtlk_mmb_drv_t mtlk_mmb_drv_t;

/**************************************************************
 *  Export global objects                                     *
 **************************************************************/

extern int bb_cpu_ddr_mb_number;

/**************************************************************
 *  Export global interfaces                                  *
 **************************************************************/

void __MTLK_IFUNC
mtlk_mmb_drv_get_name(mtlk_mmb_drv_t *obj, char *buf, int buf_len);

struct device * __MTLK_IFUNC
mtlk_mmb_drv_get_device(mtlk_mmb_drv_t *obj);

int __MTLK_IFUNC
mtlk_mmb_drv_postpone_irq_handler(mtlk_mmb_drv_t *obj);

uint32 __MTLK_IFUNC
mtlk_mmb_drv_get_cpu_freq(mtlk_mmb_drv_t *obj);

#endif /* __MTLK_MMB_DRV_H_INCLUDED__ */

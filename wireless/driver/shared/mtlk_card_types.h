/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/* $Id$ */

#ifndef __HW_MTLK_CARD_TYPES_H__
#define __HW_MTLK_CARD_TYPES_H__

typedef enum
{
  MTLK_CARD_UNKNOWN = 0x8888, /* Just magics to avoid    */
  MTLK_CARD_FIRST = 0xABCD,   /* accidental coincidences */

#ifdef MTCFG_LINDRV_HW_PCIE
  MTLK_CARD_PCIE,
#endif
#ifdef MTCFG_LINDRV_HW_PCIG3
  MTLK_CARD_PCIG3,
#endif
#ifdef MTCFG_LINDRV_HW_AHBG35
  MTLK_CARD_AHBG35,
#endif

  MTLK_CARD_LAST
} mtlk_card_type_t;

#define MTLK_VENDOR_ID              0x1a30  
#define UNKNOWN_DEVICE_ID           0x0000
#define WAVE_III_PCI_DEVICE_ID      0x0700
#define WAVE_III_PCIE_DEVICE_ID     0x0710
#define WAVE_IV_VB_DEVICE_ID        0x0780
#define WAVE_IV_USB_DEVICE_ID       0x07A0
#define WAVE_IV_PCIE_DEVICE_ID      0x07B0
#define WAVE_IV_AR10_DEVICE_ID      0x07C0
#define WAVE_IV_GRX390_DEVICE_ID    0x07D0

typedef struct {
  uint32 id;
  char   *name;
} mtlk_chip_info_t;

#if defined(SAFE_PLACE_TO_DEFINE_CHIP_INFO)
mtlk_chip_info_t const mtlk_chip_info[] = {
  { WAVE_III_PCI_DEVICE_ID  , "wave300" /* PCI  */},
  { WAVE_III_PCIE_DEVICE_ID , "wave300" /* PCIe */},
  { WAVE_IV_AR10_DEVICE_ID  , "ar10"    /* AR10 */},
  { WAVE_IV_VB_DEVICE_ID    , "wave400" /* VB   */},
  { WAVE_IV_USB_DEVICE_ID   , "wave400" /* USB  */},
  { WAVE_IV_PCIE_DEVICE_ID  , "wave400" /* PCIE */},
  { WAVE_IV_GRX390_DEVICE_ID, "ar10"    /* GRX390, fw binary name is the same with AR10 */},
  { UNKNOWN_DEVICE_ID       , NULL      }
};
#else /* SAFE_PLACE_TO_DEFINE_CHIP_INFO */
extern mtlk_chip_info_t const mtlk_chip_info[];
#endif/* SAFE_PLACE_TO_DEFINE_CHIP_INFO */

#undef SAFE_PLACE_TO_DEFINE_CHIP_INFO

static __INLINE mtlk_chip_info_t const *
mtlk_chip_info_get(uint32 id)
{
  mtlk_chip_info_t const *p;

  for(p = mtlk_chip_info; UNKNOWN_DEVICE_ID != p->id; p++)
  {
    if (p->id == id)
      return p;
  }
  
  return NULL;
}

#endif /* __HW_MTLK_CARD_TYPES_H__ */

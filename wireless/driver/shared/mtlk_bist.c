/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "hw_mmb.h"

#define LOG_LOCAL_GID   GID_BIST
#define LOG_LOCAL_FID   0

/*****************************************************************************
* Local definitions
******************************************************************************/
#ifdef MTCFG_LINDRV_HW_AHBG35

#define MPS_OFFSET              0xBF107000

#define WLAN_REDUN_FUSE_AR10    (MPS_OFFSET + 0x0368) /* AR10:   WLAN_REDUN_FUSE_0 */
#define WLAN_REDUN_FUSE_GRX390  (MPS_OFFSET + 0x03B0) /* GRX390: PPE_SW_WLAN_FUSE_1 */

#define WLAN_FUSE_BITS_NUM      (10)
#define WLAN_FUSE_BIT_MASK      ((1 << WLAN_FUSE_BITS_NUM) - 1)

#endif /* MTCFG_LINDRV_HW_AHBG35 */

/*****************************************************************************
* Function implementation
******************************************************************************/
#ifdef MTCFG_LINDRV_HW_AHBG35
static void
_mtlk_bist_fix_vec_chunk (_mtlk_g35_ccr_t *g35_mem, uint32 read_fuse_10_bits)
{
    uint32 faulty_bit, faulty_vector, memory_encoding_bits;
    uint32 fix_vec_sel;

    faulty_bit = (read_fuse_10_bits & 0x0000001f);
    faulty_vector = (0x1 << faulty_bit);
    memory_encoding_bits = ((read_fuse_10_bits >> 5) & 0x0000001f);

    mtlk_osal_emergency_print("RAM #: %d, CLMN #: %d",
                              memory_encoding_bits, faulty_bit);

    if (memory_encoding_bits == 0x0) {
      /* Memory is not corrupted */
      return;
    }

    if (memory_encoding_bits <= 0x8) {
      /* IRAM */
      fix_vec_sel = ((0x0f + (2 * memory_encoding_bits)) & 0x1f);
      _mtlk_g35_ccr_iram_cache_fix_vec(g35_mem,
                                       fix_vec_sel,
                                       faulty_vector);
    }
    else if (memory_encoding_bits < 0xd) {
      /* SHRAM */
      fix_vec_sel = ((0xF + (2 * memory_encoding_bits)) & 0x1f);
      _mtlk_g35_ccr_shram_fix_vec(g35_mem,
                                  fix_vec_sel,
                                  faulty_vector);
    }
    else if (memory_encoding_bits < 0x11) {
      /* ICACHE */
      fix_vec_sel = (memory_encoding_bits - 0xd);
      _mtlk_g35_ccr_iram_cache_fix_vec(g35_mem,
                                       fix_vec_sel,
                                       faulty_vector);
    }
    else if (memory_encoding_bits < 0x15) {
      /* DCACHE */
      fix_vec_sel = (memory_encoding_bits - 0x9);
      _mtlk_g35_ccr_iram_cache_fix_vec(g35_mem,
                                       fix_vec_sel,
                                       faulty_vector);
    }
    else {
      /* Not supported */
      fix_vec_sel = 0;
      faulty_vector = 0;
    }

    mtlk_osal_emergency_print("vector select: 0x%02X, vector: 0x%02X", fix_vec_sel, faulty_vector);
}

/* AR10: NumOfChunks = 5.
 * Every word contains up to 3 chunks by 10 bits each.
 *   Num    Offset  StartBit
 *     1        0        0
 *     2        0       10
 *     3        0       20
 *     4        1        0
 *     5        1       10
 */
#define _FUSE_AR10_CHUNKS_TOTAL     (5)
#define _FUSE_AR10_CHUNKS_PER_WORD  (3)

static void
_mtlk_bist_fuse_check_g35_ar10 (_mtlk_g35_ccr_t *g35_mem)
{
    uint32  *fuse_mem = (uint32 *)WLAN_REDUN_FUSE_AR10;
    uint32  fuse_word, fuse_chunk;
    int     i;

    /* 1st word */
    fuse_word = *fuse_mem;
    for (i = 1; i <= _FUSE_AR10_CHUNKS_TOTAL; i++)
    {
        /* extract and check one chunk */
        fuse_chunk = fuse_word & WLAN_FUSE_BIT_MASK;
        mtlk_osal_emergency_print("e-fuse: num %d, address 0x%p, word: 0x%08X, value: 0x%03X",
                                  i, fuse_mem, fuse_word, fuse_chunk);

        if (fuse_chunk) {
            _mtlk_bist_fix_vec_chunk (g35_mem, fuse_chunk);
        }

         /* shift current word OR read next word */
        if (0 == (i % _FUSE_AR10_CHUNKS_PER_WORD)) {
            fuse_word = *(++fuse_mem);
        } else {
            fuse_word >>= WLAN_FUSE_BITS_NUM;
        }
    }
}

/* GRX390: NumOfChunks = 6.
 *  One dedicated word for each chunk of 10 bits, start bit always 0.
 *  First 5 chunks are shared between WLAN and PPE, thus bit 18 should be checked for 0.
 *  This is an indication that the rest of the bits indicate a fix related to WLAN.
 */
#define _FUSE_GRX390_CHUNKS_WLAN_PPE  (5)
#define _FUSE_GRX390_CHUNKS_TOTAL     (6)
#define _FUSE_GRX390_FLAG_BIT         (18)

static void
_mtlk_bist_fuse_check_g35_grx390 (_mtlk_g35_ccr_t *g35_mem)
{
    uint32  *fuse_mem = (uint32 *)WLAN_REDUN_FUSE_GRX390;
    uint32  fuse_word, fuse_chunk;
    int     i;

    for (i = 1; i <= _FUSE_GRX390_CHUNKS_TOTAL; i++, fuse_mem++)
    {
        fuse_word = *fuse_mem;

        /* extract and check a chunk */
        fuse_chunk = fuse_word & WLAN_FUSE_BIT_MASK;
        mtlk_osal_emergency_print("e-fuse: num %d, address 0x%p, word: 0x%08X, value: 0x%03X",
                                  i, fuse_mem, fuse_word, fuse_chunk);

        if ((i > _FUSE_GRX390_CHUNKS_WLAN_PPE) ||
            (0 == (1u & (fuse_word >> _FUSE_GRX390_FLAG_BIT))))
        {
            if (fuse_chunk) {
                _mtlk_bist_fix_vec_chunk (g35_mem, fuse_chunk);
            }
        }
    }
}

static void
_mtlk_bist_fuse_check_g35 (mtlk_ccr_t *ccr)
{
    _mtlk_g35_ccr_t *g35_mem = &ccr->mem.g35;
    uint32          chip_id  = ccr->chip_info->id;

    mtlk_osal_emergency_print("ChipID: 0x%04X", chip_id);

    switch (chip_id)
    {
    case WAVE_IV_AR10_DEVICE_ID:
        _mtlk_bist_fuse_check_g35_ar10 (g35_mem);
        break;

    case WAVE_IV_GRX390_DEVICE_ID:
        _mtlk_bist_fuse_check_g35_grx390 (g35_mem);
        break;

    default:    /* should not be here */
        break;  /* nothing */
    }
}
#endif /* MTCFG_LINDRV_HW_AHBG35 */

int __MTLK_IFUNC
mtlk_bist_read_and_write (mtlk_ccr_t *ccr)
{
  CARD_SELECTOR_START(ccr->hw_type)
    IF_CARD_PCIG3 ( );
    IF_CARD_PCIE  ( );
    IF_CARD_AHBG35( _mtlk_bist_fuse_check_g35(ccr) );
  CARD_SELECTOR_END();

  return MTLK_ERR_OK;
}

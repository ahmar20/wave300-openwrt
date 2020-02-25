/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id:
 *
 * 
 *
 * Shared Auxiliary routines
 *
 */

#ifndef _MTLK_AUX_H
#define _MTLK_AUX_H

#include "mhi_umi.h"
#include "mhi_mib_id.h"
#include "core.h"

/*
 * Driver-only network mode.
 * Should be used on STA for dual-band scan
 */
#define NETWORK_11ABG_MIXED  NUM_OF_NETWORK_MODES
#define NETWORK_11ABGN_MIXED (NUM_OF_NETWORK_MODES + 1)
#define NETWORK_NONE (NUM_OF_NETWORK_MODES + 2)

#define MAX_SUPPORTED_RATES 8
#define DEFAULT_RATESET (-1)

enum preambleType
{
  PREAMBLE_LONG = 0,
  PREAMBLE_SHORT,
  PREAMBLE_MAX
};

enum userRateBit
{
  USR_RATE_1 = 0,
  USR_RATE_2,
  USR_RATE_5_5,
  USR_RATE_11,
  USR_RATE_6,
  USR_RATE_9,
  USR_RATE_12,
  USR_RATE_18,
  USR_RATE_24,
  USR_RATE_36,
  USR_RATE_48,
  USR_RATE_54,
  USR_RATE_MAX
};

enum userRateMask
{
  USR_RATE_MSK_NO  = 0,
  USR_RATE_MSK_1   = (1 << USR_RATE_1  ),
  USR_RATE_MSK_2   = (1 << USR_RATE_2  ),
  USR_RATE_MSK_5_5 = (1 << USR_RATE_5_5),
  USR_RATE_MSK_11  = (1 << USR_RATE_11 ),
  USR_RATE_MSK_6   = (1 << USR_RATE_6  ),
  USR_RATE_MSK_9   = (1 << USR_RATE_9  ),
  USR_RATE_MSK_12  = (1 << USR_RATE_12 ),
  USR_RATE_MSK_18  = (1 << USR_RATE_18 ),
  USR_RATE_MSK_24  = (1 << USR_RATE_24 ),
  USR_RATE_MSK_36  = (1 << USR_RATE_36 ),
  USR_RATE_MSK_48  = (1 << USR_RATE_48 ),
  USR_RATE_MSK_54  = (1 << USR_RATE_54 ),
};

#define DEFAULT_2_4_BASIC_RATES         USR_RATE_MSK_1 | USR_RATE_MSK_2  | USR_RATE_MSK_5_5 | USR_RATE_MSK_11
#define DEFAULT_5_2_BASIC_RATES         USR_RATE_MSK_6 | USR_RATE_MSK_12 | USR_RATE_MSK_24

#define DEFAULT_2_4_SUPPORTED_RATES_B   DEFAULT_2_4_BASIC_RATES
#define DEFAULT_2_4_SUPPORTED_RATES     DEFAULT_2_4_BASIC_RATES | USR_RATE_MSK_9 | USR_RATE_MSK_18 | USR_RATE_MSK_36 | USR_RATE_MSK_54
#define DEFAULT_5_2_SUPPORTED_RATES     DEFAULT_5_2_BASIC_RATES | USR_RATE_MSK_9 | USR_RATE_MSK_18 | USR_RATE_MSK_36 | USR_RATE_MSK_48 | USR_RATE_MSK_54

#define DEFAULT_2_4_EXTENDED_RATES_B    USR_RATE_MSK_NO
#define DEFAULT_2_4_EXTENDED_RATES      USR_RATE_MSK_6 | USR_RATE_MSK_12 | USR_RATE_MSK_24 | USR_RATE_MSK_48
#define DEFAULT_5_2_EXTENDED_RATES      USR_RATE_MSK_NO

static __INLINE int
mtlk_aux_is_11n_rate (uint8 rate)
{
  return (rate >= LM_PHY_11N_RATE_6_5 &&
          rate <= LM_PHY_11N_RATE_6_DUP);
}

uint32 __MTLK_IFUNC get_operate_rate_set (uint8 net_mode);
uint32 __MTLK_IFUNC get_basic_rateset(struct nic *nic, uint8 net_mode, BOOL use_default);
uint32 __MTLK_IFUNC get_supported_rateset(struct nic *nic, uint8 net_mode, BOOL use_default);
uint32 __MTLK_IFUNC get_extended_rateset(struct nic *nic, uint8 net_mode, BOOL use_default);
uint32 __MTLK_IFUNC user_rates_to_fw(struct nic *nic, uint32 user_bitmask);
int __MTLK_IFUNC count_rates(uint32 bitmask);
uint8 __MTLK_IFUNC get_net_mode (uint8 band, uint8 is_ht);
uint8 __MTLK_IFUNC net_mode_to_band (uint8 net_mode);
uint8 __MTLK_IFUNC net_mode_to_group (uint8 net_mode);
BOOL __MTLK_IFUNC is_ht_net_mode (uint8 net_mode);
BOOL __MTLK_IFUNC is_mixed_net_mode (uint8 net_mode);
const char * __MTLK_IFUNC net_mode_to_string (uint8 net_mode);

#endif // _MTLK_AUX_H


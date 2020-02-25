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
#include "mtlkinc.h"
#include "mtlkaux.h"
#include "eeprom.h"

#define LOG_LOCAL_GID   GID_AUX
#define LOG_LOCAL_FID   1

static const uint32 OperateRateSet[] = {
  LM_PHY_11B_RATE_MSK,                          /* 0x00007f00 - 11b@2.4GHz   */
  LM_PHY_11G_RATE_MSK,                          /* 0x00007fff - 11g@2.4GHz   */
  LM_PHY_11G_RATE_MSK | LM_PHY_11N_RATE_MSK,    /* 0xffffffff - 11n@2.4GHz   */
  LM_PHY_11G_RATE_MSK,                          /* 0x00007fff - 11bg@2.4GHz  */
  LM_PHY_11G_RATE_MSK | LM_PHY_11N_RATE_MSK,    /* 0xffffffff - 11gn@2.4GHz  */
  LM_PHY_11G_RATE_MSK | LM_PHY_11N_RATE_MSK,    /* 0xffffffff - 11bgn@2.4GHz */
  LM_PHY_11A_RATE_MSK,                          /* 0x000000ff - 11a@5.2GHz   */
  LM_PHY_11A_RATE_MSK | LM_PHY_11N_RATE_MSK,    /* 0xffff80ff - 11n@5.2GHz   */
  LM_PHY_11A_RATE_MSK | LM_PHY_11N_RATE_MSK,    /* 0xffff80ff - 11an@5.2GHz  */

  /* these ARE NOT real network modes. they are for
     unconfigured dual-band STA which may be NETWORK_11ABG_MIXED
     or NETWORK_11ABGN_MIXED */
  LM_PHY_11G_RATE_MSK,                          /* 0x00007fff, - 11abg       */
  LM_PHY_11G_RATE_MSK | LM_PHY_11N_RATE_MSK,    /* 0xffffffff, - 11abgn      */
};

static const uint32 defaultBasicRateSet[NUM_OF_NETWORK_MODES] = {
  DEFAULT_2_4_BASIC_RATES,          /* 11b@2.4GHz   */
  DEFAULT_2_4_BASIC_RATES,          /* 11g@2.4GHz   */
  DEFAULT_2_4_BASIC_RATES,          /* 11n@2.4GHz   */
  DEFAULT_2_4_BASIC_RATES,          /* 11bg@2.4GHz  */
  DEFAULT_2_4_BASIC_RATES,          /* 11gn@2.4GHz  */
  DEFAULT_2_4_BASIC_RATES,          /* 11bgn@2.4GHz */
  DEFAULT_5_2_BASIC_RATES,          /* 11a@5.2GHz   */
  DEFAULT_5_2_BASIC_RATES,          /* 11n@5.2GHz   */
  DEFAULT_5_2_BASIC_RATES,          /* 11an@5.2GHz  */
};

static const uint32 defaultSupportedRateSet[NUM_OF_NETWORK_MODES] = {
  DEFAULT_2_4_SUPPORTED_RATES_B,    /* 11b@2.4GHz   */
  DEFAULT_2_4_SUPPORTED_RATES,      /* 11g@2.4GHz   */
  DEFAULT_2_4_SUPPORTED_RATES,      /* 11n@2.4GHz   */
  DEFAULT_2_4_SUPPORTED_RATES,      /* 11bg@2.4GHz  */
  DEFAULT_2_4_SUPPORTED_RATES,      /* 11gn@2.4GHz  */
  DEFAULT_2_4_SUPPORTED_RATES,      /* 11bgn@2.4GHz */
  DEFAULT_5_2_SUPPORTED_RATES,      /* 11a@5.2GHz   */
  DEFAULT_5_2_SUPPORTED_RATES,      /* 11n@5.2GHz   */
  DEFAULT_5_2_SUPPORTED_RATES,      /* 11an@5.2GHz  */
};

static const uint32 defaultExtendedRateSet[NUM_OF_NETWORK_MODES] = {
  DEFAULT_2_4_EXTENDED_RATES_B,     /* 11b@2.4GHz   */
  DEFAULT_2_4_EXTENDED_RATES,       /* 11g@2.4GHz   */
  DEFAULT_2_4_EXTENDED_RATES,       /* 11n@2.4GHz   */
  DEFAULT_2_4_EXTENDED_RATES,       /* 11bg@2.4GHz  */
  DEFAULT_2_4_EXTENDED_RATES,       /* 11gn@2.4GHz  */
  DEFAULT_2_4_EXTENDED_RATES,       /* 11bgn@2.4GHz */
  DEFAULT_5_2_EXTENDED_RATES,       /* 11a@5.2GHz   */
  DEFAULT_5_2_EXTENDED_RATES,       /* 11n@5.2GHz   */
  DEFAULT_5_2_EXTENDED_RATES,       /* 11an@5.2GHz  */
};

static const uint32 ratesetBitmask[USR_RATE_MAX][PREAMBLE_MAX] = {
  { LM_PHY_11B_RATE_1_MSK,  LM_PHY_11B_RATE_1_MSK        },
  { LM_PHY_11B_RATE_2_MSK,  LM_PHY_11B_RATE_2_SHORT_MSK  },
  { LM_PHY_11B_RATE_5_MSK,  LM_PHY_11B_RATE_5_SHORT_MSK  },
  { LM_PHY_11B_RATE_11_MSK, LM_PHY_11B_RATE_11_SHORT_MSK },
  { LM_PHY_11A_RATE_6_MSK,  LM_PHY_11A_RATE_6_MSK        },
  { LM_PHY_11A_RATE_9_MSK,  LM_PHY_11A_RATE_9_MSK        },
  { LM_PHY_11A_RATE_12_MSK, LM_PHY_11A_RATE_12_MSK       },
  { LM_PHY_11A_RATE_18_MSK, LM_PHY_11A_RATE_18_MSK       },
  { LM_PHY_11A_RATE_24_MSK, LM_PHY_11A_RATE_24_MSK       },
  { LM_PHY_11A_RATE_36_MSK, LM_PHY_11A_RATE_36_MSK       },
  { LM_PHY_11A_RATE_48_MSK, LM_PHY_11A_RATE_48_MSK       },
  { LM_PHY_11A_RATE_54_MSK, LM_PHY_11A_RATE_54_MSK       },
};

uint32 __MTLK_IFUNC
get_operate_rate_set (uint8 net_mode)
{
  ASSERT(net_mode < NETWORK_NONE);
  return OperateRateSet[net_mode];
}

static uint32
get_rateset(struct nic *nic, uint8 net_mode, BOOL use_default, int pdbEntry, const uint32 *defaultRateSet)
{
  uint32 rateset = DEFAULT_RATESET;

  ASSERT(net_mode < NUM_OF_NETWORK_MODES);

  if (FALSE == use_default)
  {
    rateset = MTLK_CORE_PDB_GET_INT(nic, pdbEntry);
  }

  if (DEFAULT_RATESET == rateset)
  {
    rateset = defaultRateSet[net_mode];
  }

  return rateset;
}

uint32 __MTLK_IFUNC
get_basic_rateset(struct nic *nic, uint8 net_mode, BOOL use_default)
{
  return get_rateset(nic, net_mode, use_default, PARAM_DB_CORE_BASIC_RATE_SET, defaultBasicRateSet);
}

uint32 __MTLK_IFUNC
get_supported_rateset(struct nic *nic, uint8 net_mode, BOOL use_default)
{
  return get_rateset(nic, net_mode, use_default, PARAM_DB_CORE_SUPPORTED_RATE_SET, defaultSupportedRateSet);
}

uint32 __MTLK_IFUNC
get_extended_rateset(struct nic *nic, uint8 net_mode, BOOL use_default)
{
  return get_rateset(nic, net_mode, use_default, PARAM_DB_CORE_EXTENDED_RATE_SET, defaultExtendedRateSet);
}

uint32 __MTLK_IFUNC
user_rates_to_fw(struct nic *nic, uint32 user_bitmask)
{
  uint32 fw_bitmask = 0;
  int i;
  int preamble = PREAMBLE_LONG;

  if (MTLK_CORE_PDB_GET_INT(mtlk_core_get_master(nic), PARAM_DB_CORE_SHORT_PREAMBLE))
  {
    preamble = PREAMBLE_SHORT;
  }

  for (i=0; i<USR_RATE_MAX; i++)
  {
    if (user_bitmask & (1 << i))
    {
      fw_bitmask |= ratesetBitmask[i][preamble];
    }
  }

  return fw_bitmask;
}

int __MTLK_IFUNC
count_rates(uint32 bitmask)
{
  int count = 0;
  int i;

  for (i=0; i<USR_RATE_MAX; i++)
  {
    count += (bitmask & 1); /* lowest bit test */
    bitmask >>= 1;
  }

  return count;
}

/*
 * It should be noted, that
 * get_net_mode(net_mode_to_band(net_mode), is_ht_net_mode(net_mode)) != net_mode
 * because there are {ht, !ht}x{2.4GHz, 5.2GHz, both} == 6 combinations,
 * and there are 11 Network Modes.
 */
uint8 __MTLK_IFUNC get_net_mode (uint8 band, uint8 is_ht)
{
  switch (band) {
  case MTLK_HW_BAND_2_4_GHZ:
    if (is_ht)
      return NETWORK_11BGN_MIXED;
    else
      return NETWORK_11BG_MIXED;
  case MTLK_HW_BAND_5_2_GHZ:
    if (is_ht)
      return NETWORK_11AN_MIXED;
    else
      return NETWORK_11A_ONLY;
  case MTLK_HW_BAND_BOTH:
    if (is_ht)
      return NETWORK_11ABGN_MIXED;
    else
      return NETWORK_11ABG_MIXED;
  default:
    break;
  }

  ASSERT(0);

  return 0; /* just fake cc */
}

uint8 __MTLK_IFUNC net_mode_to_band (uint8 net_mode)
{
  switch (net_mode) {
  case NETWORK_11BG_MIXED: /* walk through */
  case NETWORK_11BGN_MIXED: /* walk through */
  case NETWORK_11B_ONLY: /* walk through */
  case NETWORK_11GN_MIXED: /* walk through */
  case NETWORK_11G_ONLY: /* walk through */
  case NETWORK_11N_2_4_ONLY:
    return MTLK_HW_BAND_2_4_GHZ;
  case NETWORK_11AN_MIXED: /* walk through */
  case NETWORK_11A_ONLY: /* walk through */
  case NETWORK_11N_5_ONLY:
    return MTLK_HW_BAND_5_2_GHZ;
  case NETWORK_11ABG_MIXED: /* walk through */
  case NETWORK_11ABGN_MIXED:
    return MTLK_HW_BAND_BOTH;
  default:
    break;
  }

  ASSERT(0);

  return 0; /* just fake cc */
}

/* Groups of compatible network modes according to firmware team.
 * These modes can co-exist within different VAP of single AP.
 * Different bands are in separate groups too.
 * Exact return value makes no sense. It must be different for each group. */
uint8 __MTLK_IFUNC net_mode_to_group (uint8 net_mode)
{
  switch (net_mode) {
  case NETWORK_11B_ONLY:
  case NETWORK_11N_2_4_ONLY:
    return 0;
  case NETWORK_11BG_MIXED:
  case NETWORK_11BGN_MIXED:
  case NETWORK_11GN_MIXED:
  case NETWORK_11G_ONLY:
    return 1;
  case NETWORK_11AN_MIXED:
  case NETWORK_11A_ONLY:
  case NETWORK_11N_5_ONLY:
    return 2;
  case NETWORK_11ABG_MIXED:
  case NETWORK_11ABGN_MIXED:
    return 3;
  }

  ASSERT(0);

  /* In case of error, return value other than any correct one */
  return 255;
}

BOOL __MTLK_IFUNC is_ht_net_mode (uint8 net_mode)
{
  switch (net_mode) {
  case NETWORK_11ABG_MIXED: /* walk through */
  case NETWORK_11A_ONLY: /* walk through */
  case NETWORK_11BG_MIXED: /* walk through */
  case NETWORK_11B_ONLY: /* walk through */
  case NETWORK_11G_ONLY:
    return FALSE;
  case NETWORK_11ABGN_MIXED: /* walk through */
  case NETWORK_11AN_MIXED: /* walk through */
  case NETWORK_11BGN_MIXED: /* walk through */
  case NETWORK_11GN_MIXED: /* walk through */
  case NETWORK_11N_2_4_ONLY: /* walk through */
  case NETWORK_11N_5_ONLY:
    return TRUE;
  default:
    break;
  }

  ASSERT(0);

  return FALSE; /* just fake cc */
}

BOOL __MTLK_IFUNC is_mixed_net_mode (uint8 net_mode)
{
  switch (net_mode) {
  case NETWORK_11ABG_MIXED:
  case NETWORK_11BG_MIXED:
  case NETWORK_11ABGN_MIXED:
  case NETWORK_11AN_MIXED:
  case NETWORK_11BGN_MIXED:
  case NETWORK_11GN_MIXED:
    return TRUE;
  case NETWORK_11B_ONLY:
  case NETWORK_11G_ONLY:
  case NETWORK_11A_ONLY:
  case NETWORK_11N_2_4_ONLY:
  case NETWORK_11N_5_ONLY:
    return FALSE;
  default:
    break;
  }

  ASSERT(0);

  return FALSE; /* just fake cc */
}

const char * __MTLK_IFUNC
net_mode_to_string (uint8 net_mode)
{ 
  switch (net_mode) {
  case NETWORK_11B_ONLY:
    return "802.11b";
  case NETWORK_11G_ONLY:
    return "802.11g";
  case NETWORK_11N_2_4_ONLY:
    return "802.11n(2.4)";
  case NETWORK_11BG_MIXED:
    return "802.11bg";
  case NETWORK_11GN_MIXED:
    return "802.11gn";
  case NETWORK_11BGN_MIXED:
    return "802.11bgn"; 
  case NETWORK_11A_ONLY:
    return "802.11a";
  case NETWORK_11N_5_ONLY:
    return "802.11n(5.2)";
  case NETWORK_11AN_MIXED:
    return "802.11an";
  case NETWORK_11ABG_MIXED:
    return "802.11abg";
  case NETWORK_11ABGN_MIXED:
    return "802.11abgn";
  }

  return "invalid mode";
}


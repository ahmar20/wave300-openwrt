/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
 * 
 *
 * Core's parameters DB definitions
 *
 * Written by: Grygorii Strashko
 *
 */

#ifndef __CORE_PDB_DEF_H_
#define __CORE_PDB_DEF_H_

#include "mtlk_param_db.h"
#include "channels.h"
#include "mtlkaux.h"
#include "core_priv.h"
#include "mtlk_coreui.h"

#define MTLK_IDEFS_ON
#include "mtlkidefs.h"

static const uint32 mtlk_core_initial_zero_int = 0;

static const uint32  mtlk_core_initial_bridge_mode = 0;
static const uint32  mtlk_core_initial_ap_forwarding = 1;
static const uint32  mtlk_core_initial_reliable_mcast = 1;

static const uint8   mtlk_core_initial_mac_addr[ETH_ALEN] = {0};
static const uint8   mtlk_core_initial_bssid[ETH_ALEN] = {0};

static const uint32  mtlk_core_initial_coex_cfg = FALSE;
static const uint32  mtlk_core_initial_sta_force_spectrum_mode = SPECTRUM_AUTO;
static const uint32  mtlk_core_initial_spectrum_mode = SPECTRUM_40MHZ;
static const uint32  mtlk_core_initial_user_spectrum_mode = SPECTRUM_40MHZ;
static const uint32  mtlk_core_initial_bonding_mode = ALTERNATE_LOWER;
static const uint32  mtlk_core_initial_ui_rescan_exemption_time = 0; /* ms */
static const uint32  mtlk_core_initial_channel = 0;
static const uint32  mtlk_core_initial_power_selection = 0;
static const uint32  mtlk_core_initial_ra_protection = 1;
static const uint32  mtlk_core_initial_force_ncb = 0;
static const uint32  mtlk_core_initial_n_rate_bo = 0;
static const uint32  mtlk_core_initial_debug_tpc = 0;
static const uint32  mtlk_core_initial_mac_soft_reset_enable = FALSE;
static const uint32  mtlk_core_initial_country_code = 234;
static const uint32  mtlk_core_initial_dot11d_enabled = TRUE;
static const uint32  mtlk_core_initial_basic_rate_set = DEFAULT_RATESET;
static const uint32  mtlk_core_initial_supported_rate_set = DEFAULT_RATESET;
static const uint32  mtlk_core_initial_extended_rate_set = DEFAULT_RATESET;
static const uint32  mtlk_core_initial_legacy_forced_rate = NO_RATE;
static const uint32  mtlk_core_initial_ht_forced_rate = NO_RATE;
static const uint32  mtlk_core_initial_agg_rate_limit_mode   = 0;   /* Disable */
static const uint32  mtlk_core_initial_agg_rate_limit_maxRate = 0;
static const uint32  mtlk_core_initial_rx_high_threshold      = (uint32)(-82);
static const uint32  mtlk_core_initial_cca_threshold = (uint32)(-62);
static const uint32  mtlk_core_initial_mac_watchdog_timeout_ms = MAC_WATCHDOG_DEFAULT_TIMEOUT_MS;
static const uint32  mtlk_core_initial_mac_watchdog_period_ms = MAC_WATCHDOG_DEFAULT_PERIOD_MS;
static const char    mtlk_core_initial_nick_name[] = "";
static const char    mtlk_core_initial_essid[] = "";
static const uint32  mtlk_core_initial_net_mode_cfg = NETWORK_11A_ONLY;
static const uint32  mtlk_core_initial_net_mode_cur = NETWORK_11A_ONLY;
static const uint32  mtlk_core_initial_frequency_band_cfg = MTLK_HW_BAND_2_4_GHZ;
static const uint32  mtlk_core_initial_frequency_band_cur = MTLK_HW_BAND_2_4_GHZ;
static const uint32  mtlk_core_initial_is_ht_cfg = TRUE;
static const uint32  mtlk_core_initial_is_ht_cur = TRUE;
static const uint32  mtlk_core_initial_l2nat_aging_timeout = 600;
/* MIBS */
static const uint32  mtlk_core_initial_short_preamble = TRUE;
static const uint32  mtlk_core_initial_tx_power = 0;
static const uint32  mtlk_core_initial_short_cyclic_prefix_rx     = 1;
static const uint32  mtlk_core_initial_short_cyclic_prefix_tx     = 1;
static const uint32  mtlk_core_initial_short_cyclic_prefix_rate31 = 0;
static const uint32  mtlk_core_initial_short_slot_time = TRUE;
static const uint32  mtlk_core_initial_sm_enable = TRUE;
static const uint8   mtlk_core_initial_tx_antennas[MTLK_NUM_ANTENNAS_BUFSIZE] = {1, 2, 3, 0};
static const uint8   mtlk_core_initial_rx_antennas[MTLK_NUM_ANTENNAS_BUFSIZE] = {1, 2, 3, 0};

static const uint32  mtlk_core_initial_ACL_mode = 0; /* ACL off */
static const uint32  mtlk_core_initial_STBC = 0;
static const uint32  mtlk_core_initial_AMPDU_maxlen = 2;
static const uint32  mtlk_core_initial_RTS_thresh = 2347;
static const uint32  mtlk_core_initial_adv_coding = 1;
static const uint32  mtlk_core_initial_OFDM_prot = 1;
static const uint32  mtlk_core_initial_HT_prot = 1;
static const uint32  mtlk_core_initial_overlap_prot = 1;
static const uint32  mtlk_core_initial_long_retry = 7;
static const uint32  mtlk_core_initial_short_retry = 7;
static const uint32  mtlk_core_initial_MSDU_lifetime = 512;
static const uint32  mtlk_core_initial_beacon_period = 100;
static const uint32  mtlk_core_initial_DTIM_period = 5;
static const uint32  mtlk_core_initial_disc_on_NACKs = 0;
/* 802.11w begin */
static const uint32  mtlk_core_initial_pmf_activated  = 0;
static const uint32  mtlk_core_initial_pmf_required   = 0;
static const uint32  mtlk_core_initial_saq_retr_tmout = 201;
static const uint32  mtlk_core_initial_saq_max_tmout  = 1000;
/* 802.11w end */
static const uint32  mtlk_core_initial_power_limit_11b_boost = 0;
static const uint32  mtlk_core_initial_power_limit_bpsk_boost = 0;
static const uint32  mtlk_core_initial_power_limit_auto_resp = 0;
static const uint32  mtlk_core_initial_lna_gain = 0;
static const uint32  mtlk_core_initial_sta_limit_max = DEFAULT_MAX_STAs_SUPPORTED;

static const uint32  mtlk_core_initial_current_tx_antenna = 0;
static const uint32  mtlk_core_initial_disconnect_on_nacks_weight = 0;
static const uint32  mtlk_core_initial_mib_use_long_preamble_for_multicast = 0;
static const uint32  mtlk_core_initial_mib_cb_databins_per_symbol = 0;

/* Interference Detection */
static const uint32 mtlk_core_initial_interfdet_mode = FALSE;
static const uint32 mtlk_core_initial_interfdet_short_scan_max_time = 100;
static const uint32 mtlk_core_initial_interfdet_long_scan_max_time = 1000;

static const uint32 mtlk_core_initial_mc_ps_fsdus = DEFAULT_MPS_TO_TRANSMITT_IN_DTIM;

static const mtlk_pdb_initial_value mtlk_core_parameters[] =
{
  /* ID,                          TYPE,                 FLAGS,                        SIZE,                            POINTER TO CONST */
  {PARAM_DB_CORE_MAC_ADDR,        PARAM_DB_TYPE_MAC,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_mac_addr),        mtlk_core_initial_mac_addr},
  {PARAM_DB_CORE_BRIDGE_MODE,     PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_bridge_mode),     &mtlk_core_initial_bridge_mode},
  {PARAM_DB_CORE_AP_FORWARDING,   PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_ap_forwarding),   &mtlk_core_initial_ap_forwarding},
  {PARAM_DB_CORE_RELIABLE_MCAST,  PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_reliable_mcast),   &mtlk_core_initial_reliable_mcast},
  {PARAM_DB_CORE_BSSID,           PARAM_DB_TYPE_MAC,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_bssid),        mtlk_core_initial_bssid},

  {PARAM_DB_CORE_COEX_CFG,                 PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_coex_cfg),        &mtlk_core_initial_coex_cfg},
  {PARAM_DB_CORE_STA_FORCE_SPECTRUM_MODE,  PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_sta_force_spectrum_mode),   &mtlk_core_initial_sta_force_spectrum_mode},
  {PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE, PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_spectrum_mode),   &mtlk_core_initial_spectrum_mode},
  {PARAM_DB_CORE_SELECTED_SPECTRUM_MODE,   PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_spectrum_mode),   &mtlk_core_initial_spectrum_mode},
  {PARAM_DB_CORE_USER_SPECTRUM_MODE,       PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_spectrum_mode),   &mtlk_core_initial_spectrum_mode},
  {PARAM_DB_CORE_SELECTED_BONDING_MODE,    PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_bonding_mode),   &mtlk_core_initial_bonding_mode},
  {PARAM_DB_CORE_USER_BONDING_MODE,        PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_bonding_mode),   &mtlk_core_initial_bonding_mode},
  {PARAM_DB_CORE_UP_RESCAN_EXEMPTION_TIME, PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_ui_rescan_exemption_time),   &mtlk_core_initial_ui_rescan_exemption_time},
  {PARAM_DB_CORE_CHANNEL_CFG,     PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_channel),   &mtlk_core_initial_channel},
  {PARAM_DB_CORE_CHANNEL_CUR,     PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_channel),   &mtlk_core_initial_channel},
  {PARAM_DB_CORE_POWER_SELECTION, PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_power_selection),   &mtlk_core_initial_power_selection},
  {PARAM_DB_CORE_RA_PROTECTION,   PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_ra_protection),   &mtlk_core_initial_ra_protection},
  {PARAM_DB_CORE_FORCE_NCB,       PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_force_ncb),   &mtlk_core_initial_force_ncb},
  {PARAM_DB_CORE_N_RATE_BO_QAM16,     PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_n_rate_bo),   &mtlk_core_initial_n_rate_bo},
  {PARAM_DB_CORE_N_RATE_BO_QAM64_2_3, PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_n_rate_bo),   &mtlk_core_initial_n_rate_bo},
  {PARAM_DB_CORE_N_RATE_BO_QAM64_3_4, PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_n_rate_bo),   &mtlk_core_initial_n_rate_bo},
  {PARAM_DB_CORE_N_RATE_BO_QAM64_5_6, PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_n_rate_bo),   &mtlk_core_initial_n_rate_bo},
  {PARAM_DB_CORE_DEBUG_TPC,       PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_debug_tpc),         &mtlk_core_initial_debug_tpc},
  {PARAM_DB_CORE_MAC_SOFT_RESET_ENABLE, PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_mac_soft_reset_enable),   &mtlk_core_initial_mac_soft_reset_enable},
  {PARAM_DB_CORE_COUNTRY_CODE,    PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_country_code),   &mtlk_core_initial_country_code},
  {PARAM_DB_CORE_DOT11D_ENABLED,  PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_dot11d_enabled),   &mtlk_core_initial_dot11d_enabled},
  {PARAM_DB_CORE_BASIC_RATE_SET,  PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_basic_rate_set),   &mtlk_core_initial_basic_rate_set},
  {PARAM_DB_CORE_SUPPORTED_RATE_SET,    PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_supported_rate_set),   &mtlk_core_initial_supported_rate_set},
  {PARAM_DB_CORE_EXTENDED_RATE_SET,     PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_extended_rate_set),    &mtlk_core_initial_extended_rate_set},
  {PARAM_DB_CORE_LEGACY_FORCED_RATE_SET,  PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_legacy_forced_rate),   &mtlk_core_initial_legacy_forced_rate},
  {PARAM_DB_CORE_HT_FORCED_RATE_SET,  PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_ht_forced_rate),   &mtlk_core_initial_ht_forced_rate},
  {PARAM_DB_CORE_AGG_RATE_LIMIT_MODE,     PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_agg_rate_limit_mode),   &mtlk_core_initial_agg_rate_limit_mode},
  {PARAM_DB_CORE_AGG_RATE_LIMIT_MAXRATE,  PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_agg_rate_limit_maxRate), &mtlk_core_initial_agg_rate_limit_maxRate},
  {PARAM_DB_CORE_RX_HIGH_THRESHOLD,     PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_rx_high_threshold),     &mtlk_core_initial_rx_high_threshold},
  {PARAM_DB_CORE_CCA_THRESHOLD,         PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_cca_threshold),    &mtlk_core_initial_cca_threshold},
  {PARAM_DB_CORE_MAC_WATCHDOG_TIMER_TIMEOUT_MS,  PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_mac_watchdog_timeout_ms),   &mtlk_core_initial_mac_watchdog_timeout_ms},
  {PARAM_DB_CORE_MAC_WATCHDOG_TIMER_PERIOD_MS,   PARAM_DB_TYPE_INT,    PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_mac_watchdog_period_ms),   &mtlk_core_initial_mac_watchdog_period_ms},
  {PARAM_DB_CORE_NICK_NAME,       PARAM_DB_TYPE_STRING, PARAM_DB_VALUE_FLAG_NO_FLAG,  MTLK_ESSID_MAX_SIZE,   mtlk_core_initial_nick_name},
  {PARAM_DB_CORE_ESSID,           PARAM_DB_TYPE_STRING, PARAM_DB_VALUE_FLAG_NO_FLAG,  MTLK_ESSID_MAX_SIZE,   mtlk_core_initial_essid},
  {PARAM_DB_CORE_NET_MODE_CFG,    PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_net_mode_cfg),   &mtlk_core_initial_net_mode_cfg},
  {PARAM_DB_CORE_NET_MODE_CUR,    PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_net_mode_cur),   &mtlk_core_initial_net_mode_cur},
  {PARAM_DB_CORE_FREQ_BAND_CFG,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_frequency_band_cfg),   &mtlk_core_initial_frequency_band_cfg},
  {PARAM_DB_CORE_FREQ_BAND_CUR,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_frequency_band_cur),   &mtlk_core_initial_frequency_band_cur},
  {PARAM_DB_CORE_IS_HT_CFG,       PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_is_ht_cfg),   &mtlk_core_initial_is_ht_cfg},
  {PARAM_DB_CORE_IS_HT_CUR,       PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_is_ht_cur),   &mtlk_core_initial_is_ht_cur},
  {PARAM_DB_CORE_L2NAT_AGING_TIMEOUT,  PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_l2nat_aging_timeout),   &mtlk_core_initial_l2nat_aging_timeout},
  /* MIBS */
  {PARAM_DB_CORE_SHORT_PREAMBLE,  PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_short_preamble),   &mtlk_core_initial_short_preamble},
  {PARAM_DB_CORE_TX_POWER,        PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_tx_power),   &mtlk_core_initial_tx_power},
  {PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_RX,     PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_short_cyclic_prefix_rx),     &mtlk_core_initial_short_cyclic_prefix_rx},
  {PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_TX,     PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_short_cyclic_prefix_tx),     &mtlk_core_initial_short_cyclic_prefix_tx},
  {PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_RATE31, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_short_cyclic_prefix_rate31), &mtlk_core_initial_short_cyclic_prefix_rate31},
  {PARAM_DB_CORE_SHORT_SLOT_TIME, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_short_slot_time),   &mtlk_core_initial_short_slot_time},
  {PARAM_DB_CORE_SM_ENABLE,       PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_sm_enable),   &mtlk_core_initial_sm_enable},
  {PARAM_DB_CORE_TX_ANTENNAS,     PARAM_DB_TYPE_BINARY, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_tx_antennas),   &mtlk_core_initial_tx_antennas},
  {PARAM_DB_CORE_RX_ANTENNAS,     PARAM_DB_TYPE_BINARY, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_rx_antennas),   &mtlk_core_initial_rx_antennas},

  {PARAM_DB_CORE_ACL_MODE,        PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_ACL_mode),     &mtlk_core_initial_ACL_mode},
  {PARAM_DB_CORE_STBC,            PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_STBC),         &mtlk_core_initial_STBC},
  {PARAM_DB_CORE_AMPDU_MAXLEN,    PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_AMPDU_maxlen), &mtlk_core_initial_AMPDU_maxlen},
  {PARAM_DB_CORE_RTS_THRESH,      PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_RTS_thresh),   &mtlk_core_initial_RTS_thresh},
  {PARAM_DB_CORE_ADVANCED_CODING, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_adv_coding),   &mtlk_core_initial_adv_coding},
  {PARAM_DB_CORE_OFDM_PROTECTION, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_OFDM_prot),    &mtlk_core_initial_OFDM_prot},
  {PARAM_DB_CORE_HT_PROTECTION,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_HT_prot),      &mtlk_core_initial_HT_prot},
  {PARAM_DB_CORE_OVERLAPPING_PROT,PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_overlap_prot), &mtlk_core_initial_overlap_prot},
  {PARAM_DB_CORE_LONG_RETRY_LIMIT,PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_long_retry),   &mtlk_core_initial_long_retry},
  {PARAM_DB_CORE_SHORT_RETRY_LIMIT,PARAM_DB_TYPE_INT,PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_short_retry),  &mtlk_core_initial_short_retry},
  {PARAM_DB_CORE_MSDU_LIFETIME,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_MSDU_lifetime),&mtlk_core_initial_MSDU_lifetime},
  {PARAM_DB_CORE_BEACON_PERIOD,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_beacon_period),&mtlk_core_initial_beacon_period},
  {PARAM_DB_CORE_DTIM_PERIOD,     PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_DTIM_period),  &mtlk_core_initial_DTIM_period},
  {PARAM_DB_CORE_DISC_ON_NACKS,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_disc_on_NACKs),&mtlk_core_initial_disc_on_NACKs},
  {PARAM_DB_CORE_POWER_LIMIT_11B_BOOST,     PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_power_limit_11b_boost),  &mtlk_core_initial_power_limit_11b_boost},
  {PARAM_DB_CORE_POWER_LIMIT_BPSK_BOOST,    PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_power_limit_bpsk_boost), &mtlk_core_initial_power_limit_bpsk_boost},
  {PARAM_DB_CORE_POWER_LIMIT_AUTO_RESPONCE, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_power_limit_auto_resp),  &mtlk_core_initial_power_limit_auto_resp},
  {PARAM_DB_CORE_LNA_GAIN,        PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_lna_gain),     &mtlk_core_initial_lna_gain},

  /* 802.11w begin */
  {PARAM_DB_CORE_PMF_ACTIVATED,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_pmf_activated),  &mtlk_core_initial_pmf_activated},
  {PARAM_DB_CORE_PMF_REQUIRED,    PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_pmf_required),   &mtlk_core_initial_pmf_required},
  {PARAM_DB_CORE_SAQ_RETR_TMOUT,  PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_saq_retr_tmout), &mtlk_core_initial_saq_retr_tmout},
  {PARAM_DB_CORE_SAQ_MAX_TMOUT,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_saq_max_tmout),  &mtlk_core_initial_saq_max_tmout},
  /* 802.11w end */

  {PARAM_DB_CORE_CUR_TX_ANTENNAS,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_current_tx_antenna),                  &mtlk_core_initial_current_tx_antenna},
  {PARAM_DB_CORE_DISC_ON_NACKS_WGHT,PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_disconnect_on_nacks_weight),          &mtlk_core_initial_disconnect_on_nacks_weight},
  {PARAM_DB_CORE_LONG_PREAMB_MC,    PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_mib_use_long_preamble_for_multicast), &mtlk_core_initial_mib_use_long_preamble_for_multicast},
  {PARAM_DB_CORE_CB_BINS_PER_SYMB,  PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_mib_cb_databins_per_symbol),          &mtlk_core_initial_mib_cb_databins_per_symbol},

  /* Interference Detection */
  {PARAM_DB_INTERFDET_MODE,                             PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_interfdet_mode), &mtlk_core_initial_interfdet_mode},
  {PARAM_DB_INTERFDET_20MHZ_DETECTION_THRESHOLD,        PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_zero_int),       &mtlk_core_initial_zero_int},
  {PARAM_DB_INTERFDET_20MHZ_NOTIFICATION_THRESHOLD,     PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_zero_int),       &mtlk_core_initial_zero_int},
  {PARAM_DB_INTERFDET_40MHZ_DETECTION_THRESHOLD,        PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_zero_int),       &mtlk_core_initial_zero_int},
  {PARAM_DB_INTERFDET_40MHZ_NOTIFICATION_THRESHOLD,     PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_zero_int),       &mtlk_core_initial_zero_int},
  {PARAM_DB_INTERFDET_SCAN_NOISE_THRESHOLD,             PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_zero_int),       &mtlk_core_initial_zero_int},
  {PARAM_DB_INTERFDET_SCAN_MINIMUM_NOISE,               PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_zero_int),       &mtlk_core_initial_zero_int},
  {PARAM_DB_INTERFDET_ACTIVE_POLLING_TIMEOUT,           PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_zero_int),       &mtlk_core_initial_zero_int},
  {PARAM_DB_INTERFDET_SHORT_SCAN_POLLING_TIMEOUT,       PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_zero_int),       &mtlk_core_initial_zero_int},
  {PARAM_DB_INTERFDET_LONG_SCAN_POLLING_TIMEOUT,        PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_zero_int),       &mtlk_core_initial_zero_int},
  {PARAM_DB_INTERFDET_ACTIVE_NOTIFICATION_TIMEOUT,      PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_zero_int),       &mtlk_core_initial_zero_int},
  {PARAM_DB_INTERFDET_SHORT_SCAN_NOTIFICATION_TIMEOUT,  PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_zero_int),       &mtlk_core_initial_zero_int},
  {PARAM_DB_INTERFDET_LONG_SCAN_NOTIFICATION_TIMEOUT,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG,  sizeof(mtlk_core_initial_zero_int),       &mtlk_core_initial_zero_int},

  {PARAM_DB_FW_LED_GPIO_DISABLE_TESTBUS, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_zero_int), &mtlk_core_initial_zero_int},
  {PARAM_DB_FW_LED_GPIO_ACTIVE_GPIOs,    PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_zero_int), &mtlk_core_initial_zero_int},
  {PARAM_DB_FW_LED_GPIO_LED_POLARITY,    PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_zero_int), &mtlk_core_initial_zero_int},

  {PARAM_DB_FW_LED_STATE_BASEB_LED, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_zero_int), &mtlk_core_initial_zero_int},
  {PARAM_DB_FW_LED_STATE_LED_STATE, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_zero_int), &mtlk_core_initial_zero_int},

  {PARAM_DB_CONSECUTIVE_11B_TH, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_zero_int), &mtlk_core_initial_zero_int},
  {PARAM_DB_CONSECUTIVE_11N_TH, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_zero_int), &mtlk_core_initial_zero_int},

  {PARAM_DB_11B_ANTSEL_RATE,  PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_zero_int), &mtlk_core_initial_zero_int},
  {PARAM_DB_11B_ANTSEL_RXANT, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_zero_int), &mtlk_core_initial_zero_int},
  {PARAM_DB_11B_ANTSEL_TXANT, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_zero_int), &mtlk_core_initial_zero_int},

  {PARAM_DB_CORE_STA_LIMIT_MIN,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_zero_int),      &mtlk_core_initial_zero_int},
  {PARAM_DB_CORE_STA_LIMIT_MAX,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_sta_limit_max), &mtlk_core_initial_sta_limit_max},

  {PARAM_DB_FW_MC_PS_MAX_FSDUS,   PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_mc_ps_fsdus),    &mtlk_core_initial_mc_ps_fsdus},

  {PARAM_DB_LAST_VALUE_ID,        0,                    0,                            0,                               NULL},
};

#ifdef MTCFG_LINDRV_HW_PCIG3
static const uint32 mtlk_core_initial_pcig3_offline_acm = DEFAULT_WAVE300_OFFLINE_CALIB_ALGO_MASK;
static const uint32 mtlk_core_initial_pcig3_online_acm = DEFAULT_WAVE300_ONLINE_CALIB_ALGO_MASK;
static const uint32 mtlk_core_initial_pcig3_two_ant_tx_enable = 0; /* Disable */
static const uint32  mtlk_core_initial_pcig3_rx_duty_cycle_ontime   = 1525;
static const uint32  mtlk_core_initial_pcig3_rx_duty_cycle_offtime  = 305;

static const mtlk_pdb_initial_value mtlk_core_parameters_pcig3[] =
{
  {PARAM_DB_CORE_CALIBRATION_ALGO_MASK, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_pcig3_offline_acm),       &mtlk_core_initial_pcig3_offline_acm},
  {PARAM_DB_CORE_ONLINE_ACM,            PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_pcig3_online_acm),        &mtlk_core_initial_pcig3_online_acm},
  {PARAM_DB_CORE_TWO_ANT_TX_ENABLE,     PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_pcig3_two_ant_tx_enable), &mtlk_core_initial_pcig3_two_ant_tx_enable},
  {PARAM_DB_CORE_RX_DUTY_CYCLE_ONTIME,  PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_pcig3_rx_duty_cycle_ontime),   &mtlk_core_initial_pcig3_rx_duty_cycle_ontime},
  {PARAM_DB_CORE_RX_DUTY_CYCLE_OFFTIME, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_pcig3_rx_duty_cycle_offtime),  &mtlk_core_initial_pcig3_rx_duty_cycle_offtime},
  {PARAM_DB_LAST_VALUE_ID,        0,                    0,                            0,                               NULL},
};
#endif /* MTCFG_LINDRV_HW_PCIG3 */

#ifdef MTCFG_LINDRV_HW_PCIE
static const uint32  mtlk_core_initial_pcie_offline_acm = DEFAULT_WAVE300_OFFLINE_CALIB_ALGO_MASK;
static const uint32  mtlk_core_initial_pcie_online_acm = DEFAULT_WAVE300_ONLINE_CALIB_ALGO_MASK;
static const uint32  mtlk_core_initial_pcie_two_ant_tx_enable = 0; /* Disable */
static const uint32  mtlk_core_initial_pcie_rx_duty_cycle_ontime   = 1525;
static const uint32  mtlk_core_initial_pcie_rx_duty_cycle_offtime  = 305;

static const mtlk_pdb_initial_value mtlk_core_parameters_pcie[] =
{
  {PARAM_DB_CORE_CALIBRATION_ALGO_MASK, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_pcie_offline_acm),       &mtlk_core_initial_pcie_offline_acm},
  {PARAM_DB_CORE_ONLINE_ACM,            PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_pcie_online_acm),        &mtlk_core_initial_pcie_online_acm},
  {PARAM_DB_CORE_TWO_ANT_TX_ENABLE,     PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_pcie_two_ant_tx_enable), &mtlk_core_initial_pcie_two_ant_tx_enable},
  {PARAM_DB_CORE_RX_DUTY_CYCLE_ONTIME,  PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_pcie_rx_duty_cycle_ontime),   &mtlk_core_initial_pcie_rx_duty_cycle_ontime},
  {PARAM_DB_CORE_RX_DUTY_CYCLE_OFFTIME, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_pcie_rx_duty_cycle_offtime),  &mtlk_core_initial_pcie_rx_duty_cycle_offtime},
  {PARAM_DB_LAST_VALUE_ID,        0,                    0,                            0,                               NULL},
};
#endif /* MTCFG_LINDRV_HW_PCIE */

#ifdef MTCFG_LINDRV_HW_AHBG35
static const uint32  mtlk_core_initial_g35_offline_acm = DEFAULT_AR10_OFFLINE_CALIB_ALGO_MASK;
static const uint32  mtlk_core_initial_g35_online_acm = DEFAULT_AR10_ONLINE_CALIB_ALGO_MASK;
static const uint32  mtlk_core_initial_g35_two_ant_tx_enable = 1; /* Enable */
static const uint32  mtlk_core_initial_g35_rx_duty_cycle_ontime   = 18315;
static const uint32  mtlk_core_initial_g35_rx_duty_cycle_offtime  = 3663;

static const mtlk_pdb_initial_value mtlk_core_parameters_g35[] =
{
  {PARAM_DB_CORE_CALIBRATION_ALGO_MASK, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_g35_offline_acm),       &mtlk_core_initial_g35_offline_acm},
  {PARAM_DB_CORE_ONLINE_ACM,            PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_g35_online_acm),        &mtlk_core_initial_g35_online_acm},
  {PARAM_DB_CORE_TWO_ANT_TX_ENABLE,     PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_g35_two_ant_tx_enable), &mtlk_core_initial_g35_two_ant_tx_enable},
  {PARAM_DB_CORE_RX_DUTY_CYCLE_ONTIME,  PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_g35_rx_duty_cycle_ontime),  &mtlk_core_initial_g35_rx_duty_cycle_ontime},
  {PARAM_DB_CORE_RX_DUTY_CYCLE_OFFTIME, PARAM_DB_TYPE_INT, PARAM_DB_VALUE_FLAG_NO_FLAG, sizeof(mtlk_core_initial_g35_rx_duty_cycle_offtime), &mtlk_core_initial_g35_rx_duty_cycle_offtime},
  {PARAM_DB_LAST_VALUE_ID,        0,                    0,                            0,                               NULL},
};
#endif /* MTCFG_LINDRV_HW_AHBG35 */

#define MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __CORE_PDB_DEF_H_ */

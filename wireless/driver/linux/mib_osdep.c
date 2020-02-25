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
 * Module responsible for configuration.
 *
 * Authors: originaly written by Joel Isaacson;
 *  further development and support by: Andriy Tkachuk, Artem Migaev,
 *  Oleksandr Andrushchenko.
 *
 */

#include "mtlkinc.h"

#include "mhi_mac_event.h"
#include "mib_osdep.h"
#include "mtlkparams.h"
#include "scan.h"
#include "eeprom.h"
#include "drvver.h"
#include "aocs.h" /*for dfs.h*/
#include "mtlkmib.h"
#include "mtlkhal.h"
#include "mtlkaux.h"
#include "mtlk_coreui.h"

#define LOG_LOCAL_GID   GID_MIBS
#define LOG_LOCAL_FID   1

/*
  We must adjust the minimal values of aggregation parameters according to the definitions imposed by the Firmware.
*/
#define ADDBA_BK_DEFAULT_AGGR_MAX_PACKETS  0
#define ADDBA_BE_DEFAULT_AGGR_MAX_PACKETS  12
#define ADDBA_VI_DEFAULT_AGGR_MAX_PACKETS  4
#define ADDBA_VO_DEFAULT_AGGR_MAX_PACKETS  2

#if (ADDBA_BK_DEFAULT_AGGR_MAX_PACKETS > NO_MAX_PACKETS_AGG_SUPPORTED_BK) 
#undef ADDBA_BK_DEFAULT_AGGR_MAX_PACKETS 
#define ADDBA_BK_DEFAULT_AGGR_MAX_PACKETS NO_MAX_PACKETS_AGG_SUPPORTED_BK
#endif
#if (ADDBA_BE_DEFAULT_AGGR_MAX_PACKETS > NO_MAX_PACKETS_AGG_SUPPORTED_BE) 
#undef ADDBA_BE_DEFAULT_AGGR_MAX_PACKETS 
#define ADDBA_BE_DEFAULT_AGGR_MAX_PACKETS NO_MAX_PACKETS_AGG_SUPPORTED_BE
#endif
#if (ADDBA_VI_DEFAULT_AGGR_MAX_PACKETS > NO_MAX_PACKETS_AGG_SUPPORTED_VI) 
#undef ADDBA_VI_DEFAULT_AGGR_MAX_PACKETS 
#define ADDBA_VI_DEFAULT_AGGR_MAX_PACKETS NO_MAX_PACKETS_AGG_SUPPORTED_VI
#endif
#if (ADDBA_VO_DEFAULT_AGGR_MAX_PACKETS > NO_MAX_PACKETS_AGG_SUPPORTED_VO) 
#undef ADDBA_VO_DEFAULT_AGGR_MAX_PACKETS 
#define ADDBA_VO_DEFAULT_AGGR_MAX_PACKETS NO_MAX_PACKETS_AGG_SUPPORTED_VO
#endif

/* UseAggregation
   AcceptAggregation
   MaxNumOfPackets
   MaxNumOfBytes
   TimeoutInterval
   MinSizeOfPacketInAggr
   ADDBATimeout
   AggregationWindowSize */
#define ADDBA_BK_DEFAULTS { 0, 1, ADDBA_BK_DEFAULT_AGGR_MAX_PACKETS,  12000, 1, 10, 0xFFFF, MAX_REORD_WINDOW}
#define ADDBA_BE_DEFAULTS { 1, 1, ADDBA_BE_DEFAULT_AGGR_MAX_PACKETS,  19200, 1, 10, 0xFFFF, MAX_REORD_WINDOW}
#define ADDBA_VI_DEFAULTS { 1, 1, ADDBA_VI_DEFAULT_AGGR_MAX_PACKETS,  12000, 1, 10, 0xFFFF, MAX_REORD_WINDOW}
#define ADDBA_VO_DEFAULTS { 1, 1, ADDBA_VO_DEFAULT_AGGR_MAX_PACKETS,  10000, 1, 10, 0xFFFF, MAX_REORD_WINDOW}

const mtlk_core_cfg_t def_card_cfg = 
{
  { /* addba */
    { /* per TIDs  */
      ADDBA_BE_DEFAULTS,
      ADDBA_BK_DEFAULTS,
      ADDBA_BK_DEFAULTS,
      ADDBA_BE_DEFAULTS,
      ADDBA_VI_DEFAULTS,
      ADDBA_VI_DEFAULTS,
      ADDBA_VO_DEFAULTS,
      ADDBA_VO_DEFAULTS
    }
  },
  { /* wme_bss */
    { /* class */
      /*   cwmin    cwmax   aifs    txop */
      {     4,      10,     3,      0   }, /* class[0] - BE */
      {     4,      10,     7,      0   }, /* class[1] - BK */
      {     3,       4,     2,   3008   }, /* class[2] - VI */
      {     2,       3,     2,   1504   }  /* class[3] - VO */
    }
  },
  { /* wme_ap */
    { /* class */
      /*   cwmin    cwmax   aifs    txop */
      {     4,       6,     3,      0   }, /* class[0] - BE */
      {     4,      10,     7,      0   }, /* class[1] - BK */
      {     3,       4,     1,   3008   }, /* class[2] - VI */
      {     2,       3,     1,   1504   }  /* class[3] - VO */
    }
  },
  /* is hidden SSID */
  FALSE
};

/*****************************************************************************
**
** NAME         mtlk_mib_set_nic_cfg
**
** PARAMETERS   nic            Card context
**
** RETURNS      none
**
** DESCRIPTION  Fills the card configuration structure with user defined
**              values (or default ones)
**
******************************************************************************/
void mtlk_mib_set_nic_cfg (struct nic *nic)
{
  nic->slow_ctx->cfg = def_card_cfg;
}

int
mtlk_mib_set_pre_activate (struct nic *nic)
{
  MIB_VALUE uValue;

  int   res;
  uint8 freq_band_cfg;
  uint8 net_mode;

  if (!mtlk_vap_is_master_ap(nic->vap_handle))
    return MTLK_ERR_OK;

  memset(&uValue, 0, sizeof(MIB_VALUE));

  freq_band_cfg = mtlk_core_get_freq_band_cfg(nic);
  net_mode = get_net_mode(freq_band_cfg, TRUE);

  uValue.sPreActivateType.u8NetworkMode                    = net_mode;
  uValue.sPreActivateType.u8UpperLowerChannel              = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_BONDING_MODE);
  uValue.sPreActivateType.u8SpectrumMode                   = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE);
  uValue.sPreActivateType.u8ShortSlotTimeOptionEnabled11g  = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SHORT_SLOT_TIME);
  uValue.sPreActivateType.u8ShortPreambleOptionImplemented = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SHORT_PREAMBLE);
  uValue.sPreActivateType.u32BSSOperationalRateSet         = HOST_TO_MAC32(get_operate_rate_set(net_mode));
  uValue.sPreActivateType.u32BSSbasicRateSet               = HOST_TO_MAC32(user_rates_to_fw(nic, get_basic_rateset(nic, net_mode, TRUE)));
  uValue.sPreActivateType.u32BSSSupportedRateSet           = HOST_TO_MAC32(user_rates_to_fw(nic, get_supported_rateset(nic, net_mode, TRUE)));
  uValue.sPreActivateType.u32BSSExtendedRateSet            = HOST_TO_MAC32(user_rates_to_fw(nic, get_extended_rateset(nic, net_mode, TRUE)));

  ILOG2_DDDDDDDDD("SENDING PREACTIVATION MIB:\n"
       "u8NetworkMode                       = %d\n"
       "u8UpperLowerChannel                 = %d\n"
       "u8SpectrumMode                      = %d\n"
       "u8ShortSlotTimeOptionEnabled11g     = %d\n"
       "u8ShortPreambleOptionImplemented    = %d\n"
       "u32OperationalRateSet               = %X\n"
       "u32BSSbasicRateSet                  = %X\n"
       "u32BSSsupportedRateSet              = %X\n"
       "u32BSSextendedRateSet               = %X",
      uValue.sPreActivateType.u8NetworkMode,
      uValue.sPreActivateType.u8UpperLowerChannel,
      uValue.sPreActivateType.u8SpectrumMode,
      uValue.sPreActivateType.u8ShortSlotTimeOptionEnabled11g,
      uValue.sPreActivateType.u8ShortPreambleOptionImplemented,
      uValue.sPreActivateType.u32BSSOperationalRateSet,
      uValue.sPreActivateType.u32BSSbasicRateSet,
      uValue.sPreActivateType.u32BSSSupportedRateSet,
      uValue.sPreActivateType.u32BSSExtendedRateSet);

  res = mtlk_set_mib_value_raw(mtlk_vap_get_txmm(nic->vap_handle),
                               MIB_PRE_ACTIVATE, &uValue);
  if (MTLK_ERR_OK != res) {
    ELOG_V("Failed to set MIB_PRE_ACTIVATE");
    return -ENODEV;
  }

  return res;
}

int
mtlk_mib_set_forced_rates (struct nic *nic)
{
  uint16 is_force_rate; /* FORCED_RATE_LEGACY_MASK & FORCED_RATE_HT_MASK */

  /*
   * Driver should first disable adaptive rate,
   * in order to avoid condition
   * where MIB_IS_FORCE_RATE configured to use forced rate
   * and MIB_{LEGACY,HT}_FORCE_RATE configured to use NO_RATE.
   */
  mtlk_set_mib_value_uint16(mtlk_vap_get_txmm(nic->vap_handle), MIB_IS_FORCE_RATE, 0);

  mtlk_set_mib_value_uint16(mtlk_vap_get_txmm(nic->vap_handle),
    MIB_LEGACY_FORCE_RATE, MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_LEGACY_FORCED_RATE_SET));
  mtlk_set_mib_value_uint16(mtlk_vap_get_txmm(nic->vap_handle),
    MIB_HT_FORCE_RATE, MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_HT_FORCED_RATE_SET));

  is_force_rate = 0;
  if (NO_RATE != MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_LEGACY_FORCED_RATE_SET)) {
    is_force_rate |= FORCED_RATE_LEGACY_MASK;
  }
  if (NO_RATE != MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_HT_FORCED_RATE_SET)) {
    is_force_rate |= FORCED_RATE_HT_MASK;
  }

  mtlk_set_mib_value_uint16(mtlk_vap_get_txmm(nic->vap_handle), MIB_IS_FORCE_RATE, is_force_rate);

  return MTLK_ERR_OK;
}

int
mtlk_mib_set_power_selection (struct nic *nic)
{
  mtlk_set_mib_value_uint8(mtlk_vap_get_txmm(nic->vap_handle),
                           MIB_USER_POWER_SELECTION,
                           MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_POWER_SELECTION));

  return MTLK_ERR_OK;
}

static int
mtlk_set_mib_values_ex (struct nic *nic, mtlk_txmm_msg_t* man_msg)
{
  mtlk_txmm_data_t* man_entry;
  UMI_MIB *psSetMib;
  mtlk_l2nat_t *l2nat = &nic->l2nat;
  mtlk_txmm_t *txmm = mtlk_vap_get_txmm(nic->vap_handle);
  mtlk_pdb_size_t size = 0;

  man_entry = mtlk_txmm_msg_get_empty_data(man_msg, txmm);
  if (!man_entry) {
    ELOG_V("Can't get MM data");
    return -ENOMEM;
  }

  ILOG2_V("Must do MIB's");

  /* MIB_EEPROM_VERSION */
  if (mtlk_vap_is_master(nic->vap_handle) && !mtlk_vap_is_dut(nic->vap_handle)) {
    mtlk_set_mib_eeprom_info(txmm, mtlk_core_get_eeprom(nic));
  }

  /* MIB_IEEE_ADDRESS */
  if (mtlk_vap_is_master(nic->vap_handle) && !mtlk_vap_is_dut(nic->vap_handle)) {
    MIB_VALUE uValue;

    memset(&uValue, 0, sizeof(MIB_VALUE));
    MTLK_CORE_PDB_GET_MAC(nic, PARAM_DB_CORE_MAC_ADDR, uValue.au8ListOfu8.au8Elements);

    if (MTLK_ERR_OK != mtlk_set_mib_value_raw(mtlk_vap_get_txmm(nic->vap_handle), MIB_IEEE_ADDRESS, &uValue)) {
      ELOG_V("Failed to set MIB_IEEE_ADDRESS");
      return -ENODEV;
    }
  }

  if (mtlk_vap_is_master_ap(nic->vap_handle)) {
    int res = MTLK_ERR_OK;
    res |= mtlk_set_mib_value_uint8(txmm,  MIB_USE_SPACE_TIME_BLOCK_CODE,       MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_STBC));
    res |= mtlk_set_mib_value_uint8(txmm,  MIB_RECEIVE_AMPDU_MAX_LENGTH,        MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_AMPDU_MAXLEN));
    res |= mtlk_set_mib_value_uint16(txmm, MIB_RTS_THRESHOLD,                   MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_RTS_THRESH));
    res |= mtlk_set_mib_value_uint8(txmm,  MIB_ADVANCED_CODING_SUPPORTED,       MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_ADVANCED_CODING));
    res |= mtlk_set_mib_value_uint8(txmm,  MIB_OFDM_PROTECTION_METHOD,          MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_OFDM_PROTECTION));
    res |= mtlk_set_mib_value_uint8(txmm,  MIB_OVERLAPPING_PROTECTION_ENABLE,   MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_OVERLAPPING_PROT));
    res |= mtlk_set_mib_value_uint8(txmm,  MIB_HT_PROTECTION_METHOD,            MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_HT_PROTECTION));
    res |= mtlk_set_mib_value_uint16(txmm, MIB_LONG_RETRY_LIMIT,                MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_LONG_RETRY_LIMIT));
    res |= mtlk_set_mib_value_uint16(txmm, MIB_SHORT_RETRY_LIMIT,               MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SHORT_RETRY_LIMIT));
    res |= mtlk_set_mib_value_uint32(txmm, MIB_TX_MSDU_LIFETIME,                MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_MSDU_LIFETIME));
    res |= mtlk_set_mib_value_uint16(txmm, MIB_BEACON_PERIOD,                   MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_BEACON_PERIOD));
    res |= mtlk_set_mib_value_uint8(txmm,  MIB_DISCONNECT_ON_NACKS_ENABLE,      MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_DISC_ON_NACKS));
    res |= mtlk_set_mib_value_uint16(txmm, MIB_CURRENT_TX_ANTENNA,              MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CUR_TX_ANTENNAS));
    res |= mtlk_set_mib_value_uint16(txmm, MIB_DISCONNECT_ON_NACKS_WEIGHT,      MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_DISC_ON_NACKS_WGHT));
    res |= mtlk_set_mib_value_uint8(txmm,  MIB_USE_LONG_PREAMBLE_FOR_MULTICAST, MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_LONG_PREAMB_MC));
    res |= mtlk_set_mib_value_uint8(txmm,  MIB_CB_DATABINS_PER_SYMBOL,          MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CB_BINS_PER_SYMB));
    res |= mtlk_set_mib_value_uint32(txmm, MIB_SAQUERY_RETRY_TIMEOUT,           MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SAQ_RETR_TMOUT));
    res |= mtlk_set_mib_value_uint32(txmm, MIB_SAQUERY_MAX_TIMEOUT,             MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SAQ_MAX_TMOUT));
    res |= mtlk_set_mib_value_uint8(txmm,  MIB_TWO_ANTENNA_TRANSMISSION_ENABLE, MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_TWO_ANT_TX_ENABLE));

    if (MTLK_ERR_OK != res)
    {
      ELOG_V("Failed to set MIB item");
      return -ENODEV;
    }
  }

  if (mtlk_vap_is_ap(nic->vap_handle))
  {
    mtlk_set_mib_value_uint16(txmm, MIB_DTIM_PERIOD, MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_DTIM_PERIOD));
    mtlk_set_mib_acl(txmm, nic->slow_ctx->acl, nic->slow_ctx->acl_mask);
    mtlk_set_mib_acl_mode(txmm, MIB_ACL_MODE, MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_ACL_MODE));
  }

  // Check if TPC close loop is ON and we have calibrations in EEPROM for
  // selected frequency band.
  if (!mtlk_vap_is_slave_ap(nic->vap_handle)) {
    uint16 calibration = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CALIBRATION_ALGO_MASK);

    if (MTLK_BFIELD_GET(calibration, TPC_BIT_VAL))
    {
        uint8 freq_band_cur = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_FREQ_BAND_CUR);
        if (mtlk_eeprom_is_band_supported(
                mtlk_core_get_eeprom(nic),
                freq_band_cur) != MTLK_ERR_OK)
        {
            mtlk_set_hw_state(nic, MTLK_HW_STATE_ERROR);
            ELOG_S("TPC close loop is ON and no calibrations for current frequency "
                 "(%s GHz) in EEPROM",
                 MTLK_HW_BAND_2_4_GHZ == freq_band_cur ? "2.4" :
                 MTLK_HW_BAND_5_2_GHZ == freq_band_cur ? "5" : "both");
        }
    }
    else
    { // TPC close loop is OFF. Check if TxPower is not zero.
        uint16 val = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_TX_POWER);
        ILOG1_D("TxPower = %u", val);
        if (!val)
        {
            MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_TX_POWER, DEFAULT_TX_POWER);
        }
    }
  }

  if (!mtlk_vap_is_slave_ap(nic->vap_handle)) {
    mtlk_mib_set_forced_rates(nic);

    /* Set supported antennas for transmission */
    psSetMib = (UMI_MIB*)man_entry->payload;
    memset(psSetMib, 0, sizeof(*psSetMib));
    size = MTLK_NUM_ANTENNAS_BUFSIZE;

    if (MTLK_ERR_OK != 
        MTLK_CORE_PDB_GET_BINARY(nic, PARAM_DB_CORE_TX_ANTENNAS, 
                                 psSetMib->uValue.au8ListOfu8.au8Elements, &size))
    {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_SUPPORTED_TX_ANTENNAS);
      return -ENODEV;
    }

    if (MTLK_ERR_OK != mtlk_set_mib_value_raw(txmm, MIB_SUPPORTED_TX_ANTENNAS, &psSetMib->uValue))
    {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_SUPPORTED_TX_ANTENNAS);
      return -ENODEV;
    }

    /* Set supported antennas for reception */
    psSetMib = (UMI_MIB*)man_entry->payload;
    memset(psSetMib, 0, sizeof(*psSetMib));
    size = MTLK_NUM_ANTENNAS_BUFSIZE;
    
    if (MTLK_ERR_OK != 
        MTLK_CORE_PDB_GET_BINARY(nic, PARAM_DB_CORE_RX_ANTENNAS, 
                                 psSetMib->uValue.au8ListOfu8.au8Elements, &size))
    {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_SUPPORTED_RX_ANTENNAS);
      return -ENODEV;
    }

    if (MTLK_ERR_OK != mtlk_set_mib_value_raw(txmm, MIB_SUPPORTED_RX_ANTENNAS, &psSetMib->uValue))
    {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_SUPPORTED_RX_ANTENNAS);
      return -ENODEV;
    }  

    /* set transmission power */
    if (MTLK_ERR_OK != 
        mtlk_set_mib_value_uint8 (txmm, MIB_TX_POWER, 
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_TX_POWER))) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_TX_POWER);
      return -ENODEV;
    }

    if (MTLK_ERR_OK != 
        mtlk_set_mib_value_uint8 (txmm, MIB_USE_RX_SHORT_CYCLIC_PREFIX,
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_RX))) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_USE_RX_SHORT_CYCLIC_PREFIX);
      return -ENODEV;
    }

    if (MTLK_ERR_OK !=
        mtlk_set_mib_value_uint8 (txmm, MIB_USE_TX_SHORT_CYCLIC_PREFIX,
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_TX))) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_USE_TX_SHORT_CYCLIC_PREFIX);
      return -ENODEV;
    }

    if (MTLK_ERR_OK !=
        mtlk_set_mib_value_uint8 (txmm, MIB_USE_SHORT_CYCLIC_PREFIX_RATE_31,
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_RATE31))) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_USE_SHORT_CYCLIC_PREFIX_RATE_31);
      return -ENODEV;
    }

    if (MTLK_ERR_OK !=
        mtlk_set_mib_value_uint16 (txmm, MIB_CALIBRATION_ALGO_MASK, 
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CALIBRATION_ALGO_MASK))) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_CALIBRATION_ALGO_MASK);
      return -ENODEV;
    }

    if (MTLK_ERR_OK !=
        mtlk_set_mib_value_uint16 (txmm, MIB_ONLINE_CALIBRATION_ALGO_MASK,
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_ONLINE_ACM))) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", PARAM_DB_CORE_ONLINE_ACM);
      return -ENODEV;
    }

    if (MTLK_ERR_OK != 
        mtlk_set_mib_value_uint8 (txmm, MIB_SM_ENABLE, 
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SM_ENABLE))) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_SM_ENABLE);
      return -ENODEV;
    }
  }

  if (mtlk_vap_is_master_ap(nic->vap_handle))
  {
    int i = 0;
    
    // set WME BSS parameters (relevant only on AP)
    man_entry->id           = UM_MAN_SET_MIB_REQ;
    man_entry->payload_size = sizeof(*psSetMib);

    psSetMib = (UMI_MIB*)man_entry->payload;

    memset(psSetMib, 0, sizeof(*psSetMib));

    psSetMib->u16ObjectID = cpu_to_le16(MIB_WME_PARAMETERS);

    for (i = 0; i < NTS_PRIORITIES; i++) {
      psSetMib->uValue.sWMEParameters.au8CWmin[i] = nic->slow_ctx->cfg.wme_bss.wme_class[i].cwmin;
      psSetMib->uValue.sWMEParameters.au16Cwmax[i] = cpu_to_le16(nic->slow_ctx->cfg.wme_bss.wme_class[i].cwmax);
      psSetMib->uValue.sWMEParameters.au8AIFS[i] = nic->slow_ctx->cfg.wme_bss.wme_class[i].aifsn;
      psSetMib->uValue.sWMEParameters.au16TXOPlimit[i] = cpu_to_le16(nic->slow_ctx->cfg.wme_bss.wme_class[i].txop / 32);
    }

    if (mtlk_txmm_msg_send_blocked(man_msg,
                                   MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out",
          le16_to_cpu(psSetMib->u16ObjectID));
      return -ENODEV;
    }

    if (psSetMib->u16Status == cpu_to_le16(UMI_OK)) {
      ILOG2_D("Successfully set MIB item 0x%04x.",
        le16_to_cpu(psSetMib->u16ObjectID));
    } else {
      ELOG_DD("Failed to set MIB item 0x%x, error %d.",
        le16_to_cpu(psSetMib->u16ObjectID), le16_to_cpu(psSetMib->u16Status));
    }

    // set WME AP parameters (relevant only on AP)
    man_entry->id           = UM_MAN_SET_MIB_REQ;
    man_entry->payload_size = sizeof(*psSetMib);

    psSetMib = (UMI_MIB*)man_entry->payload;

    memset(psSetMib, 0, sizeof(*psSetMib));

    psSetMib->u16ObjectID = cpu_to_le16(MIB_WME_QAP_PARAMETERS);

    for (i = 0; i < NTS_PRIORITIES; i++) {
      psSetMib->uValue.sWMEParameters.au8CWmin[i] = nic->slow_ctx->cfg.wme_ap.wme_class[i].cwmin;
      psSetMib->uValue.sWMEParameters.au16Cwmax[i] = cpu_to_le16(nic->slow_ctx->cfg.wme_ap.wme_class[i].cwmax);
      psSetMib->uValue.sWMEParameters.au8AIFS[i] = nic->slow_ctx->cfg.wme_ap.wme_class[i].aifsn;
      psSetMib->uValue.sWMEParameters.au16TXOPlimit[i] = cpu_to_le16(nic->slow_ctx->cfg.wme_ap.wme_class[i].txop / 32);
    }

    if (mtlk_txmm_msg_send_blocked(man_msg,
                                   MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out",
          le16_to_cpu(psSetMib->u16ObjectID));
      return -ENODEV;
    }

    if (psSetMib->u16Status == cpu_to_le16(UMI_OK)) {
      ILOG2_D("Successfully set MIB item 0x%04x.",
        le16_to_cpu(psSetMib->u16ObjectID));
    } else {
      ELOG_DD("Failed to set MIB item 0x%x, error %d",
        le16_to_cpu(psSetMib->u16ObjectID), le16_to_cpu(psSetMib->u16Status));
    }
  }

  if (mtlk_vap_is_ap(nic->vap_handle)) {
    if (MTLK_ERR_OK != mtlk_set_mib_value_uint8(txmm, MIB_AP_MAX_PKTS_IN_SP, nic->uapsd_max_sp)) {
      ELOG_D("Failed to set MIB item 0x%x, timed-out", MIB_AP_MAX_PKTS_IN_SP);
      return -ENODEV;
    }
  }

  // set driver features
  if (mtlk_vap_is_master_ap(nic->vap_handle)) {
    mtlk_set_mib_value_uint8(txmm, MIB_SPECTRUM_MODE, 
                             MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE));
  }

  l2nat->l2nat_aging_timeout = HZ * MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_L2NAT_AGING_TIMEOUT);
  ILOG1_D("l2nat_aging_timeout set to %u", l2nat->l2nat_aging_timeout);

  /* this is requirement from MAC */
  MTLK_ASSERT(ASSOCIATE_FAILURE_TIMEOUT < CONNECT_TIMEOUT);
  if (!mtlk_vap_is_ap(nic->vap_handle)) {
    mtlk_set_mib_value_uint32(txmm, MIB_ASSOCIATE_FAILURE_TIMEOUT, ASSOCIATE_FAILURE_TIMEOUT);
  }
  ILOG2_V("End Mibs");
  return 0;
}

int
mtlk_set_vap_mibs (struct nic *nic)
{
  int             res       = -ENOMEM;
  mtlk_txmm_msg_t man_msg;

  /* The function sets all known FW MIBs excluding ones
     related with security and depended on channel & spectrum.
     Channel depended MIBs set after AOCS finished.
     Called just after VAP add, before core activation and on recovery.
  */
  if (mtlk_txmm_msg_init(&man_msg) == MTLK_ERR_OK) {
    res = mtlk_set_mib_values_ex(nic, &man_msg);
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  else {
    ELOG_V("Can't init TXMM msg");
  }

  return res;
}

void
mtlk_mib_update_pm_related_mibs (struct nic *nic,
                                 mtlk_aux_pm_related_params_t *data)
{
  /* Update MIB DB */
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE,
                        (int)data->u8SpectrumMode);
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_SHORT_SLOT_TIME,
                        (int)data->u8ShortSlotTimeOptionEnabled11g);
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_SHORT_PREAMBLE, 
                        (int)data->u8ShortPreambleOptionImplemented);
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_SELECTED_BONDING_MODE,
                        (int)data->u8UpperLowerChannel);
}

int
mtlk_mib_set_security (struct nic *nic)
{
  int res = MTLK_ERR_OK;
  mtlk_txmm_t *txmm = mtlk_vap_get_txmm(nic->vap_handle);

  res = mtlk_set_mib_value_uint8(txmm, MIB_WEP_DEFAULT_KEYID, nic->slow_ctx->default_key);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Unable to restore WEP TX key index", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  res = mtlk_set_mib_value_raw(txmm, MIB_WEP_DEFAULT_KEYS, (MIB_VALUE*)&nic->slow_ctx->wep_keys);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Failed to restore WEP key", mtlk_vap_get_oid(nic->vap_handle));
  }

  res = mtlk_set_mib_value_uint8(txmm, MIB_AUTHENTICATION_PREFERENCE, nic->authentication);
  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: Failed to restore access policy to %i", mtlk_vap_get_oid(nic->vap_handle), nic->authentication);
    goto FINISH;
  }

  res = mtlk_set_mib_rsn(txmm, nic->rsn_enabled);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Failed to restore RSN", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  res = mtlk_set_mib_value_uint8(txmm, MIB_PRIVACY_INVOKED, nic->slow_ctx->wep_enabled);
  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: Failed to restore WEP encryption (err=%d)", mtlk_vap_get_oid(nic->vap_handle), res);
    goto FINISH;
  }

FINISH:
  MTLK_ASSERT(res == MTLK_ERR_OK);
  return res;
}

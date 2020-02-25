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
*/

#ifndef __MTLK_COREUI_H__
#define __MTLK_COREUI_H__

#include "mtlkerr.h"
#include "txmm.h"
#include "addba.h"
#include "channels.h"
#include "dfs.h"
#include "mtlkqos.h"
#include "bitrate.h"
#include "dataex.h"
#include "mtlkdfdefs.h"
#include "mtlk_ab_manager.h"
#include "mtlk_coc.h"
#ifdef MTCFG_PMCU_SUPPORT
#include "mtlk_pcoc.h"
#endif /*MTCFG_PMCU_SUPPORT*/
#include "ta.h"
#include "fw_recovery.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/**
*\file mtlk_coreui.h

*\brief Core interface for DF UI module

*\defgroup COREUI Core interface for DF UI
*\{

*/

#define MTLK_CHANNEL_NOT_USED 0
#define MTLK_NUM_ANTENNAS_BUFSIZE 4
#define MTLK_CORE_BITRATE_AUTO (-1)
#define MTLK_ESSID_MAX_SIZE (IW_ESSID_MAX_SIZE + 1)

void __MTLK_IFUNC
mtlk_core_handle_tx_data(mtlk_vap_handle_t vap_handle, mtlk_nbuf_t *nbuf);

void __MTLK_IFUNC
mtlk_core_handle_tx_ctrl(mtlk_vap_handle_t    vap_handle,
                         mtlk_user_request_t *req,
                         uint32               id,
                         mtlk_clpb_t         *data);

BOOL __MTLK_IFUNC
mtlk_core_net_state_is_connected(uint16 net_state);

typedef enum
{
  MTLK_CORE_REQ_ACTIVATE_OPEN = MTLK_CORE_ABILITITES_START, /*!< Activate the core for AP */
  MTLK_CORE_REQ_GET_AP_CAPABILITIES, /*!< Get AP capabilities */
  MTLK_CORE_REQ_CONNECT_STA,         /*!< Connect and activate the core for STA */
  MTLK_CORE_REQ_DISCONNECT_STA,      /*!< Disconnect for STA */
  MTLK_CORE_REQ_AP_DISCONNECT_STA,   /*!< Disconnect STA for AP */
  MTLK_CORE_REQ_AP_DISCONNECT_ALL,   /*!< Disconnect all for AP */
  MTLK_CORE_REQ_DEACTIVATE,          /*!< Deactivate the core    */
  MTLK_CORE_REQ_START_SCANNING,      /*!< Start scanning */
  MTLK_CORE_REQ_GET_SCANNING_RES,    /*!< Get scanning results */
  MTLK_CORE_REQ_SET_MAC_ADDR,        /*!< Assign MAC address     */
  MTLK_CORE_REQ_GET_MAC_ADDR,        /*!< Query MAC address      */
  MTLK_CORE_REQ_GET_STATUS,          /*!< Get core status & statistics    */
  MTLK_CORE_REQ_RESET_STATS,         /*!< Reset core statistics  */
  MTLK_CORE_REQ_SET_DELBA_REQ,       /*!< Process request to DELBA */
  MTLK_CORE_REQ_GET_ADDBA_CFG,       /*!< Process request to ADDBA */
  MTLK_CORE_REQ_SET_ADDBA_CFG,       /*!< Process request to ADDBA */
  MTLK_CORE_REQ_GET_ADDBA_STATE,     /*!< Process request to aggregation state */
  MTLK_CORE_REQ_GET_WME_BSS_CFG,     /*!< Process request to WME BSS */
  MTLK_CORE_REQ_SET_WME_BSS_CFG,     /*!< Process request to WME BSS */
  MTLK_CORE_REQ_GET_WME_AP_CFG,      /*!< Process request to WME AP */
  MTLK_CORE_REQ_SET_WME_AP_CFG,      /*!< Process request to WME AP */
  MTLK_CORE_REQ_GET_AOCS_CFG,        /*!< Process request to AOCS */
  MTLK_CORE_REQ_SET_AOCS_CFG,        /*!< Process request to AOCS */
  MTLK_CORE_REQ_GET_DOT11H_CFG,      /*!< Process request to DOT11H on AP/STA*/
  MTLK_CORE_REQ_SET_DOT11H_CFG,      /*!< Process request to DOT11H on AP/STA */
  MTLK_CORE_REQ_GET_DOT11H_AP_CFG,   /*!< Process request to DOT11H on AP */
  MTLK_CORE_REQ_SET_DOT11H_AP_CFG,   /*!< Process request to DOT11H on AP */
  MTLK_CORE_REQ_GET_MIBS_CFG,        /*!< Process MIBs */
  MTLK_CORE_REQ_SET_MIBS_CFG,        /*!< Process MIBs */
  MTLK_CORE_REQ_GET_COUNTRY_CFG,     /*!< Process MIBs */
  MTLK_CORE_REQ_SET_COUNTRY_CFG,     /*!< Process MIBs */
  MTLK_CORE_REQ_GET_L2NAT_CFG,       /*!< Process L2NAT */
  MTLK_CORE_REQ_SET_L2NAT_CFG,       /*!< Process L2NAT */
  MTLK_CORE_REQ_SET_WDS_CFG,         /*!< Process WDS Configuration */
  MTLK_CORE_REQ_GET_WDS_CFG,         /*!< Process WDS Configuration */
  MTLK_CORE_REQ_GET_WDS_PEERAP,      /*!< Process WDS Configuration */
  MTLK_CORE_REQ_SET_WDS_DBG,         /*!< Process WDS Configuration */
  MTLK_CORE_REQ_GET_DOT11D_CFG,      /*!< Process DOT11D */
  MTLK_CORE_REQ_SET_DOT11D_CFG,      /*!< Process DOT11D */
  MTLK_CORE_REQ_GET_MAC_WATCHDOG_CFG,/*!< Process MAC watchdog */
  MTLK_CORE_REQ_SET_MAC_WATCHDOG_CFG,/*!< Process MAC watchdog */
  MTLK_CORE_REQ_GET_STADB_CFG,       /*!< Process STADB */
  MTLK_CORE_REQ_SET_STADB_CFG,       /*!< Process STADB */
  MTLK_CORE_REQ_GET_SQ_CFG,          /*!< Process SendQueue */
  MTLK_CORE_REQ_SET_SQ_CFG,          /*!< Process SendQueue */
  MTLK_CORE_REQ_GET_CORE_CFG,        /*!< Process Core */
  MTLK_CORE_REQ_SET_CORE_CFG,        /*!< Process Core */
  MTLK_CORE_REQ_GET_MASTER_CFG,      /*!< Process Core */
  MTLK_CORE_REQ_SET_MASTER_CFG,      /*!< Process Core */
  MTLK_CORE_REQ_GET_HSTDB_CFG,       /*!< Process HSTDB */
  MTLK_CORE_REQ_SET_HSTDB_CFG,       /*!< Process HSTDB */
  MTLK_CORE_REQ_GET_SCAN_CFG,        /*!< Process Scan */
  MTLK_CORE_REQ_SET_SCAN_CFG,        /*!< Process Scan */
  MTLK_CORE_REQ_GET_TX_POWER_LIMIT,  /*!< Tx Power Limit */
  MTLK_CORE_REQ_SET_HW_DATA_CFG,     /*!< Process HW data */
  MTLK_CORE_REQ_GET_EEPROM_CFG,      /*!< Process EEPROM */
#ifdef EEPROM_DATA_VALIDATION
  MTLK_CORE_REQ_GET_EEPROM_FW,       /*!< Process EEPROM FW */
#endif /* EEPROM_DATA_VALIDATION */
  MTLK_CORE_REQ_GET_QOS_CFG,         /*!< Process QoS */
  MTLK_CORE_REQ_SET_QOS_CFG,         /*!< Process QoS */
  MTLK_CORE_REQ_GET_COC_CFG,         /*!< Process COC */
  MTLK_CORE_REQ_SET_COC_CFG,         /*!< Process COC */
#ifdef MTCFG_PMCU_SUPPORT
  MTLK_CORE_REQ_GET_PCOC_CFG,         /*!< Process PCOC */
  MTLK_CORE_REQ_SET_PCOC_CFG,         /*!< Process PCOC */
#endif /*MTCFG_PMCU_SUPPORT*/
  MTLK_CORE_REQ_L2NAT_CLEAR_TABLE,   /*!< Clear L2NAT table */
  MTLK_CORE_REQ_GET_ANTENNA_GAIN,    /*!< Get Antenna gain */
  MTLK_CORE_REQ_GET_AOCS_TBL,        /*!< Get AOCS table */
  MTLK_CORE_REQ_GET_AOCS_CHANNELS_TBL, /*!< Get AOCS channels table */
#ifdef BT_ACS_DEBUG
  MTLK_CORE_REQ_SET_AOCS_CHANNELS_TBL_DBG, /*!< Set AOCS channels table */
#endif  /* BT_ACS_DEBUG */
  MTLK_CORE_REQ_GET_AOCS_HISTORY,      /*!< Get AOCS history */
  MTLK_CORE_REQ_GET_AOCS_PENALTIES,    /*!< Get AOCS penalties table */
#ifdef AOCS_DEBUG
  MTLK_CORE_REQ_SET_AOCS_CL,           /*!< Set AOCS CL */
#endif /* AOCS_DEBUG */
  MTLK_CORE_REQ_GET_HW_LIMITS,         /*!< Get HW Limits */
  MTLK_CORE_REQ_GET_REG_LIMITS,        /*!< Get Reg Limits */
  MTLK_CORE_REQ_STOP_LM,               /*!< Stop Lower MAC */
  MTLK_CORE_REQ_MAC_CALIBRATE,         /*!< Control MAC gpio */
  MTLK_CORE_REQ_CTRL_MAC_GPIO,         /*!< Control MAC gpio */
  MTLK_CORE_REQ_GEN_DATA_EXCHANGE,     /*!< Gen data exchange */
  MTLK_CORE_REQ_GET_IW_GENERIC,        /*!< IW Generic */
  MTLK_CORE_REQ_GET_EE_CAPS,           /*!< Get EE Caps */
  MTLK_CORE_REQ_GET_L2NAT_STATS,       /*!< Get L2NAT statistic */
  MTLK_CORE_REQ_GET_SQ_STATUS,         /*!< Get SQ status and statistic information */
  MTLK_CORE_REQ_SET_MAC_ASSERT,        /*!< Perform MAC assert */
  MTLK_CORE_REQ_GET_MC_IGMP_TBL,       /*!< Get IGMP MCAST table */
  MTLK_CORE_REQ_GET_BCL_MAC_DATA,      /*!< Get BCL MAC data */
  MTLK_CORE_REQ_SET_BCL_MAC_DATA,      /*!< Get BCL MAC data */
  MTLK_CORE_REQ_GET_RANGE_INFO,        /*!< Get supported bitrates and channels info*/
  MTLK_CORE_REQ_GET_STADB_STATUS,      /*!< Get status&statistic data from STADB (list)*/
  MTLK_CORE_REQ_SET_WEP_ENC_CFG,       /*!< Configure WEP encryption */
  MTLK_CORE_REQ_GET_WEP_ENC_CFG,       /*!< Get WEP encryption configuration */
  MTLK_CORE_REQ_SET_AUTH_CFG,          /*!< Configure authentication parameters */
  MTLK_CORE_REQ_GET_AUTH_CFG,          /*!< Get authentication parameters */
  MTLK_CORE_REQ_SET_GENIE_CFG,         /*!< Get rsn/general_ie */
  MTLK_CORE_REQ_GET_ENCEXT_CFG,        /*!< Get Extended encoding data structure */
  MTLK_CORE_REQ_SET_ENCEXT_CFG,        /*!< Set Extended encoding parmeters */
  MTLK_CORE_REQ_MBSS_ADD_VAP,          /*!< MBSS AddVap issued by Master */
  MTLK_CORE_REQ_MBSS_DEL_VAP,          /*!< MBSS DelVap issued by Master */
  MTLK_CORE_REQ_MBSS_SET_VARS,         /*!< MBSS Set Values */
  MTLK_CORE_REQ_MBSS_GET_VARS,         /*!< MBSS Get Values */
  MTLK_CORE_REQ_GET_SERIALIZER_INFO,   /*!< Get Serializer Info */
  MTLK_CORE_REQ_SET_COEX_20_40_RSSI_THR_CFG,    /*!< Set Coex 20/40 RSSI threshold configuration */
  MTLK_CORE_REQ_GET_COEX_20_40_RSSI_THR_CFG,    /*!< Get Coex 20/40 RSSI threshold configuration */
  MTLK_CORE_REQ_SET_COEX_20_40_MODE_CFG,    /*!< Set Coex 20/40 mode configuration */
  MTLK_CORE_REQ_GET_COEX_20_40_MODE_CFG,    /*!< Get Coex 20/40 mode configuration */
  MTLK_CORE_REQ_SET_COEX_20_40_AP_FORCE_PARAMS_CFG,    /*!< Set Coex 20/40 ap force params configuration */
  MTLK_CORE_REQ_GET_COEX_20_40_AP_FORCE_PARAMS_CFG,    /*!< Get Coex 20/40 ap force params configuration */
  MTLK_CORE_REQ_SET_COEX_20_40_STA_EXEMPTION_REQ_CFG,    /*!< Set Coex 20/40 exemption request configuration */
  MTLK_CORE_REQ_GET_COEX_20_40_STA_EXEMPTION_REQ_CFG,    /*!< Get Coex 20/40 exemption request configuration */
  MTLK_CORE_REQ_SET_COEX_20_40_AP_MIN_NUM_OF_EXM_STA_CFG,     /*!< Set Coex 20/40 minimum STAs to not exempt configuration */
  MTLK_CORE_REQ_GET_COEX_20_40_AP_MIN_NUM_OF_EXM_STA_CFG,     /*!< Get Coex 20/40 minimum STAs to not exempt configuration */
  MTLK_CORE_REQ_SET_COEX_20_40_TIMES_CFG,    /*!< Set Coex 20/40 times configuration */
  MTLK_CORE_REQ_GET_COEX_20_40_TIMES_CFG,    /*!< Get Coex 20/40 times configuration */
  MTLK_CORE_REQ_SET_INTERFDET_PARAMS_CFG,    /*!< Set Interference Detection configuration */
  MTLK_CORE_REQ_GET_INTERFDET_MODE_CFG,      /*!< Get Interference Detection mode */
  MTLK_CORE_REQ_SET_FW_LED_CFG,        /*!< Set FW GPIO LED configuration */
  MTLK_CORE_REQ_GET_FW_LED_CFG,        /*!< Get FW GPIO LED configuration */
  MTLK_CORE_REQ_SET_DBG_CLI,           /*!< Send debug data to FW */
  MTLK_CORE_REQ_SET_FW_DEBUG,          /*!< Send debug data to FW */
  MTLK_CORE_REQ_SET_FW_LOG_SEVERITY,   /*!< Set FW Log Severity */
  MTLK_CORE_REQ_GET_TA_CFG,            /*!< Process TA */
  MTLK_CORE_REQ_SET_TA_CFG,            /*!< Process TA */
  MTLK_CORE_REQ_SET_ENHANCED11B_CFG,   /*!< Set enhanced 11b configuration */
  MTLK_CORE_REQ_GET_ENHANCED11B_CFG,   /*!< Get enhanced 11b configuration */
  MTLK_CORE_REQ_SET_11B_CFG,           /*!< Set 11b configuration */
  MTLK_CORE_REQ_GET_11B_CFG,           /*!< Get 11b configuration */
  MTLK_CORE_REQ_SET_DOT11W_CFG,        /*!< Set 802.11w configuration */
  MTLK_CORE_REQ_GET_DOT11W_CFG,        /*!< Get 802.11w configuration */
  MTLK_CORE_REQ_SET_UAPSD_MODE,        /*!< Set U-APSD mode */
  MTLK_CORE_REQ_GET_UAPSD_MODE,        /*!< Get U-APSD mode */
  MTLK_CORE_REQ_SET_UAPSD_CFG,         /*!< Set U-APSD configuration */
  MTLK_CORE_REQ_GET_UAPSD_CFG,         /*!< Get U-APSD configuration */
  MTLK_CORE_REQ_GET_PS_STATUS,         /*!< Get PS status */
  MTLK_CORE_REQ_SET_RECOVERY_CFG,      /*!< Set FW Recovery configuration */
  MTLK_CORE_REQ_GET_RECOVERY_CFG,      /*!< Get FW Recovery configuration */
  MTLK_CORE_REQ_SET_MC_PS_SIZE_CFG,    /*!< Set multicast PS size */
  MTLK_CORE_REQ_GET_MC_PS_SIZE_CFG,    /*!< Get multicast PS size */

} mtlk_core_tx_req_id_t;

typedef enum _mtlk_aocs_ac_e {
  AC_DISABLED,
  AC_ENABLED,
  AC_NOT_USED,
} mtlk_aocs_ac_e;

/* universal accessors for data, passed between Core and DF through clipboard */
#define MTLK_DECLARE_CFG_START(name)  typedef struct _##name {
#define MTLK_DECLARE_CFG_END(name)    } __MTLK_IDATA name;

#define MTLK_CFG_ITEM(type,name) type name; \
                                 uint8 name##_filled;

#define MTLK_CFG_ITEM_ARRAY(type,name,num_of_elems) type name[num_of_elems]; \
                                                     uint8 name##_filled;

/* integer values */
#define MTLK_CFG_SET_ITEM(obj,name,src) \
  {\
    (obj)->name##_filled = 1; \
    (obj)->name = src; \
  }

#define MTLK_CFG_SET_ITEM_BY_FUNC(obj,name,func,func_params,func_res) \
  {\
    func_res = func func_params; \
    if (MTLK_ERR_OK == func_res) {\
      (obj)->name##_filled = 1;\
    }\
    else {\
      (obj)->name##_filled = 0;\
    }\
  }

#define MTLK_CFG_SET_ITEM_BY_FUNC_VOID(obj,name,func,func_params) \
  {\
    func func_params; \
    (obj)->name##_filled = 1; \
  }

#define MTLK_CFG_GET_ITEM(obj,name,dest) \
  if(1==(obj)->name##_filled){(dest)=(obj)->name;}

#define MTLK_CFG_GET_ITEM_BY_FUNC(obj,name,func,func_params,func_res) \
  if(1==(obj)->name##_filled){func_res=func func_params;}

#define MTLK_CFG_GET_ITEM_BY_FUNC_VOID(obj,name,func,func_params) \
  if(1==(obj)->name##_filled){func func_params;}

/* integer buffers -> used loop for provide different type sizes in assigning */
#define MTLK_CFG_SET_ARRAY_ITEM(obj,name,src,num_elems_received,result) \
  { \
    result=MTLK_ERR_PARAMS; \
    if (ARRAY_SIZE((obj)->name) == num_elems_received) {\
      uint32 counter; \
      for (counter=0;counter<num_elems_received;counter++) { \
        (obj)->name[counter]=(src)[counter]; \
      } \
      (obj)->name##_filled = 1; \
      result=MTLK_ERR_OK; \
    }\
  }

#define MTLK_CFG_GET_ARRAY_ITEM(obj,name,dst,num_elems) \
  { \
      if ((obj)->name##_filled == 1) {\
        uint32 counter; \
        for (counter=0;counter<num_elems;counter++) { \
          (dst)[counter]=(obj)->name[counter]; \
        }\
      }\
  }

#define MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC(obj,name,func,func_params,func_res) \
  {\
    func_res=func func_params; \
    if (MTLK_ERR_OK == func_res) {\
      (obj)->name##_filled = 1;\
    }\
    else {\
      (obj)->name##_filled = 0;\
    }\
  }

#define MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC_VOID(obj,name,func,func_params) \
  {\
    func func_params; \
    (obj)->name##_filled = 1; \
  }

/* Card Capabilities */
MTLK_DECLARE_CFG_START(mtlk_card_capabilities_t)
  MTLK_CFG_ITEM(uint32, max_vaps_supported)
  MTLK_CFG_ITEM(uint32, max_stas_supported)
  MTLK_CFG_ITEM(uint32, max_acls_supported)
MTLK_DECLARE_CFG_END(mtlk_card_capabilities_t)

/* ADDBA configuration structure */
MTLK_DECLARE_CFG_START(mtlk_addba_tid_cfg_entity_t)
  MTLK_CFG_ITEM(int, use_aggr)                    /* UseAggregation           */
  MTLK_CFG_ITEM(int, accept_aggr)                 /* AcceptAggregation        */
  MTLK_CFG_ITEM(uint16, max_nof_packets)          /* MaxNumOfPackets          */
  MTLK_CFG_ITEM(uint32, max_nof_bytes)            /* MaxNumOfBytes            */
  MTLK_CFG_ITEM(uint32, timeout_interval)         /* TimeoutInterval          */
  MTLK_CFG_ITEM(uint32, min_packet_size_in_aggr)  /* MinSizeOfPacketInAggr    */
  MTLK_CFG_ITEM(uint16, addba_timeout)            /* ADDBA timeout            */
  MTLK_CFG_ITEM(uint8,  aggr_win_size)            /* Aggregation Window Size  */
MTLK_DECLARE_CFG_END(mtlk_addba_tid_cfg_entity_t)

MTLK_DECLARE_CFG_START(mtlk_delba_cfg_entity_t)
  MTLK_CFG_ITEM(mtlk_delba_req_t, delba_req)
MTLK_DECLARE_CFG_END(mtlk_delba_cfg_entity_t)

MTLK_DECLARE_CFG_START(mtlk_addba_cfg_entity_t)
  MTLK_CFG_ITEM_ARRAY(mtlk_addba_tid_cfg_entity_t, tid, MTLK_ADDBA_NOF_TIDs)
MTLK_DECLARE_CFG_END(mtlk_addba_cfg_entity_t)

/* WME configuration structure */
MTLK_DECLARE_CFG_START(mtlk_wme_class_cfg_entity_t)
  MTLK_CFG_ITEM(uint32, cwmin)
  MTLK_CFG_ITEM(uint32, cwmax)
  MTLK_CFG_ITEM(uint32, aifsn)
  MTLK_CFG_ITEM(uint32, txop)
MTLK_DECLARE_CFG_END(mtlk_wme_class_cfg_entity_t)

MTLK_DECLARE_CFG_START(mtlk_wme_cfg_entity_t)
  MTLK_CFG_ITEM_ARRAY(mtlk_wme_class_cfg_entity_t, wme_class, NTS_PRIORITIES)
MTLK_DECLARE_CFG_END(mtlk_wme_cfg_entity_t)

/* AOCS configuration structure */
MTLK_DECLARE_CFG_START(mtlk_aocs_ac_t)
  MTLK_CFG_ITEM_ARRAY(BOOL, ac, NTS_PRIORITIES)
MTLK_DECLARE_CFG_END(mtlk_aocs_ac_t)

MTLK_DECLARE_CFG_START(mtlk_interf_t)
  MTLK_CFG_ITEM(BOOL, is_enabled)
  MTLK_CFG_ITEM(uint8, threshold)
  MTLK_CFG_ITEM(int8, threshold_rssi)
MTLK_DECLARE_CFG_END(mtlk_interf_t)

MTLK_DECLARE_CFG_START(mtlk_aocs_cfg_t)
  MTLK_CFG_ITEM(uint8, weight_ch_load)
  MTLK_CFG_ITEM(uint8, weight_nof_bss)
  MTLK_CFG_ITEM(uint8, weight_tx_power)
  MTLK_CFG_ITEM(uint8, weight_sm_required)
  MTLK_CFG_ITEM(uint8, cfm_rank_sw_threshold)
  MTLK_CFG_ITEM(uint8, alpha_filter_coefficient)
  MTLK_CFG_ITEM(uint8, bonding)
  MTLK_CFG_ITEM(uint8, restrict_mode)
  MTLK_CFG_ITEM(int8, dbg_non_occupied_period)
  MTLK_CFG_ITEM(uint16, udp_msdu_threshold_aocs)
  MTLK_CFG_ITEM(uint16, udp_lower_threshold)
  MTLK_CFG_ITEM(uint16, udp_threshold_window)
  MTLK_CFG_ITEM(uint16, tcp_measurement_window)
  MTLK_CFG_ITEM(uint32, udp_aocs_window_time_ms)
  MTLK_CFG_ITEM(uint32, type)
  MTLK_CFG_ITEM(uint32, udp_msdu_per_window_threshold)
  MTLK_CFG_ITEM(uint32, tcp_throughput_threshold)
  MTLK_CFG_ITEM(BOOL, use_tx_penalties)
  MTLK_CFG_ITEM(BOOL, udp_msdu_debug_enabled)
  MTLK_CFG_ITEM(mtlk_osal_msec_t, scan_aging_ms)
  MTLK_CFG_ITEM(mtlk_osal_msec_t, confirm_rank_aging_ms)
  MTLK_CFG_ITEM_ARRAY(uint8, restricted_channels, MAX_CHANNELS)
  MTLK_CFG_ITEM(mtlk_aocs_ac_t, msdu_tx_ac)
  MTLK_CFG_ITEM(mtlk_aocs_ac_t, msdu_rx_ac)
  MTLK_CFG_ITEM_ARRAY(int32, penalties, MTLK_AOCS_PENALTIES_BUFSIZE)
  MTLK_CFG_ITEM(mtlk_interf_t, non_wifi_interf)
MTLK_DECLARE_CFG_END(mtlk_aocs_cfg_t)

/* DOT11H configuration structure for AP */
MTLK_DECLARE_CFG_START(mtlk_11h_ap_cfg_t)
  MTLK_CFG_ITEM(BOOL, enable_sm_required)
  MTLK_CFG_ITEM(uint16, channel_switch)
  MTLK_CFG_ITEM(uint16, channel_emu)
  MTLK_CFG_ITEM(int16, next_channel)
  MTLK_CFG_ITEM(int8, debugChannelSwitchCount)
  MTLK_CFG_ITEM(int16, debugChannelAvailabilityCheckTime)
MTLK_DECLARE_CFG_END(mtlk_11h_ap_cfg_t)

/* DOT11H configuration structure for AP/STA */
MTLK_DECLARE_CFG_START(mtlk_11h_cfg_t)
  MTLK_CFG_ITEM(uint8, radar_detect)
  MTLK_CFG_ITEM_ARRAY(char, status, MTLK_DOT11H_STATUS_BUFSIZE)
MTLK_DECLARE_CFG_END(mtlk_11h_cfg_t)

/* MIBs configuration structure */
MTLK_DECLARE_CFG_START(mtlk_mibs_cfg_t)
  MTLK_CFG_ITEM(uint32, acl_mode)
  MTLK_CFG_ITEM(uint32, short_cyclic_prefix_rx)
  MTLK_CFG_ITEM(uint32, short_cyclic_prefix_tx)
  MTLK_CFG_ITEM(uint32, short_cyclic_prefix_rate31)
  MTLK_CFG_ITEM(uint32, short_preamble_option)
  MTLK_CFG_ITEM(uint32, short_slot_time_option)
  MTLK_CFG_ITEM(uint32, sm_enable)

  MTLK_CFG_ITEM(uint16, calibr_algo_mask)
  MTLK_CFG_ITEM(uint16, online_calibr_algo_mask)
  MTLK_CFG_ITEM(uint16, short_retry_limit)
  MTLK_CFG_ITEM(uint16, long_retry_limit)
  MTLK_CFG_ITEM(uint16, tx_msdu_lifetime)
  MTLK_CFG_ITEM(uint16, current_tx_antenna)
  MTLK_CFG_ITEM(uint16, beacon_period)
  MTLK_CFG_ITEM(uint16, disconnect_on_nacks_weight)
  MTLK_CFG_ITEM(uint16, rts_threshold)
  MTLK_CFG_ITEM(uint16, tx_power)

  MTLK_CFG_ITEM(uint8, advanced_coding_supported)
  MTLK_CFG_ITEM(uint8, overlapping_protect_enabled)
  MTLK_CFG_ITEM(uint8, ofdm_protect_method)
  MTLK_CFG_ITEM(uint8, ht_method)
  MTLK_CFG_ITEM(uint8, dtim_period)
  MTLK_CFG_ITEM(uint8, receive_ampdu_max_len)
  MTLK_CFG_ITEM(uint8, cb_databins_per_symbol)
  MTLK_CFG_ITEM(uint8, use_long_preamble_for_multicast)
  MTLK_CFG_ITEM(uint8, use_space_time_block_code)
  MTLK_CFG_ITEM(uint8, two_antenna_tx_enable)
  MTLK_CFG_ITEM(uint8, disconnect_on_nacks_enable)

  MTLK_CFG_ITEM_ARRAY(char, tx_antennas, MTLK_NUM_ANTENNAS_BUFSIZE)
  MTLK_CFG_ITEM_ARRAY(char, rx_antennas, MTLK_NUM_ANTENNAS_BUFSIZE)
MTLK_DECLARE_CFG_END(mtlk_mibs_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_country_cfg_t)
  MTLK_CFG_ITEM_ARRAY(char, country, MTLK_CHNLS_COUNTRY_BUFSIZE)
MTLK_DECLARE_CFG_END(mtlk_country_cfg_t)

/* L2NAT configuration structure */
MTLK_DECLARE_CFG_START(mtlk_l2nat_cfg_t)
  MTLK_CFG_ITEM(int32, aging_timeout)
  MTLK_CFG_ITEM(IEEE_ADDR, address)
MTLK_DECLARE_CFG_END(mtlk_l2nat_cfg_t)

/* WDS configuration structure */
MTLK_DECLARE_CFG_START(mtlk_wds_cfg_t)
  MTLK_CFG_ITEM(IEEE_ADDR, peer_ap_addr_add)
  MTLK_CFG_ITEM(IEEE_ADDR, peer_ap_addr_del)
  MTLK_CFG_ITEM(mtlk_clpb_t *, peer_ap_vect)
  MTLK_CFG_ITEM(uint8, peer_ap_key_idx)
MTLK_DECLARE_CFG_END(mtlk_wds_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_wds_dbg_t)
  MTLK_CFG_ITEM(int, dummy)
MTLK_DECLARE_CFG_END(mtlk_wds_dbg_t)

/* DOT11D configuration structure */
MTLK_DECLARE_CFG_START(mtlk_dot11d_cfg_t)
  MTLK_CFG_ITEM(BOOL, is_dot11d)
  MTLK_CFG_ITEM(BOOL, should_reset_tx_limits)
MTLK_DECLARE_CFG_END(mtlk_dot11d_cfg_t)

/* MAC watchdog configuration structure */
MTLK_DECLARE_CFG_START(mtlk_mac_wdog_cfg_t)
  MTLK_CFG_ITEM(uint16, mac_watchdog_timeout_ms)
  MTLK_CFG_ITEM(uint32, mac_watchdog_period_ms)
MTLK_DECLARE_CFG_END(mtlk_mac_wdog_cfg_t)

/* STADB configuration structure */
MTLK_DECLARE_CFG_START(mtlk_stadb_cfg_t)
  MTLK_CFG_ITEM(uint32, sta_keepalive_timeout)
  MTLK_CFG_ITEM(uint32, keepalive_interval)
  MTLK_CFG_ITEM(uint32, aggr_open_threshold)
MTLK_DECLARE_CFG_END(mtlk_stadb_cfg_t)

/* SendQueue configuration structure */
MTLK_DECLARE_CFG_START(mtlk_sq_cfg_t)
  MTLK_CFG_ITEM_ARRAY(int32, sq_limit, NTS_PRIORITIES)
  MTLK_CFG_ITEM_ARRAY(int32, peer_queue_limit, NTS_PRIORITIES)
MTLK_DECLARE_CFG_END(mtlk_sq_cfg_t)

/* General Core configuration structure */
MTLK_DECLARE_CFG_START(mtlk_core_rate_cfg_t)
  MTLK_CFG_ITEM(int, int_rate)
  MTLK_CFG_ITEM(int, frac_rate)
  MTLK_CFG_ITEM(uint16, array_idx)
MTLK_DECLARE_CFG_END(mtlk_core_rate_cfg_t)

/* Aggregation Rate Limit */
MTLK_DECLARE_CFG_START(mtlk_agg_rate_limit_t)
  MTLK_CFG_ITEM(uint8, mode)
  MTLK_CFG_ITEM(uint16, maxRate)
MTLK_DECLARE_CFG_END(mtlk_agg_rate_limit_t)

/* LNA gain */
MTLK_DECLARE_CFG_START(mtlk_lna_gain_t)
  MTLK_CFG_ITEM(int8, lna_gain_bypass)
  MTLK_CFG_ITEM(int8, lna_gain_high)
MTLK_DECLARE_CFG_END(mtlk_lna_gain_t)

/* RX duty cycle */
MTLK_DECLARE_CFG_START(mtlk_rx_duty_cycle_t)
  MTLK_CFG_ITEM(uint32, onTime)
  MTLK_CFG_ITEM(uint32, offTime)
MTLK_DECLARE_CFG_END(mtlk_rx_duty_cycle_t)

/* FW log severity configuration structure */
MTLK_DECLARE_CFG_START(mtlk_fw_log_severity_t)
  MTLK_CFG_ITEM(uint32, newLevel)
  MTLK_CFG_ITEM(uint32, targetCPU)
MTLK_DECLARE_CFG_END(mtlk_fw_log_severity_t)

/* Enhanced 11b */
MTLK_DECLARE_CFG_START(mtlk_enhanced11b_th_t)
  MTLK_CFG_ITEM(uint8, Consecutive11nTH)
  MTLK_CFG_ITEM(uint8, Consecutive11bTH)
MTLK_DECLARE_CFG_END(mtlk_enhanced11b_th_t)

MTLK_DECLARE_CFG_START(mtlk_enhanced11b_cfg_t)
  MTLK_CFG_ITEM(mtlk_enhanced11b_th_t, thresholds)
MTLK_DECLARE_CFG_END(mtlk_enhanced11b_cfg_t)

/* 11b configuration */
MTLK_DECLARE_CFG_START(mtlk_11b_antsel_t)
  MTLK_CFG_ITEM(uint8, txAnt)
  MTLK_CFG_ITEM(uint8, rxAnt)
  MTLK_CFG_ITEM(uint8, rate)
MTLK_DECLARE_CFG_END(mtlk_11b_antsel_t)

MTLK_DECLARE_CFG_START(mtlk_11b_cfg_t)
  MTLK_CFG_ITEM(mtlk_11b_antsel_t, antsel)
MTLK_DECLARE_CFG_END(mtlk_11b_cfg_t)

/* WME configuration structure */
MTLK_DECLARE_CFG_START(mtlk_gen_core_country_name_t)
  MTLK_CFG_ITEM_ARRAY(char, name, 3)
MTLK_DECLARE_CFG_END(mtlk_gen_core_country_name_t)

MTLK_DECLARE_CFG_START(mtlk_gen_core_cfg_t)
  MTLK_CFG_ITEM(int, bridge_mode)
  MTLK_CFG_ITEM(BOOL, dbg_sw_wd_enable)
  MTLK_CFG_ITEM(BOOL, reliable_multicast)
  MTLK_CFG_ITEM(BOOL, ap_forwarding)
  MTLK_CFG_ITEM(uint8, spectrum_mode)
  MTLK_CFG_ITEM(uint8, net_mode)
  MTLK_CFG_ITEM(uint32, basic_rates)
  MTLK_CFG_ITEM(uint32, supported_rates)
  MTLK_CFG_ITEM(uint32, extended_rates)
  MTLK_CFG_ITEM(uint32, num_macs_to_set)
  MTLK_CFG_ITEM_ARRAY(IEEE_ADDR, macs_to_set, MAX_ADDRESSES_IN_ACL)
  MTLK_CFG_ITEM(uint32, num_macs_to_del)
  MTLK_CFG_ITEM_ARRAY(IEEE_ADDR, macs_to_del, MAX_ADDRESSES_IN_ACL)
  MTLK_CFG_ITEM_ARRAY(IEEE_ADDR, mac_mask, MAX_ADDRESSES_IN_ACL)

  MTLK_CFG_ITEM_ARRAY(char, nickname, MTLK_ESSID_MAX_SIZE)
  MTLK_CFG_ITEM_ARRAY(char, essid, MTLK_ESSID_MAX_SIZE)
  MTLK_CFG_ITEM_ARRAY(char, bssid, ETH_ALEN)
  MTLK_CFG_ITEM_ARRAY(mtlk_gen_core_country_name_t, countries_supported, MAX_COUNTRIES)
  MTLK_CFG_ITEM(uint16, channel)
  MTLK_CFG_ITEM(uint8, bonding)
  MTLK_CFG_ITEM(uint8, frequency_band_cur)
  MTLK_CFG_ITEM(BOOL, is_hidden_ssid)
  MTLK_CFG_ITEM(uint32, up_rescan_exemption_time)
MTLK_DECLARE_CFG_END(mtlk_gen_core_cfg_t)

typedef struct _mtlk_core_n_rate_bo_t {
  int8 qam16;
  int8 qam64_2_3;
  int8 qam64_3_4;
  int8 qam64_5_6;
} mtlk_core_n_rate_bo_t;

MTLK_DECLARE_CFG_START(mtlk_master_core_cfg_t)
  MTLK_CFG_ITEM(mtlk_core_rate_cfg_t, legacy_force_rate)
  MTLK_CFG_ITEM(mtlk_core_rate_cfg_t, ht_force_rate)
  MTLK_CFG_ITEM(mtlk_core_n_rate_bo_t, n_rate_bo)
  MTLK_CFG_ITEM(mtlk_agg_rate_limit_t, agg_rate_limit)
  MTLK_CFG_ITEM(mtlk_lna_gain_t, lna_gain)
  MTLK_CFG_ITEM(mtlk_rx_duty_cycle_t, rx_duty_cycle)
  MTLK_CFG_ITEM(int8, rx_high_threshold)
  MTLK_CFG_ITEM(int8, power_selection)
  MTLK_CFG_ITEM(int8, cca_threshold)
  MTLK_CFG_ITEM(uint8, ra_protection)
  MTLK_CFG_ITEM(uint8, force_ncb)
  MTLK_CFG_ITEM(uint8, debug_tpc)
MTLK_DECLARE_CFG_END(mtlk_master_core_cfg_t)

/* HSTDB structure */
MTLK_DECLARE_CFG_START(mtlk_hstdb_cfg_t)
  MTLK_CFG_ITEM(uint32, wds_host_timeout)
  MTLK_CFG_ITEM(IEEE_ADDR, address)
MTLK_DECLARE_CFG_END(mtlk_hstdb_cfg_t)

/* Scan structure */
MTLK_DECLARE_CFG_START(mtlk_scan_cfg_t)
  MTLK_CFG_ITEM(unsigned long, cache_expire)
  MTLK_CFG_ITEM(uint8, channels_per_chunk_limit)
  MTLK_CFG_ITEM(uint16, pause_between_chunks)
  MTLK_CFG_ITEM(BOOL, is_background_scan)

  MTLK_CFG_ITEM_ARRAY(char, essid, MIB_ESSID_LENGTH + 1)
MTLK_DECLARE_CFG_END(mtlk_scan_cfg_t)

/* QoS structure */
MTLK_DECLARE_CFG_START(mtlk_qos_cfg_t)
  MTLK_CFG_ITEM(int, map)
MTLK_DECLARE_CFG_END(mtlk_qos_cfg_t)

/* COC structure */
MTLK_DECLARE_CFG_START(mtlk_coc_mode_cfg_t)
  MTLK_CFG_ITEM(BOOL, is_auto_mode)
  MTLK_CFG_ITEM(mtlk_coc_antenna_cfg_t, antenna_params)
  MTLK_CFG_ITEM(mtlk_coc_auto_cfg_t, auto_params)
MTLK_DECLARE_CFG_END(mtlk_coc_mode_cfg_t)

#ifdef MTCFG_PMCU_SUPPORT
/* PCOC structure */
MTLK_DECLARE_CFG_START(mtlk_pcoc_mode_cfg_t)
  MTLK_CFG_ITEM(BOOL,  is_enabled)
  MTLK_CFG_ITEM(BOOL,  is_active)
  MTLK_CFG_ITEM(uint8, traffic_state)
  MTLK_CFG_ITEM(mtlk_pcoc_params_t, params)
  MTLK_CFG_ITEM(uint32, pmcu_debug)
MTLK_DECLARE_CFG_END(mtlk_pcoc_mode_cfg_t)
#endif /*MTCFG_PMCU_SUPPORT*/

/* EEPROM structure */
MTLK_DECLARE_CFG_START(mtlk_eeprom_data_cfg_t)
  MTLK_CFG_ITEM(uint16, eeprom_version)
  MTLK_CFG_ITEM_ARRAY(uint8, mac_address, ETH_ALEN)
  MTLK_CFG_ITEM(uint8, country_code)
  MTLK_CFG_ITEM(uint8, type)
  MTLK_CFG_ITEM(uint8, revision)
  MTLK_CFG_ITEM(uint16, vendor_id)
  MTLK_CFG_ITEM(uint16, device_id)
  MTLK_CFG_ITEM(uint16, sub_vendor_id)
  MTLK_CFG_ITEM(uint16, sub_device_id)
  MTLK_CFG_ITEM_ARRAY(uint8, sn, MTLK_EEPROM_SN_LEN)
  MTLK_CFG_ITEM(uint8, production_week)
  MTLK_CFG_ITEM(uint8, production_year)
MTLK_DECLARE_CFG_END(mtlk_eeprom_data_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_eeprom_cfg_t)
  MTLK_CFG_ITEM(BOOL, is_if_stopped)
  MTLK_CFG_ITEM(mtlk_eeprom_data_cfg_t, eeprom_data)
  MTLK_CFG_ITEM_ARRAY(uint8, eeprom_raw_data, MTLK_MAX_EEPROM_SIZE)
MTLK_DECLARE_CFG_END(mtlk_eeprom_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_hw_cfg_t)
  MTLK_CFG_ITEM_ARRAY(char, buf, 20) /* name */
  MTLK_CFG_ITEM(int16, field_01)     /* freq */
  MTLK_CFG_ITEM(uint16, field_02)    /* spectrum */
  MTLK_CFG_ITEM(uint8, field_03)     /* tx_lim */
MTLK_DECLARE_CFG_END(mtlk_hw_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_ant_cfg_t)
  MTLK_CFG_ITEM(int32, field_01)     /* freq */
  MTLK_CFG_ITEM(int32, field_02)     /* gain */
MTLK_DECLARE_CFG_END(mtlk_ant_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_tx_power_limit_cfg_t)
  MTLK_CFG_ITEM(int8, field_01)     /* 11B boost */
  MTLK_CFG_ITEM(int8, field_02)     /* BPSK boost */
  MTLK_CFG_ITEM(int8, field_03)     /* Auto-response boost */
MTLK_DECLARE_CFG_END(mtlk_tx_power_limit_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_hw_data_cfg_t)
  MTLK_CFG_ITEM(mtlk_hw_cfg_t, hw_cfg)
  MTLK_CFG_ITEM(mtlk_ant_cfg_t, ant)
  MTLK_CFG_ITEM(mtlk_tx_power_limit_cfg_t, power_limit)
MTLK_DECLARE_CFG_END(mtlk_hw_data_cfg_t)

/* mbss add vap structure - special case: neither setter nor getter */
MTLK_DECLARE_CFG_START(mtlk_mbss_int_add_vap_cfg_t)
  MTLK_CFG_ITEM(uint32, added_vap_index)
MTLK_DECLARE_CFG_END(mtlk_mbss_int_add_vap_cfg_t)

#define MTLK_MBSS_VAP_LIMIT_DEFAULT ((uint32)-1)
#define VAP_LIMIT_SET_SIZE           (2)

MTLK_DECLARE_CFG_START(mtlk_mbss_vap_limit_data_cfg_t)
  MTLK_CFG_ITEM(uint32, lower_limit)
  MTLK_CFG_ITEM(uint32, upper_limit)
MTLK_DECLARE_CFG_END(mtlk_mbss_vap_limit_data_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_mbss_cfg_t)
  MTLK_CFG_ITEM(uint32, deleted_vap_index)
  MTLK_CFG_ITEM(uint32, added_vap_index)
  MTLK_CFG_ITEM(mtlk_mbss_vap_limit_data_cfg_t, vap_limits)
MTLK_DECLARE_CFG_END(mtlk_mbss_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_rcvry_cfg_t)
  MTLK_CFG_ITEM(rcvry_cfg_t, recovery_cfg)
MTLK_DECLARE_CFG_END(mtlk_rcvry_cfg_t)

typedef struct _mtlk_core_ui_gen_data_t{
  WE_GEN_DATAEX_REQUEST request;
  WE_GEN_DATAEX_LED leds_status;
} mtlk_core_ui_gen_data_t;

typedef struct _mtlk_core_ui_range_info_t {
  uint8   num_bitrates;
  int     bitrates[BITRATES_MAX];
  uint8   num_channels;
  uint8   channels[MAX_CHANNELS];
} mtlk_core_ui_range_info_t;

typedef struct _mtlk_core_ui_enc_cfg_t {
  uint8   wep_enabled;
  uint8   authentication;
  uint8   key_id;
  MIB_WEP_DEF_KEYS wep_keys;
} mtlk_core_ui_enc_cfg_t;

typedef struct _mtlk_core_ui_auth_cfg_t {
  int16   wep_enabled;
  int16   rsn_enabled;
  int16   authentication;
} mtlk_core_ui_auth_cfg_t;

typedef struct _mtlk_core_ui_auth_state_t {
  UMI_RSN_IE  rsnie;
  int16       cipher_pairwise;
  uint8       group_cipher;
} mtlk_core_ui_auth_state_t;

typedef struct mtlk_core_ui_mlme_cfg_t {
  uint16      cmd;
  uint16      reason_code;
  IEEE_ADDR   sta_addr;
} mtlk_core_ui_mlme_cfg_t;

MTLK_DECLARE_CFG_START(mtlk_core_ui_genie_cfg_t)
  MTLK_CFG_ITEM(uint8, wps_in_progress)
  MTLK_CFG_ITEM(uint8, rsnie_reset)
  MTLK_CFG_ITEM(uint8, rsn_enabled)
  MTLK_CFG_ITEM(UMI_RSN_IE, rsnie)

  MTLK_CFG_ITEM(uint8, gen_ie_set)
  MTLK_CFG_ITEM(uint8, gen_ie_type)
  MTLK_CFG_ITEM(uint16, gen_ie_len)
  MTLK_CFG_ITEM_ARRAY(uint8, gen_ie, UMI_MAX_GENERIC_IE_SIZE)
MTLK_DECLARE_CFG_END(mtlk_core_ui_genie_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_core_ui_encext_cfg_t)
  MTLK_CFG_ITEM(uint8, wep_enabled)
  MTLK_CFG_ITEM(uint16, alg_type)
  MTLK_CFG_ITEM(IEEE_ADDR, sta_addr)
  MTLK_CFG_ITEM(uint16, default_key_idx)
  MTLK_CFG_ITEM(uint16, key_idx)
  MTLK_CFG_ITEM(uint16, key_len)
  MTLK_CFG_ITEM_ARRAY(uint8, key, UMI_RSN_TK1_LEN + UMI_RSN_TK2_LEN)
  MTLK_CFG_ITEM_ARRAY(uint8, rx_seq, 6)
MTLK_DECLARE_CFG_END(mtlk_core_ui_encext_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_serializer_command_info_t)
  MTLK_CFG_ITEM(uint32, priority)
  MTLK_CFG_ITEM(BOOL, is_current)
  MTLK_CFG_ITEM(mtlk_slid_t, issuer_slid)
MTLK_DECLARE_CFG_END(mtlk_serializer_command_info_t)

typedef struct _mtlk_core_ui_get_stadb_status_req_t {
  uint8      get_hostdb;
  uint8      use_cipher;
} mtlk_core_ui_get_stadb_status_req_t;

/* 20/40 coexistence feature */
/* coexistence 20/40 configuration structures */
MTLK_DECLARE_CFG_START(mtlk_coex_20_40_mode_cfg_t)//pre-activate definition, get ability only
  MTLK_CFG_ITEM(BOOL,   coexistence_mode)
  MTLK_CFG_ITEM(int32,  rssi_threshold)
  MTLK_CFG_ITEM(BOOL,   intolerance_mode)
MTLK_DECLARE_CFG_END(mtlk_coex_20_40_mode_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_coex_20_40_ap_force_params_cfg_t)//AP only, pre-activate definition, get ability only
MTLK_CFG_ITEM(BOOL,   ap_force_scan_params_on_assoc_sta)
MTLK_CFG_ITEM(uint16, wait_for_scan_results_interval)
MTLK_DECLARE_CFG_END(mtlk_coex_20_40_ap_force_params_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_coex_20_40_exm_req_cfg_t)//abilities - only on STA and between start to stop
  MTLK_CFG_ITEM(BOOL,   exemption_req)
MTLK_DECLARE_CFG_END(mtlk_coex_20_40_exm_req_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_coex_20_40_min_num_exm_sta_cfg_t)//abilities - AP only, pre-activate definition, get ability only
  MTLK_CFG_ITEM(uint32,   min_non_exempted_sta)
MTLK_DECLARE_CFG_END(mtlk_coex_20_40_min_num_exm_sta_cfg_t)


MTLK_DECLARE_CFG_START(mtlk_coex_20_40_times_cfg_t)//abilities - AP - always, STA - while not started
  MTLK_CFG_ITEM(uint16, delay_factor)
  MTLK_CFG_ITEM(uint16, obss_scan_interval)
  MTLK_CFG_ITEM(uint16, scan_activity_threshold)
  MTLK_CFG_ITEM(uint16, scan_passive_dwell)
  MTLK_CFG_ITEM(uint16, scan_active_dwell)
  MTLK_CFG_ITEM(uint16, scan_passive_total_per_channel)
  MTLK_CFG_ITEM(uint16, scan_active_total_per_channel)
MTLK_DECLARE_CFG_END(mtlk_coex_20_40_times_cfg_t)

/* Interference Detection */
typedef struct
{
  uint32 active_polling_timeout;
  uint32 short_scan_polling_timeout;
  uint32 long_scan_polling_timeout;
  uint32 active_notification_timeout;
  uint32 short_scan_notification_timeout;
  uint32 long_scan_notification_timeout;
} __MTLK_IDATA mtlk_interfdet_req_timeouts_t;

typedef struct
{
  int8   detection_threshold_20mhz;
  int8   notification_threshold_20mhz;
  int8   detection_threshold_40mhz;
  int8   notification_threshold_40mhz;
  uint8  scan_noise_threshold;
  int8   scan_minimum_noise;
  BOOL   mode;
} __MTLK_IDATA mtlk_interfdet_req_thresh_t;

typedef struct
{
  uint32 short_scan_max_time;
  uint32 long_scan_max_time;              
} __MTLK_IDATA mtlk_interfdet_req_scantimes_t;

MTLK_DECLARE_CFG_START(mtlk_interfdet_cfg_t)
  MTLK_CFG_ITEM(mtlk_interfdet_req_timeouts_t,  req_timeouts)
  MTLK_CFG_ITEM(mtlk_interfdet_req_thresh_t,    req_thresh)
  MTLK_CFG_ITEM(mtlk_interfdet_req_scantimes_t, req_scantimes)
MTLK_DECLARE_CFG_END(mtlk_interfdet_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_interfdet_mode_cfg_t)
  MTLK_CFG_ITEM(BOOL, interfdet_mode)
MTLK_DECLARE_CFG_END(mtlk_interfdet_mode_cfg_t)

typedef struct
{
  uint8  disable_testbus;
  uint8  active_gpios;
  uint8  led_polatity;
} __MTLK_IDATA mtlk_fw_led_gpio_cfg_t;

typedef struct
{
  uint8  baseb_led;
  uint8  led_state;
} __MTLK_IDATA mtlk_fw_led_state_t;

MTLK_DECLARE_CFG_START(mtlk_fw_led_cfg_t);
  MTLK_CFG_ITEM(mtlk_fw_led_gpio_cfg_t,  gpio_cfg);
  MTLK_CFG_ITEM(mtlk_fw_led_state_t,     led_state);
MTLK_DECLARE_CFG_END(mtlk_fw_led_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_dbg_cli_cfg_t);
  MTLK_CFG_ITEM(UmiDbgCliReq_t,  DbgCliReq);
MTLK_DECLARE_CFG_END(mtlk_dbg_cli_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_fw_debug_cfg_t);
  MTLK_CFG_ITEM(UMI_FW_DBG_REQ, FWDebugReq);
MTLK_DECLARE_CFG_END(mtlk_fw_debug_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_mc_ps_size_cfg_t);
  MTLK_CFG_ITEM(uint32, maxNumberOfFsdus);
MTLK_DECLARE_CFG_END(mtlk_mc_ps_size_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_ta_crit_t);
  MTLK_CFG_ITEM(uint32, id);
  MTLK_CFG_ITEM(uint32, signature);
  MTLK_CFG_ITEM(uint32, fcn);
  MTLK_CFG_ITEM(uint32, clb);
  MTLK_CFG_ITEM(uint32, clb_ctx);
  MTLK_CFG_ITEM(uint32, tmr_cnt);
  MTLK_CFG_ITEM(uint32, tmr_period);
MTLK_DECLARE_CFG_END(mtlk_ta_crit_t)

MTLK_DECLARE_CFG_START(mtlk_ta_wss_counter_t);
  MTLK_CFG_ITEM(uint32, prev);
  MTLK_CFG_ITEM(uint32, delta);
MTLK_DECLARE_CFG_END(mtlk_ta_wss_counter_t)

MTLK_DECLARE_CFG_START(mtlk_ta_sta_wss_t);
  MTLK_CFG_ITEM(uint32, sta_entry);
  MTLK_CFG_ITEM(BOOL,   coc_wss_valid);
  MTLK_CFG_ITEM(mtlk_ta_wss_counter_t, coc_rx_bytes);
  MTLK_CFG_ITEM(mtlk_ta_wss_counter_t, coc_tx_bytes);
#ifdef MTCFG_PMCU_SUPPORT
  MTLK_CFG_ITEM(BOOL,   pcoc_wss_valid);
  MTLK_CFG_ITEM(mtlk_ta_wss_counter_t, pcoc_rx_bytes);
  MTLK_CFG_ITEM(mtlk_ta_wss_counter_t, pcoc_tx_bytes);
#endif /*MTCFG_PMCU_SUPPORT*/
MTLK_DECLARE_CFG_END(mtlk_ta_sta_wss_t)

MTLK_DECLARE_CFG_START(mtlk_ta_debug_info_cfg_t)
  MTLK_CFG_ITEM(uint32, nof_crit);
  MTLK_CFG_ITEM(uint32, nof_sta_wss);
  MTLK_CFG_ITEM_ARRAY(mtlk_ta_crit_t, crit, TA_CRIT_NUM);
  MTLK_CFG_ITEM_ARRAY(mtlk_ta_sta_wss_t, sta_wss, 64);
MTLK_DECLARE_CFG_END(mtlk_ta_debug_info_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_ta_cfg_t)
  MTLK_CFG_ITEM(uint32, timer_resolution);
  MTLK_CFG_ITEM(mtlk_ta_debug_info_cfg_t, debug_info);
MTLK_DECLARE_CFG_END(mtlk_ta_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_dot11w_cfg_t)
  MTLK_CFG_ITEM(uint32, pmf_activated);
  MTLK_CFG_ITEM(uint32, pmf_required);
  MTLK_CFG_ITEM(uint32, saq_retr_tmout);
  MTLK_CFG_ITEM(uint32, saq_max_tmout);
MTLK_DECLARE_CFG_END(mtlk_dot11w_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_uapsd_mode_cfg_t)
  MTLK_CFG_ITEM(BOOL, uapsd_enabled)
MTLK_DECLARE_CFG_END(mtlk_uapsd_mode_cfg_t)

MTLK_DECLARE_CFG_START(mtlk_uapsd_cfg_t)
  MTLK_CFG_ITEM(uint32, uapsd_max_sp);
MTLK_DECLARE_CFG_END(mtlk_uapsd_cfg_t)

typedef enum
{
  MTLK_CORE_UI_ASSERT_TYPE_NONE,
  MTLK_CORE_UI_ASSERT_TYPE_FW_UMIPS,
  MTLK_CORE_UI_ASSERT_TYPE_FW_LMIPS,
  MTLK_CORE_UI_ASSERT_TYPE_DRV_DIV0,
  MTLK_CORE_UI_ASSERT_TYPE_DRV_BLOOP,
  MTLK_CORE_UI_ASSERT_TYPE_LAST
} mtlk_core_ui_dbg_assert_type_e; /* MTLK_CORE_REQ_SET_MAC_ASSERT */

typedef enum
{
  MTLK_CORE_UI_RCVRY_TYPE_NONE = RCVRY_TYPE_NONE,
  MTLK_CORE_UI_RCVRY_TYPE_FAST = RCVRY_TYPE_FAST,
  MTLK_CORE_UI_RCVRY_TYPE_FULL = RCVRY_TYPE_FULL,
  MTLK_CORE_UI_RCVRY_TYPE_COMPLETE = RCVRY_TYPE_COMPLETE,
  MTLK_CORE_UI_RCVRY_TYPE_UNDEF = RCVRY_TYPE_UNDEF,
  MTLK_CORE_UI_RCVRY_TYPE_LAST
} mtlk_core_ui_rcvry_type_e;

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

void mtlk_core_abilities_disable_vap_ops(mtlk_vap_handle_t vap_handle);
void mtlk_core_abilities_enable_vap_ops(mtlk_vap_handle_t vap_handle);
void mtlk_core_abilities_disable_11b_abilities(mtlk_vap_handle_t vap_handle);
void mtlk_core_abilities_enable_11b_abilities(mtlk_vap_handle_t vap_handle);
void mtlk_core_abilities_disable_ps_abilities(mtlk_vap_handle_t vap_handle);
void mtlk_core_abilities_enable_ps_abilities(mtlk_vap_handle_t vap_handle);

#endif /* !__MTLK_COREUI_H__ */

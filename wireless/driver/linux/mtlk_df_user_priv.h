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
 * Driver framework implementation for Linux
 *
 */

#ifndef __DF_USER_PRIVATE_H__
#define __DF_USER_PRIVATE_H__

#include <net/iw_handler.h>
#include "mtlk_vap_manager.h"

#define MAX_PROC_STR_LEN     32
#define TEXT_SIZE IW_PRIV_SIZE_MASK /* 2047 */

#ifdef MTCFG_DEBUG
#define AOCS_DEBUG
#endif

#if defined(CONFIG_IFX_PPA_API_DIRECTPATH) || defined(CONFIG_LTQ_PPA_API_DIRECTPATH)
#define MTLK_HAVE_PPA
#endif

typedef struct _mtlk_df_user_t mtlk_df_user_t;

enum {
  PRM_ID_FIRST = 0x7fff, /* Range 0x0000 - 0x7fff reserved for MIBs */
  /* FW Recovery */
  PRM_ID_FW_RECOVERY = (PRM_ID_FIRST + 1), /* number is fixed for applications usage */

  /* AP Capabilities */
  PRM_ID_AP_CAPABILITIES_MAX_STAs,
  PRM_ID_AP_CAPABILITIES_MAX_VAPs,
  PRM_ID_AP_CAPABILITIES_MAX_ACLs,

  /* FW GPIO LED */
  PRM_ID_CFG_LED_GPIO,
  PRM_ID_CFG_LED_STATE,

  /* AOCS configuration ioctles */
  PRM_ID_AOCS_WEIGHT_CL,
  PRM_ID_AOCS_WEIGHT_TX,
  PRM_ID_AOCS_WEIGHT_BSS,
  PRM_ID_AOCS_WEIGHT_SM,
  PRM_ID_AOCS_CFM_RANK_SW_THRESHOLD,
  PRM_ID_AOCS_SCAN_AGING,
  PRM_ID_AOCS_CONFIRM_RANK_AGING,
  PRM_ID_AOCS_EN_PENALTIES,
  PRM_ID_AOCS_AFILTER,
  PRM_ID_AOCS_BONDING,
  PRM_ID_AOCS_MSDU_THRESHOLD,
  PRM_ID_AOCS_WIN_TIME,
  PRM_ID_AOCS_MSDU_PER_WIN_THRESHOLD,
  PRM_ID_AOCS_LOWER_THRESHOLD,
  PRM_ID_AOCS_THRESHOLD_WINDOW,
  PRM_ID_AOCS_MSDU_DEBUG_ENABLED,
  PRM_ID_AOCS_IS_ENABLED,
  PRM_ID_AOCS_MEASUREMENT_WINDOW,
  PRM_ID_AOCS_THROUGHPUT_THRESHOLD,
  PRM_ID_AOCS_NON_OCCUPANCY_PERIOD,
  PRM_ID_AOCS_RESTRICT_MODE,
  PRM_ID_AOCS_RESTRICTED_CHANNELS,
  PRM_ID_AOCS_MSDU_TX_AC,
  PRM_ID_AOCS_MSDU_RX_AC,
  PRM_ID_AOCS_PENALTIES,
  PRM_ID_AOCS_NON_WIFI_NOISE,

  /* ADDBA configuration ioctles */
  PRM_ID_DELBA_REQ,
  PRM_ID_BE_BAUSE,
  PRM_ID_BK_BAUSE,
  PRM_ID_VI_BAUSE,
  PRM_ID_VO_BAUSE,
  PRM_ID_BE_BAACCEPT,
  PRM_ID_BK_BAACCEPT,
  PRM_ID_VI_BAACCEPT,
  PRM_ID_VO_BAACCEPT,
  PRM_ID_BE_BATIMEOUT,
  PRM_ID_BK_BATIMEOUT,
  PRM_ID_VI_BATIMEOUT,
  PRM_ID_VO_BATIMEOUT,
  PRM_ID_BE_BAWINSIZE,
  PRM_ID_BK_BAWINSIZE,
  PRM_ID_VI_BAWINSIZE,
  PRM_ID_VO_BAWINSIZE,
  PRM_ID_BE_AGGRMAXBTS,
  PRM_ID_BK_AGGRMAXBTS,
  PRM_ID_VI_AGGRMAXBTS,
  PRM_ID_VO_AGGRMAXBTS,
  PRM_ID_BE_AGGRMAXPKTS,
  PRM_ID_BK_AGGRMAXPKTS,
  PRM_ID_VI_AGGRMAXPKTS,
  PRM_ID_VO_AGGRMAXPKTS,
  PRM_ID_BE_AGGRMINPTSZ,
  PRM_ID_BK_AGGRMINPTSZ,
  PRM_ID_VI_AGGRMINPTSZ,
  PRM_ID_VO_AGGRMINPTSZ,
  PRM_ID_BE_AGGRTIMEOUT,
  PRM_ID_BK_AGGRTIMEOUT,
  PRM_ID_VI_AGGRTIMEOUT,
  PRM_ID_VO_AGGRTIMEOUT,

  /* WME BSS configuration */
  PRM_ID_BE_AIFSN,
  PRM_ID_BK_AIFSN,
  PRM_ID_VI_AIFSN,
  PRM_ID_VO_AIFSN,
  PRM_ID_BE_CWMAX,
  PRM_ID_BK_CWMAX,
  PRM_ID_VI_CWMAX,
  PRM_ID_VO_CWMAX,
  PRM_ID_BE_CWMIN,
  PRM_ID_BK_CWMIN,
  PRM_ID_VI_CWMIN,
  PRM_ID_VO_CWMIN,
  PRM_ID_BE_TXOP,
  PRM_ID_BK_TXOP,
  PRM_ID_VI_TXOP,
  PRM_ID_VO_TXOP,
  /* WME BSS configuration end */

  /* WME AP configuration */
  PRM_ID_BE_AIFSNAP,
  PRM_ID_BK_AIFSNAP,
  PRM_ID_VI_AIFSNAP,
  PRM_ID_VO_AIFSNAP,
  PRM_ID_BE_CWMAXAP,
  PRM_ID_BK_CWMAXAP,
  PRM_ID_VI_CWMAXAP,
  PRM_ID_VO_CWMAXAP,
  PRM_ID_BE_CWMINAP,
  PRM_ID_BK_CWMINAP,
  PRM_ID_VI_CWMINAP,
  PRM_ID_VO_CWMINAP,
  PRM_ID_BE_TXOPAP,
  PRM_ID_BK_TXOPAP,
  PRM_ID_VI_TXOPAP,
  PRM_ID_VO_TXOPAP,

  /* 11H configuration */
  PRM_ID_11H_ENABLE_SM_CHANNELS,
  PRM_ID_11H_BEACON_COUNT,
  PRM_ID_11H_CHANNEL_AVAILABILITY_CHECK_TIME,
  PRM_ID_11H_EMULATE_RADAR_DETECTION,
  PRM_ID_11H_SWITCH_CHANNEL,
  PRM_ID_11H_NEXT_CHANNEL,
  PRM_ID_11H_RADAR_DETECTION,
  PRM_ID_11H_STATUS,

  /* L2NAT configuration */
  PRM_ID_L2NAT_AGING_TIMEOUT,
  PRM_ID_L2NAT_ADDR_FIRST,
  PRM_ID_L2NAT_DEFAULT_HOST,

  /* WDS configuration */
  PRM_ID_ADD_PEERAP,
  PRM_ID_DEL_PEERAP,
  PRM_ID_PEERAP_KEY_IDX,
  PRM_ID_PEERAP_LIST,
  PRM_ID_PEERAP_DBG,

  /* DOT11D configuration */
  PRM_ID_11D,
  PRM_ID_11D_RESTORE_DEFAULTS,

  /* MAC watchdog configuration */
  PRM_ID_MAC_WATCHDOG_TIMEOUT_MS,
  PRM_ID_MAC_WATCHDOG_PERIOD_MS,

  /* STADB configuration */
  PRM_ID_STA_KEEPALIVE_TIMEOUT,
  PRM_ID_STA_KEEPALIVE_INTERVAL,
  PRM_ID_AGGR_OPEN_THRESHOLD,

  /* SendQueue configuration */
  PRM_ID_SQ_LIMITS,
  PRM_ID_SQ_PEER_LIMITS,

  /* General Core configuration */
  PRM_ID_BRIDGE_MODE,
  PRM_ID_DBG_SW_WD_ENABLE,
  PRM_ID_RELIABLE_MULTICAST,
  PRM_ID_AP_FORWARDING,
  PRM_ID_SPECTRUM_MODE,
  PRM_ID_NETWORK_MODE,
  PRM_ID_CHANNEL,
  PRM_ID_HIDDEN_SSID,
  PRM_ID_FW_LOG_SEVERITY,

  /* Master Core configuration */
  PRM_ID_POWER_SELECTION,
  PRM_ID_DEBUG_TPC,
  PRM_ID_BSS_BASIC_RATE_SET,
  PRM_ID_BSS_SUPPORTED_RATE_SET,
  PRM_ID_BSS_EXTENDED_RATE_SET,
  PRM_ID_CORE_COUNTRIES_SUPPORTED,
  PRM_ID_NICK_NAME,
  PRM_ID_ESSID,
  PRM_ID_BSSID,
  PRM_ID_LEGACY_FORCE_RATE,
  PRM_ID_HT_FORCE_RATE,
  PRM_ID_ACL,
  PRM_ID_ACL_DEL,
  PRM_ID_ACL_RANGE,
  PRM_ID_UP_RESCAN_EXEMPTION_TIME,
  PRM_ID_AGG_RATE_LIMIT,
  PRM_ID_SHORT_PREAMBLE_OPTION_IMPLEMENTED,
  PRM_ID_SHORT_SLOT_TIME_OPTION_ENABLED_11G,
  PRM_ID_VAP_LNA_GAINS,
  PRM_ID_RX_HIGH_THRESHOLD,
  PRM_ID_RX_DUTY_CYCLE,
  PRM_ID_CCA_THRESHOLD,

  /* HSTDB configuration */
  PRM_ID_WDS_HOST_TIMEOUT,
  PRM_ID_HSTDB_LOCAL_MAC,

  /* Scan configuration */
  PRM_ID_SCAN_CACHE_LIFETIME,
  PRM_ID_BG_SCAN_CH_LIMIT,
  PRM_ID_BG_SCAN_PAUSE,
  PRM_ID_IS_BACKGROUND_SCAN,
  PRM_ID_ACTIVE_SCAN_SSID,

  /* EEPROM configuration */
  PRM_ID_EEPROM,

  /* TX limits configuration */
  PRM_ID_HW_LIMITS,
  PRM_ID_ANT_GAIN,
  PRM_ID_TX_POWER_LIMIT,

  /* QoS configuration */
  PRM_ID_USE_8021Q,

#ifdef MTCFG_IRB_DEBUG
  /* IRB pinger configuration */
  PRM_ID_IRB_PINGER_ENABLED,
  PRM_ID_IRB_PINGER_STATS,
#endif
#ifdef MTLK_HAVE_PPA
  /* PPA direct path configuration */
  PRM_ID_PPA_API_DIRECTPATH,
#endif
  /* COC configuration */
  PRM_ID_COC_POWER_MODE,
  PRM_ID_COC_AUTO_PARAMS,

#ifdef MTCFG_PMCU_SUPPORT
  /* PCOC configuration */
  PRM_ID_PCOC_POWER_MODE,
  PRM_ID_PCOC_AUTO_PARAMS,
  PRM_ID_PCOC_PMCU_DEBUG,
#endif

  /* MBSS configuration */
  PRM_ID_VAP_ADD,
  PRM_ID_VAP_DEL,
  PRM_ID_VAP_STA_LIMS,

  /* user requests */
  PRM_ID_AOCS_HISTORY,
  PRM_ID_AOCS_TABLE,
  PRM_ID_AOCS_CHANNELS,

  PRM_ID_REG_LIMITS,
  PRM_ID_CORE_STATUS,
  PRM_ID_DEBUG_L2NAT,
  PRM_ID_DEBUG_MAC_ASSERT,
  PRM_ID_BCL_READ_LM,
  PRM_ID_BCL_READ_UM,
  PRM_ID_BCL_READ_SHRAM,
  PRM_ID_DEBUG_IGMP_READ,
  PRM_ID_RESET_STATS,
  PRM_ID_L2NAT_CLEAR_TABLE,
#ifdef AOCS_DEBUG
  PRM_ID_AOCS_PROC_CL,
#endif
  /* Standart ioctles */
  PRM_ID_CORE_GET_STATS,
  PRM_ID_CORE_RANGE,
  PRM_ID_CORE_MLME,
  PRM_ID_STADB_GET_STATUS,
  PRM_ID_CORE_SCAN,
  PRM_ID_CORE_SCAN_GET_RESULTS,
  PRM_ID_CORE_ENC,
  PRM_ID_CORE_GENIE,
  PRM_ID_CORE_AUTH,
  PRM_ID_CORE_ENCEXT,
  PRM_ID_CORE_MAC_ADDR,

  /* 20/40 coexistence parameters */
  PRM_ID_COEX_MODE,
  PRM_ID_COEX_THRESHOLD,
  PRM_ID_INTOLERANCE_MODE,
  PRM_ID_SCAN_AP_FORCE_SCAN_PARAMS_ON_ASSOC_STA,
  PRM_ID_SCAN_AP_WAIT_FOR_SCAN_RESULTS_INTERVAL,
  PRM_ID_EXEMPTION_REQ,
  PRM_ID_MIN_NON_EXEMPTED_STA,
  PRM_ID_DELAY_FACTOR,
  PRM_ID_OBSS_SCAN_INTERVAL,
  PRM_ID_SCAN_ACTIVITY_THRESHOLD,
  PRM_ID_SCAN_PASSIVE_DWELL,
  PRM_ID_SCAN_ACTIVE_DWELL,
  PRM_ID_SCAN_PASSIVE_TOTAL_PER_CHANNEL,
  PRM_ID_SCAN_ACTIVE_TOTAL_PER_CHANNEL,

  /* Interference Detection */
  PRM_ID_INTERFER_TIMEOUTS,
  PRM_ID_INTERFER_THRESH,
  PRM_ID_INTERFER_SCANTIMES,
  PRM_ID_INTERFERENCE_MODE,

  /* Traffic Analyzer */
  PRM_ID_TA_TIMER_RESOLUTION,
  PRM_ID_TA_DBG,

  PRM_ID_ENHANCED11B_TH,

  /* 11B configuration */
  PRM_ID_11B_ANTENNA_SELECTION,

  /* 802.11w */
  PRM_ID_PMF_ACTIVATED,
  PRM_ID_PMF_REQUIRED,
  PRM_ID_SAQ_RETR_TMOUT,
  PRM_ID_SAQ_MAX_TMOUT,

  /* U-APSD mode */
  PRM_ID_UAPSD_MODE,
  PRM_ID_UAPSD_MAX_SP,

  PRM_ID_DBG_CLI,
  PRM_ID_FW_DEBUG,

  PRM_ID_MC_PS_SIZE,

#ifdef MTCFG_USE_GENL
  PRM_ID_GENL_FAMILY_ID,
#endif

  PRM_ID_RA_PROTECTION,
  PRM_ID_FORCE_NCB,
  PRM_ID_N_RATES_BOS,

  PRM_USE_SHORT_CYCLIC_PREFIX_RX,
  PRM_USE_SHORT_CYCLIC_PREFIX_TX,
  PRM_USE_SHORT_CYCLIC_PREFIX_RATE31,

  PRM_ID_LAST
};


/*********************************************************************
 *               DF User management interface
 *********************************************************************/
mtlk_df_user_t*
mtlk_df_user_create(mtlk_df_t *df);

void
mtlk_df_user_delete(mtlk_df_user_t* df_user);

int
mtlk_df_user_start(mtlk_df_t *df, mtlk_df_user_t *df_user, mtlk_vap_manager_interface_e intf);

void
mtlk_df_user_stop(mtlk_df_user_t *df_user, mtlk_vap_manager_interface_e intf);

/* User-friendly interface/device name */
const char*
mtlk_df_user_get_name(mtlk_df_user_t *df_user);
const uint32
mtlk_df_user_get_flags(mtlk_df_user_t *df_user);
int
mtlk_df_user_set_flags(mtlk_df_user_t *df_user, uint32 newflags);

/* statistic utility functions */
uint32 mtlk_df_get_stat_info_len(void);
int mtlk_df_get_stat_info_idx(uint32 num);
const char* mtlk_df_get_stat_info_name(uint32 num);

/*********************************************************************
 * IOCTL handlers
 *********************************************************************/
int __MTLK_IFUNC
mtlk_df_ui_iw_bcl_mac_data_get (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_iw_bcl_mac_data_set (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu, char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_stop_lower_mac (struct net_device *dev,
                                       struct iw_request_info *info,
                                       union iwreq_data *wrqu,
                                       char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_mac_calibrate (struct net_device *dev,
                                      struct iw_request_info *info,
                                      union iwreq_data *wrqu,
                                      char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_iw_generic (struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu,
                                   char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_ctrl_mac_gpio (struct net_device *dev,
                                      struct iw_request_info *info,
                                      union iwreq_data *wrqu,
                                      char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getname (struct net_device *dev,
                          struct iw_request_info *info,
                          char *name,
                          char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getmode (struct net_device *dev,
                                struct iw_request_info *info,
                                u32 *mode,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setnick (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getnick (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setessid (struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu,
                           char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getessid (struct net_device *dev,
                           struct iw_request_info *info,
                           union iwreq_data *wrqu,
                           char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getap (struct net_device *dev,
                              struct iw_request_info *info,
                              union iwreq_data *wrqu,
                              char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getfreq (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setfreq (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setrtsthr (struct net_device *dev,
                                  struct iw_request_info *info,
                                  union iwreq_data *wrqu,
                                  char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getrtsthr (struct net_device *dev,
                                  struct iw_request_info *info,
                                  union iwreq_data *wrqu,
                                  char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_gettxpower (struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu,
                                   char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_settxpower (struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu,
                                   char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setretry (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getretry (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_int (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_int (struct net_device *dev,
                                struct iw_request_info *info,
                                union iwreq_data *wrqu,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_text (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_text (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_intvec (struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu,
                                   char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_intvec (struct net_device *dev,
                                   struct iw_request_info *info,
                                   union iwreq_data *wrqu,
                                   char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_addr (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_addr (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_addrvec (struct net_device *dev,
                                    struct iw_request_info *info,
                                    union iwreq_data *wrqu,
                                    char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_addrvec (struct net_device *dev,
                                    struct iw_request_info *info,
                                    union iwreq_data *wrqu,
                                    char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_mac_addr (struct net_device *dev,
                                     struct iw_request_info *info,
                                     union  iwreq_data *wrqu,
                                     char   *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_mac_addr (struct net_device *dev,
                                     struct iw_request_info *info,
                                     union  iwreq_data *wrqu,
                                     char   *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setap (struct net_device *dev,
                              struct iw_request_info *info,
                              struct sockaddr *sa,
                              char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getrange (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setmlme (struct net_device *dev,
                                struct iw_request_info *info,
                                struct iw_point *data,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_set_scan (struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_scan_results (struct net_device *dev,
                                         struct iw_request_info *info,
                                         union iwreq_data *wrqu,
                                         char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getaplist (struct net_device *dev,
                                  struct iw_request_info *info,
                                  union iwreq_data *wrqu,
                                  char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_connection_info (struct net_device *dev,
                                            struct iw_request_info *info,
                                            union iwreq_data *wrqu,
                                            char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setenc (struct net_device *dev,
                               struct iw_request_info *info,
                               union  iwreq_data *wrqu,
                               char   *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getenc (struct net_device *dev,
                               struct iw_request_info *info,
                               union  iwreq_data *wrqu,
                               char   *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setauth (struct net_device *dev,
                                struct iw_request_info *info,
                                union  iwreq_data *wrqu,
                                char   *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getauth (struct net_device *dev,
                                struct iw_request_info *info,
                                struct iw_param *param,
                                char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setgenie (struct net_device *dev,
                                 struct iw_request_info *info,
                                 struct iw_point *data,
                                 char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_getencext (struct net_device *dev,
                                  struct iw_request_info *info,
                                  struct iw_point *encoding,
                                  char *extra);


int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_setencext (struct net_device *dev,
                                  struct iw_request_info *info,
                                  struct iw_point *encoding,
                                  char *extra);

struct iw_statistics* __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_get_iw_stats (struct net_device *dev);

int __MTLK_IFUNC
mtlk_df_ui_linux_ioctl_bcl_drv_data_exchange (struct net_device *dev,
            struct iw_request_info *info,
            union iwreq_data *wrqu, char *extra);

#endif /*__DF_USER_PRIVATE_H__*/

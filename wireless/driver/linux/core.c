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
 * Core functionality
 *
 */
#include "mtlkinc.h"
#include "core_priv.h"
#include "mtlk_df.h"
#include "core.h"
#include "mtlkhal.h"
#include "mtlk_coreui.h"
#include "drvver.h"
#include "mhi_mac_event.h"
#include "mtlk_packets.h"
#include "mtlkparams.h"
#include "nlmsgs.h"
#include "aocs.h"
#include "aocshistory.h"
#include "sq.h"
#include "mtlk_snprintf.h"
#include "eeprom.h"
#include "dfs.h"
#include "bitrate.h"
#include "mtlk_fast_mem.h"
#include "mtlk_gpl_helper.h"
#include "wpa.h"
#include "mtlkaux.h"
#include "mtlk_param_db.h"
#include "mtlkwssa_drvinfo.h"
#include "coex20_40.h"
#include "mtlk_wssd.h"
#include "wds.h"
#include "ta.h"
#include "core_common.h"
#include "mtlk_df_nbuf.h"
#include "bt_acs.h"
#include "mtlk_coc.h"
#ifdef MTCFG_PMCU_SUPPORT
  #include "mtlk_pcoc.h"
#endif /*MTCFG_PMCU_SUPPORT*/
#include "mtlk_ps.h"
#include "fw_recovery.h"

#define DEFAULT_NUM_TX_ANTENNAS NUM_TX_ANTENNAS_GEN3
#define DEFAULT_NUM_RX_ANTENNAS (3)

#define LOG_LOCAL_GID   GID_CORE
#define LOG_LOCAL_FID   4

#ifdef MTCFG_DEBUG
#define AOCS_DEBUG
#endif

/* acessors for configuration data passed from DF to Core */
#define MTLK_CFG_START_CHEK_ITEM_AND_CALL() do{
#define MTLK_CFG_END_CHEK_ITEM_AND_CALL() }while(0);

#define MTLK_CFG_CHECK_ITEM_VOID(obj,name) \
  if(1!=(obj)->name##_filled) {break;}

#define MTLK_CFG_CHECK_ITEM_AND_CALL(obj,name,func,func_args,func_res) \
  if(1==(obj)->name##_filled){func_res=func func_args;if(MTLK_ERR_OK != func_res)break;}

#define MTLK_CFG_CHECK_ITEM_AND_CALL_VOID MTLK_CFG_GET_ITEM_BY_FUNC_VOID

#define MTLK_CFG_CHECK_ITEM_AND_CALL_EX(obj,name,func,func_args,func_res,etalon_res) \
  if(1==(obj)->name##_filled) {\
    func_res=func func_args;\
    if(etalon_res != func_res) {\
      func_res = MTLK_ERR_UNKNOWN;\
      break;\
    }else {\
      func_res=MTLK_ERR_OK;\
    }\
  }

#define MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(obj,name,func,mibid,retdata,core) \
  if (!mtlk_mib_request_is_allowed(core->vap_handle, mibid)) { \
  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(obj, name, func,(mtlk_vap_get_txmm(mtlk_core_get_master(core)->vap_handle), mibid,(retdata))); \
  } else { \
  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(obj, name, func,(mtlk_vap_get_txmm(core->vap_handle), mibid,(retdata))); \
  } \
  ILOG2_DDDD("CID-%04x: Read Mib 0x%x, Val 0x%x, Vap 0x%x", mtlk_vap_get_oid(core->vap_handle), mibid, *(retdata), mtlk_vap_get_id(core->vap_handle));

#define UAPSD_SET_AC_LIMIT_AND_UAPSD_ENABLED(ac, uapsd_max_sp)              \
  if (MTLK_BFIELD_GET(psConnect->uapsdParams, UAPSD_BITMAP_##ac##_AC)) {    \
        mtlk_osal_atomic_set(&ppeer->limit_per_ac[AC_##ac], uapsd_max_sp);  \
        uapsd_enabled = TRUE;                                               \
  }

static int
_mtlk_core_on_peer_disconnect(mtlk_core_t *core,
                             sta_entry   *sta,
                             uint16       reason,
                             uint16       fail_reason);

typedef enum __mtlk_core_async_priorities_t
{
  _MTLK_CORE_PRIORITY_MAINTENANCE = 0,
  _MTLK_CORE_PRIORITY_NETWORK,
  _MTLK_CORE_PRIORITY_INTERNAL,
  _MTLK_CORE_PRIORITY_USER,
  _MTLK_CORE_PRIORITY_EMERGENCY,
  _MTLK_CORE_NUM_PRIORITIES
} _mtlk_core_async_priorities_t;

#define SCAN_CACHE_AGEING (3600) /* 1 hour */

static const IEEE_ADDR EMPTY_MAC_ADDR = { {0x00, 0x00, 0x00, 0x00, 0x00, 0x00} };
static const IEEE_ADDR EMPTY_MAC_MASK = { {0xFF, 0xFF, 0xFF, 0xFF, 0xFF, 0xFF} };

/* driver is halted - in case if was not initialized yet
   or after critical error */
#define NET_STATE_HALTED         (1 << 0)
/* driver is initializing */
#define NET_STATE_IDLE           (1 << 1)
/* driver has been initialized */
#define NET_STATE_READY          (1 << 2)
/* activation request was sent - waiting for CFM */
#define NET_STATE_ACTIVATING     (1 << 3)
/* got connection event */
#define NET_STATE_CONNECTED      (1 << 4)
/* disconnect started */
#define NET_STATE_DISCONNECTING  (1 << 5)

#define  BF_LNA_GAIN_BYPASS  MTLK_BFIELD_INFO(0, 8)    /*  8 bits starting bit0 */
#define  BF_LNA_GAIN_HIGH    MTLK_BFIELD_INFO(8, 8)    /*  8 bits starting bit8 */
#define  BF_LNA_GAIN_IS_SET  MTLK_BFIELD_INFO(31, 1)   /*  1 bit  starting bit31 */

static const uint32 _mtlk_core_wss_id_map[] =
{
  MTLK_WWSS_WLAN_STAT_ID_PACKETS_SENT,                                             /* MTLK_CORE_CNT_PACKETS_SENT                                                    */
  MTLK_WWSS_WLAN_STAT_ID_PACKETS_RECEIVED,                                         /* MTLK_CORE_CNT_PACKETS_RECEIVED                                                */
  MTLK_WWSS_WLAN_STAT_ID_BYTES_SENT,                                               /* MTLK_CORE_CNT_BYTES_SENT                                                      */
  MTLK_WWSS_WLAN_STAT_ID_BYTES_RECEIVED,                                           /* MTLK_CORE_CNT_BYTES_RECEIVED                                                  */
  MTLK_WWSS_WLAN_STAT_ID_UNICAST_PACKETS_SENT,                                     /* MTLK_CORE_CNT_UNICAST_PACKETS_SENT */
  MTLK_WWSS_WLAN_STAT_ID_UNICAST_PACKETS_RECEIVED,                                 /* MTLK_CORE_CNT_UNICAST_PACKETS_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_PACKETS_SENT,                                   /* MTLK_CORE_CNT_MULTICAST_PACKETS_SENT */
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_PACKETS_RECEIVED,                               /* MTLK_CORE_CNT_MULTICAST_PACKETS_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_PACKETS_SENT,                                   /* MTLK_CORE_CNT_BROADCAST_PACKETS_SENT */
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_PACKETS_RECEIVED,                               /* MTLK_CORE_CNT_BROADCAST_PACKETS_RECEIVED */

#if MTLK_MTIDL_WLAN_STAT_FULL
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_FW,                                  /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_FW                                         */
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_TOO_OLD,                         /* MTLK_CORE_CNT_RX_PACKETS_DISCARDED_DRV_TOO_OLD */
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_DUPLICATE,                       /* MTLK_CORE_CNT_RX_PACKETS_DISCARDED_DRV_DUPLICATE */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_NO_PEERS,                        /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_NO_PEERS                                   */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_ACM,                             /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_ACM                                    */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_EAPOL_CLONED,                    /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_EAPOL_CLONED                               */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_DIRECTED,    /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_DIRECTED           */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_MCAST,       /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_MCAST              */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_NO_RESOURCES,                    /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_NO_RESOURCES                           */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_SQ_OVERFLOW,                     /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_SQ_OVERFLOW                                */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_EAPOL_FILTER,                    /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_EAPOL_FILTER                               */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_DROP_ALL_FILTER,                 /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DROP_ALL_FILTER                            */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_TX_QUEUE_OVERFLOW,               /* MTLK_CORE_CNT_TX_PACKETS_DISCARDED_TX_QUEUE_OVERFLOW                          */
  MTLK_WWSS_WLAN_STAT_ID_802_1X_PACKETS_RECEIVED,                                  /* MTLK_CORE_CNT_802_1X_PACKETS_RECEIVED                                         */
  MTLK_WWSS_WLAN_STAT_ID_802_1X_PACKETS_SENT,                                      /* MTLK_CORE_CNT_802_1X_PACKETS_SENT                                             */
  MTLK_WWSS_WLAN_STAT_ID_PAIRWISE_MIC_FAILURE_PACKETS,                              /* MTLK_CORE_CNT_PAIRWISE_MIC_FAILURE_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_GROUP_MIC_FAILURE_PACKETS,                                 /* MTLK_CORE_CNT_GROUP_MIC_FAILURE_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_UNICAST_REPLAYED_PACKETS,                                  /* MTLK_CORE_CNT_UNICAST_REPLAYED_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_REPLAYED_PACKETS,                                /* MTLK_CORE_CNT_MULTICAST_REPLAYED_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_FWD_RX_PACKETS,                                            /* MTLK_CORE_CNT_FWD_RX_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_FWD_RX_BYTES,                                              /* MTLK_CORE_CNT_FWD_RX_BYTES */
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_BYTES_SENT,                                      /* MTLK_CORE_CNT_MULTICAST_BYTES_SENT */
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_BYTES_RECEIVED,                                  /* MTLK_CORE_CNT_MULTICAST_BYTES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_BYTES_SENT,                                      /* MTLK_CORE_CNT_BROADCAST_BYTES_SENT */
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_BYTES_RECEIVED,                                  /* MTLK_CORE_CNT_BROADCAST_BYTES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_DAT_FRAMES_RECEIVED,                                       /* MTLK_CORE_CNT_DAT_FRAMES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_CTL_FRAMES_RECEIVED,                                       /* MTLK_CORE_CNT_CTL_FRAMES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_MAN_FRAMES_RECEIVED,                                       /* MTLK_CORE_CNT_MAN_FRAMES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_RECEIVED,                                     /* MTLK_CORE_CNT_COEX_EL_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_REQUESTED,                      /* MTLK_CORE_CNT_COEX_EL_SCAN_EXEMPTION_REQUESTED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_GRANTED,                        /* MTLK_CORE_CNT_COEX_EL_SCAN_EXEMPTION_GRANTED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED,                /* MTLK_CORE_CNT_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_20_TO_40,                               /* MTLK_CORE_CNT_CHANNEL_SWITCH_20_TO_40 */
  MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_40_TO_20,                               /* MTLK_CORE_CNT_CHANNEL_SWITCH_40_TO_20 */
  MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_40_TO_40,                               /* MTLK_CORE_CNT_CHANNEL_SWITCH_40_TO_40 */
  MTLK_WWSS_WLAN_STAT_ID_AGGR_ACTIVE,                                               /* MTLK_CORE_CNT_AGGR_ACTIVE  */
  MTLK_WWSS_WLAN_STAT_ID_REORD_ACTIVE,                                              /* MTLK_CORE_CNT_REORD_ACTIVE */
  MTLK_WWSS_WLAN_STAT_ID_SQ_DPCS_SCHEDULED,                                         /* MTLK_CORE_CNT_SQ_DPCS_SCHEDULED */
  MTLK_WWSS_WLAN_STAT_ID_SQ_DPCS_ARRIVED,                                           /* MTLK_CORE_CNT_SQ_DPCS_ARRIVED */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_REQ_RECEIVED,         /* MTLK_CORE_CNT_RX_ADDBA_REQ_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_CFMD_FAIL,        /* MTLK_CORE_CNT_RX_ADDBA_RES_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_CFMD_SUCCESS,     /* MTLK_CORE_CNT_RX_ADDBA_RES_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_LOST,             /* MTLK_CORE_CNT_RX_ADDBA_RES_LOST */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_NEGATIVE_SENT,    /* MTLK_CORE_CNT_RX_ADDBA_RES_NEGATIVE_SENT */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_NOT_CFMD,         /* MTLK_CORE_CNT_RX_ADDBA_RES_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_POSITIVE_SENT,    /* MTLK_CORE_CNT_RX_ADDBA_RES_POSITIVE_SENT */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_REACHED,          /* MTLK_CORE_CNT_RX_ADDBA_RES_REACHED */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_RETRANSMISSIONS,  /* MTLK_CORE_CNT_RX_ADDBA_RES_RETRANSMISSIONS */
  MTLK_WWSS_WLAN_STAT_ID_RX_BAR_WITHOUT_REORDERING,     /* MTLK_CORE_CNT_RX_BAR_WITHOUT_REORDERING */
  MTLK_WWSS_WLAN_STAT_ID_RX_AGGR_PKT_WITHOUT_REORDERING,/* MTLK_CORE_CNT_RX_AGGR_PKT_WITHOUT_REORDERING */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_CFMD_FAIL,        /* MTLK_CORE_CNT_RX_DELBA_REQ_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_CFMD_SUCCESS,     /* MTLK_CORE_CNT_RX_DELBA_REQ_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_LOST,             /* MTLK_CORE_CNT_RX_DELBA_REQ_LOST */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_NOT_CFMD,         /* MTLK_CORE_CNT_RX_DELBA_REQ_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_RCV,              /* MTLK_CORE_CNT_RX_DELBA_REQ_RCV */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_REACHED,          /* MTLK_CORE_CNT_RX_DELBA_REQ_REACHED */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_RETRANSMISSIONS,  /* MTLK_CORE_CNT_RX_DELBA_REQ_RETRANSMISSIONS */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_SENT,             /* MTLK_CORE_CNT_RX_DELBA_REQ_SENT */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_SENT_BY_TIMEOUT,      /* MTLK_CORE_CNT_RX_DELBA_SENT_BY_TIMEOUT */
  MTLK_WWSS_WLAN_STAT_ID_TX_ACK_ON_BAR_DETECTED,        /* MTLK_CORE_CNT_TX_ACK_ON_BAR_DETECTED */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_CFMD_FAIL,        /* MTLK_CORE_CNT_TX_ADDBA_REQ_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_CFMD_SUCCESS,     /* MTLK_CORE_CNT_TX_ADDBA_REQ_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_NOT_CFMD,         /* MTLK_CORE_CNT_TX_ADDBA_REQ_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_SENT,             /* MTLK_CORE_CNT_TX_ADDBA_REQ_SENT */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_RCV_NEGATIVE,     /* MTLK_CORE_CNT_TX_ADDBA_RES_RCV_NEGATIVE */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_RCV_POSITIVE,     /* MTLK_CORE_CNT_TX_ADDBA_RES_RCV_POSITIVE */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_TIMEOUT,          /* MTLK_CORE_CNT_TX_ADDBA_RES_TIMEOUT */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_CFMD_FAIL,       /* MTLK_CORE_CNT_TX_CLOSE_AGGR_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_CFMD_SUCCESS,    /* MTLK_CORE_CNT_TX_CLOSE_AGGR_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_NOT_CFMD,        /* MTLK_CORE_CNT_TX_CLOSE_AGGR_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_SENT,            /* MTLK_CORE_CNT_TX_CLOSE_AGGR_SENT */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_CFMD_FAIL,        /* MTLK_CORE_CNT_TX_DELBA_REQ_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_CFMD_SUCCESS,     /* MTLK_CORE_CNT_TX_DELBA_REQ_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_LOST,             /* MTLK_CORE_CNT_TX_DELBA_REQ_LOST */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_NOT_CFMD,         /* MTLK_CORE_CNT_TX_DELBA_REQ_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_RCV,              /* MTLK_CORE_CNT_TX_DELBA_REQ_RCV */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_REACHED,          /* MTLK_CORE_CNT_TX_DELBA_REQ_REACHED */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_RETRANSMISSIONS,  /* MTLK_CORE_CNT_TX_DELBA_REQ_RETRANSMISSIONS */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_SENT,             /* MTLK_CORE_CNT_TX_DELBA_REQ_SENT */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_CFMD_FAIL,        /* MTLK_CORE_CNT_TX_OPEN_AGGR_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_CFMD_SUCCESS,     /* MTLK_CORE_CNT_TX_OPEN_AGGR_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_NOT_CFMD,         /* MTLK_CORE_CNT_TX_OPEN_AGGR_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_SENT,             /* MTLK_CORE_CNT_TX_OPEN_AGGR_SENT */
#endif /* MTLK_MTIDL_WLAN_STAT_FULL */
};

/* API between Core and HW */
static int
_mtlk_core_start (mtlk_vap_handle_t vap_handle);
static int
_mtlk_core_release_tx_data (mtlk_vap_handle_t vap_handle, const mtlk_core_release_tx_data_t *data);
static int
_mtlk_core_handle_rx_data (mtlk_vap_handle_t vap_handle, mtlk_core_handle_rx_data_t *data);
static void
_mtlk_core_handle_rx_ctrl (mtlk_vap_handle_t vap_handle, uint32 id, void *payload, uint32 payload_buffer_size);
static int
_mtlk_core_get_prop (mtlk_vap_handle_t vap_handle, mtlk_core_prop_e prop_id, void* buffer, uint32 size);
static int
_mtlk_core_set_prop (mtlk_vap_handle_t vap_handle, mtlk_core_prop_e prop_id, void *buffer, uint32 size);
static void
_mtlk_core_stop (mtlk_vap_handle_t vap_handle);
static void
_mtlk_core_prepare_stop (mtlk_vap_handle_t vap_handle);

static mtlk_core_vft_t const core_vft = {
  _mtlk_core_start,
  _mtlk_core_release_tx_data,
  _mtlk_core_handle_rx_data,
  _mtlk_core_handle_rx_ctrl,
  _mtlk_core_get_prop,
  _mtlk_core_set_prop,
  _mtlk_core_stop,
  _mtlk_core_prepare_stop
};

/* API between Core and DF UI */
static int  __MTLK_IFUNC
_mtlk_core_get_ant_gain(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_aocs_table(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_aocs_history(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_aocs_channels(mtlk_handle_t hcore, const void* data, uint32 data_size);
#ifdef BT_ACS_DEBUG
static int __MTLK_IFUNC
_mtlk_core_set_aocs_channels_dbg(mtlk_handle_t hcore, const void* data, uint32 data_size);
#endif /* BT_ACS_DEBUG */
static int __MTLK_IFUNC
_mtlk_core_get_aocs_penalties(mtlk_handle_t hcore, const void* data, uint32 data_size);
#ifdef AOCS_DEBUG
static int __MTLK_IFUNC
_mtlk_core_get_aocs_debug_update_cl(mtlk_handle_t hcore, const void* data, uint32 data_size);
#endif /* AOCS_DEBUG */
static int __MTLK_IFUNC
_mtlk_core_get_hw_limits(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_reg_limits(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_ee_caps(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int
_mtlk_core_get_stadb_sta_list(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_l2nat_stats(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_sq_status(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_ps_status(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_set_mac_assert(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_mc_igmp_tbl(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_bcl_mac_data_get (mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_bcl_mac_data_set (mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_range_info_get (mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_connect_sta(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
handleDisconnectMe(mtlk_handle_t core_object, const void *payload, uint32 size);
static int __MTLK_IFUNC
_mtlk_core_ap_disconnect_sta(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_start_scanning(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_scanning_res(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_set_wep_enc_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_wep_enc_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_set_auth_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_auth_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_set_genie_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_get_enc_ext_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int __MTLK_IFUNC
_mtlk_core_set_enc_ext_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size);
static int
_mtlk_core_get_status(mtlk_handle_t hcore, const void* data, uint32 data_size);

/* Core utilities */
static void
_mtlk_core_bswap_bcl_request(UMI_BCL_REQUEST *req, BOOL hdr_only);
static int
_mtlk_core_set_channel(mtlk_core_t *core, uint16 channel, uint8 spectrum_mode, BOOL userCfg);
static void
mtlk_core_configuration_dump(mtlk_core_t *core);
static int
mtlk_core_set_acl(struct nic *nic, IEEE_ADDR *mac, IEEE_ADDR *mac_mask);
static int
mtlk_core_del_acl(struct nic *nic, IEEE_ADDR *mac);
static int
mtlk_core_set_gen_ie(struct nic *nic, u8 *ie, u16 ie_len, u8 ie_type);
static int
mtlk_core_set_user_bonding(mtlk_core_t *core, uint8 bonding);
static uint32
mtlk_core_get_available_bitrates (struct nic *nic);
static int
mtlk_core_update_network_mode(mtlk_core_t* nic, uint8 net_mode);


static void
_mtlk_core_country_code_set_default(mtlk_core_t* core);
static void
_mtlk_core_sta_country_code_update_on_connect(mtlk_core_t* core, uint8 country_code);
static void
_mtlk_core_sta_country_code_set_default_on_activate(mtlk_core_t* core);
static int
_mtlk_core_set_country_from_ui(mtlk_core_t *core, char *val);
static int
  _mtlk_core_set_is_dot11d(mtlk_core_t *core, BOOL is_dot11d);
static void
  _mtlk_core_switch_cb_mode_callback(mtlk_handle_t context, uint16 primary_channel, int secondary_channel_offset);
static void
  _mtlk_core_send_ce_callback(mtlk_handle_t context, UMI_COEX_EL *coexistence_element);
static int
  __MTLK_IFUNC _mtlk_core_send_cmf_serialized_callback(mtlk_handle_t context, const void *payload, uint32 size);
static void
  _mtlk_core_send_cmf_callback(mtlk_handle_t context, const UMI_COEX_FRAME *coexistence_frame);
static int
_mtlk_core_send_exemption_policy_serialized_callback(mtlk_handle_t context, const void *payload, uint32 size);
static void
  _mtlk_core_send_exemption_policy_callback(mtlk_handle_t context, BOOL must_grant_exemption_requests);
static int 
  __MTLK_IFUNC _mtlk_core_scan_async_serialized_callback(mtlk_handle_t context, const void *payload, uint32 size);
static int
  _mtlk_core_scan_async_callback(mtlk_handle_t context, BOOL scan_5mhz_band);
static void
  _mtlk_core_scan_set_obss_callback(mtlk_handle_t context, BOOL is_background);
static int
  _mtlk_core_register_scan_completion_notification_callback(mtlk_handle_t caller_context, mtlk_handle_t core_context, scan_completed_notification_callback_type *callback);
static int _mtlk_core_enumerate_bss_info_callback(mtlk_handle_t caller_context,
  mtlk_handle_t core_context, bss_enumerator_callback_type callback, uint32 expiration_time);
#ifdef BT_ACS_DEBUG
static int _mtlk_core_enumerate_bss_info_by_aocs_callback(mtlk_handle_t caller_context,
  mtlk_handle_t core_context, bss_enumerator_callback_type callback, uint32 expiration_time);
#endif /* BT_ACS_DEBUG */
static int _mtlk_core_ability_control_callback(mtlk_handle_t context,
  eABILITY_OPS operation, const uint32* ab_id_list, uint32 ab_id_num);
static uint16 _mtlk_core_get_cur_channels_callback(mtlk_handle_t context,
  int *secondary_channel_offset);
static unsigned long _mtlk_core_modify_cache_expire_callback (mtlk_handle_t context, unsigned long new_time, BOOL force_change);
static int _mtlk_core_update_fw_obss_scan_parameters_callback (mtlk_handle_t context);
static int _mtlk_core_create_20_40(struct nic* nic);
static void _mtlk_core_delete_20_40(struct nic* nic);
static void _mtlk_core_notify_ap_of_station_connection(struct nic *nic, const IEEE_ADDR  *addr, const UMI_RSN_IE *rsn_ie,
  BOOL supports_20_40, BOOL received_scan_exemption, BOOL is_intolerant, BOOL is_legacy);
static void _mtlk_core_notify_ap_of_station_disconnection(struct nic *nic, const IEEE_ADDR  *addr);
static int _mtlk_core_set_wep (struct nic *nic, int wep_enabled);
static uint8 _mtlk_core_get_user_spectrum_mode(mtlk_core_t *core);
static int _mtlk_core_set_fw_interfdet_req(mtlk_core_t *core, BOOL interfdet_mode, BOOL is_spectrum_40, BOOL is_long_scan);
static int _mtlk_core_set_mbss_vap_limits(mtlk_core_t *core, uint32 min_limit, uint32 max_limit);

int __MTLK_IFUNC _mtlk_core_get_mc_ps_size_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size);
int __MTLK_IFUNC mtlk_core_set_mc_ps_size (struct nic *nic, uint32 data);

static int _mtlk_core_process_antennas_configuration(mtlk_core_t *nic);
static int _mtlk_core_set_sm_required(mtlk_core_t *core, BOOL enable_sm_required);
static __INLINE BOOL _mtlk_core_has_connections(mtlk_core_t *core);
static __INLINE int _mtlk_core_save_tx_power_limit(mtlk_core_t *core, mtlk_tx_power_limit_cfg_t *cfg);
static int _mtlk_core_send_lna_gains_on_preactivate (mtlk_core_t *core);

static int mtlk_core_set_ra_protection (mtlk_core_t *core);
static int mtlk_core_set_force_ncb (mtlk_core_t *core);
static int mtlk_core_set_n_rate_bo (mtlk_core_t *core);

static int _mtlk_core_send_current_rx_high_threshold (mtlk_core_t *core);

static int _mtlk_core_send_current_cca_threshold (mtlk_core_t *core);

/* with checking ALLOWED option */
#define _mtlk_core_get_cnt(core, id)        (TRUE == id##_ALLOWED) ? __mtlk_core_get_cnt(core, id) : 0

static __INLINE uint32
__mtlk_core_get_cnt (mtlk_core_t       *core,
                    mtlk_core_wss_cnt_id_e cnt_id)
{
  MTLK_ASSERT(cnt_id >= 0 && cnt_id < MTLK_CORE_CNT_LAST);

  return mtlk_wss_get_stat(core->wss, cnt_id);
}

static __INLINE void
_mtlk_core_on_mic_failure (mtlk_core_t       *core,
                           mtlk_df_ui_mic_fail_type_t mic_fail_type)
{
  MTLK_ASSERT((MIC_FAIL_PAIRWISE == mic_fail_type) || (MIC_FAIL_GROUP== mic_fail_type));
  switch(mic_fail_type) {
  case MIC_FAIL_PAIRWISE:
    mtlk_core_inc_cnt(core, MTLK_CORE_CNT_PAIRWISE_MIC_FAILURE_PACKETS);
    break;
  case MIC_FAIL_GROUP:
    mtlk_core_inc_cnt(core, MTLK_CORE_CNT_GROUP_MIC_FAILURE_PACKETS);
    break;
  default:
    WLOG_V("Wrong type of pairwise packet");
    break;
  }
}

/* ======================================================*/
/* Core internal wrapper for asynchronous execution.     */
/* Uses serializer, command can't be tracked/canceled,   */
/* allocated on heap and deleted by completion callback. */
static void
_mtlk_core_async_clb(mtlk_handle_t user_context)
{
  int res = MTLK_ERR_BUSY;
  _core_async_exec_t *ctx = (_core_async_exec_t *) user_context;

  if (_mtlk_abmgr_is_ability_enabled(mtlk_vap_get_abmgr(ctx->vap_handle),
                                     ctx->ability_id))
  {
    res = ctx->func(ctx->receiver, &ctx[1], ctx->data_size);
  }
  else
  {
    WLOG_D("Requested ability 0x%X is disabled or never was registered", ctx->ability_id);
  }

  if(NULL != ctx->user_req)
    mtlk_df_ui_req_complete(ctx->user_req, res);
}

static void
_mtlk_core_async_compl_clb(serializer_result_t res,
                           mtlk_command_t* command,
                           mtlk_handle_t completion_ctx)
{
  _core_async_exec_t *ctx = (_core_async_exec_t *) completion_ctx;

  mtlk_command_cleanup(&ctx->cmd);
  mtlk_osal_mem_free(ctx);
}

static int
_mtlk_core_execute_async_ex (struct nic *nic, mtlk_ability_id_t ability_id, mtlk_handle_t receiver,
                             mtlk_core_task_func_t func, const void *data, size_t size,
                             _mtlk_core_async_priorities_t priority,
                             mtlk_user_request_t *req,
                             mtlk_slid_t issuer_slid)
{
  int res;
  _core_async_exec_t *ctx;

  MTLK_ASSERT(0 == sizeof(_core_async_exec_t) % sizeof(void*));

  ctx = mtlk_osal_mem_alloc(sizeof(_core_async_exec_t) + size,
                            MTLK_MEM_TAG_ASYNC_CTX);
  if(NULL == ctx)
  {
    ELOG_D("CID-%04x: Failed to allocate execution context object", mtlk_vap_get_oid(nic->vap_handle));
    return MTLK_ERR_NO_MEM;
  }

  ctx->receiver     = receiver;
  ctx->data_size    = size;
  ctx->func         = func;
  ctx->user_req     = req;
  ctx->vap_handle   = nic->vap_handle;
  ctx->ability_id   = ability_id;
  memcpy(&ctx[1], data, size);

  res = mtlk_command_init(&ctx->cmd, _mtlk_core_async_clb, HANDLE_T(ctx), issuer_slid);
  if(MTLK_ERR_OK != res)
  {
    mtlk_osal_mem_free(ctx);
    ELOG_D("CID-%04x: Failed to initialize command object", mtlk_vap_get_oid(nic->vap_handle));
    return res;
  }

  res = mtlk_serializer_enqueue(&nic->slow_ctx->serializer, priority,
                                &ctx->cmd, _mtlk_core_async_compl_clb,
                                HANDLE_T(ctx));
  if(MTLK_ERR_OK != res)
  {
    mtlk_osal_mem_free(ctx);
    ELOG_DD("CID-%04x: Failed to enqueue command object (error: %d)", mtlk_vap_get_oid(nic->vap_handle), res);
    return res;
  }

  return res;
}

#define _mtlk_core_execute_async(nic, ability_id, receiver, func, data, size, priority, req) \
  _mtlk_core_execute_async_ex((nic), (ability_id), (receiver), (func), (data), (size), (priority), (req), MTLK_SLID)

int __MTLK_IFUNC mtlk_core_schedule_internal_task_ex (struct nic *nic,
                                                      mtlk_handle_t object,
                                                      mtlk_core_task_func_t func,
                                                      const void *data, size_t size,
                                                      mtlk_slid_t issuer_slid)
{
  return _mtlk_core_execute_async_ex(nic, MTLK_ABILITY_NONE, object, func, data, size,
                                     _MTLK_CORE_PRIORITY_INTERNAL, NULL, issuer_slid);
}

int __MTLK_IFUNC mtlk_core_schedule_user_task_ex (struct nic *nic,
                                                  mtlk_handle_t object,
                                                  mtlk_core_task_func_t func,
                                                  const void *data, size_t size,
                                                  mtlk_slid_t issuer_slid)
{
  return _mtlk_core_execute_async_ex(nic, MTLK_ABILITY_NONE, object, func, data, size,
                                     _MTLK_CORE_PRIORITY_USER, NULL, issuer_slid);
}

/*! Function for scheduling out of order (emergency) task.
    Sends message confirmation for message object specified

    \param   nic              Pointer to the core object
    \param   object           Handle of receiver object
    \param   func             Task callback
    \param   data             Pointer to the data buffer provided by caller
    \param   data_size        Size of data buffer provided by caller

*/
int __MTLK_IFUNC mtlk_core_schedule_emergency_task (struct nic *nic,
                                                    mtlk_handle_t object,
                                                    mtlk_core_task_func_t func,
                                                    const void *data, size_t size,
                                                    mtlk_slid_t issuer_slid)
{
  return _mtlk_core_execute_async_ex(nic, MTLK_ABILITY_NONE, object, func, data, size,
                                     _MTLK_CORE_PRIORITY_EMERGENCY, NULL, issuer_slid);
}

/*! Function for scheduling serialized task on demand of HW module activities
    Sends message confirmation for message object specified

    \param   nic              Pointer to the core object
    \param   object           Handle of receiver object
    \param   func             Task callback
    \param   data             Pointer to the data buffer provided by caller
    \param   data_size        Size of data buffer provided by caller

*/
int __MTLK_IFUNC mtlk_core_schedule_hw_task(struct nic *nic,
                                            mtlk_handle_t object,
                                            mtlk_core_task_func_t func,
                                            const void *data, size_t size,
                                            mtlk_slid_t issuer_slid)
{
  return _mtlk_core_execute_async_ex(nic, MTLK_ABILITY_NONE, object, func, data, size,
                                     _MTLK_CORE_PRIORITY_NETWORK, NULL, issuer_slid);
}
/* ======================================================*/

/* ======================================================*/
/* Function for processing HW tasks                      */

typedef enum __mtlk_hw_task_type_t
{
  SYNCHRONOUS,
  SERIALIZABLE
} _mtlk_core_task_type_t;

static void
_mtlk_process_hw_task_ex (mtlk_core_t* nic,
                          _mtlk_core_task_type_t task_type, mtlk_core_task_func_t task_func,
                          mtlk_handle_t object, const void* data, uint32 data_size, mtlk_slid_t issuer_slid)
{
    if(SYNCHRONOUS == task_type)
    {
      task_func(object, data, data_size);
    }
    else
    {
      if(MTLK_ERR_OK != mtlk_core_schedule_hw_task(nic, object,
                                                   task_func, data, data_size, issuer_slid))
      {
        ELOG_DP("CID-%04x: Hardware task schedule for callback 0x%p failed.", mtlk_vap_get_oid(nic->vap_handle), task_func);
      }
    }
}

#define _mtlk_process_hw_task(nic, task_type, task_func, object, data, data_size) \
  _mtlk_process_hw_task_ex((nic), (task_type), (task_func), (object), (data), (data_size), MTLK_SLID)

static void
_mtlk_process_user_task_ex (mtlk_core_t* nic, mtlk_user_request_t *req,
                            _mtlk_core_task_type_t task_type, mtlk_ability_id_t ability_id,
                            mtlk_core_task_func_t task_func,
                            mtlk_handle_t object, mtlk_clpb_t* data, mtlk_slid_t issuer_slid)
{
    if(SYNCHRONOUS == task_type)
    {
      int res = MTLK_ERR_BUSY;
      /* check is ability enabled for execution */
      if (_mtlk_abmgr_is_ability_enabled(mtlk_vap_get_abmgr(nic->vap_handle), ability_id)) {
        res = task_func(object, &data, sizeof(mtlk_clpb_t*));
      }
      else
      {
        WLOG_D("Requested ability 0x%X is disabled or never was registered", ability_id);
      }


      mtlk_df_ui_req_complete(req, res);
    }
    else
    {
      int result = _mtlk_core_execute_async_ex(nic, ability_id, object, task_func,
                                               &data, sizeof(data), _MTLK_CORE_PRIORITY_USER, req,
                                               issuer_slid);

      if(MTLK_ERR_OK != result)
      {
        ELOG_DPD("CID-%04x: User task schedule for callback 0x%p failed (error %d).",
                 mtlk_vap_get_oid(nic->vap_handle), task_func, result);
        mtlk_df_ui_req_complete(req, result);
      }
    }
}

#define _mtlk_process_user_task(nic, req, task_type, ability_id, task_func, object, data) \
  _mtlk_process_user_task_ex((nic), (req), (task_type), (ability_id), (task_func), (object), (data), MTLK_SLID)

static void
_mtlk_process_emergency_task_ex (mtlk_core_t* nic,
                                 mtlk_core_task_func_t task_func,
                                 mtlk_handle_t object, const void* data, uint32 data_size, mtlk_slid_t issuer_slid)
{
  if (MTLK_ERR_OK != mtlk_core_schedule_emergency_task(nic, object,
                                                      task_func, data, data_size, issuer_slid)) {
    ELOG_DP("CID-%04x: Emergency task schedule for callback 0x%p failed.", mtlk_vap_get_oid(nic->vap_handle), task_func);
  }
}

#define _mtlk_process_emergency_task(nic, task_func, object, data, data_size) \
  _mtlk_process_emergency_task_ex((nic), (task_func), (object), (data), (data_size), MTLK_SLID)

/* ======================================================*/

static int __MTLK_IFUNC
handleDisconnectSta(mtlk_handle_t core_object, const void *payload, uint32 size);
static void cleanup_on_disconnect(struct nic *nic);

mtlk_eeprom_data_t* __MTLK_IFUNC
mtlk_core_get_eeprom(mtlk_core_t* core)
{
  mtlk_eeprom_data_t *ee_data = NULL;

  mtlk_vap_get_hw_vft(core->vap_handle)->get_prop(core->vap_handle,
    MTLK_HW_PROP_EEPROM_DATA, &ee_data, sizeof(&ee_data));

  return ee_data;
}


static char *
mtlk_net_state_to_string(uint32 state)
{
  switch (state) {
  case NET_STATE_HALTED:
    return "NET_STATE_HALTED";
  case NET_STATE_IDLE:
    return "NET_STATE_IDLE";
  case NET_STATE_READY:
    return "NET_STATE_READY";
  case NET_STATE_ACTIVATING:
    return "NET_STATE_ACTIVATING";
  case NET_STATE_CONNECTED:
    return "NET_STATE_CONNECTED";
  case NET_STATE_DISCONNECTING:
    return "NET_STATE_DISCONNECTING";
  default:
    break;
  }
  ILOG1_D("Unknown state 0x%04X", state);
  return "NET_STATE_UNKNOWN";
}

mtlk_hw_state_e
mtlk_core_get_hw_state (mtlk_core_t *nic)
{
  mtlk_hw_state_e hw_state = MTLK_HW_STATE_LAST;

  mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_PROP_STATE, &hw_state, sizeof(hw_state));
  return hw_state;
}

int
mtlk_set_hw_state (mtlk_core_t *nic, int st)
{
  mtlk_hw_state_e ost;
  mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_PROP_STATE, &ost, sizeof(ost));
  ILOG1_DD("%i -> %i", ost, st);
  return mtlk_vap_get_hw_vft(nic->vap_handle)->set_prop(nic->vap_handle, MTLK_HW_PROP_STATE, &st, sizeof(st));
}

uint16 __MTLK_IFUNC mtlk_core_get_sq_size(mtlk_core_t *nic, uint16 access_category)
{
  return mtlk_sq_get_qsize( nic->sq, access_category);
}

static void
mtlk_core_set_net_state_halted (mtlk_core_t *core)
{
  mtlk_osal_lock_acquire(&core->net_state_lock);
  core->net_state = NET_STATE_HALTED;
  mtlk_osal_lock_release(&core->net_state_lock);
}

static int
mtlk_core_set_net_state(mtlk_core_t *core, uint32 new_state)
{
  uint32 allow_mask;
  mtlk_hw_state_e hw_state;
  int result = MTLK_ERR_OK;

  mtlk_osal_lock_acquire(&core->net_state_lock);
  if (new_state == NET_STATE_HALTED) {
    if (core->net_state != NET_STATE_HALTED) {
      ELOG_DD("CID-%04x: Going to net state HALTED (net_state=%d)", mtlk_vap_get_oid(core->vap_handle), core->net_state);
      mtlk_vap_get_hw_vft(core->vap_handle)->set_prop(core->vap_handle, MTLK_HW_RESET, NULL, 0);
      core->net_state = NET_STATE_HALTED;
    }
    goto FINISH;
  }
  /* allow transition from NET_STATE_HALTED to NET_STATE_IDLE
     while in hw state MTLK_HW_STATE_READY */
  hw_state = mtlk_core_get_hw_state(core);
  if ((hw_state != MTLK_HW_STATE_READY) && (hw_state != MTLK_HW_STATE_UNLOADING) &&
      (new_state != NET_STATE_IDLE)) {
    ELOG_DD("CID-%04x: Wrong hw_state=%d", mtlk_vap_get_oid(core->vap_handle), hw_state);
    result = MTLK_ERR_HW;
    goto FINISH;
  }
  allow_mask = 0;
  switch (new_state) {
  case NET_STATE_IDLE:
    allow_mask = NET_STATE_HALTED; /* on core_start */
    break;
  case NET_STATE_READY:
    allow_mask = NET_STATE_IDLE | NET_STATE_ACTIVATING |
      NET_STATE_DISCONNECTING;
    break;
  case NET_STATE_ACTIVATING:
    allow_mask = NET_STATE_READY;
    break;
  case NET_STATE_DISCONNECTING:
    allow_mask = NET_STATE_CONNECTED;
    break;
  case NET_STATE_CONNECTED:
    allow_mask = NET_STATE_ACTIVATING;
    break;
  default:
    break;
  }
  /* check mask */
  if (core->net_state & allow_mask) {
    ILOG1_SS("Going from %s to %s",
      mtlk_net_state_to_string(core->net_state),
      mtlk_net_state_to_string(new_state));
    core->net_state = new_state;
  } else {
    ILOG1_SS("Failed to change state from %s to %s",
      mtlk_net_state_to_string(core->net_state),
      mtlk_net_state_to_string(new_state));
    result = MTLK_ERR_WRONG_CONTEXT;
  }
FINISH:
  mtlk_osal_lock_release(&core->net_state_lock);
  return result;
}

static int
mtlk_core_get_net_state(mtlk_core_t *core)
{
  uint32 net_state;
  mtlk_hw_state_e hw_state;

  hw_state = mtlk_core_get_hw_state(core);
  if (hw_state != MTLK_HW_STATE_READY && hw_state != MTLK_HW_STATE_UNLOADING) {
    net_state = NET_STATE_HALTED;
    goto FINISH;
  }
  net_state = core->net_state;
FINISH:
  return net_state;
}

static int __MTLK_IFUNC
check_mac_watchdog (mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry;
  UMI_MAC_WATCHDOG *mac_watchdog;
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(0 == size);
  MTLK_UNREFERENCED_PARAM(payload);
  MTLK_UNREFERENCED_PARAM(size);
  MTLK_ASSERT(FALSE == mtlk_vap_is_slave_ap(nic->vap_handle));

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txdm(nic->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto END;
  }

  man_entry->id = UM_DBG_MAC_WATCHDOG_REQ;
  man_entry->payload_size = sizeof(UMI_MAC_WATCHDOG);

  mac_watchdog = (UMI_MAC_WATCHDOG *)man_entry->payload;
  mac_watchdog->u16Timeout =
      HOST_TO_MAC16(MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_TIMEOUT_MS));

  res = mtlk_txmm_msg_send_blocked(&man_msg,
          MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_TIMEOUT_MS));
  if (res == MTLK_ERR_OK) {
    switch (mac_watchdog->u8Status) {
    case UMI_OK:
      break;
    case UMI_MC_BUSY:
      break;
    case UMI_TIMEOUT:
      res = MTLK_ERR_UMI;
      break;
    default:
      res = MTLK_ERR_UNKNOWN;
      break;
    }
  }
  mtlk_txmm_msg_cleanup(&man_msg);

END:
  if (res != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: MAC watchdog error %d", mtlk_vap_get_oid(nic->vap_handle), res);
    mtlk_core_set_net_state(nic, NET_STATE_HALTED);
  } else {
    if (MTLK_ERR_OK !=
        mtlk_osal_timer_set(&nic->slow_ctx->mac_watchdog_timer,
                            MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_PERIOD_MS))) {
      ELOG_D("CID-%04x: Cannot schedule MAC watchdog timer", mtlk_vap_get_oid(nic->vap_handle));
      mtlk_core_set_net_state(nic, NET_STATE_HALTED);
    }
  }

  return MTLK_ERR_OK;
}

static uint32
mac_watchdog_timer_handler (mtlk_osal_timer_t *timer, mtlk_handle_t data)
{
  int err;
  struct nic *nic = (struct nic *)data;

  err = _mtlk_core_execute_async(nic, MTLK_ABILITY_NONE, HANDLE_T(nic), check_mac_watchdog,
                                 NULL, 0, _MTLK_CORE_PRIORITY_MAINTENANCE, NULL);

  if (err != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't schedule MAC WATCHDOG task (err=%d)", mtlk_vap_get_oid(nic->vap_handle), err);
  }

  return 0;
}

static int
get_cipher (mtlk_core_t* core, mtlk_nbuf_t *nbuf)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);

  if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_DIRECTED)) {
    MTLK_ASSERT(mtlk_nbuf_priv_get_src_sta(nbuf_priv) != NULL);
    return mtlk_sta_get_cipher(mtlk_nbuf_priv_get_src_sta(nbuf_priv));
  } else {
    return core->group_cipher;
  }
}

/* Get Received Security Counter buffer */
static int
_get_rsc_buf(mtlk_core_t* core, mtlk_nbuf_t *nbuf, int off)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);
  u8 cipher = get_cipher(core, nbuf);

  if (cipher == IW_ENCODE_ALG_TKIP || cipher == IW_ENCODE_ALG_CCMP) {
    return mtlk_nbuf_priv_set_rsc_buf(nbuf_priv, nbuf->data + off);
  } else {
    return 0;
  }
}

static int
_mtlk_core_mac_reset_stats (mtlk_core_t *nic)
{
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry;
  int res = MTLK_ERR_OK;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txdm(nic->vap_handle), NULL);
  if (man_entry) {
    UMI_RESET_STATISTICS *pstats = (UMI_RESET_STATISTICS *)man_entry->payload;

    man_entry->id           = UM_DBG_RESET_STATISTICS_REQ;
    man_entry->payload_size = sizeof(*pstats);
    pstats->u16Status       = 0;

    res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
    if (res != MTLK_ERR_OK) {
      ELOG_D("CID-%04x: MAC Reset Stat sending timeout", mtlk_vap_get_oid(nic->vap_handle));
    }

    mtlk_txmm_msg_cleanup(&man_msg);
  }
  else {
    ELOG_D("CID-%04x: Can't reset statistics due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NO_RESOURCES;
  }

  return res;
}

static void __MTLK_IFUNC
clean_all_sta_on_disconnect_sta_clb (mtlk_handle_t    usr_ctx,
                                     sta_entry *sta)
{
  struct nic      *nic  = HANDLE_T_PTR(struct nic, usr_ctx);
  const IEEE_ADDR *addr = mtlk_sta_get_addr(sta);

  ILOG1_Y("Station %Y disconnected", addr->au8Addr);

  /* Notify Traffic analyzer about STA disconnect */
  mtlk_ta_on_disconnect(mtlk_vap_get_ta(nic->vap_handle), sta);

  if (BR_MODE_WDS == MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE)){
    /* Don't remove HOST from DB if in WDS mode */
    mtlk_hstdb_stop_all_by_sta(&nic->slow_ctx->hstdb, sta);
  }
  else {
    mtlk_hstdb_remove_all_by_sta(&nic->slow_ctx->hstdb, sta);
  }

  if (!sta->peer_ap)
  {
    mtlk_df_ui_notify_node_disconect(mtlk_vap_get_df(nic->vap_handle), addr->au8Addr);
  }

  /* Remove all hosts MAC addr from switch MAC table */
  mtlk_hstdb_ppa_remove_all_by_sta(&nic->slow_ctx->hstdb, sta);

  /* Remove sta MAC addr from switch MAC table */
  mtlk_df_user_ppa_remove_mac_addr(mtlk_vap_get_df(nic->vap_handle), addr->au8Addr);
}

static void
_mtlk_core_clean_vap_tx_on_disconnect(struct nic *nic)
{
  mtlk_sq_peer_ctx_cancel_all_packets_for_vap(nic->sq, &nic->sq_broadcast_ctx, nic->vap_handle);
}

static void
clean_all_sta_on_disconnect (struct nic *nic)
{
  BOOL wait_all_packets_confirmed;

  wait_all_packets_confirmed = (mtlk_core_get_net_state(nic) != NET_STATE_HALTED);

  mtlk_stadb_disconnect_all(&nic->slow_ctx->stadb,
                            clean_all_sta_on_disconnect_sta_clb,
                            HANDLE_T(nic),
                            wait_all_packets_confirmed);

  _mtlk_core_clean_vap_tx_on_disconnect(nic);
}

static void
cleanup_on_disconnect (struct nic *nic)
{
  if (!mtlk_vap_is_ap(nic->vap_handle)) {
    unsigned char bssid[ETH_ALEN];
    mtlk_pdb_get_mac(
            mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_BSSID, bssid);
    // Drop BSSID persistance in BSS cache.
    // We've been disconnected from BSS, thus we don't know
    // whether it's alive or not.
    mtlk_cache_set_persistent(&nic->slow_ctx->cache,
                              bssid,
                              FALSE);
    /* rollback network mode */
    MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_NET_MODE_CUR, mtlk_core_get_network_mode_cfg(nic));
  }

  if (!_mtlk_core_has_connections(nic)) {
    if (mtlk_core_get_net_state(nic) == NET_STATE_DISCONNECTING) {
      mtlk_pdb_set_mac(mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_BSSID,
                       mtlk_osal_eth_zero_addr);
      mtlk_core_set_net_state(nic, NET_STATE_READY);
    }
    _mtlk_core_clean_vap_tx_on_disconnect(nic);
  }
}

static int
_mtlk_core_send_disconnect_req_blocked (struct nic *nic, const IEEE_ADDR *addr, uint16 reason, uint16 fail_reason, uint16 reason_code)
{
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_DISCONNECT   *psUmiDisconnect;
  int               net_state = mtlk_core_get_net_state(nic);
  int               res = MTLK_ERR_OK;
  IEEE_ADDR         addr_to_send;
  BOOL              is_peer_ap;

  is_peer_ap = FALSE;

  if (((mtlk_vap_is_ap(nic->vap_handle) && addr == NULL) || !mtlk_vap_is_ap(nic->vap_handle)) && (net_state != NET_STATE_HALTED)) {
    /* check if we can disconnect in current net state */
    res = mtlk_core_set_net_state(nic, NET_STATE_DISCONNECTING);
    if (res != MTLK_ERR_OK) {
      goto FINISH;
    }
  }

  memset(&addr_to_send, 0, sizeof(addr_to_send));

  if (addr == NULL && !mtlk_vap_is_ap(nic->vap_handle)) {
    mtlk_pdb_get_mac(
            mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_BSSID, addr_to_send.au8Addr);
  }
  else if (addr) {
    addr_to_send = *addr;
  }

#ifdef PHASE_3
  if (!mtlk_vap_is_ap(nic->vap_handle)) {
    /* stop cache updation on STA. */
    mtlk_stop_cache_update(nic);
  }
#endif
  if (mtlk_vap_is_ap(nic->vap_handle) && addr == NULL) {
    clean_all_sta_on_disconnect(nic);
  }
  else {
    sta_entry *sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, addr_to_send.au8Addr);
    if (sta == NULL) {
      ILOG1_Y("STA not found during cleanup: %Y", &addr_to_send);
    } else {
      is_peer_ap = sta->peer_ap;
      if (BR_MODE_WDS == MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE)){
        /* Don't remove HOST from DB if in WDS mode */
        mtlk_hstdb_stop_all_by_sta(&nic->slow_ctx->hstdb, sta);
      }
      else {
        mtlk_hstdb_remove_all_by_sta(&nic->slow_ctx->hstdb, sta);
      }

      mtlk_stadb_remove_sta(&nic->slow_ctx->stadb, sta);

      /* Remove all hosts MAC addresses from switch MAC table after removing STA from stadb */
      mtlk_hstdb_ppa_remove_all_by_sta(&nic->slow_ctx->hstdb, sta);

      /* Notify Traffic analyzer about STA disconnect */
      mtlk_ta_on_disconnect(mtlk_vap_get_ta(nic->vap_handle), sta);

      /*Send indication if STA has been disconnected from AP*/
      _mtlk_core_on_peer_disconnect(nic, sta, reason, fail_reason);
      mtlk_sta_decref(sta); /* De-reference of find */
    }
  }

  if (net_state == NET_STATE_HALTED) {
    /* Do not send anything to halted MAC or if STA hasn't been connected */
    res = MTLK_ERR_OK;
    goto FINISH;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), &res);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send DISCONNECT request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  man_entry->id           = UM_MAN_DISCONNECT_REQ;
  man_entry->payload_size = sizeof(UMI_DISCONNECT);
  psUmiDisconnect         = (UMI_DISCONNECT *)man_entry->payload;

  psUmiDisconnect->u16Status = UMI_OK;
  psUmiDisconnect->sStationID = addr_to_send;
  psUmiDisconnect->reasonCode =  HOST_TO_MAC16(reason_code);

  mtlk_dump(5, psUmiDisconnect, sizeof(UMI_DISCONNECT), "dump of UMI_DISCONNECT:");

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't send DISCONNECT request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), res);
    goto FINISH;
  }

  cleanup_on_disconnect(nic);

  if (mtlk_vap_is_ap(nic->vap_handle)) {
    ILOG1_YD("Station %Y disconnected (status %u)",
      &addr_to_send, MAC_TO_HOST16(psUmiDisconnect->u16Status));
    if (!is_peer_ap) {
      _mtlk_core_notify_ap_of_station_disconnection(nic, &addr_to_send);
    }

    /* Remove MAC addr from switch MAC table after removing STA from stadb */
    if (NULL != addr) {
      mtlk_df_user_ppa_remove_mac_addr(mtlk_vap_get_df(nic->vap_handle), addr->au8Addr);
    }
  } else {
    ILOG1_YD("Disconnected from AP %Y (status %u)",
      &addr_to_send, MAC_TO_HOST16(psUmiDisconnect->u16Status));
    mtlk_df_ui_notify_disassociation(mtlk_vap_get_df(nic->vap_handle));
    if (mtlk_core_is_20_40_active(nic))
    {
      mtlk_20_40_sta_notify_disconnection_from_ap(mtlk_core_get_coex_sm(nic));
      mtlk_20_40_sta_restore_obss_scan_parameters(mtlk_core_get_coex_sm(nic));
      mtlk_scan_set_load_progmodel_policy(&nic->slow_ctx->scan, TRUE);
    }
  }

  /* update disconnections statistics */
  nic->pstats.num_disconnects++;

  if (MAC_TO_HOST16(psUmiDisconnect->u16Status) != UMI_OK) {
    WLOG_DYD("CID-%04x: Station %Y disconnect failed in FW (status=%u)", mtlk_vap_get_oid(nic->vap_handle),
        &addr_to_send, MAC_TO_HOST16(psUmiDisconnect->u16Status));
    res = MTLK_ERR_MAC;
    goto FINISH;
  }

  res = MTLK_ERR_OK;

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}

static int
clear_group_key(struct nic *nic)
{
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_CLEAR_KEY    *umi_cl_key;
  int res;

  ILOG1_D("CID-%04x: Clear group key", mtlk_vap_get_oid(nic->vap_handle));

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), NULL);
  if (!man_entry) {
    return -ENOMEM;
  }

  umi_cl_key = (UMI_CLEAR_KEY*) man_entry->payload;
  memset(umi_cl_key, 0, sizeof(*umi_cl_key));
  man_entry->id = UM_MAN_CLEAR_KEY_REQ;
  man_entry->payload_size = sizeof(*umi_cl_key);

  umi_cl_key->u16KeyType = cpu_to_le16(UMI_RSN_GROUP_KEY);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK)
    ELOG_DD("CID-%04x: mtlk_mm_send_blocked failed: %i", mtlk_vap_get_oid(nic->vap_handle), res);

  mtlk_txmm_msg_cleanup(&man_msg);

  return (res == MTLK_ERR_OK ? 0 : -EFAULT);
}

static void __MTLK_IFUNC
  _mtlk_core_trigger_connect_complete_event(mtlk_core_t *nic, BOOL success)
{
  nic->activation_status = success;
  mtlk_osal_event_set(&nic->slow_ctx->connect_event);
}

static int
reset_security_stuff(struct nic *nic)
{
  int res = MTLK_ERR_OK;

  memset(&nic->slow_ctx->rsnie, 0, sizeof(nic->slow_ctx->rsnie));
  if (mtlk_vap_is_ap(nic->vap_handle))
    clear_group_key(nic);
  if ((res = mtlk_set_mib_rsn(mtlk_vap_get_txmm(nic->vap_handle), 0)) != MTLK_ERR_OK)
    ELOG_D("CID-%04x: Failed to reset RSN", mtlk_vap_get_oid(nic->vap_handle));
  nic->rsn_enabled = 0;
  if (nic->slow_ctx->wep_enabled) {
    /* Disable WEP encryption */
    res = _mtlk_core_set_wep(nic, FALSE);
    nic->slow_ctx->wep_enabled = FALSE;
  }
  nic->slow_ctx->default_key = 0;
  nic->slow_ctx->wps_in_progress = FALSE;
  nic->group_cipher = IW_ENCODE_ALG_NONE;

  return res;
}

BOOL
can_disconnect_now(struct nic *nic)
{
  return !mtlk_core_scan_is_running(nic);
}

/* This interface can be used if we need to disconnect while in
 * atomic context (for example, when disconnecting from a timer).
 * Disconnect process requires blocking function calls, so we
 * have to schedule a work.
 */

struct mtlk_core_disconnect_sta_data
{
  IEEE_ADDR          addr;
  uint16             fail_reason;
  uint16             reason_code;
  int               *res;
  mtlk_osal_event_t *done_evt;
};

static void
_mtlk_core_schedule_disconnect_sta (struct nic *nic, const sta_entry *sta,
                                    uint16 fail_reason, uint16 reason_code,int *res, mtlk_osal_event_t *done_evt)
{
  int err;
  struct mtlk_core_disconnect_sta_data data;

  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(sta != NULL);

  memset(&data, 0, sizeof(data));

  data.addr     = *(mtlk_sta_get_addr(sta));
  data.fail_reason = fail_reason;
  data.res      = res;
  data.done_evt = done_evt;
  data.reason_code = reason_code;

  err = _mtlk_core_execute_async(nic, MTLK_ABILITY_NONE, HANDLE_T(nic), handleDisconnectSta, &data,
                                 sizeof(data), _MTLK_CORE_PRIORITY_NETWORK, NULL);
  if (err != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't schedule DISCONNECT STA (err=%d)", mtlk_vap_get_oid(nic->vap_handle), err);
  }
}

/***************************************************************************
 * Disconnecting routines for STA
 ***************************************************************************/
/* This interface can be used if we need to disconnect while in
 * atomic context (for example, when disconnecting from a timer).
 * Disconnect process requires blocking function calls, so we
 * have to schedule a work.
 */
static void
_mtlk_core_schedule_disconnect_me (struct nic *nic, uint16 fail_reason, uint16 reason_code)
{
  int err;
  struct mtlk_core_disconnect_sta_data data;

  MTLK_ASSERT(nic != NULL);

  memset(&data, 0, sizeof(data));
  data.fail_reason = fail_reason;
  data.reason_code = reason_code;

  err = _mtlk_core_execute_async(nic, MTLK_ABILITY_NONE, HANDLE_T(nic), handleDisconnectMe, &data, sizeof(data),
                                 _MTLK_CORE_PRIORITY_NETWORK, NULL);
  if (err != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't schedule DISCONNECT (err=%d)", mtlk_vap_get_oid(nic->vap_handle), err);
  }
}

static int __MTLK_IFUNC
_mtlk_core_disconnect_sta(mtlk_core_t *nic, uint16 reason, uint16 fail_reason, uint16 reason_code)
{
  uint32 net_state;

  net_state = mtlk_core_get_net_state(nic);
  if ((net_state != NET_STATE_CONNECTED) &&
      (net_state != NET_STATE_HALTED)) { /* allow disconnect for clean up */
    ILOG0_DS("CID-%04x: disconnect in invalid state %s", mtlk_vap_get_oid(nic->vap_handle),
          mtlk_net_state_to_string(net_state));
    return MTLK_ERR_NOT_READY;
  }

  if (!can_disconnect_now(nic)) {
    _mtlk_core_schedule_disconnect_me(nic, fail_reason, reason_code);
    return MTLK_ERR_OK;
  }

  reset_security_stuff(nic);

  return _mtlk_core_send_disconnect_req_blocked(nic, NULL, reason, fail_reason, reason_code);
}

static int __MTLK_IFUNC
_mtlk_core_hanle_disconnect_sta_req(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  int res = MTLK_ERR_PARAMS;
  mtlk_core_ui_mlme_cfg_t *mlme_cfg;
  uint32 size;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  mlme_cfg = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == mlme_cfg) || (sizeof(*mlme_cfg) != size) ) {
    ELOG_D("CID-%04x: Failed to get MLME configuration parameters from CLPB", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_UNKNOWN;
    goto finish;
  }

  res = _mtlk_core_disconnect_sta(nic, UMI_BSS_UNSPECIFIED, FM_STATUSCODE_USER_REQUEST, mlme_cfg->reason_code);

finish:
  return res;

}

static int __MTLK_IFUNC
handleDisconnectMe(mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);
  struct mtlk_core_disconnect_sta_data *data =
    (struct mtlk_core_disconnect_sta_data *)payload;

  MTLK_ASSERT(sizeof(uint16) == size);

  ILOG2_V("Handling disconnect request");
  _mtlk_core_disconnect_sta(nic, UMI_BSS_UNSPECIFIED, data->fail_reason, data->reason_code);

  return MTLK_ERR_OK;
}

/*****************************************************************************
**
** NAME         mtlk_send_null_packet
**
** PARAMETERS   nic           Card context
**              sta                 Destination STA
**
** RETURNS      none
**
** DESCRIPTION  Function used to send NULL packets from STA to AP in order
**              to support AutoReconnect feature (NULL packets are treated as
**              keepalive data)
**
******************************************************************************/
static void
mtlk_send_null_packet (struct nic *nic, sta_entry *sta)
{
  mtlk_hw_send_data_t data;
  int                 ires = MTLK_ERR_UNKNOWN;
  mtlk_nbuf_t        *nbuf;
  mtlk_nbuf_priv_t   *nbuf_priv;
  mtlk_sq_peer_ctx_t *sq_ppeer;

  MTLK_ASSERT (NULL != nic);

  // Transmit only if connected
  if (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED)
    return;

  // STA only ! Transmit only if data was not stopped (by 11h)
  if (mtlk_dot11h_is_data_stop(mtlk_core_get_dfs(nic)) == 1) {
    goto END;
  }

  MTLK_ASSERT (NULL != sta);
  sq_ppeer = &sta->sq_peer_ctx;

  /* If there is any unconfirmed packet in FW, do not send null data packet */
  if (mtlk_osal_atomic_get(&sq_ppeer->used) > 0) {
    goto END;
  }

  // XXX, klogg: revisit
  // Use NULL sk_buffer (really UGLY)
  nbuf = mtlk_df_nbuf_alloc(mtlk_vap_get_manager(nic->vap_handle), 0);
  if (!nbuf) {
    ILOG1_V("there is no free network buffer to send NULL packet");
    goto END;
  }
  nbuf_priv = mtlk_nbuf_priv(nbuf);
  mtlk_nbuf_priv_set_dst_sta(nbuf_priv, sta);
  mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_UNICAST);

  memset(&data, 0, sizeof(data));

  data.msg = mtlk_vap_get_hw_vft(nic->vap_handle)->get_msg_to_send(nic->vap_handle, NULL);
  // Check free MSDUs
  if (!data.msg) {
    ILOG1_V("there is no free msg to send NULL packet");
    mtlk_df_nbuf_free(mtlk_vap_get_manager(nic->vap_handle), nbuf);
    goto END;
  }

  ILOG3_P("got from hw msg %p", data.msg);

  data.nbuf            = nbuf;
  data.size            = 0;
  data.sta_id          = mtlk_sta_get_sta_id(sta);
  data.wds             = 0;
  data.access_category = UMI_USE_DCF;
  data.encap_type      = ENCAP_TYPE_ILLEGAL;
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  data.rf_mgmt_data    = mtlk_sta_get_rf_mgmt_data(sta);
#endif

  if(sta->peer_ap)
  {
    data.wds = 1;
  }

  /* store access_category */
  mtlk_nbuf_priv_set_tx_pck_ac(nbuf_priv, data.access_category);

  mtlk_osal_atomic_inc(&sq_ppeer->used);
  mtlk_osal_atomic_inc(&sq_ppeer->used_per_ac[data.access_category]);

  ires = mtlk_vap_get_hw_vft(nic->vap_handle)->send_data(nic->vap_handle, &data);
  if (__LIKELY(ires == MTLK_ERR_OK)) {
#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
    if (mtlk_vap_is_ap(nic->vap_handle)) {
      mtlk_aocs_msdu_tx_inc_nof_used(mtlk_core_get_master(nic)->slow_ctx->aocs, AC_BE);
    }
#endif
  }
  else {
    WLOG_DD("CID-%04x: hw_send (NULL) failed with Err#%d", mtlk_vap_get_oid(nic->vap_handle), ires);
    mtlk_osal_atomic_dec(&sq_ppeer->used);
    mtlk_osal_atomic_dec(&sq_ppeer->used_per_ac[data.access_category]);
    mtlk_vap_get_hw_vft(nic->vap_handle)->release_msg_to_send(nic->vap_handle, data.msg);
  }

END:
  return;
}



/*****************************************************************************
**
** DESCRIPTION  Determines if packet should be passed to upper layer and/or
**              forwarded to BSS. Sets nbuf flags.
**
** 1. Unicast (to us)   -> pass to upper layer.
** 2. Broadcast packet  -> pass to upper layer AND forward to BSS.
** 3. Multicast packet  -> forward to BSS AND check if AP is in multicast group,
**                           if so - pass to upper layer.
** 4. Unicast (not to us) -> if STA found in connected list - forward to BSS.
**
******************************************************************************/
static mtlk_nbuf_priv_t *
analyze_rx_packet (mtlk_core_t* nic, mtlk_nbuf_t *nbuf)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);
  struct ethhdr *ether_header = (struct ethhdr *)nbuf->data;
  int bridge_mode;

  bridge_mode = MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE);

  // check if device in PROMISCUOUS mode
  if (mtlk_df_ui_is_promiscuous(mtlk_vap_get_df(nic->vap_handle)))
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_CONSUME);

  // check if destination MAC is our address
  if (0 == MTLK_CORE_HOT_PATH_PDB_CMP_MAC(nic, CORE_DB_CORE_MAC_ADDR, ether_header->h_dest))
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_CONSUME);

  // check if destination MAC is broadcast address
  else if (mtlk_osal_eth_is_broadcast(ether_header->h_dest)) {
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_BROADCAST | MTLK_NBUFF_CONSUME);
    mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_BROADCAST_PACKETS_RECEIVED);
    mtlk_core_add_cnt(nic, MTLK_CORE_CNT_BROADCAST_BYTES_RECEIVED, mtlk_df_nbuf_get_data_length(nbuf));
    if (mtlk_vap_is_ap(nic->vap_handle) && MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_AP_FORWARDING))
      mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_FORWARD);
  }
  // check if destination MAC is multicast address
  else if (mtlk_osal_eth_is_multicast(ether_header->h_dest)) {
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_MULTICAST);
    mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_MULTICAST_PACKETS_RECEIVED);
    mtlk_core_add_cnt(nic, MTLK_CORE_CNT_MULTICAST_BYTES_RECEIVED, mtlk_df_nbuf_get_data_length(nbuf));

    if (mtlk_vap_is_ap(nic->vap_handle) && MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_AP_FORWARDING))
      mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_FORWARD);

    //check if we subscribed to all multicast
    if(mtlk_df_ui_check_is_mc_group_member(mtlk_vap_get_df(nic->vap_handle), ether_header->h_dest))
      mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_CONSUME);
  }
  // check if destination MAC is unicast address of connected STA
  else {
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_UNICAST);
    if (mtlk_vap_is_ap(nic->vap_handle) && MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_AP_FORWARDING)) {
      // Search of DESTINATION MAC ADDRESS of RECEIVED packet
      sta_entry *dst_sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, ether_header->h_dest);
      if ((NULL == dst_sta) && (BR_MODE_WDS == bridge_mode) ) {
        dst_sta = mtlk_hstdb_find_sta(&nic->slow_ctx->hstdb,
                                      ether_header->h_dest);
      }

      if (dst_sta != NULL) {
        if (dst_sta != mtlk_nbuf_priv_get_src_sta(nbuf_priv)) {
          mtlk_nbuf_priv_set_dst_sta(nbuf_priv, dst_sta);
          mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_FORWARD);
        }
        else {
          ILOG3_V("Loop detected ! Don't forward packet !");
        }
        mtlk_sta_decref(dst_sta); /* De-reference of find */
      }
    }
  }

  return nbuf_priv;
}



/*****************************************************************************
**
** NAME         mtlk_core_handle_tx_data
**
** PARAMETERS   vap_handle           Handle of VAP object
**              nbuf                 Skbuff to transmit
**
** DESCRIPTION  Entry point for TX buffers
**
******************************************************************************/
void __MTLK_IFUNC
mtlk_core_handle_tx_data(mtlk_vap_handle_t vap_handle, mtlk_nbuf_t *nbuf)
{
  mtlk_core_t *nic = mtlk_vap_get_core(vap_handle);
  mtlk_nbuf_priv_t *nbuf_priv;
  struct ethhdr *ether_header = (struct ethhdr *)nbuf->data;
  uint16 ac;
  int bridge_mode;

  MTLK_ASSERT(NULL != nic);

  CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_TX_OS);

  bridge_mode = MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE);

  /* Transmit only if connected to someone */
  if (unlikely(!_mtlk_core_has_connections(nic))) {
    mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_NO_PEERS);
    goto ERR;
  }

  /* get private fields */
  nbuf_priv = mtlk_nbuf_priv(nbuf);

#if defined(MTCFG_PER_PACKET_STATS) && defined(MTCFG_TSF_TIMER_ACCESS_ENABLED)
  mtlk_nbuf_priv_stats_set(nbuf_priv, MTLK_NBUF_STATS_TS_SQ_IN, mtlk_hw_get_timestamp(nic->vap_handle));
#endif

  /* store vap index in private data, will be retrieved when SQ sends packet to HW */
  mtlk_nbuf_priv_set_vap_handle(nbuf_priv, nic->vap_handle);

  /* analyze (and probably adjust) packet's priority and AC */
  ac = mtlk_qos_get_ac(&nic->qos, nbuf);
  if (unlikely(ac == AC_INVALID)) {
    sta_entry *sta = mtlk_nbuf_priv_get_dst_sta(nbuf_priv);
    if (sta) {
      mtlk_sta_on_packet_dropped(sta, MTLK_TX_DISCARDED_DRV_ACM);
    } else {
      mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_ACM);
    }
    goto ERR;
  }

  /* check frame urgency (currently for the 802.1X packets only) */
  if (mtlk_wlan_pkt_is_802_1X(ether_header->h_proto))
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_URGENT);

  switch (bridge_mode)
  {
  case BR_MODE_MAC_CLONING: /* MAC cloning stuff */
    /* these frames will generate MIC failures on AP and they have
     * no meaning in MAC Cloning mode - so we just drop them silently */
    if (!mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_URGENT) &&
         (0 != MTLK_CORE_HOT_PATH_PDB_CMP_MAC(nic, CORE_DB_CORE_MAC_ADDR, ether_header->h_source))) {
      sta_entry *sta = mtlk_nbuf_priv_get_dst_sta(nbuf_priv);
      if (sta) {
        mtlk_sta_on_packet_dropped(sta, MTLK_TX_DISCARDED_EAPOL_CLONED);
      } else {
        mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_EAPOL_CLONED);
      }
      goto ERR;
    }
    break;

  case BR_MODE_L2NAT: /* L2NAT stuff */
    /* call the hook */
    nbuf = mtlk_l2nat_on_tx(nic, nbuf);

    /* update ethernet header & nbuf_priv pointers */
    ether_header = (struct ethhdr *)nbuf->data;
    nbuf_priv = mtlk_nbuf_priv(nbuf);

    if (!mtlk_vap_is_ap(nic->vap_handle) &&
        (0 != MTLK_CORE_HOT_PATH_PDB_CMP_MAC(nic, CORE_DB_CORE_MAC_ADDR, ether_header->h_source))) {
      mtlk_hstdb_update_default_host(&nic->slow_ctx->hstdb, ether_header->h_source);
    }
    break;

  case BR_MODE_WDS: /* WDS stuff */
    if (!mtlk_vap_is_ap(nic->vap_handle) &&
        (0 != MTLK_CORE_HOT_PATH_PDB_CMP_MAC(nic, CORE_DB_CORE_MAC_ADDR, ether_header->h_source))) {
      sta_entry *sta = mtlk_stadb_get_ap(&nic->slow_ctx->stadb);
      if (sta) {
        /* update or add host in case 802.3 SA is not ours */
        mtlk_hstdb_update_host(&nic->slow_ctx->hstdb, ether_header->h_source, sta);
        mtlk_sta_decref(sta); /* De-reference of get_ap */
      }
    }
    break;

  default: /* no bridging */
    break;
  }

  ILOG4_Y("802.3 tx DA: %Y", ether_header->h_dest);
  ILOG4_Y("802.3 tx SA: %Y", ether_header->h_source);

  /* check frame destination */
  if (mtlk_osal_eth_is_broadcast(ether_header->h_dest)) {
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_BROADCAST);
  }
  else if (mtlk_osal_eth_is_multicast(ether_header->h_dest)) {
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_MULTICAST);
  }
  else {
    sta_entry * dst_sta = NULL;
    /* On STA we have only AP connected at id 0, so we do not need
     * to perform search of destination id */
    if (!mtlk_vap_is_ap(nic->vap_handle)) {
      dst_sta = mtlk_stadb_get_ap(&nic->slow_ctx->stadb);
    /* On AP we need to find destination STA id in database of peers
     * (both registered HOSTs and connected STAs) */
    } else {
      dst_sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, ether_header->h_dest);
      if (dst_sta == NULL && bridge_mode == BR_MODE_WDS) {
        dst_sta = mtlk_hstdb_find_sta(&nic->slow_ctx->hstdb,
                                      ether_header->h_dest);
      }
    }

    if (dst_sta) {
      mtlk_nbuf_priv_set_dst_sta(nbuf_priv, dst_sta);
      mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_UNICAST);
      mtlk_sta_decref(dst_sta); /* De-reference of find or get_ap */
    }
    else {
      mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_DIRECTED);
      ILOG3_Y("Unknown destination (%Y)", ether_header->h_dest);
      if (bridge_mode == BR_MODE_WDS){
        /* In bridge mode, in case of IP UNICAST packet to unknown destination,
           substitute original packet with ARP probe */
        if (NULL == subst_ip_to_arp_probe(&nbuf, vap_handle, &nic->slow_ctx->last_arp_probe)){
          goto ERR; /* Fail to generate ARP packet */
        }
      }else{
        goto ERR; /* Fail to generate ARP packet */
      }
    }
  } /* if unicast packet*/

  mtlk_mc_transmit(nic, nbuf);

  CPU_STAT_END_TRACK(CPU_STAT_ID_TX_OS);
  return;

ERR:
  mtlk_df_nbuf_free(mtlk_vap_get_manager(nic->vap_handle), nbuf);
  CPU_STAT_END_TRACK(CPU_STAT_ID_TX_OS);
}

static int
parse_and_get_replay_counter(mtlk_nbuf_t *nbuf, u8 *rc, u8 cipher)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);

  ASSERT(rc != NULL);

  if (cipher != IW_ENCODE_ALG_TKIP && cipher != IW_ENCODE_ALG_CCMP)
    return 1;

  rc[0] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 7);
  rc[1] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 6);
  rc[2] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 5);
  rc[3] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 4);

  if (cipher == IW_ENCODE_ALG_TKIP) {
    rc[4] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 0);
    rc[5] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 2);
  } else if (cipher == IW_ENCODE_ALG_CCMP) {
    rc[4] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 1);
    rc[5] = mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 0);
  }

  return 0;
}

static int
_detect_replay(mtlk_core_t *core, mtlk_nbuf_t *nbuf, u8 *last_rc, BOOL is_mgmt)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);
  sta_entry *sta;
  u8 cipher;
  int res;
  u8 rc[6]; // replay counter
  u8 rsn_bits = mtlk_nbuf_priv_get_rcn_bits(nbuf_priv);
  u8 nfrags;  // number of fragments this MSDU consisted of
  struct ethhdr *ether_header = NULL;
  char *is_mgmt_string = is_mgmt ? "Management" : "Data";

  sta = mtlk_nbuf_priv_get_src_sta(nbuf_priv);
  ASSERT(sta != NULL);

  cipher = get_cipher(core, nbuf);

  if (cipher != IW_ENCODE_ALG_TKIP && cipher != IW_ENCODE_ALG_CCMP)
    return 0;

  res = parse_and_get_replay_counter(nbuf, rc, cipher);
  ASSERT(res == 0);

  ILOG3_DDDDDDDDDDDDS("last RSC %02x%02x%02x%02x%02x%02x, got RSC %02x%02x%02x%02x%02x%02x frame type %s",
   last_rc[0], last_rc[1], last_rc[2], last_rc[3], last_rc[4], last_rc[5],
   rc[0], rc[1], rc[2], rc[3], rc[4], rc[5], is_mgmt_string);

  res = memcmp(rc, last_rc, sizeof(rc));

  if (res <= 0) {
    ILOG2_YDDDDDDDDDDDDS("replay detected from %Y last RSC %02x%02x%02x%02x%02x%02x, got RSC %02x%02x%02x%02x%02x%02x frame type %s",
        mtlk_sta_get_addr(sta)->au8Addr,
        last_rc[0], last_rc[1], last_rc[2], last_rc[3], last_rc[4], last_rc[5],
        rc[0], rc[1], rc[2], rc[3], rc[4], rc[5], is_mgmt_string);

    ether_header = (struct ethhdr *)nbuf->data;
    if(mtlk_osal_eth_is_multicast(ether_header->h_source) || mtlk_osal_eth_is_broadcast(ether_header->h_source)) {
      mtlk_core_inc_cnt(core, MTLK_CORE_CNT_MULTICAST_REPLAYED_PACKETS);
    }
    else {
      mtlk_core_inc_cnt(core, MTLK_CORE_CNT_UNICAST_REPLAYED_PACKETS);
    }

    return 1;
  }

  ILOG3_D("rsn bits 0x%02x", rsn_bits);

  if (rsn_bits & UMI_NOTIFICATION_MIC_FAILURE) {

    WLOG_DYS("CID-%04x: MIC failure from %Y frame type %s", mtlk_vap_get_oid(core->vap_handle), mtlk_sta_get_addr(sta)->au8Addr, is_mgmt_string);

    if(IW_ENCODE_ALG_TKIP == cipher)
    {
      mtlk_df_ui_mic_fail_type_t mic_fail_type =
          mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_DIRECTED) ? MIC_FAIL_PAIRWISE : MIC_FAIL_GROUP;

      mtlk_df_ui_notify_mic_failure(
          mtlk_vap_get_df(core->vap_handle),
          mtlk_sta_get_addr(sta)->au8Addr,
          mic_fail_type);

      _mtlk_core_on_mic_failure(core, mic_fail_type);
    }

    return 2;
  }

  nfrags = (rsn_bits & 0xf0) >> 4;
  if (nfrags) {
    uint64 u64buf = 0;
    char *p = (char*)&u64buf + sizeof(u64buf)-sizeof(rc);
    memcpy(p, rc, sizeof(rc));
    u64buf = be64_to_cpu(u64buf);
    u64buf += nfrags;
    u64buf = cpu_to_be64(u64buf);
    memcpy(rc, p, sizeof(rc));
  }

  memcpy(last_rc, rc, sizeof(rc));

  return 0;
}

#define SEQUENCE_NUMBER_LIMIT                    (0x1000)
#define SEQ_DISTANCE(seq1, seq2) (((seq2) - (seq1) + SEQUENCE_NUMBER_LIMIT) \
                                    % SEQUENCE_NUMBER_LIMIT)

// Send Packet to the OS's protocol stack
// (or forward)
static void
send_up (mtlk_core_t* nic, mtlk_nbuf_t *nbuf)
{
  uint32 net_state;
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);
  sta_entry *sta = mtlk_nbuf_priv_get_src_sta(nbuf_priv);
  int bridge_mode;

  bridge_mode = MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE);

  /* Count 802_1x packets */
  if (mtlk_wlan_pkt_is_802_1X(MTLK_ETH_GET_ETHER_TYPE(nbuf->data)))
    mtlk_sta_on_rx_packet_802_1x(sta);

  if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_FORWARD)) {

    /* Clone received packet for forwarding */
    mtlk_nbuf_t *cl_nbuf = NULL;
    uint16 ac;

    CPU_STAT_BEGIN_TRACK(CPU_STAT_ID_TX_FWD);

    /* Count rxed data bo be forwarded */
    mtlk_sta_on_rx_packet_forwarded(sta, nbuf);

    /* analyze (and probably adjust) packet's priority and AC */
    ac = mtlk_qos_get_ac(&nic->qos, nbuf);
    if (unlikely(ac == AC_INVALID)) {
      if (sta) {
        mtlk_sta_on_packet_dropped(sta, MTLK_TX_DISCARDED_DRV_ACM);
      } else {
        mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_ACM);
      }
    } else
      cl_nbuf = mtlk_df_nbuf_clone_with_priv(mtlk_vap_get_manager(nic->vap_handle), nbuf);

    if (likely(cl_nbuf != NULL)) {
      mtlk_mc_transmit(nic, cl_nbuf);
    } else
      nic->pstats.fwd_dropped++;

    CPU_STAT_END_TRACK(CPU_STAT_ID_TX_FWD);

  }

  if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_CONSUME)) {
    struct ethhdr *ether_header = (struct ethhdr *)nbuf->data;
#if defined MTLK_DEBUG_IPERF_PAYLOAD_RX
    //check if it is an iperf's packet we use to debug
    mtlk_iperf_payload_t *iperf = debug_ooo_is_iperf_pkt((uint8*) ether_header);
    if (iperf != NULL) {
      iperf->ts.tag_tx_to_os = htonl(debug_iperf_priv.tag_tx_to_os);
      debug_iperf_priv.tag_tx_to_os++;
    }
#endif

    ILOG3_Y("802.3 rx DA: %Y", nbuf->data);
    ILOG3_Y("802.3 rx SA: %Y", nbuf->data+ETH_ALEN);

    ILOG3_D("packet protocol %04x", ntohs(ether_header->h_proto));

    /* NOTE: Operations below can change packet payload, so it seems that we
     * may need to perform skb_unshare for the packte. But they are available
     * only for station, which does not perform packet forwarding on RX, so
     * packet cannot be cloned (flag MTLK_NBUFF_FORWARD is unset). In future this may
     * change (everything changes...) so we will need to unshare here */
    if (!mtlk_vap_is_ap(nic->vap_handle) && (bridge_mode != BR_MODE_WDS))
      mtlk_mc_restore_mac(nbuf);
    switch (bridge_mode)
    {
    case BR_MODE_MAC_CLONING:
      if (mtlk_wlan_pkt_is_802_1X(ether_header->h_proto)) {
        mtlk_osal_copy_eth_addresses(nbuf->data,
            mtlk_df_ui_get_mac_addr(mtlk_vap_get_df(nic->vap_handle)));
        ILOG2_Y("MAC Cloning enabled, DA set to %Y", nbuf->data);
      }
      break;
    case BR_MODE_L2NAT:
      mtlk_l2nat_on_rx(nic, nbuf);
      break;
    default:
      break;
    }

    net_state = mtlk_core_get_net_state(nic);
    if (net_state != NET_STATE_CONNECTED) {
      mtlk_df_nbuf_free(mtlk_vap_get_manager(nic->vap_handle), nbuf);
      if (net_state != NET_STATE_DISCONNECTING)
        WLOG_DD("CID-%04x: Data rx in not connected state (current state is %d), dropped", mtlk_vap_get_oid(nic->vap_handle), net_state);
      return;
    }

#ifdef MTLK_DEBUG_CHARIOT_OOO
    /* check out-of-order */
    {
      int diff, seq_prev;

      seq_prev = nic->seq_prev_sent[nbuf_priv->seq_qos];
      //ILOG2_DD("qos %d sn %u\n", nbuf_priv->seq_qos, nbuf_priv->seq_num);
      diff = SEQ_DISTANCE(seq_prev, nbuf_priv->seq_num);
      if (diff > SEQUENCE_NUMBER_LIMIT / 2)
        ILOG2_DDD("ooo: qos %u prev = %u, cur %u\n", nbuf_priv->seq_qos, seq_prev, nbuf_priv->seq_num);
      nic->seq_prev_sent[nbuf_priv->seq_qos] = nbuf_priv->seq_num;
    }
#endif

    // Count only packets sent to OS
    mtlk_sta_on_packet_indicated(sta, nbuf);

    mtlk_df_ui_indicate_rx_data(mtlk_vap_get_df(nic->vap_handle), nbuf);

    ILOG3_PD("nbuf %p, rx_packets %lu", nbuf, _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_PACKETS_RECEIVED));
  } else {
    mtlk_df_nbuf_free(mtlk_vap_get_manager(nic->vap_handle), nbuf);
    ILOG3_P("nbuf %p dropped - consumption is disabled", nbuf);
  }
}

static void
send_up_sub_frames (mtlk_core_t* nic, mtlk_nbuf_t *nbuf)
{
  mtlk_nbuf_t *nbuf_next;

  do {
    nbuf_next = mtlk_nbuf_priv_get_sub_frame(mtlk_nbuf_priv(nbuf));
    send_up(nic, nbuf);
    nbuf = nbuf_next;
  } while(nbuf);
}

int
mtlk_detect_replay_or_sendup(mtlk_core_t* core, mtlk_nbuf_t *nbuf, u8 *rsn)
{
  int res=0;
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);

  if (mtlk_nbuf_priv_get_src_sta(nbuf_priv) != NULL)
    res = _detect_replay(core, nbuf, rsn, FALSE);

  if (res != 0) {
    mtlk_df_nbuf_free_sub_frames(mtlk_vap_get_manager(core->vap_handle), nbuf);
  } else {
    send_up_sub_frames(core, nbuf);
  }

  return res;
}

static uint8
_mtlk_core_get_antennas_cnt(mtlk_core_t *core, mtlk_pdb_id_t id_array)
{
  mtlk_pdb_size_t size = MTLK_NUM_ANTENNAS_BUFSIZE;
  uint8 count = 0;
  uint8 val_array[MTLK_NUM_ANTENNAS_BUFSIZE];

  if (MTLK_ERR_OK != MTLK_CORE_PDB_GET_BINARY(core, id_array, val_array, &size))
  {
    MTLK_ASSERT(0); /* Can not retrieve antennas configuration from PDB */
    return 0;
  }

  for (count = 0; (0 != val_array[count]) && (count < (MTLK_NUM_ANTENNAS_BUFSIZE - 1)); count++);

  return count;

}

static uint32
mtlk_get_total_tx_power_dBm(mtlk_core_t *core)
{
uint32 calibr_algo_mask;
uint32 ant_num;
uint32 power;

  calibr_algo_mask =  MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_CALIBRATION_ALGO_MASK);
  ant_num = _mtlk_core_get_antennas_cnt(core, PARAM_DB_CORE_TX_ANTENNAS);

  /* If TPC is enabled then MIB_TX_POWER is always ignored in fw and regulatory domain is used */
  if(MTLK_BFIELD_GET(calibr_algo_mask, TPC_BIT_VAL)){

    /* calculate the power limit configured for all antennas */
    power =  mtlk_calc_tx_power_lim(&core->slow_ctx->tx_limits, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_CHANNEL_CUR),
                                     country_code_to_domain(mtlk_core_get_country_code(core)),
                                     MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE),
                                     MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_BONDING_MODE), ant_num);

    /* add back 3 dBm or 4.7 dBm depends of antennas number */
    power += mtlk_get_antennas_factor(ant_num);

    power = power>>3; /* Calculate to dBm */

    /* add vakue of MIB_USER_POWER_SELECTION a.k.a. iwpriv sPowerSelection */
    power += MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_POWER_SELECTION);
  }
  else{

    /* If MIB_TX_POWER is zero then default 17 dBm is using */
    power = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_TX_POWER);
    if (!power){
      power = DEFAULT_TX_POWER;
    }
  }

  return (power);
}

int16 __MTLK_IFUNC
mtlk_calc_tx_power_lim_wrapper(mtlk_handle_t usr_data, int8 spectrum_mode, uint8 channel)
{
    struct nic* nic = (struct nic*)usr_data;

    return mtlk_calc_tx_power_lim(&nic->slow_ctx->tx_limits,
                                  channel,
                                  country_code_to_domain(mtlk_core_get_country_code(nic)),
                                  spectrum_mode,
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_BONDING_MODE),
                                  _mtlk_core_get_antennas_cnt(nic, PARAM_DB_CORE_TX_ANTENNAS));
}

int16 __MTLK_IFUNC
mtlk_scan_calc_tx_power_lim_wrapper(mtlk_handle_t usr_data, int8 spectrum_mode, uint8 reg_domain, uint8 channel)
{
    struct nic* nic = (struct nic*)usr_data;

    return mtlk_calc_tx_power_lim(&nic->slow_ctx->tx_limits,
                                  channel,
                                  reg_domain,
                                  spectrum_mode,
                                  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_BONDING_MODE),
                                  _mtlk_core_get_antennas_cnt(nic, PARAM_DB_CORE_TX_ANTENNAS));
}

int16 __MTLK_IFUNC
mtlk_get_antenna_gain_wrapper(mtlk_handle_t usr_data, uint8 channel)
{
    struct nic* nic = (struct nic*)usr_data;

    return mtlk_get_antenna_gain(&nic->slow_ctx->tx_limits, channel);
}

int __MTLK_IFUNC
mtlk_reload_tpc_wrapper (uint8 channel, mtlk_handle_t usr_data)
{
    struct nic* nic = (struct nic*)usr_data;

    return mtlk_reload_tpc(MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE),
        MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_BONDING_MODE),
        channel,
        mtlk_vap_get_txmm(nic->vap_handle),
        nic->txmm_async_eeprom_msgs,
        ARRAY_SIZE(nic->txmm_async_eeprom_msgs),
        mtlk_core_get_eeprom(nic),
        mtlk_eeprom_get_num_antennas(mtlk_core_get_eeprom(nic)));
}

void
mtlk_record_xmit_err(struct nic *nic, mtlk_nbuf_t *nbuf)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);

  if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_FORWARD)) {
/*    TODO: GS: nic->core_stats.tx_dropped++; check it. It's taken from MAC statistic for output
  } else  {*/
    nic->pstats.fwd_dropped++;
  }

  if (++nic->pstats.tx_cons_drop_cnt > nic->pstats.tx_max_cons_drop)
    nic->pstats.tx_max_cons_drop = nic->pstats.tx_cons_drop_cnt;
}

/*****************************************************************************
**
** NAME         mtlk_xmit
**
** PARAMETERS   nbuf                 Skbuff to transmit
**              dev                 Device context
**
** RETURNS      Skbuff transmission status
**
** DESCRIPTION  This function called to perform packet transmission.
**
******************************************************************************/
int
mtlk_xmit (mtlk_core_t* nic, mtlk_nbuf_t *nbuf)
{
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);
  int res = MTLK_ERR_PKT_DROPPED;
  unsigned short ac = mtlk_qos_get_ac_by_tid(nbuf->priority);
  sta_entry *sta = NULL;
  uint32 ntx_free = 0;
  mtlk_hw_send_data_t data;
  uint32 sta_id = UMI_SID_MULTICAST;
  struct ethhdr *ether_header = (struct ethhdr *)nbuf->data;
  mtlk_pckt_filter_e sta_filter_stored;
  int bridge_mode;

  if(nic->is_stopping | nic->is_stopped)
  {
    goto end;
  }

  memset(&data, 0, sizeof(data));

  mtlk_dump(5, nbuf->data, nbuf->len, "nbuf->data received from OS");

  bridge_mode = MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE);

  /* For AP's unicast and reliable multicast if destination STA
   * id is known - get sta entry, otherwise - drop packet */
  if (__LIKELY(mtlk_nbuf_priv_check_flags(nbuf_priv,
                                          MTLK_NBUFF_UNICAST | MTLK_NBUFF_RMCAST))) {
    sta = mtlk_nbuf_priv_get_dst_sta(nbuf_priv);
    if (__UNLIKELY(sta == NULL)) {
      ELOG_DY("CID-%04x: Destination STA (%Y) is not known!", mtlk_vap_get_oid(nic->vap_handle),
           ether_header->h_dest);
      goto tx_skip;
    }
  }
  /* In STA mode all packets are unicast -
   * so we always have peer entry in this mode */
  else if (!mtlk_vap_is_ap(nic->vap_handle)) {
    sta = mtlk_stadb_get_ap(&nic->slow_ctx->stadb);
    if (__LIKELY(sta != NULL)) {
      mtlk_nbuf_priv_set_dst_sta(nbuf_priv, sta);
      mtlk_sta_decref(sta); /* De-reference of get_ap
                             * We may dereference STA since
                             * it is referenced by packet now.
                             */
    }
  }

  if (NULL != sta) {

    /* Count 802_1x packets */
    if (mtlk_wlan_pkt_is_802_1X(MTLK_ETH_GET_ETHER_TYPE(nbuf->data)))
      mtlk_sta_on_tx_packet_802_1x(sta);

    sta_filter_stored = mtlk_sta_get_packets_filter(sta);
    if (MTLK_PCKT_FLTR_ALLOW_ALL == sta_filter_stored) {
      /* all packets are allowed */
      ;
    }
    else if ((MTLK_PCKT_FLTR_ALLOW_802_1X == sta_filter_stored) &&
              mtlk_wlan_pkt_is_802_1X(MTLK_ETH_GET_ETHER_TYPE(nbuf->data))) {
      /* only 802.1x packets are allowed */
      ;
    }
    else {
      if (MTLK_PCKT_FLTR_ALLOW_802_1X == sta_filter_stored) {
        mtlk_sta_on_packet_dropped(sta, MTLK_TX_DISCARDED_EAPOL_FILTER);
      } else {
        mtlk_sta_on_packet_dropped(sta,  MTLK_TX_DISCARDED_DROP_ALL_FILTER);
      }

      goto tx_skip;
    }
   }

  data.msg = mtlk_vap_get_hw_vft(nic->vap_handle)->get_msg_to_send(nic->vap_handle, &ntx_free);
  if (ntx_free == 0) { // Do not accept packets from OS anymore
    ILOG2_V("mtlk_xmit 0, call mtlk_flctrl_stop");
    mtlk_flctrl_stop_data(nic->hw_tx_flctrl, nic->flctrl_id);
  }

  if (!data.msg) {
    if(NULL != sta) {
      mtlk_sta_on_packet_dropped(sta, MTLK_TX_DISCARDED_TX_QUEUE_OVERFLOW);
    } else {
      mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_TX_QUEUE_OVERFLOW);
    }

    ++nic->pstats.ac_dropped_counter[ac];
    nic->pstats.tx_overruns++;
    goto tx_skip;
  }

  ++nic->pstats.ac_used_counter[ac];

  ILOG4_P("got from hw_msg %p", data.msg);

  /* check if wds should be used */
  if (bridge_mode == BR_MODE_WDS && (sta != NULL) ) {
    if (mtlk_vap_is_ap(nic->vap_handle)) {
      /* WDS AP: Use 4 address for peer APs only */
      if (sta->peer_ap){
        data.wds = 1;
      }
    } else {
      /* WDS STA: Use 4 address only if 802.3 DA not equal to destination STA's MAC address */
      if ( memcmp(mtlk_sta_get_addr(sta)->au8Addr, ether_header->h_dest, ETH_ALEN) ){
        data.wds = 1;
      }
    }

    if (data.wds){
      ILOG4_DYY("WDS packet: Peer %d, STA %Y, ETH Dest %Y", sta->peer_ap, mtlk_sta_get_addr(sta), ether_header->h_dest);
    }
  }

#ifdef MTCFG_RF_MANAGEMENT_MTLK
  data.rf_mgmt_data    = MTLK_RF_MGMT_DATA_DEFAULT;
#endif
  if ( sta ) {
    sta_id = mtlk_sta_get_sta_id(sta);
#ifdef MTCFG_RF_MANAGEMENT_MTLK
    data.rf_mgmt_data    = mtlk_sta_get_rf_mgmt_data(sta);
#endif
  }
  ILOG3_D("SID: %d", sta_id);

  data.nbuf            = nbuf;
  data.size            = nbuf->len;
  data.sta_id          = sta_id;
  data.access_category = (uint8)nbuf->priority;

  if (ntohs(ether_header->h_proto) <= ETH_DATA_LEN) {
    data.encap_type = ENCAP_TYPE_8022;
  } else {
    switch (ether_header->h_proto) {
    case __constant_htons(ETH_P_AARP):
    case __constant_htons(ETH_P_IPX):
      data.encap_type = ENCAP_TYPE_STT;
      break;
    default:
      data.encap_type = ENCAP_TYPE_RFC1042;
      break;
    }
  }

#ifdef MTCFG_PER_PACKET_STATS
  mtlk_nbuf_priv_stats_set(nbuf_priv, MTLK_NBUF_STATS_DATA_SIZE, data.size);
#endif
#if defined(MTCFG_PER_PACKET_STATS) && defined(MTCFG_TSF_TIMER_ACCESS_ENABLED)
  mtlk_nbuf_priv_stats_set(nbuf_priv, MTLK_NBUF_STATS_TS_FW_IN, mtlk_hw_get_timestamp(nic->vap_handle));
#endif

  res = mtlk_vap_get_hw_vft(nic->vap_handle)->send_data(mtlk_nbuf_priv_get_vap_handle(nbuf_priv), &data);
  if (__LIKELY(res == MTLK_ERR_OK)) {
#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
    if(mtlk_vap_is_ap(nic->vap_handle)) {
      mtlk_core_t *master_core = mtlk_core_get_master(nic);
      mtlk_aocs_msdu_tx_inc_nof_used(master_core->slow_ctx->aocs, ac);
      mtlk_aocs_on_tx_msdu_sent(master_core->slow_ctx->aocs,
                                ac,
                                mtlk_sq_get_limit(master_core->sq, ac),
                                mtlk_sq_get_qsize(master_core->sq, ac));
    }
#endif
    mtlk_df_ui_notify_tx_start(mtlk_vap_get_df(nic->vap_handle));
  }
  else {
    if(NULL != sta) {
      mtlk_sta_on_packet_dropped(sta, MTLK_TX_DISCARDED_TX_QUEUE_OVERFLOW);
    } else {
      mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_TX_QUEUE_OVERFLOW);
    }
    goto tx_skip; /* will also release msg */
  }

  nic->pstats.sta_session_tx_packets++;
  nic->pstats.ac_tx_counter[ac]++;

  if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_FORWARD)) {
    nic->pstats.fwd_tx_packets++;
    nic->pstats.fwd_tx_bytes += nbuf->len;
  }

  // reset consecutive drops counter
  nic->pstats.tx_cons_drop_cnt = 0;

  return MTLK_ERR_OK;

tx_skip:
  if (data.msg) {
    mtlk_vap_get_hw_vft(nic->vap_handle)->release_msg_to_send(nic->vap_handle, data.msg);
  }

  mtlk_record_xmit_err(nic, nbuf);

end:

  return res;
}

static int
_mtlk_core_send_current_debug_tpc (mtlk_core_t *core)
{
  return mtlk_mmb_send_debug_tpc(mtlk_vap_get_hw(core->vap_handle),
                                 MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_DEBUG_TPC));
}

static int
set_80211d_mibs (struct nic *nic, uint8 spectrum, uint16 channel)
{
  uint8 country_domain =
      country_code_to_domain(mtlk_core_get_country_code(nic));

  uint8 mitigation = mtlk_get_channel_mitigation(
      country_domain,
      TRUE,
      spectrum,
      channel);

  ILOG3_D("Setting MIB_COUNTRY for 0x%x domain with HT enabled", country_domain);
  mtlk_set_mib_value_uint8(mtlk_vap_get_txmm(nic->vap_handle), MIB_SM_MITIGATION_FACTOR, mitigation);
  mtlk_mib_set_power_selection(nic);

  return mtlk_set_country_mib(mtlk_vap_get_txmm(nic->vap_handle),
                              country_domain,
                              TRUE,
                              mtlk_core_get_freq_band_cur(nic),
                              mtlk_vap_is_ap(nic->vap_handle),
                              country_code_to_country(mtlk_core_get_country_code(nic)),
                              mtlk_core_get_dot11d(nic));
}

#ifdef DEBUG_WPS
static char test_wps_ie0[] = {0xdd, 7, 0x00, 0x50, 0xf2, 0x04, 1, 2, 3};
static char test_wps_ie1[] = {0xdd, 7, 0x00, 0x50, 0xf2, 0x04, 4, 5, 6};
static char test_wps_ie2[] = {0xdd, 7, 0x00, 0x50, 0xf2, 0x04, 7, 8, 9};
#endif

static int bt_acs_send_scan_report(mtlk_core_t *core, mtlk_aocs_evt_select_t *aocs_data)
{
  int rc;
  uint8 *curr_ptr;
  uint32 scan_report_mem_size;
  uint32 chan_cnt, chan_bss_num, max_bss_num;
  mtlk_slist_entry_t *prev, *next;
  struct bt_acs_scan_report_t   *scan_report;
  struct bt_acs_channel_info_t  *chan_info;
  struct bt_acs_bss_info_t      *bss_info;
  mtlk_aocs_channel_data_t **aocs_chan_arr, **aocs_chan_curr;   /* dyn. allocated array of pointers */

  rc = MTLK_ERR_OK;

  ILOG1_V("------------ report composition ------- ");

  /* allocate memory for notification event */
  chan_cnt = mtlk_aocs_chan_list_size(core->slow_ctx->aocs);
  max_bss_num = mtlk_slist_size(&core->slow_ctx->cache.bss_list);
  scan_report_mem_size = (sizeof(struct bt_acs_scan_report_t) +
                         (sizeof(struct bt_acs_channel_info_t) * chan_cnt) +
                         (sizeof(struct bt_acs_bss_info_t) * max_bss_num));

  scan_report = mtlk_osal_mem_alloc(scan_report_mem_size, MTLK_MEM_TAG_CORE);

  /* create array of pointers to chan_data indexed by channel number */
  aocs_chan_arr = mtlk_aocs_create_chann_arr_asc(core->slow_ctx->aocs);
  aocs_chan_curr = aocs_chan_arr;

  if (!scan_report || !aocs_chan_arr){
    rc = MTLK_ERR_NO_MEM;
    goto clean_up;
  }

  /* Fill up scan report header */
  scan_report->event_id = BT_ACS_EVNT_ID_SCAN_REPORT;
  scan_report->selected_channel = aocs_data->channel;
  if (mtlk_aocs_get_spectrum_mode(core->slow_ctx->aocs)){
    scan_report->selected_bonding =
      (aocs_data->bonding == ALTERNATE_LOWER) ? BT_ACS_BONDING_LOWER : BT_ACS_BONDING_UPPER;
  }else{
    scan_report->selected_bonding = BT_ACS_BONDING_NONE;
  }
  scan_report->channel_num = chan_cnt;

  curr_ptr = (uint8*)scan_report;
  curr_ptr += sizeof(struct bt_acs_scan_report_t);
  prev = mtlk_slist_head(&core->slow_ctx->cache.bss_list);

  while(chan_cnt){
    struct bt_acs_channel_info_t  chan_info_loc;

    chan_cnt --;
    while(*aocs_chan_curr == NULL) {
      aocs_chan_curr ++; /* Skip gaps in the array */
    }

    chan_info = (struct bt_acs_channel_info_t*)curr_ptr;
    curr_ptr += sizeof(struct bt_acs_channel_info_t);

    /* Fill up AOCS related data (chan. num, CL, noise floor)  */
    chan_info_loc.num         = (*aocs_chan_curr)->stat.channel;
    chan_info_loc.floor_noise = (*aocs_chan_curr)->stat.fl_noise_metric;
    chan_info_loc.bss_num     = (*aocs_chan_curr)->stat.num_20mhz_bss +
                                (*aocs_chan_curr)->stat.num_40mhz_lw_bss +
                                (*aocs_chan_curr)->stat.num_40mhz_up_bss;

    if (chan_info_loc.num == aocs_data->channel){
      scan_report->selected_ch_floor_noise = chan_info_loc.floor_noise;
    }

    /* Fill up BSS data */
    /* Go over BSS cache entries. Starts from the
       entry stopped during previous channel processing */
    chan_bss_num = 0;
    while (prev && (next = mtlk_slist_next(prev)) != NULL){
      cache_entry_t *place = MTLK_LIST_GET_CONTAINING_RECORD(next, cache_entry_t, link_entry);
      place = MTLK_LIST_GET_CONTAINING_RECORD(next, cache_entry_t, link_entry);

      /* BSS cache entries are sorted in the same order as channels.
         Thus, skip BSS loop if channel number not reaches BSS number yet */
      if (chan_info_loc.num < place->bss.channel){
        break;
      }

      ILOG1_DYS("        #%2d BSSID %Y, %s", chan_bss_num, place->bss.bssid, place->bss.essid);
      /* process BSS entries only for current channel */
      if (chan_info_loc.num == place->bss.channel){
        /* Not more than BT_ACS_MAX_BSS_PER_CHANNEL BSSes per channel */
        if (chan_bss_num < BT_ACS_MAX_BSS_PER_CHANNEL){
          bss_info = (struct bt_acs_bss_info_t *)curr_ptr;  /* ATT: unaligned structure */
          curr_ptr += sizeof(struct bt_acs_bss_info_t);

          memcpy(bss_info->bssid, place->bss.bssid, IEEE_ADDR_LEN);
          memcpy(bss_info->ssid, place->bss.essid, sizeof(bss_info->ssid));

          bss_info->rssi = (int8)place->bss.max_rssi;
          if (place->bss.spectrum){
            bss_info->bonding =
              (place->bss.upper_lower == ALTERNATE_LOWER) ? BT_ACS_BONDING_LOWER : BT_ACS_BONDING_UPPER;
          }else{
            bss_info->bonding = BT_ACS_BONDING_NONE;
          }
          bss_info->is_intolerant = place->bss.forty_mhz_intolerant;
          ILOG1_DDD("            Added to report: rssi %d, bonding %d, intol %d ", bss_info->rssi, bss_info->bonding, bss_info->is_intolerant);
        }
        chan_bss_num++;
      }

      prev = next;
    } /* End of BSS loop within chan */

    aocs_chan_curr ++;
    chan_info_loc.bss_num = chan_bss_num;
    memcpy(chan_info, &chan_info_loc, sizeof(struct bt_acs_channel_info_t));
    ILOG1_DDD("    Ch: %d, Bss num %d, floor noise %d", chan_info->num, chan_info->bss_num, chan_info->floor_noise);

  } /* End of channel loop */

  ILOG1_DDD("Selected: Channel %d, Bonding %d, noise %d",
    scan_report->selected_channel,
    scan_report->selected_bonding,
    scan_report->selected_ch_floor_noise);

  ILOG1_V("---------- end of scan report --------- ");
  mtlk_nl_bt_acs_send_brd_msg(scan_report, (curr_ptr - (uint8*)scan_report));

clean_up:
  if (scan_report){
    mtlk_osal_mem_free(scan_report);
  }

  if (aocs_chan_arr){
    mtlk_osal_mem_free(aocs_chan_arr);
  }

  return rc;
}

static int bt_acs_send_interf_event(uint8 channel, int8	maximumValue)
{
  int rc;
  struct bt_acs_interf_event_t *interf_event;

  rc = MTLK_ERR_OK;

  /* allocate memory for notification event */
  interf_event = mtlk_osal_mem_alloc(sizeof(*interf_event), MTLK_MEM_TAG_CORE);
  if (!interf_event){
    return MTLK_ERR_NO_MEM;
  }

  interf_event->event_id = BT_ACS_EVNT_ID_INTERF_DET;
  interf_event->channel = channel;
  interf_event->floor_noise = maximumValue;

  mtlk_nl_bt_acs_send_brd_msg(interf_event, sizeof(*interf_event));

  mtlk_osal_mem_free(interf_event);

  return rc;
}

/*
+----+-------++-----------------++------------------- -----------------------------------------+
|     \      ||  Interf. Dis.   ||                   Inerf. Enabled                            |
|      \     ||--------+--------||--------+--------+----------+----------+----------+----------|
|       \    || No Meas| No Meas|| No Meas| No Meas| Measure  | Measure  | Measure  | Measure  |
|        \   || Scan20 | Scan40 || Scan20 | Scan40 | Scan20   | Scan20   | Scan40   | Scan40   |
|         \  || Regular| Regular|| Regular| Regular|  LONG    | SHORT    |  LONG    |  SHORT   |
|          \ || all       all   || all    | all    | unrestr  | restr    | unrestr  | restr    |
| Spectrum  \|| clr $$ | clr $$ || clr $$ | clr $$ |          |          |          |          |
+------------++--------+--------++--------+--------+----------+----------+----------+----------+
|   20       ||   1    |   .    ||    .   |   .    |    1     |    2     |    .     |    .     |
|   40       ||   2    |   1    ||    1   |   .    |    .     |    .     |    2     |    3     |
| 20/40,coex ||   2    |   1    ||    .   |   1    |    2     |    3     |    .     |    .     |
+------------++--------+--------++--------+--------+----------+----------+----------+----------+

1..3 - scan execution order
.    - scan skipped
*/
#define SCAN_TABLE_IDX_20     0
#define SCAN_TABLE_IDX_40     1
#define SCAN_TABLE_IDX_20_40  2
#define SCAN_TABLE_IDX_COEX   3
#define SCAN_TABLE_IDX_LAST   4

#define SCAN_TABLE_NO_MEAS_20_REGULAR_ALL   1
#define SCAN_TABLE_NO_MEAS_40_REGULAR_ALL   2
#define SCAN_TABLE_MEAS_20_LONG_UNRESTR     3
#define SCAN_TABLE_MEAS_20_SHORT_RESTR      4
#define SCAN_TABLE_MEAS_40_LONG_UNRESTR     5
#define SCAN_TABLE_MEAS_40_SHORT_RESTR      6
#define SCAN_TABLE_MEAS_LAST                7

#define SCAN_TABLE_ROW_LAST 4

static const uint8 scan_table_interf_en[SCAN_TABLE_IDX_LAST][SCAN_TABLE_ROW_LAST] = {
  /* SCAN_TABLE_IDX_20 */
  { SCAN_TABLE_MEAS_20_LONG_UNRESTR,   SCAN_TABLE_MEAS_20_SHORT_RESTR,  0, 0},

  /* SCAN_TABLE_IDX_40 */
  { SCAN_TABLE_NO_MEAS_20_REGULAR_ALL, SCAN_TABLE_MEAS_40_LONG_UNRESTR, SCAN_TABLE_MEAS_40_SHORT_RESTR, 0},

  /* SCAN_TABLE_IDX_20_40 */
  { SCAN_TABLE_NO_MEAS_40_REGULAR_ALL, SCAN_TABLE_MEAS_20_LONG_UNRESTR, SCAN_TABLE_MEAS_20_SHORT_RESTR, 0},

};

static const uint8 scan_table_interf_dis[SCAN_TABLE_IDX_LAST][SCAN_TABLE_ROW_LAST] = {
  /* SCAN_TABLE_IDX_20 */
  { SCAN_TABLE_NO_MEAS_20_REGULAR_ALL, 0, 0, 0},

  /* SCAN_TABLE_IDX_40 */
  { SCAN_TABLE_NO_MEAS_40_REGULAR_ALL, SCAN_TABLE_NO_MEAS_20_REGULAR_ALL, 0, 0},

  /* SCAN_TABLE_IDX_20_40 */
  { SCAN_TABLE_NO_MEAS_40_REGULAR_ALL, SCAN_TABLE_NO_MEAS_20_REGULAR_ALL, 0, 0},

};


static int
_mtlk_core_perform_initial_scan(mtlk_core_t *core, uint16 channel, BOOL rescan_exempted)
{
  MTLK_ASSERT(NULL != core);

  if (rescan_exempted && channel != 0) {
    ILOG0_DD("CID-%04x: in Rescan Exemption period (ch=%d): skipping the Initial Scan", mtlk_vap_get_oid(core->vap_handle),
             channel);
  }
  else if (channel == 0 || !mtlk_aocs_is_type_none(core->slow_ctx->aocs) || mtlk_20_40_is_feature_enabled(core->coex_20_40_sm)) {

    const uint8 *scan_tbl = NULL;
    uint32 spectrum_mode;

    /* Select table */
    if (TRUE == mtlk_core_get_interfdet_mode_cfg(core)){
      scan_tbl = (uint8*)scan_table_interf_en;
    }
    else{
      scan_tbl = (uint8*)scan_table_interf_dis;
    }

    /* Select row */
    spectrum_mode = _mtlk_core_get_user_spectrum_mode(core);

    if ((SPECTRUM_40MHZ == spectrum_mode && mtlk_20_40_is_feature_enabled(core->coex_20_40_sm))){
      scan_tbl += SCAN_TABLE_IDX_20_40 * SCAN_TABLE_ROW_LAST;
    }
    else if (SPECTRUM_40MHZ == spectrum_mode){
      scan_tbl += SCAN_TABLE_IDX_40 * SCAN_TABLE_ROW_LAST;
    }
    else {
      scan_tbl += SCAN_TABLE_IDX_20 * SCAN_TABLE_ROW_LAST;
    }

    /* Process all scans in row */
    while(*scan_tbl){
      BOOL interfdet_mode, is_spectrum_40, is_long_scan;

      interfdet_mode = is_spectrum_40 = is_long_scan = FALSE;

      switch(*scan_tbl++){
      case SCAN_TABLE_NO_MEAS_20_REGULAR_ALL:
        interfdet_mode = FALSE;
        is_spectrum_40 = FALSE;
        break;
      case SCAN_TABLE_NO_MEAS_40_REGULAR_ALL:
        interfdet_mode = FALSE;
        is_spectrum_40 = TRUE;
        break;
      case SCAN_TABLE_MEAS_20_LONG_UNRESTR :
        interfdet_mode = TRUE;
        is_spectrum_40 = FALSE;
        is_long_scan = TRUE;
        break;
      case SCAN_TABLE_MEAS_20_SHORT_RESTR  :
        interfdet_mode = TRUE;
        is_spectrum_40 = FALSE;
        is_long_scan = FALSE;
        break;
      case SCAN_TABLE_MEAS_40_LONG_UNRESTR :
        interfdet_mode = TRUE;
        is_spectrum_40 = TRUE;
        is_long_scan = TRUE;
        break;
      case SCAN_TABLE_MEAS_40_SHORT_RESTR  :
        interfdet_mode = TRUE;
        is_spectrum_40 = TRUE;
        is_long_scan = FALSE;
        break;
      default:
        MTLK_ASSERT(!"Invalid scan table");
        break;
      }

      mtlk_scan_set_is_interf_enabled(&core->slow_ctx->scan, interfdet_mode);

      /* Enable or Disable Interference detection if not disabled yet */
      if ((FALSE == interfdet_mode && TRUE == mtlk_core_is_interfdet_enabled(core)) ||
          (TRUE  == interfdet_mode)) {

        ILOG0_DSSS("CID-%04x: Configure Interf. Det: %s, %s, %s", mtlk_vap_get_oid(core->vap_handle),
          interfdet_mode ? "EN":"DIS",
          is_spectrum_40 ? "40MHz":"20MHz",
          is_long_scan ? "LONG":"SHORT");

        if (MTLK_ERR_OK != _mtlk_core_set_fw_interfdet_req(core, interfdet_mode, is_spectrum_40, is_long_scan)){
            ELOG_D("CID-%04x: Interfence Detector configuration failed", mtlk_vap_get_oid(core->vap_handle));
            return MTLK_ERR_SCAN_FAILED;
        }
        mtlk_core_interfdet_enable(core, interfdet_mode);

        if (interfdet_mode){ /* Configure SHORT/LONG scan if interference detection enabled */
          mtlk_scan_set_is_long_scan_enabled(&core->slow_ctx->scan, is_long_scan);
        }
      }

      /* do scan */
      ILOG0_DS("CID-%04x: Scan started %s", mtlk_vap_get_oid(core->vap_handle), is_spectrum_40 ? "40MHz":"20MHz");
      if (MTLK_ERR_OK != mtlk_scan_sync(&core->slow_ctx->scan,
                                        mtlk_core_get_freq_band_cfg(core),
                                        is_spectrum_40 ? SPECTRUM_40MHZ : SPECTRUM_20MHZ)){
        ELOG_D("CID-%04x: Initial scan failed", mtlk_vap_get_oid(core->vap_handle));
        return MTLK_ERR_SCAN_FAILED;
      }

    } /* End of While scan table */

  } /* End of AOCS || Coex */
  else {
    ILOG0_DDDD("CID-%04x: AOCS and 20/40 Coexistence are completely OFF (ch=%d type=%d 20/40=%d): skipping the Initial Scan", mtlk_vap_get_oid(core->vap_handle),
      channel, mtlk_aocs_get_type(core->slow_ctx->aocs), mtlk_20_40_is_feature_enabled(core->coex_20_40_sm));
  }
  return MTLK_ERR_OK;
}

static void
_mtlk_mbss_undo_preactivate (struct nic *nic)
{
  if (nic->aocs_started) {
    mtlk_aocs_stop(nic->slow_ctx->aocs);
    mtlk_aocs_stop_watchdog(nic->slow_ctx->aocs);
    nic->aocs_started = FALSE;
  }
}

static void
_mtlk_core_mbss_set_last_deactivate_ts (struct nic *nic)
{
  uint32 ts;

  MTLK_ASSERT(nic != NULL);

  ts = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
  if (ts == INVALID_DEACTIVATE_TIMESTAMP) {
    ++ts;
  }

  mtlk_core_get_master(nic)->slow_ctx->deactivate_ts = ts;
}

static BOOL
_mtlk_core_mbss_is_rescan_exempted (struct nic *nic)
{
  BOOL res = FALSE;

  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(mtlk_vap_is_master(nic->vap_handle) == TRUE);
  MTLK_ASSERT(mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle)) == 0);
  MTLK_ASSERT(mtlk_vap_is_ap(nic->vap_handle) == TRUE);

  if (mtlk_aocs_is_type_none(nic->slow_ctx->aocs) == FALSE) { /* AV: Note: AOCS type not related with scan exemption */
    ILOG0_D("Re-Scan cannot be exempted (AOCS type = %d)", mtlk_aocs_get_type(nic->slow_ctx->aocs));
  }
  else if (mtlk_core_is_20_40_active(nic) == TRUE) {
    ILOG0_V("Re-Scan cannot be exempted (20/40 is active)");
  }
  else {
    uint32 rescan_exemption_interval =
      MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_UP_RESCAN_EXEMPTION_TIME);

    ILOG3_DD("i=%u dts=%u", rescan_exemption_interval, nic->slow_ctx->deactivate_ts);

    if (rescan_exemption_interval &&
        nic->slow_ctx->deactivate_ts != INVALID_DEACTIVATE_TIMESTAMP) {
      uint32 now_ts = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
      uint32 delta  = (uint32)(now_ts - nic->slow_ctx->deactivate_ts);

      res = (delta < rescan_exemption_interval);

      ILOG3_DDD("rescan_exempted=%u nts=%u delta=%u", res, now_ts, delta);
    }
  }

  return res;
}

static int
_mtlk_mbss_send_preactivate_req (struct nic *nic)
{
  mtlk_get_channel_data_t   params;
  uint8                     u8SwitchMode;
  mtlk_txmm_data_t          *man_entry = NULL;
  mtlk_txmm_msg_t           man_msg;
  int                       result = MTLK_ERR_OK;
  UMI_MBSS_PRE_ACTIVATE     *pPreActivate;
  UMI_MBSS_PRE_ACTIVATE_HDR *pPreActivateHeader;
  uint8                     u8SmRequired;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), &result);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send PRE_ACTIVATE request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  man_entry->id           = UM_MAN_MBSS_PRE_ACTIVATE_REQ;
  man_entry->payload_size = mtlk_get_umi_mbss_pre_activate_size();

  memset(man_entry->payload, 0, man_entry->payload_size);
  pPreActivate = (UMI_MBSS_PRE_ACTIVATE *)(man_entry->payload);

  pPreActivateHeader = &pPreActivate->sHdr;
  pPreActivateHeader->u16Status = HOST_TO_MAC16(UMI_OK);
  pPreActivateHeader->u8_40mhzIntolerant = mtlk_20_40_is_intolerance_declared(mtlk_core_get_coex_sm(nic));
  pPreActivateHeader->u8_CoexistenceEnabled = mtlk_core_is_20_40_active(nic);
  params.reg_domain = country_code_to_domain(mtlk_core_get_country_code(nic));
  params.is_ht = TRUE;
  params.ap = mtlk_vap_is_ap(nic->vap_handle);
  params.spectrum_mode = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE);
  params.bonding  = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_BONDING_MODE);
  params.channel = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CHANNEL_CUR);

  u8SwitchMode = mtlk_get_chnl_switch_mode(
    MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE),
    MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_BONDING_MODE),
    0);

  mtlk_channels_fill_mbss_pre_activate_req_ext_data(&params, nic, u8SwitchMode, man_entry->payload);

  u8SmRequired = pPreActivate->sFrequencyElement.u8SmRequired;
  mtlk_dot11h_initial_wait_begin(mtlk_core_get_dfs(nic), u8SmRequired);

  memcpy(pPreActivate->storedCalibrationChannelBitMap,
         nic->storedCalibrationChannelBitMap,
         sizeof(nic->storedCalibrationChannelBitMap));

  ILOG0_D("CID-%04x: Sending UMI FW Preactivation", mtlk_vap_get_oid(nic->vap_handle));
  result = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't send PRE_ACTIVATE request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), result);
    goto FINISH;
  }

  if (MAC_TO_HOST16(pPreActivateHeader->u16Status) != UMI_OK) {
    ELOG_DD("CID-%04x: Error returned for PRE_ACTIVATE request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), MAC_TO_HOST16(pPreActivateHeader->u16Status));
    result = MTLK_ERR_UMI;
    goto FINISH;
  }

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return result;
}

static int
_core_set_preactivation_mibs (mtlk_core_t *core)
{
  uint32 channel, spectrum;

  MTLK_ASSERT(mtlk_vap_is_master_ap(core->vap_handle));

  channel = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_CHANNEL_CUR);
  spectrum = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE);

  if (MTLK_ERR_OK != mtlk_mib_set_pre_activate(core)) {
    return MTLK_ERR_UNKNOWN;
  }

  if (MTLK_ERR_OK != set_80211d_mibs(core, spectrum, channel)) {
    return MTLK_ERR_UNKNOWN;
  }

  if (MTLK_ERR_OK != mtlk_set_mib_value_uint8(mtlk_vap_get_txmm(core->vap_handle), MIB_SPECTRUM_MODE,
                                              MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE))) {
    return MTLK_ERR_UNKNOWN;
  }

  /* Set TPC calibration MIBs - MIB_TPC_ANT_x */
  if (MTLK_ERR_OK != mtlk_reload_tpc_wrapper(channel, HANDLE_T(core))) {
    return MTLK_ERR_UNKNOWN;
  }

  return MTLK_ERR_OK;
}

static int
_mtlk_mbss_preactivate (struct nic *nic, BOOL rescan_exempted)
{
  int                     result = MTLK_ERR_OK;
  int                     channel;
  mtlk_aocs_evt_select_t  aocs_data;
  uint8                   ap_scan_band_cfg;
  int                     actual_spectrum_mode;
  int                     prog_model_spectrum_mode;

  MTLK_ASSERT(NULL != nic);

  /* Restore bonding configured by user */
  mtlk_core_set_cur_bonding(nic, mtlk_core_get_user_bonding(nic));              /* for correct configuration dump only */
  mtlk_aocs_set_bonding(nic->slow_ctx->aocs, mtlk_core_get_user_bonding(nic));  /* channel table build based on that data */

  /* select and validate the channel and the spectrum mode*/
  channel = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CHANNEL_CFG);
  actual_spectrum_mode = _mtlk_core_get_user_spectrum_mode(nic);

  /* Limit coex to 20 if enabled */
  if (mtlk_core_is_20_40_active(nic)){
    if (actual_spectrum_mode == SPECTRUM_20MHZ){
      mtlk_20_40_limit_to_20(mtlk_core_get_coex_sm(nic), TRUE);
    }else{
      mtlk_20_40_limit_to_20(mtlk_core_get_coex_sm(nic), FALSE);
      actual_spectrum_mode = SPECTRUM_40MHZ;  /* No AUTO for AOCS if coex. is  enabled */
    }
  }
  /* 1. set auto select spectrum mode */
  mtlk_aocs_set_auto_spectrum(nic->slow_ctx->aocs, actual_spectrum_mode);

  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE, actual_spectrum_mode);

  mtlk_aocs_set_spectrum_mode(nic->slow_ctx->aocs, actual_spectrum_mode);

  ILOG0_D("CID-%04x: Pre-activation configured parameters:", mtlk_vap_get_oid(nic->vap_handle));
  mtlk_core_configuration_dump(nic);

  /************************************************************************/
  /* Add aocs initialization + loading of programming module                                                                     */
  /************************************************************************/

  mtlk_dot11h_set_event(mtlk_core_get_dfs(nic), MTLK_DFS_EVENT_CHANGE_CHANNEL_NORMAL);

  mtlk_aocs_set_config_is_ht(nic->slow_ctx->aocs, TRUE);

  mtlk_aocs_set_config_frequency_band(nic->slow_ctx->aocs, mtlk_core_get_freq_band_cur(nic));

  /* build the AOCS channel's list now */
  if (nic->aocs_started == FALSE) {
    ILOG0_D("CID-%04x: Start AOCS", mtlk_vap_get_oid(nic->vap_handle));

    if (mtlk_aocs_start(nic->slow_ctx->aocs, FALSE, mtlk_core_is_20_40_active(nic)) != MTLK_ERR_OK) {
      ELOG_D("CID-%04x: Failed to prepare AOCS for selection", mtlk_vap_get_oid(nic->vap_handle));
      result = MTLK_ERR_AOCS_FAILED;
      goto FINISH;
    }
    nic->aocs_started = TRUE;
  }

  /* now we have to perform an AP scan and update
   * the table after we have scan results. Do scan only in one band */
  ap_scan_band_cfg = mtlk_core_get_freq_band_cfg(nic);
  ILOG0_DD("CID-%04x: ap_scan_band_cfg = %d", mtlk_vap_get_oid(nic->vap_handle), ap_scan_band_cfg);
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_FREQ_BAND_CFG,
      ((ap_scan_band_cfg == MTLK_HW_BAND_2_4_GHZ) ? MTLK_HW_BAND_2_4_GHZ : MTLK_HW_BAND_5_2_GHZ) );

  /* TODO: WAVE300_SW-100: remove this after FW fix */
  /* Do not perform initial Scan in case this present */
#ifndef MBSS_FORCE_NO_AOCS_INITIAL_SELECTION
  result = _mtlk_core_perform_initial_scan(nic, channel, rescan_exempted);
  if (MTLK_ERR_OK != result) {
    goto FINISH;
  }
#endif

  aocs_data.channel = channel;
  aocs_data.bonding = mtlk_core_get_user_bonding(nic);  /* AV: Not relevant in case of channel=0 */
  aocs_data.reason = SWR_INITIAL_SELECTION;
  aocs_data.criteria = CHC_USERDEF;
  /* On initial channel selection we may use SM required channels */
  mtlk_aocs_enable_smrequired(nic->slow_ctx->aocs);

  result = mtlk_aocs_indicate_event(nic->slow_ctx->aocs, MTLK_AOCS_EVENT_SELECT_CHANNEL,
    (void*)&aocs_data, sizeof(aocs_data));
  /* After initial channel was selected we must never use sm required channels */
  mtlk_aocs_disable_smrequired(nic->slow_ctx->aocs);
  if (result == MTLK_ERR_OK){
    mtlk_aocs_indicate_event(nic->slow_ctx->aocs, MTLK_AOCS_EVENT_INITIAL_SELECTED,
      (void *)&aocs_data, sizeof(aocs_data));
  }

  if (channel == 0 && mtlk_core_get_interfdet_scan_report_en(nic)) {
    /* Report scan result if enabled */
    bt_acs_send_scan_report(nic, &aocs_data);
  }

  /* restore all after AP scan */
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_FREQ_BAND_CFG, ap_scan_band_cfg);

  /*
   * at this point spectrum & channel may be changed by AOCS
   */

  channel = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CHANNEL_CUR);
  actual_spectrum_mode = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE);

  if ((mtlk_vap_is_ap(nic->vap_handle)) && (mtlk_core_is_20_40_active(nic) == TRUE) &&
      (actual_spectrum_mode == SPECTRUM_40MHZ)) {

    uint8 secondary_channel_offset;
    switch ((signed char)MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_BONDING_MODE)) {
      case ALTERNATE_LOWER:
        secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCB;
        break;
       case ALTERNATE_UPPER:
         secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCA;
         break;
       case ALTERNATE_NONE:
         default:
         secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCN;
         MTLK_ASSERT(0);
         break;
    }
    if (mtlk_20_40_is_20_40_operation_permitted(mtlk_core_get_coex_sm(nic),
                                                channel,
                                                secondary_channel_offset) == FALSE) {
      /* The channel pair found by the AOCS is not suitable for the 40 MHz operation */
      MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_SELECTED_BONDING_MODE, ALTERNATE_NONE);
      actual_spectrum_mode = SPECTRUM_20MHZ;
      MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE, SPECTRUM_20MHZ);
      mtlk_20_40_limit_to_20(mtlk_core_get_coex_sm(nic), TRUE);
      ILOG0_DD("The channel pair found by the AOCS (primary = %d, offset = %d) is not suitable for the 40 MHz operation\n", channel, secondary_channel_offset);
      ILOG0_V("Sending a 20 MHz pre-activation request");
    }
  }

  prog_model_spectrum_mode = (actual_spectrum_mode == SPECTRUM_20MHZ) ? SPECTRUM_20MHZ : SPECTRUM_40MHZ;
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE, prog_model_spectrum_mode);

  /* after AOCS initial scan we must reload progmodel */
  if (mtlk_progmodel_load(mtlk_vap_get_txmm(nic->vap_handle), nic, mtlk_core_get_freq_band_cfg(nic),
      prog_model_spectrum_mode) != 0) {
    ELOG_D("CID-%04x: Error while downloading progmodel files", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_UNKNOWN;
    goto FINISH;
  }

  /* now check AOCS result - here all state is already restored */
  if (result != MTLK_ERR_OK) {
    ELOG_D("CID-%04x: aocs did not find available channel", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_AOCS_FAILED;
    goto FINISH;
  }
  /* update channel now */
  nic->slow_ctx->scan.last_channel = channel;
  MTLK_ASSERT(mtlk_aocs_get_cur_channel(nic->slow_ctx->aocs) == channel);

  /************************************************************************/
  /* End of aocs initialization.                                          */
  /************************************************************************/

  ILOG0_D("CID-%04x: Pre-activation selected parameters:", mtlk_vap_get_oid(nic->vap_handle));
  mtlk_core_configuration_dump(nic);

  _core_set_preactivation_mibs(nic);

  /* Set Debug TPC value */
  result = _mtlk_core_send_current_debug_tpc(nic);
pr_info("_mtlk_core_send_current_debug_tpc %i\n",result);  

  /* Set LNA */
  result = _mtlk_core_send_lna_gains_on_preactivate(nic);
pr_info("_mtlk_core_send_lna_gains_on_preactivate %i\n",result);  

  result = _mtlk_mbss_send_preactivate_req(nic);
pr_info("_mtlk_mbss_send_preactivate_req %i\n",result);  
  if (result != MTLK_ERR_OK) {
    goto FINISH;
  }

  /* send UM_MAN_CHANGE_TX_POWER_LIMIT_REQ after channel selection,
   * "MIB_TPC_ANT_" configuration, issue WAVE300_SW-2705 */
  /* send TX power limit to FW */
  result = mtlk_set_power_limit(nic);
pr_info("mtlk_set_power_limit %i\n",result);  
  result = mtlk_core_set_ra_protection(nic);
pr_info("mtlk_core_set_ra_protection %i\n",result);  

//TODO returns error??
  result = mtlk_core_set_force_ncb(nic);
pr_info("mtlk_core_set_force_ncb %i\n",result);  
  result = mtlk_core_set_n_rate_bo(nic);
pr_info("mtlk_core_set_n_rate_bo %i\n",result);  

//TODO pc2005, these new will crash the system !!!! (03.04.02.00.25 was OK without it)
//probably needs newer interface version
//  result = _mtlk_core_send_current_rx_high_threshold(nic);
//pr_info("_mtlk_core_send_current_rx_high_threshold %i\n",result);  
//  result = _mtlk_core_send_current_cca_threshold(nic);
//pr_info("_mtlk_core_send_current_cca_threshold %i\n",result);  

  mtlk_core_abilities_disable_11b_abilities(nic->vap_handle);

FINISH:
  if (result == MTLK_ERR_OK) {
    ILOG0_D("CID-%04x: _mtlk_mbss_preactivate returned successfully", mtlk_vap_get_oid(nic->vap_handle));
  }
  else {
    ELOG_D("CID-%04x: _mtlk_mbss_preactivate returned with error", mtlk_vap_get_oid(nic->vap_handle));
  }

  return result;
}

int
mtlk_mbss_send_vap_add (struct nic *nic)
{
  mtlk_txmm_data_t        *man_entry = NULL;
  mtlk_txmm_msg_t         man_msg;
  int                     result = MTLK_ERR_OK;
  UMI_VAP_DB_OP           *pAddRequest;


  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), &result);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send ADD VAP request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  man_entry->id           = UM_MAN_VAP_DB_REQ;
  man_entry->payload_size = sizeof (UMI_VAP_DB_OP);


  pAddRequest = (UMI_VAP_DB_OP *)(man_entry->payload);
  pAddRequest->u16Status = HOST_TO_MAC16(UMI_OK);
  pAddRequest->u8OperationCode = VAP_OPERATION_ADD;
  pAddRequest->u8VAPIdx = mtlk_vap_get_id(nic->vap_handle);

  result = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't send ADD VAP request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), result);
    goto FINISH;
  }

  if (MAC_TO_HOST16(pAddRequest->u16Status) != UMI_OK) {
    ELOG_DD("CID-%04x: Error returned for VAP ADD request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), MAC_TO_HOST16(pAddRequest->u16Status));
    result = MTLK_ERR_UMI;
    goto FINISH;
  }

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return result;
}

int
mtlk_mbss_send_vap_activate (struct nic *nic)
{
  mtlk_txmm_data_t* man_entry=NULL;
  mtlk_txmm_msg_t activate_msg;
  int essid_len;
  UMI_ACTIVATE_VAP *areq;
  int result = MTLK_ERR_OK;
  uint8 net_mode = NUM_OF_NETWORK_MODES;


  ILOG0_D("CID-%04x: Entering mtlk_mbss_send_vap_activate", mtlk_vap_get_oid(nic->vap_handle));

#ifdef DEBUG_WPS
  mtlk_core_set_gen_ie(nic, test_wps_ie0, sizeof(test_wps_ie0), 0);
  mtlk_core_set_gen_ie(nic, test_wps_ie2, sizeof(test_wps_ie2), 2);
#endif
  // Start activation request
  ILOG0_D("CID-%04x: Start activation", mtlk_vap_get_oid(nic->vap_handle));

  man_entry = mtlk_txmm_msg_init_with_empty_data(&activate_msg, mtlk_vap_get_txmm(nic->vap_handle), &result);
  if (man_entry == NULL)
  {
    ELOG_D("CID-%04x: Can't send ACTIVATE request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    return result;
  }

  man_entry->id           = UM_MAN_ACTIVATE_VAP_REQ;
  man_entry->payload_size = sizeof (UMI_ACTIVATE_VAP);

  areq = (UMI_ACTIVATE_VAP*)(man_entry->payload);
  memset(areq, 0, sizeof(UMI_ACTIVATE_VAP));

  essid_len = sizeof(areq->sSSID.acESSID);
  result = mtlk_pdb_get_string(mtlk_vap_get_param_db(nic->vap_handle),
                               PARAM_DB_CORE_ESSID, areq->sSSID.acESSID, &essid_len);
  if (MTLK_ERR_OK != result) {
    ELOG_D("CID-%04x: ESSID parameter has wrong length", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }
  essid_len = strlen(areq->sSSID.acESSID);

  if (0 == essid_len) {
    ELOG_D("CID-%04x: ESSID is not set", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }

  net_mode = mtlk_core_get_network_mode_cur(nic);
  mtlk_pdb_get_mac(
      mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_MAC_ADDR, areq->sBSSID.au8Addr);
  areq->sSSID.u8Length = essid_len;
  areq->isHiddenBssID = nic->slow_ctx->cfg.is_hidden_ssid;
  areq->networkMode = net_mode;
  areq->basicRateSet = HOST_TO_MAC32(user_rates_to_fw(nic, get_basic_rateset(nic, net_mode, FALSE)));
  areq->supportedRateSet = HOST_TO_MAC32(user_rates_to_fw(nic, get_supported_rateset(nic, net_mode, FALSE)));
  areq->extendedRateSet = HOST_TO_MAC32(user_rates_to_fw(nic, get_extended_rateset(nic, net_mode, FALSE)));
  areq->operationalRateSet = HOST_TO_MAC32(get_operate_rate_set(net_mode));
  areq->pmfActivated = (uint8) MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PMF_ACTIVATED);
  areq->pmfRequired  = (uint8) MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PMF_REQUIRED);
  areq->u16BSStype = HOST_TO_MAC16(CFG_ACCESS_POINT);
  areq->u16Status = HOST_TO_MAC16(UMI_OK);

  ASSERT(sizeof(areq->sRSNie) == sizeof(nic->slow_ctx->rsnie));
  memcpy(&areq->sRSNie, &nic->slow_ctx->rsnie, sizeof(areq->sRSNie));

  MTLK_BFIELD_SET(areq->u8UapsdInfo, UAPSD_ENABLE, nic->uapsd_enabled);
  MTLK_BFIELD_SET(areq->u8UapsdInfo, UAPSD_AP_MAX_SP_LENGTH, nic->uapsd_max_sp);

  if (mtlk_core_set_net_state(nic, NET_STATE_ACTIVATING) != MTLK_ERR_OK) {
    ELOG_D("CID-%04x: Failed to switch core to state ACTIVATING", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }

  nic->activation_status = FALSE;

  mtlk_osal_event_reset(&nic->slow_ctx->connect_event);

  mtlk_dump(1, areq, sizeof(UMI_ACTIVATE_VAP), "dump of UMI_ACTIVATE_VAP:");
  ILOG0_D("CID-%04x: UMI_ACTIVATE_VAP", mtlk_vap_get_oid(nic->vap_handle));
  ILOG0_DY("CID-%04x: \tsBSSID = %Y", mtlk_vap_get_oid(nic->vap_handle), areq->sBSSID.au8Addr);
  ILOG0_DS("CID-%04x: \tsSSID = %s", mtlk_vap_get_oid(nic->vap_handle), areq->sSSID.acESSID);
  ILOG0_DD("CID-%04x: \tHidden = %d", mtlk_vap_get_oid(nic->vap_handle), areq->isHiddenBssID);
  mtlk_dump(0, areq->sRSNie.au8RsnIe, sizeof(areq->sRSNie.au8RsnIe), "\tsRSNie = ");


  ILOG0_DDS("CID-%04x: mtlk_txmm_msg_send, MsgId = 0x%x, ESSID = %s", mtlk_vap_get_oid(nic->vap_handle), man_entry->id, areq->sSSID.acESSID);

  result = mtlk_txmm_msg_send_blocked(&activate_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Cannot send activate request due to TXMM err#%d", mtlk_vap_get_oid(nic->vap_handle), result);
    goto FINISH;
  }

  if (areq->u16Status != UMI_OK && areq->u16Status != UMI_ALREADY_ENABLED)
  {
    WLOG_DD("CID-%04x: Activate VAP request failed with code %d", mtlk_vap_get_oid(nic->vap_handle), areq->u16Status);
    result = MTLK_ERR_UNKNOWN;
    goto FINISH;
  }

  nic->activation_status = TRUE;
  result = MTLK_ERR_OK;

  mtlk_core_set_net_state(nic, NET_STATE_CONNECTED);
  if (mtlk_vap_is_master_ap(nic->vap_handle)) {
    result = mtlk_aocs_start_watchdog(nic->slow_ctx->aocs);
    if (result != MTLK_ERR_OK) {
      ELOG_DD("CID-%04x: Can't start AOCS watchdog: %d", mtlk_vap_get_oid(nic->vap_handle), result);
      mtlk_core_set_net_state(nic, NET_STATE_HALTED);
      goto CLEANUP;
    }
  }

FINISH:
  if (result != MTLK_ERR_OK && mtlk_core_get_net_state(nic) != NET_STATE_READY) {
    mtlk_core_set_net_state(nic, NET_STATE_READY);
  }

CLEANUP:
  mtlk_txmm_msg_cleanup(&activate_msg);
  return result;
}

static int
_mtlk_core_send_vap_deactivate_req_blocked (struct nic *nic)
{
  mtlk_txmm_msg_t     man_msg;
  mtlk_txmm_data_t    *man_entry = NULL;
  UMI_DEACTIVATE_VAP  *psUmiDeactivate;
  int                 net_state = mtlk_core_get_net_state(nic);
  int                 result = MTLK_ERR_OK;
  uint8               mac_addr[ETH_ALEN];

  if (net_state != NET_STATE_HALTED) {
    result = mtlk_core_set_net_state(nic, NET_STATE_DISCONNECTING);
    if (result != MTLK_ERR_OK) {
      goto FINISH;
    }
  }

  if (net_state == NET_STATE_HALTED) {
    /* Do not send anything to halted MAC or if STA hasn't been connected */
    clean_all_sta_on_disconnect(nic);
    result = MTLK_ERR_OK;
    goto FINISH;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), &result);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send DISCONNECT request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  man_entry->id           = UM_MAN_DEACTIVATE_VAP_REQ;
  man_entry->payload_size = sizeof(UMI_DEACTIVATE_VAP);
  psUmiDeactivate         = (UMI_DEACTIVATE_VAP *)man_entry->payload;

  psUmiDeactivate->u16Status = HOST_TO_MAC16(UMI_OK);

  mtlk_dump(2, psUmiDeactivate, sizeof(UMI_DEACTIVATE_VAP), "dump of UMI_DEACTIVATE:");

  result = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't send DEACTIVATE request to VAP (err=%d)", mtlk_vap_get_oid(nic->vap_handle), result);
    goto FINISH;
  }

  if (MAC_TO_HOST16(psUmiDeactivate->u16Status) != UMI_OK) {
    WLOG_DD("CID-%04x: Deactivation failed in FW (status=%u)", mtlk_vap_get_oid(nic->vap_handle),
      MAC_TO_HOST16(psUmiDeactivate->u16Status));
    result = MTLK_ERR_MAC;
    goto FINISH;
  }

  /* update disconnections statistics */
  nic->pstats.num_disconnects++;

  clean_all_sta_on_disconnect(nic);

  mtlk_pdb_get_mac(
      mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_MAC_ADDR, mac_addr);

  ILOG1_DYD("CID-%04x: Station %Y disconnected (status %u)",
            mtlk_vap_get_oid(nic->vap_handle),
            mac_addr,
            MAC_TO_HOST16(MAC_TO_HOST16(psUmiDeactivate->u16Status)));

  result = MTLK_ERR_OK;

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return result;
}

static int
_mtlk_mbss_deactivate_vap (mtlk_core_t *nic)
{
  int res = MTLK_ERR_OK;

  mtlk_osal_event_reset(&nic->slow_ctx->vap_removed_event);
  res = _mtlk_core_send_vap_deactivate_req_blocked(nic);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Timeout reached while waiting for VAP deactivation - Asserting FW", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  ILOG1_V("Waiting for VAP removal event...");
  res = mtlk_osal_event_wait(&nic->slow_ctx->vap_removed_event, VAP_REMOVAL_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Timeout reached while waiting for VAP deletion indication from firmware - Asserting FW", mtlk_vap_get_oid(nic->vap_handle));
  }

FINISH:
  if (MTLK_ERR_OK != res) {
    res = MTLK_ERR_FW;
    mtlk_core_set_net_state(nic, NET_STATE_HALTED);
  }
  cleanup_on_disconnect(nic);
  return res;
}

int
mtlk_mbss_send_vap_delete (struct nic *nic)
{
  mtlk_txmm_data_t        *man_entry = NULL;
  mtlk_txmm_msg_t         man_msg;
  int                     result = MTLK_ERR_OK;
  UMI_VAP_DB_OP           *pRemoveRequest;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), &result);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send Remove VAP request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  man_entry->id           = UM_MAN_VAP_DB_REQ;
  man_entry->payload_size = sizeof (UMI_VAP_DB_OP);

  pRemoveRequest = (UMI_VAP_DB_OP *)(man_entry->payload);
  pRemoveRequest->u16Status = MTLK_ERR_OK;
  pRemoveRequest->u8OperationCode = VAP_OPERATION_DEL;
  pRemoveRequest->u8VAPIdx = mtlk_vap_get_id(nic->vap_handle);

  result = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't send Remove VAP request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), result);
    goto FINISH;
  }

  if (pRemoveRequest->u16Status != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Error returned for VAP REMOVE request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), pRemoveRequest->u16Status);
    result = MTLK_ERR_UMI;
    goto FINISH;
  }

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return result;
}

static int
_mtlk_mbss_preactivate_if_needed (mtlk_core_t *core, BOOL rescan_exempted)
{
  int result = MTLK_ERR_OK;

  MTLK_ASSERT(NULL != core);
  if (0 == mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(core->vap_handle))) {
    result = _mtlk_mbss_preactivate(mtlk_core_get_master(core), rescan_exempted);
  }
  return result;
}

static BOOL
_mtlk_core_mbss_check_activation_params (struct nic *nic)
{
  BOOL res = FALSE;
  int  essid_len;
  char essid[MIB_ESSID_LENGTH+1];

  MTLK_ASSERT(NULL != nic);
  essid_len = sizeof(essid);

  if (MTLK_ERR_OK != mtlk_pdb_get_string(mtlk_vap_get_param_db(nic->vap_handle),
                                         PARAM_DB_CORE_ESSID, essid, &essid_len)) {
    ELOG_D("CID-%04x: ESSID parameter has wrong length", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }
  essid[sizeof(essid) - 1] = '\0';
  essid_len = strlen(essid);

  if (0 == essid_len) {
    ELOG_D("CID-%04x: ESSID is not set", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  res = TRUE;

FINISH:
  return res;
}

int
mtlk_mbss_init (struct nic *nic, BOOL rescan_exempted)
{
  int result = MTLK_ERR_PARAMS;
  if (_mtlk_core_mbss_check_activation_params(nic) != TRUE) {
    ELOG_D("CID-%04x: mtlk_mbss_init: call to _mtlk_core_mbss_check_activation_params returned with error", mtlk_vap_get_oid(nic->vap_handle));
    goto error_params;
  }
  ILOG0_D("CID-%04x: _mtlk_core_mbss_check_activation_params returned successfully", mtlk_vap_get_oid(nic->vap_handle));

  result = _mtlk_mbss_preactivate_if_needed (nic, rescan_exempted);
  if (result != MTLK_ERR_OK) {
    ELOG_D("CID-%04x: mtlk_mbss_init: call to _mtlk_mbss_preactivate_if_needed returned with error", mtlk_vap_get_oid(nic->vap_handle));
    goto error_preactivation;
  }
  ILOG0_D("CID-%04x: _mtlk_mbss_preactivate_if_needed returned successfully", mtlk_vap_get_oid(nic->vap_handle));

  result = mtlk_mbss_send_vap_activate(nic);
  if (result != MTLK_ERR_OK) {
    ELOG_D("CID-%04x: mtlk_mbss_init: call to mtlk_mbss_send_vap_activate returned with error", mtlk_vap_get_oid(nic->vap_handle));
    goto error_activation;
  }
  ILOG0_D("CID-%04x: mtlk_mbss_send_vap_activate returned successfully", mtlk_vap_get_oid(nic->vap_handle));

  return MTLK_ERR_OK;

error_preactivation:
error_activation:
  ILOG0_D("CID-%04x: before _mtlk_mbss_undo_preactivate", mtlk_vap_get_oid(nic->vap_handle));
  if ((0 == mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle)))) {
    _mtlk_mbss_undo_preactivate(mtlk_core_get_master(nic));
  }

  ILOG0_D("CID-%04x: after _mtlk_mbss_undo_preactivate", mtlk_vap_get_oid(nic->vap_handle));

error_params:
  return result;
}

static int
mtlk_send_activate (struct nic *nic)
{
  mtlk_txmm_data_t* man_entry = NULL;
  mtlk_txmm_msg_t activate_msg;
  int channel, bss_type, essid_len;
  UMI_MBSS_PRE_ACTIVATE *pPreActivate;
  UMI_ACTIVATE_HDR *areq;
  int result = MTLK_ERR_OK;
  uint8 u8SwitchMode;
  BOOL aocs_started = FALSE;
  mtlk_get_channel_data_t params;

  MTLK_ASSERT(!mtlk_vap_is_ap(nic->vap_handle));

  if (!mtlk_vap_is_ap(nic->vap_handle)) {
    bss_type = CFG_INFRA_STATION;
  } else {
    bss_type = CFG_ACCESS_POINT;
  }

#ifdef DEBUG_WPS
  mtlk_core_set_gen_ie(nic, test_wps_ie0, sizeof(test_wps_ie0), 0);
  mtlk_core_set_gen_ie(nic, test_wps_ie2, sizeof(test_wps_ie2), 2);
#endif
  /* Start activation request */
  ILOG2_D("CID-%04x: Start activation", mtlk_vap_get_oid(nic->vap_handle));

  if (mtlk_core_is_20_40_active(nic)) {
    man_entry = mtlk_txmm_msg_init_with_empty_data(&activate_msg, mtlk_vap_get_txmm(nic->vap_handle), &result);
    if (man_entry == NULL) {
      ELOG_D("CID-%04x: Can't send PRE_ACTIVATE request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
      return result;
    }

    man_entry->id           = UM_MAN_MBSS_PRE_ACTIVATE_REQ;
    man_entry->payload_size = mtlk_get_umi_mbss_pre_activate_size ();

    memset(man_entry->payload, 0, man_entry->payload_size);
    pPreActivate = (UMI_MBSS_PRE_ACTIVATE *)(man_entry->payload);
    /* STA mode: FW ignores other info rather then 20/40 info on PreActivate Hdr*/
    pPreActivate->sHdr.u8_CoexistenceEnabled = mtlk_20_40_is_feature_enabled(mtlk_core_get_coex_sm(nic));
    pPreActivate->sHdr.u8_40mhzIntolerant = mtlk_20_40_is_intolerance_declared(mtlk_core_get_coex_sm(nic));

    result = mtlk_txmm_msg_send_blocked(&activate_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
    mtlk_txmm_msg_cleanup(&activate_msg);

    if (result != MTLK_ERR_OK) {
      ELOG_DD("CID-%04x: Can't send PRE_ACTIVATE request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), result);
      return result;
    }
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&activate_msg, mtlk_vap_get_txmm(nic->vap_handle), &result);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send ACTIVATE request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    return result;
  }

  man_entry->id           = UM_MAN_ACTIVATE_REQ;
  man_entry->payload_size = mtlk_get_umi_activate_size();

  areq = mtlk_get_umi_activate_hdr(man_entry->payload);
  memset(areq, 0, sizeof(UMI_ACTIVATE_HDR));

  channel = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CHANNEL_CUR);
  /* for AP channel 0 means "use AOCS", but for STA channel must be set
     implicitly - we cannot send 0 to MAC in activation request */
  if ((channel == 0) && !mtlk_vap_is_master_ap(nic->vap_handle)) {
    ELOG_D("CID-%04x: Channel must be specified for station or Virtual AP", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }

  essid_len = sizeof(areq->sSSID.acESSID);
  result = mtlk_pdb_get_string(mtlk_vap_get_param_db(nic->vap_handle),
                               PARAM_DB_CORE_ESSID, areq->sSSID.acESSID, &essid_len);
  if (MTLK_ERR_OK != result) {
    ELOG_D("CID-%04x: ESSID parameter has wrong length", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }
  essid_len = strlen(areq->sSSID.acESSID);
  if (0 == essid_len) {
    ELOG_D("CID-%04x: ESSID is not set", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }

  /* Do not allow to activate if BSSID isn't set for the STA. Probably it
   * is worth to not allow this on AP as well? */
  if (!mtlk_vap_is_ap(nic->vap_handle) &&
     (0 == MTLK_CORE_HOT_PATH_PDB_CMP_MAC(nic, PARAM_DB_CORE_BSSID, mtlk_osal_eth_zero_addr))) {
    ELOG_D("CID-%04x: BSSID is not set", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }

  if (!mtlk_vap_is_ap(nic->vap_handle)) {
    _mtlk_core_sta_country_code_set_default_on_activate(nic);
  }

  mtlk_pdb_get_mac(mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_BSSID, areq->sBSSID.au8Addr);
  areq->sSSID.u8Length = essid_len;
  areq->u16RestrictedChannel = cpu_to_le16(channel);
  areq->u16BSStype = cpu_to_le16(bss_type);
  areq->isHiddenBssID = nic->slow_ctx->cfg.is_hidden_ssid;

  ILOG0_D("CID-%04x: Activation started with parameters:", mtlk_vap_get_oid(nic->vap_handle));
  mtlk_core_configuration_dump(nic);

  mtlk_dot11h_set_event(mtlk_core_get_dfs(nic), MTLK_DFS_EVENT_CHANGE_CHANNEL_NORMAL);

  u8SwitchMode = mtlk_get_chnl_switch_mode(MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE), MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_BONDING_MODE), 0);

  /* get data from Regulatory table:
     Availability Check Time,
     Scan Type
  */

  ILOG2_DD("CurrentSpectrumMode = %d\n"
           "RFSpectrumMode = %d",
           MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE),
           MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE));

  params.reg_domain = country_code_to_domain(mtlk_core_get_country_code(nic));
  params.is_ht = TRUE;
  params.ap = mtlk_vap_is_ap(nic->vap_handle);
  params.spectrum_mode = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE);
  params.bonding = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_BONDING_MODE);
  params.channel = channel;

  mtlk_channels_fill_activate_req_ext_data(&params, nic, u8SwitchMode, man_entry->payload);

  /* TODO- add SmRequired to 11d table !! */
  /*
    ILOG1_SSDY("activating (mode:%s, essid:\"%s\", chan:%d, bssid %Y)...",
                bss_type_str, nic->slow_ctx->essid, channel, nic->slow_ctx->bssid);
  */

  /*********************** END NEW **********************************/

  set_80211d_mibs(nic, MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE), channel);

  /* Set TPC calibration MIBs */
  mtlk_reload_tpc_wrapper(channel, HANDLE_T(nic));

  ASSERT(sizeof(areq->sRSNie) == sizeof(nic->slow_ctx->rsnie));
  memcpy(&areq->sRSNie, &nic->slow_ctx->rsnie, sizeof(areq->sRSNie));

  if (mtlk_core_set_net_state(nic, NET_STATE_ACTIVATING) != MTLK_ERR_OK) {
    ELOG_D("CID-%04x: Failed to switch core to state ACTIVATING", mtlk_vap_get_oid(nic->vap_handle));
    result = MTLK_ERR_NOT_READY;
    goto FINISH;
  }

  nic->activation_status = FALSE;
  mtlk_osal_event_reset(&nic->slow_ctx->connect_event);

  mtlk_dump(3, areq, sizeof(UMI_ACTIVATE_HDR), "dump of UMI_ACTIVATE_HDR:");

  result = mtlk_txmm_msg_send_blocked(&activate_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Cannot send activate request due to TXMM err#%d", mtlk_vap_get_oid(nic->vap_handle), result);
    goto FINISH;
  }

  if (areq->u16Status != UMI_OK && areq->u16Status != UMI_ALREADY_ENABLED) {
    WLOG_DD("CID-%04x: Activate VAP request failed with code %d", mtlk_vap_get_oid(nic->vap_handle), areq->u16Status);
    result = MTLK_ERR_UNKNOWN;
    goto FINISH;
  }

  /* now wait and handle connection event if any */
  ILOG4_V("Timestamp before network status wait...");

  result = mtlk_osal_event_wait(&nic->slow_ctx->connect_event, CONNECT_TIMEOUT);
  if (result == MTLK_ERR_TIMEOUT) {
    /* MAC is dead? Either fix MAC or increase timeout */
    ELOG_DS("CID-%04x: Timeout reached while waiting for %s event", mtlk_vap_get_oid(nic->vap_handle),
           mtlk_vap_is_ap(nic->vap_handle) ? "BSS creation" : "connection");
    mtlk_core_set_net_state(nic, NET_STATE_HALTED);
    goto CLEANUP;
  } else if (nic->activation_status) {
    mtlk_core_set_net_state(nic, NET_STATE_CONNECTED);
    if (mtlk_vap_is_master_ap(nic->vap_handle)) {
      result = mtlk_aocs_start_watchdog(nic->slow_ctx->aocs);
      if (result != MTLK_ERR_OK) {
        ELOG_DD("CID-%04x: Can't start AOCS watchdog: %d", mtlk_vap_get_oid(nic->vap_handle), result);
        mtlk_core_set_net_state(nic, NET_STATE_HALTED);
        goto CLEANUP;
      }
    }
  } else {
    ELOG_D("CID-%04x: Activate failed. Switch to NET_STATE_READY", mtlk_vap_get_oid(nic->vap_handle));
    mtlk_core_set_net_state(nic, NET_STATE_READY);
    goto CLEANUP;
  }

FINISH:
  if (result != MTLK_ERR_OK &&
      mtlk_core_get_net_state(nic) != NET_STATE_READY)
      mtlk_core_set_net_state(nic, NET_STATE_READY);

CLEANUP:
  mtlk_txmm_msg_cleanup(&activate_msg);

  if (result != MTLK_ERR_OK && aocs_started)
    mtlk_aocs_stop(nic->slow_ctx->aocs);

  return result;
}

static int __MTLK_IFUNC
_mtlk_core_connect_sta(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  int res = MTLK_ERR_OK;
  bss_data_t bss_found;
  int freq;
  mtlk_aux_pm_related_params_t pm_params;
  uint8 *addr;
  uint32 addr_size;
  uint32 new_spectrum_mode;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  addr = mtlk_clpb_enum_get_next(clpb, &addr_size);
  MTLK_ASSERT(NULL != addr);

  if (mtlk_vap_is_ap(nic->vap_handle)) {
    res = MTLK_ERR_NOT_SUPPORTED;
    goto end_activation;
  }

  if (  (mtlk_core_get_net_state(nic) != NET_STATE_READY)
      || mtlk_core_scan_is_running(nic)
      || mtlk_core_is_stopping(nic)) {
    ILOG1_V("Can't connect to AP - unappropriated state");
    res = MTLK_ERR_NOT_READY;
    /* We shouldn't update current network mode in these cases */
    goto end;
  }

  if (mtlk_cache_find_bss_by_bssid(&nic->slow_ctx->cache, addr, &bss_found, NULL) == 0) {
    ILOG1_V("Can't connect to AP - unknown BSS");
    res = MTLK_ERR_PARAMS;
    goto end_activation;
  }

  /* store actual BSS data */
  memcpy(&nic->slow_ctx->bss_data, &bss_found, sizeof(nic->slow_ctx->bss_data));

  /* update regulation limits for the BSS */
  if (mtlk_core_get_dot11d(nic)) {
    if (!bss_found.country_ie) {
      struct country_ie_t *country_ie;

      /* AP we are connecting to has no Country IE - use from the first found */
      country_ie = mtlk_cache_find_first_country_ie(&nic->slow_ctx->cache,
        mtlk_core_get_country_code(nic));
      if (country_ie) {
        ILOG1_V("Updating regulation limits from the first found BSS's country IE");
        mtlk_update_reg_limit_table(HANDLE_T(nic), country_ie, bss_found.power);
        mtlk_osal_mem_free(country_ie);
      }
    } else {
      ILOG1_V("Updating regulation limits from the current BSS's country IE");
      mtlk_update_reg_limit_table(HANDLE_T(nic), bss_found.country_ie, bss_found.power);
    }

    _mtlk_core_sta_country_code_update_on_connect(nic, bss_found.country_code);
  }

  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_CHANNEL_CUR, bss_found.channel);
  /* save ESSID */
  res = mtlk_pdb_set_string(mtlk_vap_get_param_db(nic->vap_handle),
                            PARAM_DB_CORE_ESSID, bss_found.essid);
  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: Can't store ESSID (err=%d)", mtlk_vap_get_oid(nic->vap_handle), res);
    goto end_activation;
  }
  /* save BSSID so we can use it on activation */
  mtlk_pdb_set_mac(mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_BSSID, addr);
  /* set bonding according to the AP */
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_SELECTED_BONDING_MODE, bss_found.upper_lower);

  /* set current frequency band */
  freq = channel_to_band(bss_found.channel);
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_FREQ_BAND_CUR, freq);

  /* set current HT capabilities */
  if (BSS_IS_WEP_ENABLED(&bss_found) && nic->slow_ctx->wep_enabled) {
    /* no HT is allowed for WEP connections */
    MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_IS_HT_CUR, FALSE);
  }
  else {
    MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_IS_HT_CUR,
        (mtlk_core_get_is_ht_cfg(nic) && bss_found.is_ht));
  }

  /* set TKIP */
  nic->slow_ctx->is_tkip = 0;
  if (mtlk_core_get_is_ht_cur(nic) && nic->slow_ctx->rsnie.au8RsnIe[0]) {
    struct wpa_ie_data d;
    ASSERT(nic->slow_ctx->rsnie.au8RsnIe[1]+2 <= sizeof(nic->slow_ctx->rsnie));
    if (wpa_parse_wpa_ie(nic->slow_ctx->rsnie.au8RsnIe,
      nic->slow_ctx->rsnie.au8RsnIe[1] + 2, &d) < 0) {
        ELOG_D("CID-%04x: Can not parse WPA/RSN IE", mtlk_vap_get_oid(nic->vap_handle));
        res = MTLK_ERR_PARAMS;
        goto end_activation;
    }
    if (d.pairwise_cipher == UMI_RSN_CIPHER_SUITE_TKIP) {
      WLOG_D("CID-%04x: Connection in HT mode with TKIP is prohibited by standard, trying non-HT mode...", mtlk_vap_get_oid(nic->vap_handle));
      MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_IS_HT_CUR, FALSE);
      nic->slow_ctx->is_tkip = 1;
    }
  }

  /* for STA spectrum mode should be set according to our own HT capabilities */
  if (mtlk_core_get_is_ht_cur(nic) == 0) {
    /* e.g. if we connect to A/N AP, but STA is A then we should select 20MHz  */
    new_spectrum_mode = SPECTRUM_20MHZ;
  } else {
    new_spectrum_mode = bss_found.spectrum;

    if (SPECTRUM_40MHZ == bss_found.spectrum) {
      uint32 sta_force_spectrum_mode =
          MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_STA_FORCE_SPECTRUM_MODE);
      /* force set spectrum mode */
      if (   (SPECTRUM_20MHZ == sta_force_spectrum_mode)
          || (   (SPECTRUM_AUTO == sta_force_spectrum_mode)
              && (MTLK_HW_BAND_2_4_GHZ == mtlk_core_get_freq_band_cur(nic)))) {

        new_spectrum_mode = SPECTRUM_20MHZ;
      }
    }
  }
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE, new_spectrum_mode);
  ILOG1_DS("CID-%04x: Set SpectrumMode: %s MHz", mtlk_vap_get_oid(nic->vap_handle), new_spectrum_mode ? "40": "20");

  /* previously set network mode shouldn't be overridden,
   * but in case it "MTLK_HW_BAND_BOTH" it need to be recalculated, this value is not
   * acceptable for MAC! */
  if(MTLK_HW_BAND_BOTH == net_mode_to_band(mtlk_core_get_network_mode_cur(nic))) {
     MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_NET_MODE_CUR,
         get_net_mode(freq, mtlk_core_get_is_ht_cur(nic)));
  }

  /* perform CB scan, but only once per band */
  if (!(nic->cb_scanned_bands &
    (freq == MTLK_HW_BAND_2_4_GHZ ? CB_SCANNED_2_4 : CB_SCANNED_5_2)))
  {
    /* perform CB scan to collect CB calibration data */
    if ((res = mtlk_scan_sync(&nic->slow_ctx->scan, freq, SPECTRUM_40MHZ)) != MTLK_ERR_OK) {
      res=  MTLK_ERR_NOT_READY;
      goto end_activation;
    }

    /* add the band to CB scanned ones */
    nic->cb_scanned_bands |= (freq == MTLK_HW_BAND_2_4_GHZ ? CB_SCANNED_2_4 : CB_SCANNED_5_2);
  }

  if (!mtlk_core_is_20_40_active(nic)) {
    res = mtlk_progmodel_load(mtlk_vap_get_txmm(nic->vap_handle), nic, freq,
             MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE));
    if (MTLK_ERR_OK != res) {
      ELOG_DD("CID-%04x: Can't configure progmodel for connection (err=%d)", mtlk_vap_get_oid(nic->vap_handle), res);
      goto end_activation;
    }
    res = mtlk_aux_pm_related_params_set_bss_based(
      mtlk_vap_get_txmm(nic->vap_handle),
      &bss_found,
      mtlk_core_get_network_mode_cur(nic),
      MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE),
      &pm_params);

    /* use spectrum mode the same as for progmodel loading */
    pm_params.u8SpectrumMode = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE);
    if (MTLK_ERR_OK != res) {
      ELOG_DD("CID-%04x: Can't set PM related params (err=%d)", mtlk_vap_get_oid(nic->vap_handle), res);
      goto end_activation;
    }
    mtlk_mib_update_pm_related_mibs(nic, &pm_params);
  }

  if (mtlk_send_activate(nic) != MTLK_ERR_OK) {
    res = MTLK_ERR_NOT_READY;
  }

end_activation:
  if (MTLK_ERR_OK != res) {
    /* rollback network mode */
    MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_NET_MODE_CUR, mtlk_core_get_network_mode_cfg(nic));
  }
end:
  return res;
}

uint32 __MTLK_IFUNC
mtlk_core_get_max_stas_supported_by_fw (mtlk_core_t *nic)
{
  uint32 res = DEFAULT_MAX_STAs_SUPPORTED;

  if (mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_FW_CAPS_MAX_STAs, &res, sizeof(res)) != MTLK_ERR_OK) {
    WLOG_V("Cannot get MAX STAs supported by FW. Forcing default");
    res = DEFAULT_MAX_STAs_SUPPORTED;
  }

  ILOG1_D("MAX STAs supported by FW: %d", res);

  return res;
}

static uint32
_mtlk_core_get_max_vaps_supported_by_fw (mtlk_core_t *nic)
{
  uint32 res = DEFAULT_MAX_VAPs_SUPPORTED;

  if (mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_FW_CAPS_MAX_VAPs, &res, sizeof(res)) != MTLK_ERR_OK) {
    WLOG_V("Cannot get MAX VAPs supported by FW. Forcing default");
    res = DEFAULT_MAX_VAPs_SUPPORTED;
  }

  ILOG1_D("MAX VAPs supported by FW: %d", res);

  return res;
}

static uint32
_mtlk_core_get_max_acls_supported_by_fw (mtlk_core_t *nic)
{
  uint32 res = MAX_ADDRESSES_IN_ACL;

  ILOG1_D("MAX ACLs supported by FW: %d", res);

  return res;
}

static BOOL __MTLK_IFUNC
_mtlk_core_sta_inactivity_on (mtlk_handle_t    usr_data,
                              const sta_entry *sta)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, usr_data);

  MTLK_UNREFERENCED_PARAM(sta);

  return mtlk_flctrl_is_data_flowing(nic->hw_tx_flctrl);
}

static void __MTLK_IFUNC
_mtlk_core_on_sta_inactive (mtlk_handle_t    usr_data,
                            const sta_entry *sta)
{
  struct nic      *nic = HANDLE_T_PTR(struct nic, usr_data);

  ILOG1_DY("CID-%04x: Disconnecting %Y due to data timeout",
           mtlk_vap_get_oid(nic->vap_handle),
           mtlk_sta_get_addr(sta));

  if (mtlk_vap_is_ap(nic->vap_handle))
    _mtlk_core_schedule_disconnect_sta(nic, sta, FM_STATUSCODE_INACTIVITY, FRAME_REASON_DISASSOC_DUE_TO_INACTIVITY, NULL, NULL);
  else
    _mtlk_core_schedule_disconnect_me(nic, FM_STATUSCODE_INACTIVITY, FRAME_REASON_DISASSOC_DUE_TO_INACTIVITY);
}

static void __MTLK_IFUNC
_mtlk_core_on_sta_keepalive (mtlk_handle_t  usr_data,
                             sta_entry     *sta)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, usr_data);

  mtlk_send_null_packet(nic, sta);
}

static int __MTLK_IFUNC
_mtlk_core_get_ap_capabilities (mtlk_handle_t hcore,
                                const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_card_capabilities_t card_capabilities;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(mtlk_vap_is_master_ap(nic->vap_handle));
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  MTLK_CFG_SET_ITEM(&card_capabilities, max_stas_supported, mtlk_core_get_max_stas_supported_by_fw(nic));
  MTLK_CFG_SET_ITEM(&card_capabilities, max_vaps_supported, _mtlk_core_get_max_vaps_supported_by_fw(nic));
  MTLK_CFG_SET_ITEM(&card_capabilities, max_acls_supported, _mtlk_core_get_max_acls_supported_by_fw(nic));

  return mtlk_clpb_push(clpb, &card_capabilities, sizeof(card_capabilities));
}
#if 0
static int
_mtlk_core_ap_set_initial_channel (mtlk_core_t *nic, BOOL *rescan_exempted) /* AV: seems to be redundant */
{
  int                     res = MTLK_ERR_UNKNOWN;
  mtlk_get_channel_data_t param;
  uint16                  channel;

  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(rescan_exempted != NULL);
  MTLK_ASSERT(mtlk_vap_is_master(nic->vap_handle) == TRUE);
  MTLK_ASSERT(mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle)) == 0);
  MTLK_ASSERT(mtlk_vap_is_ap(nic->vap_handle) == TRUE);

  *rescan_exempted = _mtlk_core_mbss_is_rescan_exempted(nic);

  if (0 == mtlk_core_get_country_code(nic)) {
    ELOG_D("CID-%04x: Failed to open - Country not specified", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto end;
  }

  param.reg_domain = country_code_to_domain(mtlk_core_get_country_code(nic));
  param.is_ht = mtlk_core_get_is_ht_cfg(nic);
  param.ap = mtlk_vap_is_ap(nic->vap_handle);
  param.bonding = nic->slow_ctx->bonding;
  param.spectrum_mode = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE);
  param.frequency_band = mtlk_core_get_freq_band_cfg(nic);
  param.disable_sm_channels = mtlk_eeprom_get_disable_sm_channels(mtlk_core_get_eeprom(nic));

  channel = (*rescan_exempted == TRUE && MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CHANNEL_CFG) == 0)?
      MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CHANNEL_CUR):
      MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CHANNEL_CFG);

  if (   (0 != channel)
      && (MTLK_ERR_OK != mtlk_check_channel(&param, channel)) ) {
    ELOG_DD("CID-%04x: Channel (%i) is not supported in current configuration.",
        mtlk_vap_get_oid(nic->vap_handle), channel);
    mtlk_core_configuration_dump(nic);
    res = MTLK_ERR_PARAMS;
    goto end;
  }

  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_CHANNEL_CUR, channel);

  if (   (CFG_BASIC_RATE_SET_LEGACY == MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_BASIC_RATE_SET))
      && (MTLK_HW_BAND_2_4_GHZ != mtlk_core_get_freq_band_cfg(nic))) {
    ILOG1_D("CID-%04x: Forcing BasicRateSet to default", mtlk_vap_get_oid(nic->vap_handle));
    MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_BASIC_RATE_SET, CFG_BASIC_RATE_SET_DEFAULT);
  }

  res = MTLK_ERR_OK;

end:
  return res;
}
#endif

int mtlk_core_init_defaults (mtlk_core_t *core)
{
  if (mtlk_vap_is_master_ap(core->vap_handle))
  {
    if (MTLK_HW_BAND_2_4_GHZ == mtlk_core_get_freq_band_cfg(core)) {
      mtlk_20_40_enable_feature(mtlk_core_get_coex_sm(core), TRUE);
    }

    mtlk_set_power_limit(core);
  }

  mtlk_core_set_mc_ps_size(core, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_FW_MC_PS_MAX_FSDUS));

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_core_activate(mtlk_handle_t hcore,
                    const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_core_t *nic_master = mtlk_core_get_master(nic);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  int res;
  sta_db_cfg_t sta_db_cfg;
  BOOL  rescan_exempted = FALSE;
  uint8 net_mode;
  uint32 oid = mtlk_vap_get_oid(nic->vap_handle);

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  ILOG1_D("CID-%04x: open interface", oid);

  if (MTLK_ERR_OK != mtlk_eeprom_is_valid(mtlk_core_get_eeprom(nic))) {
    WLOG_D("CID-%04x: Interface cannot UP after EEPROM failure", oid);
    res = MTLK_ERR_NOT_READY;
    goto end;
  }

  if (mtlk_vap_is_dut(nic->vap_handle)) {
    WLOG_D("CID-%04x: Interface cannot UP in DUT mode", oid);
    res = MTLK_ERR_NOT_SUPPORTED;
    goto end;
  }

  if (  (mtlk_core_get_net_state(nic) != NET_STATE_READY)
      || mtlk_core_scan_is_running(nic)
      || mtlk_core_is_stopping(nic)) {
    ELOG_D("CID-%04x: Failed to open - inappropriate state", oid);
    res = MTLK_ERR_NOT_READY;
    goto end;
  }

  if(
    MTLK_CORE_PDB_GET_INT(nic_master, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_TX) &&
    MTLK_CORE_PDB_GET_INT(nic_master, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_RATE31)
  ) {
    ELOG_D("CID-%04x: both TX and rate 31 SCP enabled", oid);
    res = MTLK_ERR_NOT_SUPPORTED;
    goto end;
  }

  net_mode = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_NET_MODE_CFG);

  {
    /* Check if ratesets configuration is OK */
    uint32 basicRateSet = get_basic_rateset(nic, net_mode, FALSE);
    uint32 supportedRateSet = get_supported_rateset(nic, net_mode, FALSE);
    uint32 extendedRateSet = get_extended_rateset(nic, net_mode, FALSE);
    uint32 operationalRateSet = get_operate_rate_set(net_mode);
    int supported_rates_count = count_rates(supportedRateSet);
    uint32 usedRates = user_rates_to_fw(nic, supportedRateSet | extendedRateSet);
    const char *msg;

    if (supported_rates_count > MAX_SUPPORTED_RATES)
    {
      ELOG_DDD("CID-%04x: %d supported rates (%d maximum)", oid, supported_rates_count, MAX_SUPPORTED_RATES);
      res = MTLK_ERR_NOT_READY;
      goto end;
    }
    else if (0 == basicRateSet)
    {
      msg = "Basic rateset can't be empty";
    }
    else if (0 == supportedRateSet)
    {
      msg = "Supported rateset can't be empty";
    }
    else if (basicRateSet != (basicRateSet & supportedRateSet))
    {
      msg = "Basic rateset is not a subset of Supported rateset";
    }
    else if (usedRates != (usedRates & operationalRateSet))
    {
      msg = "Found rates unspecified for current network mode";
    }
    else
    {
      goto __good;
    }

    ELOG_DS("CID-%04x: %s", oid, msg);
    res = MTLK_ERR_NOT_READY;
    goto end;

__good:
    msg = "";
  }

  {
    /* Check if new mode is compatible with global forced rate */

    uint32 rates_new = get_operate_rate_set(net_mode);
    uint16 forced_rate_idx_legacy = MTLK_CORE_PDB_GET_INT(nic_master, PARAM_DB_CORE_LEGACY_FORCED_RATE_SET);
    uint16 forced_rate_idx_ht = MTLK_CORE_PDB_GET_INT(nic_master, PARAM_DB_CORE_HT_FORCED_RATE_SET);

    if((forced_rate_idx_legacy != NO_RATE) && !((1 << forced_rate_idx_legacy) & rates_new))
    {
      ELOG_D("CID-%04x: network mode incompatible with legacy forced rate", oid);

      res = MTLK_ERR_NOT_SUPPORTED;
      goto end;
    }

    if((forced_rate_idx_ht != NO_RATE) && !((1 << forced_rate_idx_ht) & rates_new))
    {
      ELOG_D("CID-%04x: network mode incompatible with HT forced rate", oid);

      res = MTLK_ERR_NOT_SUPPORTED;
      goto end;
    }
  }

  {
    /* WEP is not allowed for HT modes */

    if (is_ht_net_mode(net_mode) && nic->slow_ctx->wep_enabled) {
      if (mtlk_vap_is_ap(nic->vap_handle)) {
        ELOG_DS("CID-%04x: AP: %s network mode isn't supported for WEP", oid,
                net_mode_to_string(net_mode));
        res = MTLK_ERR_NOT_SUPPORTED;
        goto end;
      }
      else if (!is_mixed_net_mode(net_mode)) {
        ELOG_DS("CID-%04x: STA: %s network mode isn't supported for WEP", oid,
                net_mode_to_string(net_mode));
        res = MTLK_ERR_NOT_SUPPORTED;
        goto end;
      }
    }
  }

  {
    /* Check network mode compatibility between VAPs.
     * This is a firmware limitation */

    uint32 core_cnt;
    uint32 core_idx;
    mtlk_core_t **core_array =
      mtlk_vap_get_core_array(mtlk_vap_get_manager(nic->vap_handle),
                              &core_cnt);
    mtlk_core_t *core_other;
    uint8 net_mode_group = net_mode_to_group(net_mode);
    uint8 net_mode_other;
    uint8 net_mode_group_other;

    for(core_idx = 0; core_idx < core_cnt; core_idx++)
    {
      core_other = core_array[core_idx];

      /* Is it other active VAP? */
      if((core_other != nic) && (!core_other->is_stopped))
      {
        /* Check if new mode is compatible with mode of other VAP */

        net_mode_other = mtlk_core_get_network_mode_cfg(core_other);
        net_mode_group_other = net_mode_to_group(net_mode_other);

        if(net_mode_group != net_mode_group_other)
        {
          /* We are in different network mode groups.
           * Modes are not compatible! */

          ELOG_D("CID-%04x: network mode incompatible with other VAP.", oid);

          res = MTLK_ERR_NOT_SUPPORTED;
          goto end;
        }
      }
    }
  }

  {
    /* Check band compatibility with master VAP.
     * This is important if master VAP is not yet activated
     * bacause we use master VAP for AOCS */

    uint8 band_this = net_mode_to_band(net_mode);
    uint8 net_mode_master =
      MTLK_CORE_PDB_GET_INT(nic_master, PARAM_DB_CORE_NET_MODE_CFG);
    uint8 band_master = net_mode_to_band(net_mode_master);

    if(band_this != band_master)
    {
        ELOG_D("CID-%04x: band incompatible with master VAP.", oid);

        res = MTLK_ERR_NOT_SUPPORTED;
        goto end;
    }
  }

  {
    BOOL coex_enable_adm, coex_enable_oper;
    uint32 user_spectrum_mode;

    coex_enable_adm = MTLK_CORE_PDB_GET_INT(nic_master, PARAM_DB_CORE_COEX_CFG);
    user_spectrum_mode = MTLK_CORE_PDB_GET_INT(nic_master, PARAM_DB_CORE_USER_SPECTRUM_MODE);

    coex_enable_oper = coex_enable_adm && (user_spectrum_mode != SPECTRUM_20MHZ);

    if(coex_enable_adm && !coex_enable_oper)
    {
      WLOG_D("CID-%04x: 20/40 MHz coexistence is disabled in 20 MHz mode", oid);
    }

    mtlk_20_40_enable_feature(mtlk_core_get_coex_sm(nic), coex_enable_oper);
  }

  mtlk_addba_reconfigure(&nic->slow_ctx->addba, &nic->slow_ctx->cfg.addba);

  res = _mtlk_core_process_antennas_configuration(nic);
  if (MTLK_ERR_OK != res) {
    goto end;
  }

  res = mtlk_set_vap_mibs(nic);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Failed to activate the core", oid);
    goto end;
  }

  if (mtlk_vap_is_ap(nic->vap_handle))
  {
    if (mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle)) == 0)
      rescan_exempted = _mtlk_core_mbss_is_rescan_exempted(nic_master);
    res =  mtlk_mbss_init(nic, rescan_exempted);  /* AV: rescan_exempted is used on preactivation only. So
                                                         Should be removed from here directly to preactivation */
    if (MTLK_ERR_OK != res)
    {
      ELOG_D("CID-%04x: Failed to activate the core", mtlk_vap_get_oid(nic->vap_handle));
      goto end;
    }
    if (BR_MODE_WDS == MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE)){
      wds_on_if_up(&nic->slow_ctx->wds_mng);
    }

    if ((res == MTLK_ERR_OK) &&
        mtlk_core_is_20_40_active(nic) &&
        0 == mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle))) {
          eCSM_STATES mode_to_start = CSM_STATE_20;
          if (MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE) == SPECTRUM_40MHZ) {
            mode_to_start = CSM_STATE_20_40;
          }
          res = mtlk_20_40_start(mtlk_core_get_coex_sm(nic), mode_to_start, nic->wss);
          /* Note: even though the coexistence state machine is started, it doesn't mean it
             will eventually try to jump to 40 MHz. The SM remembers the user-chosen spectrum
             mode and will refrain from moving to 40 MHz if the user limited it to 20 MHz */
    }
    if (MTLK_ERR_OK != res)
    {
      ELOG_D("CID-%04x: Failed to activate the core", oid);
      goto end;
    }
  }
  else
  { /* STA mode */
    res = mtlk_set_mib_acl(mtlk_vap_get_txmm(nic->vap_handle), nic->slow_ctx->acl, nic->slow_ctx->acl_mask);

    if (0 < mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle)))
    {
      ELOG_D("CID-%04x: STA has been already activated", oid);
      res = MTLK_ERR_PROHIB;
      goto end;
    }
    if(mtlk_core_is_20_40_active(nic)){
      res = mtlk_20_40_start(mtlk_core_get_coex_sm(nic), CSM_STATE_20, nic->wss);
      if (MTLK_ERR_OK != res)
      {
        ELOG_D("CID-%04x: Failed to activate 20/40 Coexistence", oid);
        goto end;
      }
      mtlk_scan_set_is_background_scan_enabled(&nic->slow_ctx->scan, TRUE);/* 20/40 Coexistence STA required to do BG scans */
    }
    res = mtlk_send_activate(nic);

    /* send TX power limit to FW */
    (void)mtlk_set_power_limit(nic);

    /* don't handle error code */
    res = MTLK_ERR_OK;
  }
  mtlk_vap_manager_notify_vap_activated(mtlk_vap_get_manager(nic->vap_handle));

  /* CoC configuration */
  if (1 >= mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle))) {
    mtlk_coc_reset_antenna_params(nic_master->slow_ctx->coc_mngmt);
    (void)mtlk_coc_set_power_mode(nic_master->slow_ctx->coc_mngmt,
                                  mtlk_coc_get_auto_mode_cfg(nic_master->slow_ctx->coc_mngmt));
  }

#ifdef MTCFG_PMCU_SUPPORT
  /* PCoC configuration */
  if (1 >= mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle))) {
    mtlk_pcoc_t *pcoc_obj = nic_master->slow_ctx->pcoc_mngmt;
    if(pcoc_obj)
    {
      (void)mtlk_pcoc_set_enabled_op(pcoc_obj, TRUE);
    }
  }
#endif /*MTCFG_PMCU_SUPPORT*/

  /* restore STA limits */
  _mtlk_core_set_mbss_vap_limits(nic, MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_STA_LIMIT_MIN),
                                      MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_STA_LIMIT_MAX));

  mtlk_core_abilities_disable_ps_abilities(nic->vap_handle);

  /* interface is up - start timers */
  sta_db_cfg.api.usr_data          = HANDLE_T(nic);
  sta_db_cfg.api.sta_inactivity_on = _mtlk_core_sta_inactivity_on;
  sta_db_cfg.api.on_sta_inactive   = _mtlk_core_on_sta_inactive;
  sta_db_cfg.api.on_sta_keepalive  = _mtlk_core_on_sta_keepalive;

  sta_db_cfg.addba        = &nic->slow_ctx->addba;
  sta_db_cfg.sq           = nic->sq;
  sta_db_cfg.max_nof_stas = mtlk_core_get_max_stas_supported_by_fw(nic);
  sta_db_cfg.parent_wss   = nic->wss;

  mtlk_stadb_configure(&nic->slow_ctx->stadb, &sta_db_cfg);
  mtlk_stadb_start(&nic->slow_ctx->stadb);
  nic->is_stopped = FALSE;
end:
  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_deactivate(mtlk_handle_t hcore,
                      const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  int net_state = mtlk_core_get_net_state(nic);
  int res = MTLK_ERR_OK;
  int deactivate_res  = MTLK_ERR_OK;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(0 != mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle)));
  if ((FALSE == nic->is_iface_stopping) && !mtlk_vap_is_slave_ap(nic->vap_handle)) {
    scan_terminate(&nic->slow_ctx->scan);
  }

  mtlk_osal_lock_acquire(&nic->net_state_lock);
  nic->is_iface_stopping = TRUE;
  mtlk_osal_lock_release(&nic->net_state_lock);

  /* Force disconnect of all WDS peers */
  if (mtlk_vap_is_ap(nic->vap_handle) &&
      BR_MODE_WDS == MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE)){
    wds_on_if_down(&nic->slow_ctx->wds_mng);
  }

  if (!can_disconnect_now(nic)) {
     res = MTLK_ERR_NOT_READY;
     goto FINISH;
  }

  if ((net_state == NET_STATE_CONNECTED) ||
      (net_state == NET_STATE_HALTED)) /* for cleanup after exception */  {

    /* disconnect STA(s) */
    reset_security_stuff(nic);

    if (mtlk_vap_is_ap (nic->vap_handle))
    {
      deactivate_res = _mtlk_mbss_deactivate_vap(nic);
      /* do undo_preactivate, if last active vap is left only */
      if ((1 == mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle))))
      {
         _mtlk_mbss_undo_preactivate(mtlk_core_get_master(nic));
      }
    }
    else
    {
      /* Deactivate STA*/
      _mtlk_core_send_disconnect_req_blocked(nic, NULL, UMI_BSS_UNSPECIFIED, FM_STATUSCODE_USER_REQUEST, FRAME_REASON_UNSPECIFIED);
    }
  }

  mtlk_stadb_stop(&nic->slow_ctx->stadb);
  /* Clearing cache */
  mtlk_cache_clear(&nic->slow_ctx->cache);

  mtlk_osal_lock_acquire(&nic->net_state_lock);
  nic->cb_scanned_bands = 0;
  nic->is_iface_stopping = FALSE;
  nic->is_stopped = TRUE;
  mtlk_osal_lock_release(&nic->net_state_lock);

  mtlk_vap_manager_notify_vap_deactivated(mtlk_vap_get_manager(nic->vap_handle));
  ILOG1_D("CID-%04x: interface is stopped", mtlk_vap_get_oid(nic->vap_handle));

  if ((0 == mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(nic->vap_handle))))
  {
    mtlk_coc_auto_mode_disable(mtlk_core_get_master(nic)->slow_ctx->coc_mngmt);
#ifdef MTCFG_PMCU_SUPPORT
    {
      mtlk_pcoc_t *pcoc_obj = mtlk_core_get_master(nic)->slow_ctx->pcoc_mngmt;
      if(pcoc_obj)
      {
        mtlk_pcoc_set_enabled_op(mtlk_core_get_master(nic)->slow_ctx->pcoc_mngmt, FALSE);
      }
    }
#endif /*MTCFG_PMCU_SUPPORT*/

    mtlk_20_40_stop(mtlk_core_get_coex_sm(nic));
    if (!mtlk_vap_is_ap(nic->vap_handle))
    {
      mtlk_scan_set_load_progmodel_policy(&nic->slow_ctx->scan, TRUE);
    }
  }

  if (mtlk_vap_is_master_ap (nic->vap_handle)) { // re-enable in case we disabled during channel switch
    mtlk_core_abilities_enable_vap_ops(nic->vap_handle);

    mtlk_core_abilities_enable_11b_abilities(nic->vap_handle);
  }

  mtlk_core_abilities_enable_ps_abilities(nic->vap_handle);

FINISH:
  _mtlk_core_mbss_set_last_deactivate_ts(nic);

  /*
    If deactivate_res indicates an error - we must make sure
    that the close function will not reiterate. Therefore, we return
    specific error code in this case.
  */
  if (MTLK_ERR_OK != deactivate_res)
    res = deactivate_res;
  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

int __MTLK_IFUNC
_mtlk_core_set_mac_addr (mtlk_core_t *nic, const char *mac)
{
  int res = MTLK_ERR_UNKNOWN;

  /* Validate MAC address */
  if (!mtlk_osal_is_valid_ether_addr(mac)) {
    ELOG_DY("CID-%04x: The MAC %Y is invalid", mtlk_vap_get_oid(nic->vap_handle), mac);
    res = MTLK_ERR_PARAMS;
    goto FINISH;
  }

  if (!mtlk_vap_is_slave_ap(nic->vap_handle)) {
    MIB_VALUE uValue = {0};
    /* Try to send value to the MAC */
    mtlk_osal_copy_eth_addresses(uValue.au8ListOfu8.au8Elements, mac);

    res = mtlk_set_mib_value_raw(mtlk_vap_get_txmm(nic->vap_handle),
                                 MIB_IEEE_ADDRESS, &uValue);
    if (res != MTLK_ERR_OK) {
      ELOG_D("CID-%04x: Can't set MIB_IEEE_ADDRESS", mtlk_vap_get_oid(nic->vap_handle));
      goto FINISH;
  }
  }

  mtlk_pdb_set_mac(mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_MAC_ADDR, mac);
  ILOG1_DY("CID-%04x: New MAC: %Y", mtlk_vap_get_oid(nic->vap_handle), mac);

  res = MTLK_ERR_OK;

FINISH:
  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_mac_addr_wrapper (mtlk_handle_t hcore,
                         const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  char* mac_addr;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  mac_addr = mtlk_clpb_enum_get_next(clpb, NULL);
  MTLK_ASSERT(NULL != mac_addr);

  return _mtlk_core_set_mac_addr(nic, mac_addr);
}

static int __MTLK_IFUNC
_mtlk_core_get_mac_addr (mtlk_handle_t hcore,
                         const void* data, uint32 data_size)
{
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  uint8 mac_addr[ETH_ALEN];

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  mtlk_pdb_get_mac(mtlk_vap_get_param_db(core->vap_handle), PARAM_DB_CORE_MAC_ADDR, mac_addr);
  return mtlk_clpb_push(clpb, mac_addr, sizeof(mac_addr));
}

static void
_mtlk_core_reset_stats_internal(mtlk_core_t *core)
{
  if (mtlk_vap_is_ap(core->vap_handle)) {
    mtlk_stadb_reset_cnts(&core->slow_ctx->stadb);
  }

  memset(&core->pstats, 0, sizeof(core->pstats));
}

static int __MTLK_IFUNC
_mtlk_core_reset_stats (mtlk_handle_t hcore,
                        const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if (mtlk_core_get_net_state(nic) != NET_STATE_HALTED)
  {
    ELOG_D("CID-%04x: Can not reset stats when core is active", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
  }
  else
  {
    _mtlk_core_reset_stats_internal(nic);
    res = _mtlk_core_mac_reset_stats(nic);
  }

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_l2nat_clear_table(mtlk_handle_t hcore,
                             const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_UNREFERENCED_PARAM(data);

  mtlk_l2nat_clear_table(nic);

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_addba_cfg (mtlk_handle_t hcore,
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  uint32 i;
  mtlk_addba_cfg_entity_t addba_cfg_entity;
  mtlk_addba_cfg_t *addba_cfg = &(HANDLE_T_PTR(mtlk_core_t, hcore)->slow_ctx->cfg.addba);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&addba_cfg_entity, 0, sizeof(addba_cfg_entity));

  for (i=0; i<MTLK_ADDBA_NOF_TIDs; i++) {
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], use_aggr, addba_cfg->tid[i].use_aggr);
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], accept_aggr, addba_cfg->tid[i].accept_aggr);
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], max_nof_packets, addba_cfg->tid[i].max_nof_packets);
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], max_nof_bytes, addba_cfg->tid[i].max_nof_bytes);
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], timeout_interval, addba_cfg->tid[i].timeout_interval);
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], min_packet_size_in_aggr, addba_cfg->tid[i].min_packet_size_in_aggr);
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], addba_timeout, addba_cfg->tid[i].addba_timeout);
    MTLK_CFG_SET_ITEM(&addba_cfg_entity.tid[i], aggr_win_size, addba_cfg->tid[i].aggr_win_size);
  }

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &addba_cfg_entity, sizeof(addba_cfg_entity));
  }

  return res;
}

static int
_mtlk_core_get_addba_state (mtlk_handle_t hcore,
                            const void* data, uint32 data_size)
{
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  MTLK_ASSERT(NULL != clpb);
  MTLK_ASSERT(NULL != core);
  MTLK_ASSERT(NULL != core->slow_ctx);

  return mtlk_stadb_addba_status(&core->slow_ctx->stadb, clpb);
}

static int
_mtlk_core_verify_assign_aggr_max_num_of_packets(uint16 in_max_nof_packets, uint16 *dst_max_nof_packets, uint32 tid)
{
  uint32 res = MTLK_ERR_OK;
  uint16 ac = mtlk_qos_get_ac_by_tid(tid);
  static uint8 max_nof_packets[] =
  {
    NO_MAX_PACKETS_AGG_SUPPORTED_BE,
    NO_MAX_PACKETS_AGG_SUPPORTED_BK,
    NO_MAX_PACKETS_AGG_SUPPORTED_VI,
    NO_MAX_PACKETS_AGG_SUPPORTED_VO
  };

  MTLK_ASSERT(ARRAY_SIZE(max_nof_packets) == NTS_PRIORITIES);
  MTLK_ASSERT(ac < ARRAY_SIZE(max_nof_packets));

  if (in_max_nof_packets > max_nof_packets[ac]) {
    WLOG_DDD ("Invalid max aggr: %d %d %d", tid, in_max_nof_packets, max_nof_packets[ac]);
    res = MTLK_ERR_PARAMS;
  }
  MTLK_ASSERT(dst_max_nof_packets != NULL);

  if (res == MTLK_ERR_OK) {
    *dst_max_nof_packets = in_max_nof_packets;
  }
  return res;
}

static int
_mtlk_core_check_and_set_addba_cfg_field(mtlk_addba_cfg_entity_t *src_addba, mtlk_addba_cfg_t *dst_addba)
{
  uint32 i;
  uint32 res = MTLK_ERR_OK;

  MTLK_ASSERT(NULL != src_addba);
  MTLK_ASSERT(NULL != dst_addba);

  for (i=0; i<MTLK_ADDBA_NOF_TIDs; i++) {
    MTLK_CFG_GET_ITEM(&src_addba->tid[i], use_aggr, dst_addba->tid[i].use_aggr);
    MTLK_CFG_GET_ITEM(&src_addba->tid[i], accept_aggr, dst_addba->tid[i].accept_aggr);
    MTLK_CFG_GET_ITEM_BY_FUNC(&src_addba->tid[i], max_nof_packets, _mtlk_core_verify_assign_aggr_max_num_of_packets, (src_addba->tid[i].max_nof_packets, &dst_addba->tid[i].max_nof_packets, i), res);
    if (res != MTLK_ERR_OK) {
      return res;
    }
    MTLK_CFG_GET_ITEM(&src_addba->tid[i], max_nof_bytes, dst_addba->tid[i].max_nof_bytes);
    MTLK_CFG_GET_ITEM(&src_addba->tid[i], timeout_interval, dst_addba->tid[i].timeout_interval);
    MTLK_CFG_GET_ITEM(&src_addba->tid[i], min_packet_size_in_aggr, dst_addba->tid[i].min_packet_size_in_aggr);
    MTLK_CFG_GET_ITEM(&src_addba->tid[i], addba_timeout, dst_addba->tid[i].addba_timeout);
    MTLK_CFG_GET_ITEM(&src_addba->tid[i], aggr_win_size, dst_addba->tid[i].aggr_win_size);
  }

  return res;
}

/*
  Close TX aggregation

  \param core       - handle to the core [I]
  \param delba_req  - handle to the DELBA configuration [I]

  \return
    none
*/
static void
_mtlk_core_delba_req(mtlk_core_t *core, mtlk_delba_req_t *delba_req)
{
  sta_entry *sta = NULL;

  WLOG_DYD("CID-%04x: DELBA requested for %Y TID %d",
           mtlk_vap_get_oid(core->vap_handle), delba_req->sDA.au8Addr, delba_req->tid);

  sta = mtlk_stadb_find_sta(&core->slow_ctx->stadb, delba_req->sDA.au8Addr);
  if (sta == NULL)
  {
    ELOG_DY("CID-%04x: Unknown peer for DELBA request %Y",
            mtlk_vap_get_oid(core->vap_handle), delba_req->sDA.au8Addr);
    return;
  }

  mtlk_sta_delba_req(sta, delba_req->tid);

  mtlk_sta_decref(sta); /* De-reference of find */
}

/*
  Handler of DELBA request

  \param hcore      - handle to the core [I]
  \param data       - handle to the clipboard [I]
  \param data_size  - size of clipboard handle [I]

  \return
    none
*/
static int
_mtlk_core_set_delba_cfg (mtlk_handle_t hcore,
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_delba_cfg_entity_t *delba_cfg = NULL;
  uint32 delba_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  delba_cfg = mtlk_clpb_enum_get_next(clpb, &delba_cfg_size);

  MTLK_ASSERT(NULL != delba_cfg);
  MTLK_ASSERT(sizeof(*delba_cfg) == delba_cfg_size);

  MTLK_CFG_GET_ITEM_BY_FUNC_VOID(delba_cfg, delba_req,
    _mtlk_core_delba_req, (core, &delba_cfg->delba_req));

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_set_addba_cfg (mtlk_handle_t hcore,
                         const void* data, uint32 data_size)
{
  uint32 res;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_addba_cfg_entity_t *addba_cfg = NULL;
  uint32 addba_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  addba_cfg = mtlk_clpb_enum_get_next(clpb, &addba_cfg_size);

  MTLK_ASSERT(NULL != addba_cfg);
  MTLK_ASSERT(sizeof(*addba_cfg) == addba_cfg_size);

  res = _mtlk_core_check_and_set_addba_cfg_field(addba_cfg, &core->slow_ctx->cfg.addba);

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_wme_cfg(mtlk_clpb_t *clpb, wme_cfg_t *src_wme_cfg)
{
  uint32 res = MTLK_ERR_OK;
  uint32 i;
  mtlk_wme_cfg_entity_t wme_cfg_entity;

  MTLK_ASSERT(NULL != clpb);
  MTLK_ASSERT(NULL != src_wme_cfg);

  memset(&wme_cfg_entity, 0, sizeof(wme_cfg_entity));

  for (i=0; i<NTS_PRIORITIES; i++) {
    MTLK_CFG_SET_ITEM(&wme_cfg_entity.wme_class[i], cwmin, src_wme_cfg->wme_class[i].cwmin);
    MTLK_CFG_SET_ITEM(&wme_cfg_entity.wme_class[i], cwmax, src_wme_cfg->wme_class[i].cwmax);
    MTLK_CFG_SET_ITEM(&wme_cfg_entity.wme_class[i], aifsn, src_wme_cfg->wme_class[i].aifsn);
    MTLK_CFG_SET_ITEM(&wme_cfg_entity.wme_class[i], txop, src_wme_cfg->wme_class[i].txop);
  }

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &wme_cfg_entity, sizeof(wme_cfg_entity));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_wme_bss_cfg (mtlk_handle_t hcore,
                            const void* data, uint32 data_size)
{
  wme_cfg_t *wme_bss_cfg = &(HANDLE_T_PTR(mtlk_core_t, hcore)->slow_ctx->cfg.wme_bss);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  return _mtlk_core_get_wme_cfg(clpb, wme_bss_cfg);
}

static int __MTLK_IFUNC
_mtlk_core_get_wme_ap_cfg (mtlk_handle_t hcore,
                           const void* data, uint32 data_size)
{
  wme_cfg_t *wme_ap_cfg = &(HANDLE_T_PTR(mtlk_core_t, hcore)->slow_ctx->cfg.wme_ap);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  return _mtlk_core_get_wme_cfg(clpb, wme_ap_cfg);
}

static int
_mtlk_core_check_and_set_wme_cfg_field(mtlk_wme_cfg_entity_t *src_wme, wme_cfg_t *dst_wme)
{
  uint32 i;

  MTLK_ASSERT(NULL != src_wme);
  MTLK_ASSERT(NULL != dst_wme);

  for (i=0; i<NTS_PRIORITIES; i++) {
    MTLK_CFG_GET_ITEM(&src_wme->wme_class[i], cwmin, dst_wme->wme_class[i].cwmin);
    MTLK_CFG_GET_ITEM(&src_wme->wme_class[i], cwmax, dst_wme->wme_class[i].cwmax);
    MTLK_CFG_GET_ITEM(&src_wme->wme_class[i], aifsn, dst_wme->wme_class[i].aifsn);
    MTLK_CFG_GET_ITEM(&src_wme->wme_class[i], txop, dst_wme->wme_class[i].txop);
  }

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_core_set_wme_bss_cfg (mtlk_handle_t hcore,
                            const void* data, uint32 data_size)
{
  uint32 res;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_wme_cfg_entity_t *wme_cfg = NULL;
  uint32 wme_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  wme_cfg = mtlk_clpb_enum_get_next(clpb, &wme_cfg_size);

  MTLK_ASSERT(NULL != wme_cfg);
  MTLK_ASSERT(sizeof(*wme_cfg) == wme_cfg_size);

  res = _mtlk_core_check_and_set_wme_cfg_field(wme_cfg, &core->slow_ctx->cfg.wme_bss);

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_set_wme_ap_cfg (mtlk_handle_t hcore,
                           const void* data, uint32 data_size)
{
  uint32 res;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_wme_cfg_entity_t *wme_cfg = NULL;
  uint32 wme_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  wme_cfg = mtlk_clpb_enum_get_next(clpb, &wme_cfg_size);

  MTLK_ASSERT(NULL != wme_cfg);
  MTLK_ASSERT(sizeof(*wme_cfg) == wme_cfg_size);

  res = _mtlk_core_check_and_set_wme_cfg_field(wme_cfg, &core->slow_ctx->cfg.wme_ap);

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_aocs_cfg (mtlk_handle_t hcore,
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_aocs_cfg_t aocs_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_aocs_t *aocs = core->slow_ctx->aocs;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&aocs_cfg, 0, sizeof(aocs_cfg));

  MTLK_CFG_SET_ITEM(&aocs_cfg, weight_ch_load, mtlk_aocs_get_weight(aocs, AOCS_WEIGHT_IDX_CL));
  MTLK_CFG_SET_ITEM(&aocs_cfg, weight_nof_bss, mtlk_aocs_get_weight(aocs, AOCS_WEIGHT_IDX_BSS));
  MTLK_CFG_SET_ITEM(&aocs_cfg, weight_tx_power, mtlk_aocs_get_weight(aocs, AOCS_WEIGHT_IDX_TX));
  MTLK_CFG_SET_ITEM(&aocs_cfg, weight_sm_required, mtlk_aocs_get_weight(aocs, AOCS_WEIGHT_IDX_SM));
  MTLK_CFG_SET_ITEM(&aocs_cfg, scan_aging_ms, mtlk_aocs_get_scan_aging(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, confirm_rank_aging_ms, mtlk_aocs_get_confirm_rank_aging(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, cfm_rank_sw_threshold, mtlk_aocs_get_cfm_rank_sw_threshold(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, alpha_filter_coefficient, mtlk_aocs_get_afilter(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, bonding, mtlk_core_get_user_bonding(core));
  MTLK_CFG_SET_ITEM(&aocs_cfg, use_tx_penalties, mtlk_aocs_get_penalty_enabled(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, udp_aocs_window_time_ms, mtlk_aocs_get_win_time(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, udp_lower_threshold, mtlk_aocs_get_lower_threshold(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, udp_threshold_window, mtlk_aocs_get_threshold_window(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, udp_msdu_debug_enabled, mtlk_aocs_get_msdu_debug_enabled(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, type, mtlk_aocs_get_type(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, udp_msdu_per_window_threshold, mtlk_aocs_get_msdu_win_thr(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, udp_msdu_threshold_aocs, mtlk_aocs_get_msdu_threshold(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, tcp_measurement_window, mtlk_aocs_get_measurement_window(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, tcp_throughput_threshold, mtlk_aocs_get_troughput_threshold(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, dbg_non_occupied_period, mtlk_aocs_get_dbg_non_occupied_period(aocs));
  MTLK_CFG_SET_ITEM(&aocs_cfg, restrict_mode, mtlk_aocs_get_restrict_mode(aocs));

  MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC_VOID(&aocs_cfg, restricted_channels, mtlk_aocs_get_restricted_ch,
                                        (aocs, aocs_cfg.restricted_channels));

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&aocs_cfg, msdu_tx_ac, mtlk_aocs_get_msdu_tx_ac, (aocs, &aocs_cfg.msdu_tx_ac));

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&aocs_cfg, msdu_rx_ac, mtlk_aocs_get_msdu_rx_ac, (aocs, &aocs_cfg.msdu_rx_ac));

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&aocs_cfg, non_wifi_interf, mtlk_aocs_get_non_wifi_interf, (aocs, &aocs_cfg.non_wifi_interf));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &aocs_cfg, sizeof(aocs_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_aocs_cfg (mtlk_handle_t hcore,
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_aocs_t *aocs = core->slow_ctx->aocs;
  mtlk_aocs_cfg_t *aocs_cfg = NULL;
  uint32 aocs_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  aocs_cfg = mtlk_clpb_enum_get_next(clpb, &aocs_cfg_size);

  MTLK_ASSERT(NULL != aocs_cfg);
  MTLK_ASSERT(sizeof(*aocs_cfg) == aocs_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, weight_ch_load, mtlk_aocs_set_weight,
                               (aocs, AOCS_WEIGHT_IDX_CL, aocs_cfg->weight_ch_load), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, weight_tx_power, mtlk_aocs_set_weight,
                               (aocs, AOCS_WEIGHT_IDX_TX, aocs_cfg->weight_tx_power), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, weight_nof_bss, mtlk_aocs_set_weight,
                               (aocs, AOCS_WEIGHT_IDX_BSS, aocs_cfg->weight_nof_bss), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, weight_sm_required, mtlk_aocs_set_weight,
                               (aocs, AOCS_WEIGHT_IDX_SM, aocs_cfg->weight_sm_required), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, scan_aging_ms, mtlk_aocs_set_scan_aging,
                               (aocs, aocs_cfg->scan_aging_ms), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, confirm_rank_aging_ms, mtlk_aocs_set_confirm_rank_aging,
                               (aocs, aocs_cfg->confirm_rank_aging_ms), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, cfm_rank_sw_threshold, mtlk_aocs_set_cfm_rank_sw_threshold,
                               (aocs, aocs_cfg->cfm_rank_sw_threshold), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, alpha_filter_coefficient, mtlk_aocs_set_afilter,
                               (aocs, aocs_cfg->alpha_filter_coefficient), res);

  /* TODO: GS: Move it to Core cfg */
  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, bonding, mtlk_core_set_user_bonding,
                               (core, aocs_cfg->bonding), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, use_tx_penalties, mtlk_aocs_set_penalty_enabled,
                               (aocs, aocs_cfg->use_tx_penalties), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, udp_aocs_window_time_ms, mtlk_aocs_set_win_time,
                               (aocs, aocs_cfg->udp_aocs_window_time_ms), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, udp_lower_threshold, mtlk_aocs_set_lower_threshold,
                               (aocs, aocs_cfg->udp_lower_threshold), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, udp_threshold_window, mtlk_aocs_set_threshold_window,
                               (aocs, aocs_cfg->udp_threshold_window), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, udp_msdu_debug_enabled, mtlk_aocs_set_msdu_debug_enabled,
                               (aocs, aocs_cfg->udp_msdu_debug_enabled), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, type, mtlk_aocs_set_type,
                               (aocs, aocs_cfg->type), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, udp_msdu_per_window_threshold, mtlk_aocs_set_msdu_win_thr,
                               (aocs, aocs_cfg->udp_msdu_per_window_threshold), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, udp_msdu_threshold_aocs, mtlk_aocs_set_msdu_threshold,
                               (aocs, aocs_cfg->udp_msdu_threshold_aocs), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, tcp_measurement_window, mtlk_aocs_set_measurement_window,
                               (aocs, aocs_cfg->tcp_measurement_window), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, tcp_throughput_threshold, mtlk_aocs_set_troughput_threshold,
                               (aocs, aocs_cfg->tcp_throughput_threshold), res);

  MTLK_CFG_GET_ITEM_BY_FUNC_VOID(aocs_cfg, dbg_non_occupied_period, mtlk_aocs_set_dbg_non_occupied_period,
                                (aocs, aocs_cfg->dbg_non_occupied_period));

  MTLK_CFG_GET_ITEM_BY_FUNC_VOID(aocs_cfg, restrict_mode, mtlk_aocs_set_restrict_mode,
                                (aocs, aocs_cfg->restrict_mode));

  MTLK_CFG_GET_ITEM_BY_FUNC_VOID(aocs_cfg, restricted_channels, mtlk_aocs_set_restricted_ch,
                                   (aocs, aocs_cfg->restricted_channels));

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, msdu_tx_ac, mtlk_aocs_set_msdu_tx_ac,
                               (aocs, &aocs_cfg->msdu_tx_ac), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, msdu_rx_ac, mtlk_aocs_set_msdu_rx_ac,
                               (aocs, &aocs_cfg->msdu_rx_ac), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, penalties, mtlk_aocs_set_tx_penalty,
                               (aocs, aocs_cfg->penalties, MTLK_AOCS_PENALTIES_BUFSIZE), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, non_wifi_interf.is_enabled, mtlk_aocs_set_non_wifi_interf_is_enabled,
                               (aocs, aocs_cfg->non_wifi_interf.is_enabled), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, non_wifi_interf.threshold, mtlk_aocs_set_non_wifi_ch_load_trh,
                               (aocs, aocs_cfg->non_wifi_interf.threshold), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(aocs_cfg, non_wifi_interf.threshold_rssi, mtlk_aocs_set_non_wifi_RSSI_trh,
                               (aocs, aocs_cfg->non_wifi_interf.threshold_rssi), res);

MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_dot11h_ap_cfg (mtlk_handle_t hcore,
                              const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_11h_ap_cfg_t dot11h_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  memset(&dot11h_cfg, 0, sizeof(dot11h_cfg));

  MTLK_CFG_SET_ITEM(&dot11h_cfg, debugChannelSwitchCount,
                     mtlk_dot11h_get_dbg_channel_switch_count(mtlk_core_get_dfs(core)));

  MTLK_CFG_SET_ITEM(&dot11h_cfg, debugChannelAvailabilityCheckTime,
                     mtlk_dot11h_get_dbg_channel_availability_check_time(mtlk_core_get_dfs(core)));

  MTLK_CFG_SET_ITEM(&dot11h_cfg, next_channel,
                    mtlk_dot11h_get_debug_next_channel(mtlk_core_get_dfs(core)));

  if (mtlk_vap_is_master_ap(core->vap_handle)) {
    MTLK_CFG_SET_ITEM(&dot11h_cfg, enable_sm_required,
                      !mtlk_aocs_is_smrequired_disabled(core->slow_ctx->aocs));
  }
  else {
    MTLK_CFG_SET_ITEM(&dot11h_cfg, enable_sm_required, FALSE);
  }

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &dot11h_cfg, sizeof(dot11h_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_dot11h_cfg (mtlk_handle_t hcore,
                           const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_11h_cfg_t dot11h_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  memset(&dot11h_cfg, 0, sizeof(dot11h_cfg));

  MTLK_CFG_SET_ITEM(&dot11h_cfg, radar_detect,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_DFS_RADAR_DETECTION));

  MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC_VOID(&dot11h_cfg, status, mtlk_dot11h_status,
                                        (mtlk_core_get_dfs(core), dot11h_cfg.status, MTLK_DOT11H_STATUS_BUFSIZE));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &dot11h_cfg, sizeof(dot11h_cfg));
  }

  return res;
}

static int
_mtlk_core_debug_emulate_radar_detection(mtlk_core_t *core, uint16 channel)
{
  MTLK_ASSERT(mtlk_vap_is_master_ap(core->vap_handle));
  if (mtlk_core_get_net_state(core) != NET_STATE_CONNECTED) {
    WLOG_D("CID-%04x: Can't switch channel - inappropriate state", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_UNKNOWN;
  }

  return mtlk_dot11h_debug_event(mtlk_core_get_dfs(core), MTLK_DFS_EVENT_RADAR_DETECTED, channel);
}

static int
_mtlk_core_debug_switch_channel(mtlk_core_t *core, uint16 channel)
{
  MTLK_ASSERT(mtlk_vap_is_master_ap(core->vap_handle));
  if (mtlk_core_get_net_state(core) != NET_STATE_CONNECTED) {
    WLOG_D("CID-%04x: Can't switch channel - inappropriate state", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_UNKNOWN;
  }

  return mtlk_dot11h_debug_event(mtlk_core_get_dfs(core), MTLK_DFS_EVENT_CHANGE_CHANNEL_NORMAL, channel);
}

static int __MTLK_IFUNC
_mtlk_core_set_dot11h_ap_cfg (mtlk_handle_t hcore,
                              const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_11h_ap_cfg_t *dot11h_cfg = NULL;
  uint32 dot11h_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  dot11h_cfg = mtlk_clpb_enum_get_next(clpb, &dot11h_cfg_size);

  MTLK_ASSERT(NULL != dot11h_cfg);
  MTLK_ASSERT(sizeof(*dot11h_cfg) == dot11h_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()

  MTLK_CFG_CHECK_ITEM_AND_CALL(dot11h_cfg, channel_emu, _mtlk_core_debug_emulate_radar_detection,
                               (core, dot11h_cfg->channel_emu), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(dot11h_cfg, channel_switch, _mtlk_core_debug_switch_channel,
                               (core, dot11h_cfg->channel_switch), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(dot11h_cfg, enable_sm_required, _mtlk_core_set_sm_required,
                               (core, dot11h_cfg->enable_sm_required), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(dot11h_cfg, debugChannelSwitchCount, mtlk_dot11h_set_dbg_channel_switch_count,
                                    (mtlk_core_get_dfs(core), dot11h_cfg->debugChannelSwitchCount));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(dot11h_cfg, debugChannelAvailabilityCheckTime, mtlk_dot11h_set_dbg_channel_availability_check_time,
                                    (mtlk_core_get_dfs(core), dot11h_cfg->debugChannelAvailabilityCheckTime));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(dot11h_cfg, next_channel, mtlk_dot11h_set_debug_next_channel,
                                    (mtlk_core_get_dfs(core), dot11h_cfg->next_channel));

MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_set_dot11h_cfg (mtlk_handle_t hcore,
                           const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_11h_cfg_t *dot11h_cfg = NULL;
  uint32 dot11h_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  dot11h_cfg = mtlk_clpb_enum_get_next(clpb, &dot11h_cfg_size);

  MTLK_ASSERT(NULL != dot11h_cfg);
  MTLK_ASSERT(sizeof(*dot11h_cfg) == dot11h_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()

  #ifdef MBSS_FORCE_NO_CHANNEL_SWITCH
    if (0 != dot11h_cfg->radar_detect) {
      res = MTLK_ERR_NOT_SUPPORTED;
      break;
    }
  #endif

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(dot11h_cfg, radar_detect, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_DFS_RADAR_DETECTION, dot11h_cfg->radar_detect));

MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int
_mtlk_core_get_antennas(mtlk_core_t *core, mtlk_pdb_id_t id_array, char *val)
{
  mtlk_pdb_size_t size = MTLK_NUM_ANTENNAS_BUFSIZE;
  int err = MTLK_ERR_OK;
  uint8 i = 0;
  uint8 val_array[MTLK_NUM_ANTENNAS_BUFSIZE];

  err = MTLK_CORE_PDB_GET_BINARY(core, id_array, val_array, &size);
  if (MTLK_ERR_OK != err)
  {
    MTLK_ASSERT(MTLK_ERR_OK == err); /* Can not retrieve antennas configuration from PDB */
    return err;
  }

  memset(val, '0', MTLK_NUM_ANTENNAS_BUFSIZE);
  val[MTLK_NUM_ANTENNAS_BUFSIZE - 1] = '\0';

  /* convert integer array in to the zero terminated character array */
  for (i = 0; i < (MTLK_NUM_ANTENNAS_BUFSIZE - 1); i++)
  {
    if (0 == val_array[i])
      break;

    val[i] = val_array[i] + '0';
  }

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_core_get_mibs_cfg (mtlk_handle_t hcore,
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_mibs_cfg_t mibs_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&mibs_cfg, 0, sizeof(mibs_cfg));

  MTLK_CFG_SET_ITEM(&mibs_cfg, calibr_algo_mask, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_CALIBRATION_ALGO_MASK));
  MTLK_CFG_SET_ITEM(&mibs_cfg, online_calibr_algo_mask, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_ONLINE_ACM));
  MTLK_CFG_SET_ITEM(&mibs_cfg, short_cyclic_prefix_rx, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_RX));
  MTLK_CFG_SET_ITEM(&mibs_cfg, short_cyclic_prefix_tx, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_TX));
  MTLK_CFG_SET_ITEM(&mibs_cfg, short_cyclic_prefix_rate31, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_RATE31));
  MTLK_CFG_SET_ITEM(&mibs_cfg, short_preamble_option, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SHORT_PREAMBLE));
  MTLK_CFG_SET_ITEM(&mibs_cfg, short_slot_time_option, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SHORT_SLOT_TIME));
  MTLK_CFG_SET_ITEM(&mibs_cfg, tx_power, mtlk_get_total_tx_power_dBm(mtlk_core_get_master(core)));

  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, acl_mode, mtlk_get_mib_acl_mode,
                                     MIB_ACL_MODE, &mibs_cfg.acl_mode, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, current_tx_antenna, mtlk_get_mib_value_uint16,
                                     MIB_CURRENT_TX_ANTENNA, &mibs_cfg.current_tx_antenna, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, beacon_period, mtlk_get_mib_value_uint16,
                                     MIB_BEACON_PERIOD, &mibs_cfg.beacon_period, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, disconnect_on_nacks_weight, mtlk_get_mib_value_uint16,
                                     MIB_DISCONNECT_ON_NACKS_WEIGHT, &mibs_cfg.disconnect_on_nacks_weight, core);

  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, two_antenna_tx_enable, mtlk_get_mib_value_uint8,
                                     MIB_TWO_ANTENNA_TRANSMISSION_ENABLE, &mibs_cfg.two_antenna_tx_enable, core);

  if (mtlk_core_scan_is_running(core)) {
    ILOG1_D("%CID-%04x: Request eliminated due to running scan", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto err_get;
  } else {
    MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, long_retry_limit, mtlk_get_mib_value_uint16,
                                       MIB_LONG_RETRY_LIMIT, &mibs_cfg.long_retry_limit, core);
    MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, short_retry_limit, mtlk_get_mib_value_uint16,
                                       MIB_SHORT_RETRY_LIMIT, &mibs_cfg.short_retry_limit, core);
    MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, tx_msdu_lifetime, mtlk_get_mib_value_uint16,
                                       MIB_TX_MSDU_LIFETIME, &mibs_cfg.tx_msdu_lifetime, core);
    MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, rts_threshold, mtlk_get_mib_value_uint16,
                                       MIB_RTS_THRESHOLD, &mibs_cfg.rts_threshold, core);
  }

  MTLK_CFG_SET_ITEM(&mibs_cfg, sm_enable, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SM_ENABLE));

  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, advanced_coding_supported, mtlk_get_mib_value_uint8,
                                     MIB_ADVANCED_CODING_SUPPORTED, &mibs_cfg.advanced_coding_supported, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, overlapping_protect_enabled, mtlk_get_mib_value_uint8,
                                     MIB_OVERLAPPING_PROTECTION_ENABLE, &mibs_cfg.overlapping_protect_enabled, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, ofdm_protect_method, mtlk_get_mib_value_uint8,
                                     MIB_OFDM_PROTECTION_METHOD, &mibs_cfg.ofdm_protect_method, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, ht_method, mtlk_get_mib_value_uint8,
                                     MIB_HT_PROTECTION_METHOD, &mibs_cfg.ht_method, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, dtim_period, mtlk_get_mib_value_uint8,
                                     MIB_DTIM_PERIOD, &mibs_cfg.dtim_period, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, receive_ampdu_max_len, mtlk_get_mib_value_uint8,
                                     MIB_RECEIVE_AMPDU_MAX_LENGTH, &mibs_cfg.receive_ampdu_max_len, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, cb_databins_per_symbol, mtlk_get_mib_value_uint8,
                                     MIB_CB_DATABINS_PER_SYMBOL, &mibs_cfg.cb_databins_per_symbol, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, use_long_preamble_for_multicast, mtlk_get_mib_value_uint8,
                                     MIB_USE_LONG_PREAMBLE_FOR_MULTICAST, &mibs_cfg.use_long_preamble_for_multicast, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, use_space_time_block_code, mtlk_get_mib_value_uint8,
                                     MIB_USE_SPACE_TIME_BLOCK_CODE, &mibs_cfg.use_space_time_block_code, core);
  MTLK_CFG_SET_MIB_ITEM_BY_FUNC_VOID(&mibs_cfg, disconnect_on_nacks_enable, mtlk_get_mib_value_uint8,
                                     MIB_DISCONNECT_ON_NACKS_ENABLE, &mibs_cfg.disconnect_on_nacks_enable, core);

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&mibs_cfg, tx_antennas, _mtlk_core_get_antennas,
                                 (core, PARAM_DB_CORE_TX_ANTENNAS, mibs_cfg.tx_antennas));
  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&mibs_cfg, rx_antennas, _mtlk_core_get_antennas,
                                 (core, PARAM_DB_CORE_RX_ANTENNAS, mibs_cfg.rx_antennas));

err_get:
  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &mibs_cfg, sizeof(mibs_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_country_cfg (mtlk_handle_t hcore,
                            const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_country_cfg_t country_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&country_cfg, 0, sizeof(country_cfg));

  /* TODO: This check must be dropped in favor of abilities */
  if (mtlk_core_scan_is_running(core)) {
    ILOG1_D("%CID-%04x: Request eliminated due to running scan", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto err_get;
  }

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&country_cfg, country, strncpy,
                                  (country_cfg.country, country_code_to_country(mtlk_core_get_country_code(core)), MTLK_CHNLS_COUNTRY_BUFSIZE));

err_get:
  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &country_cfg, sizeof(country_cfg));
  }

  return res;
}

static int
_mtlk_core_set_bss_rateset(mtlk_core_t *core, uint32 val, int param_db_item)
{
  MTLK_ASSERT(mtlk_vap_is_ap(core->vap_handle));

  if (mtlk_core_get_net_state(core) != NET_STATE_READY) {
    return MTLK_ERR_NOT_READY;
  }

  MTLK_CORE_PDB_SET_INT(core, param_db_item, val);
  return MTLK_ERR_OK;
}

static int
_mtlk_core_set_antennas(mtlk_core_t *core, mtlk_pdb_id_t id_array, char *val)
{
  int err = MTLK_ERR_OK;
  uint8 val_array[MTLK_NUM_ANTENNAS_BUFSIZE];
  uint8 count = 0;

  if (0 != val[MTLK_NUM_ANTENNAS_BUFSIZE - 1])
    return MTLK_ERR_VALUE;

  memset (val_array, 0, sizeof (val_array));

  /* convert zero terminated character array in to the integer array */
  for (count = 0; count < (MTLK_NUM_ANTENNAS_BUFSIZE - 1); count++)
  {
    if (val[count] < '0' || val[count] > '3')
      return MTLK_ERR_VALUE;

    val_array[count] = val[count] - '0';
    if (0 == val_array[count])
      break;
  }

  err = MTLK_CORE_PDB_SET_BINARY(core, id_array, val_array, MTLK_NUM_ANTENNAS_BUFSIZE);
  if (MTLK_ERR_OK != err)
  {
    ILOG2_V("Can not save antennas configuration in to the PDB");
    return err;
  }

  return MTLK_ERR_OK;
}

/* this allowed only for HW antennas config 3X3 */
static inline BOOL
_mtlk_core_tx_limits_antennas_is_3x3(mtlk_core_t *core)
{
    return ((3 == core->slow_ctx->tx_limits.num_tx_antennas) &&
            (3 == core->slow_ctx->tx_limits.num_rx_antennas));
}

static int
_mtlk_core_set_two_antenna_tx_enable(mtlk_core_t *core, mtlk_txmm_t *txmm, uint32 value)
{
  uint32 res = MTLK_ERR_OK;

  if (!_mtlk_core_tx_limits_antennas_is_3x3(core)) {
      ELOG_V("Invalid tx_limits antennas config");
      return MTLK_ERR_PARAMS;
  }

  if (value > 1) {
      ELOG_D("Wrong value (%d). Only 0/1 allowed.", value);
      return MTLK_ERR_PARAMS;
  }

  res = mtlk_set_mib_value_uint8(txmm, MIB_TWO_ANTENNA_TRANSMISSION_ENABLE, value);
  if (res != MTLK_ERR_OK) {
    ELOG_V("Setting MIB_TWO_ANTENNA_TRANSMISSION_ENABLE failed");
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_mibs_cfg (mtlk_handle_t hcore,
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_txmm_t *txmm = mtlk_vap_get_txmm(core->vap_handle);

  mtlk_mibs_cfg_t *mibs_cfg = NULL;
  uint32 mibs_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  mibs_cfg = mtlk_clpb_enum_get_next(clpb, &mibs_cfg_size);

  MTLK_ASSERT(NULL != mibs_cfg);
  MTLK_ASSERT(sizeof(*mibs_cfg) == mibs_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, acl_mode, mtlk_set_mib_acl_mode,
                               (txmm, MIB_ACL_MODE, mibs_cfg->acl_mode), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, acl_mode, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_ACL_MODE, mibs_cfg->acl_mode));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, calibr_algo_mask, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_CALIBRATION_ALGO_MASK, mibs_cfg->calibr_algo_mask));

  /* SCP (Short Cyclic Prefix) a.k.a. Short GI (Guard Interval)
   *
   * SCP TX option will set SCP transmission capability for all TX rates.
   * SCP rate 31 option will set SCP transmission capability for rate 31 only.
   * These two options are mutually exclusive.
   *
   * SCP RX option will merely set short guard interval bit in the beacon.
   * The receiver will receive packets with both short and long
   * guard intervals regardless configuration.
   */

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, short_cyclic_prefix_rx, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_RX, mibs_cfg->short_cyclic_prefix_rx));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, short_cyclic_prefix_tx, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_TX, mibs_cfg->short_cyclic_prefix_tx));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, short_cyclic_prefix_rate31, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_RATE31, mibs_cfg->short_cyclic_prefix_rate31));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, short_preamble_option, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_SHORT_PREAMBLE, mibs_cfg->short_preamble_option));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, short_slot_time_option, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_SHORT_SLOT_TIME, mibs_cfg->short_slot_time_option));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, tx_power, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_TX_POWER, mibs_cfg->tx_power));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, current_tx_antenna, mtlk_set_mib_value_uint16,
                               (txmm, MIB_CURRENT_TX_ANTENNA, mibs_cfg->current_tx_antenna), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, current_tx_antenna, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_CUR_TX_ANTENNAS, mibs_cfg->current_tx_antenna));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, beacon_period, mtlk_set_mib_value_uint16,
                               (txmm, MIB_BEACON_PERIOD, mibs_cfg->beacon_period), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, beacon_period, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_BEACON_PERIOD, mibs_cfg->beacon_period));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, disconnect_on_nacks_weight, mtlk_set_mib_value_uint16,
                               (txmm, MIB_DISCONNECT_ON_NACKS_WEIGHT, mibs_cfg->disconnect_on_nacks_weight), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, disconnect_on_nacks_weight, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_DISC_ON_NACKS_WGHT, mibs_cfg->disconnect_on_nacks_weight));

  if (mtlk_core_scan_is_running(core)) {
    ILOG1_D("CID-%04x: Request eliminated due to running scan", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    break;
  } else {
    MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, short_retry_limit, mtlk_set_mib_value_uint16,
                                 (txmm, MIB_SHORT_RETRY_LIMIT, mibs_cfg->short_retry_limit), res);
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, short_retry_limit, MTLK_CORE_PDB_SET_INT,
                                      (core, PARAM_DB_CORE_SHORT_RETRY_LIMIT, mibs_cfg->short_retry_limit));

    MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, long_retry_limit, mtlk_set_mib_value_uint16,
                                 (txmm, MIB_LONG_RETRY_LIMIT, mibs_cfg->long_retry_limit), res);
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, long_retry_limit, MTLK_CORE_PDB_SET_INT,
                                      (core, PARAM_DB_CORE_LONG_RETRY_LIMIT, mibs_cfg->long_retry_limit));

    MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, tx_msdu_lifetime, mtlk_set_mib_value_uint32,
                                 (txmm, MIB_TX_MSDU_LIFETIME, mibs_cfg->tx_msdu_lifetime), res);
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, tx_msdu_lifetime, MTLK_CORE_PDB_SET_INT,
                                      (core, PARAM_DB_CORE_MSDU_LIFETIME, mibs_cfg->tx_msdu_lifetime));

    MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, rts_threshold, mtlk_set_mib_value_uint16,
                                 (txmm, MIB_RTS_THRESHOLD, mibs_cfg->rts_threshold), res);
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, rts_threshold, MTLK_CORE_PDB_SET_INT,
                                      (core, PARAM_DB_CORE_RTS_THRESH, mibs_cfg->rts_threshold));
  }

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, sm_enable, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_SM_ENABLE, mibs_cfg->sm_enable));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, advanced_coding_supported, mtlk_set_mib_value_uint8,
                               (txmm, MIB_ADVANCED_CODING_SUPPORTED, mibs_cfg->advanced_coding_supported), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, advanced_coding_supported, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_ADVANCED_CODING, mibs_cfg->advanced_coding_supported));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, overlapping_protect_enabled, mtlk_set_mib_value_uint8,
                               (txmm, MIB_OVERLAPPING_PROTECTION_ENABLE, mibs_cfg->overlapping_protect_enabled), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, overlapping_protect_enabled, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_OVERLAPPING_PROT, mibs_cfg->overlapping_protect_enabled));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, ofdm_protect_method, mtlk_set_mib_value_uint8,
                               (txmm, MIB_OFDM_PROTECTION_METHOD, mibs_cfg->ofdm_protect_method), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, ofdm_protect_method, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_OFDM_PROTECTION, mibs_cfg->ofdm_protect_method));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, ht_method, mtlk_set_mib_value_uint8,
                               (txmm, MIB_HT_PROTECTION_METHOD, mibs_cfg->ht_method), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, ht_method, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_HT_PROTECTION, mibs_cfg->ht_method));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, dtim_period, mtlk_set_mib_value_uint16,
                               (txmm, MIB_DTIM_PERIOD, mibs_cfg->dtim_period), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, dtim_period, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_DTIM_PERIOD, mibs_cfg->dtim_period));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, receive_ampdu_max_len, mtlk_set_mib_value_uint8,
                               (txmm, MIB_RECEIVE_AMPDU_MAX_LENGTH, mibs_cfg->receive_ampdu_max_len), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, receive_ampdu_max_len, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_AMPDU_MAXLEN, mibs_cfg->receive_ampdu_max_len));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, cb_databins_per_symbol, mtlk_set_mib_value_uint8,
                               (txmm, MIB_CB_DATABINS_PER_SYMBOL, mibs_cfg->cb_databins_per_symbol), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, cb_databins_per_symbol, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_CB_BINS_PER_SYMB, mibs_cfg->cb_databins_per_symbol));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, use_long_preamble_for_multicast, mtlk_set_mib_value_uint8,
                               (txmm, MIB_USE_LONG_PREAMBLE_FOR_MULTICAST, mibs_cfg->use_long_preamble_for_multicast), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, use_long_preamble_for_multicast, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_LONG_PREAMB_MC, mibs_cfg->use_long_preamble_for_multicast));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, use_space_time_block_code, mtlk_set_mib_value_uint8,
                               (txmm, MIB_USE_SPACE_TIME_BLOCK_CODE, mibs_cfg->use_space_time_block_code), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, use_space_time_block_code, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_STBC, mibs_cfg->use_space_time_block_code));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, two_antenna_tx_enable, _mtlk_core_set_two_antenna_tx_enable,
                               (core, txmm, mibs_cfg->two_antenna_tx_enable), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, two_antenna_tx_enable, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_TWO_ANT_TX_ENABLE, mibs_cfg->two_antenna_tx_enable));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, online_calibr_algo_mask, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_ONLINE_ACM, mibs_cfg->online_calibr_algo_mask));
  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, online_calibr_algo_mask, mtlk_set_mib_value_uint16,
                               (txmm, MIB_ONLINE_CALIBRATION_ALGO_MASK, mibs_cfg->online_calibr_algo_mask), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, disconnect_on_nacks_enable, mtlk_set_mib_value_uint8,
                               (txmm, MIB_DISCONNECT_ON_NACKS_ENABLE, mibs_cfg->disconnect_on_nacks_enable), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mibs_cfg, disconnect_on_nacks_enable, MTLK_CORE_PDB_SET_INT,
                                    (core, PARAM_DB_CORE_DISC_ON_NACKS, mibs_cfg->disconnect_on_nacks_enable));

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, tx_antennas, _mtlk_core_set_antennas,
                               (core, PARAM_DB_CORE_TX_ANTENNAS, mibs_cfg->tx_antennas), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(mibs_cfg, rx_antennas, _mtlk_core_set_antennas,
                               (core, PARAM_DB_CORE_RX_ANTENNAS, mibs_cfg->rx_antennas), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_set_country_cfg (mtlk_handle_t hcore,
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_country_cfg_t *country_cfg = NULL;
  uint32 country_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  country_cfg = mtlk_clpb_enum_get_next(clpb, &country_cfg_size);

  MTLK_ASSERT(NULL != country_cfg);
  MTLK_ASSERT(sizeof(*country_cfg) == country_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()

  if (mtlk_core_scan_is_running(core)) {
    ILOG1_D("CID-%04x: Request eliminated due to running scan", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    break;
  }

  MTLK_CFG_CHECK_ITEM_AND_CALL(country_cfg, country, _mtlk_core_set_country_from_ui,
                               (core, country_cfg->country), res);

MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_l2nat_cfg (mtlk_handle_t hcore,
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_l2nat_cfg_t l2nat_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&l2nat_cfg, 0, sizeof(l2nat_cfg));

  MTLK_CFG_SET_ITEM(&l2nat_cfg, aging_timeout, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_L2NAT_AGING_TIMEOUT));
  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&l2nat_cfg, address, mtlk_l2nat_get_def_host, (core, &l2nat_cfg.address));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &l2nat_cfg, sizeof(l2nat_cfg));
  }

  return res;
}

static int
_mtlk_core_set_l2nat_aging_timeout(mtlk_core_t *core, int32 timeval)
{
  if (timeval < 0) {
    ELOG_DD("CID-%04x: Wrong value for aging timeout: %d", mtlk_vap_get_oid(core->vap_handle), timeval);
    return MTLK_ERR_PARAMS;
  }
  if (timeval > 0 && timeval < 60) {
    ELOG_D("CID-%04x: The lowest timeout allowed is 60 seconds.", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_PARAMS;
  }

  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_L2NAT_AGING_TIMEOUT, timeval);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_core_set_l2nat_cfg (mtlk_handle_t hcore,
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_l2nat_cfg_t *l2nat_cfg = NULL;
  uint32 l2nat_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  l2nat_cfg = mtlk_clpb_enum_get_next(clpb, &l2nat_cfg_size);

  MTLK_ASSERT(NULL != l2nat_cfg);
  MTLK_ASSERT(sizeof(*l2nat_cfg) == l2nat_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
   MTLK_CFG_CHECK_ITEM_AND_CALL(l2nat_cfg, aging_timeout, _mtlk_core_set_l2nat_aging_timeout,
                               (core, l2nat_cfg->aging_timeout), res);
   MTLK_CFG_CHECK_ITEM_AND_CALL(l2nat_cfg, address, mtlk_l2nat_user_set_def_host,
                               (core, &l2nat_cfg->address), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_set_wds_cfg (mtlk_handle_t hcore,
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_wds_cfg_t *wds_cfg;
  uint32  wds_cfg_size;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  wds_cfg = mtlk_clpb_enum_get_next(clpb, &wds_cfg_size);

  MTLK_ASSERT(NULL != wds_cfg);
  MTLK_ASSERT(sizeof(*wds_cfg) == wds_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_CHECK_ITEM_AND_CALL(wds_cfg, peer_ap_addr_add, wds_usr_add_peer_ap,
                                 (&core->slow_ctx->wds_mng, &wds_cfg->peer_ap_addr_add), res);

    MTLK_CFG_CHECK_ITEM_AND_CALL(wds_cfg, peer_ap_addr_del, wds_usr_del_peer_ap,
                                 (&core->slow_ctx->wds_mng, &wds_cfg->peer_ap_addr_del), res);

    MTLK_CFG_GET_ITEM(wds_cfg, peer_ap_key_idx, core->slow_ctx->peerAPs_key_idx);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));

}

static int __MTLK_IFUNC
_mtlk_core_set_wds_dbg (mtlk_handle_t hcore,
                        const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_wds_dbg_t *wds_dbg;
  uint32 wds_dbg_size;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  wds_dbg = mtlk_clpb_enum_get_next(clpb, &wds_dbg_size);

  MTLK_ASSERT(NULL != wds_dbg);
  MTLK_ASSERT(sizeof(*wds_dbg) == wds_dbg_size);

  MTLK_UNREFERENCED_PARAM(wds_dbg->dummy);
  //MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  //  MTLK_CFG_CHECK_ITEM_AND_CALL(wds_dbg, dummy, xxx, (), res);
  //MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  wds_get_peers_status(&core->slow_ctx->wds_mng);

  return mtlk_clpb_push(clpb, &res, sizeof(res));

}

static int __MTLK_IFUNC
_mtlk_core_get_wds_cfg (mtlk_handle_t hcore,
                        const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_wds_cfg_t wds_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&wds_cfg, 0, sizeof(wds_cfg));

  MTLK_CFG_SET_ITEM(&wds_cfg, peer_ap_key_idx, core->slow_ctx->peerAPs_key_idx);

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &wds_cfg, sizeof(wds_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_wds_peer_ap (mtlk_handle_t hcore,
                            const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_wds_cfg_t wds_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&wds_cfg, 0, sizeof(wds_cfg));

  MTLK_CFG_SET_ITEM_BY_FUNC(&wds_cfg, peer_ap_vect,
    mtlk_wds_get_peer_vect, (&core->slow_ctx->wds_mng, &wds_cfg.peer_ap_vect), res);

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &wds_cfg, sizeof(wds_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_dot11d_cfg (mtlk_handle_t hcore,
                           const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_dot11d_cfg_t dot11d_cfg;

  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&dot11d_cfg, 0, sizeof(dot11d_cfg));

  MTLK_CFG_SET_ITEM(&dot11d_cfg, is_dot11d, mtlk_core_get_dot11d(core));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &dot11d_cfg, sizeof(dot11d_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_dot11d_cfg (mtlk_handle_t hcore,
                           const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_dot11d_cfg_t *dot11d_cfg = NULL;
  uint32 dot11d_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  dot11d_cfg = mtlk_clpb_enum_get_next(clpb, &dot11d_cfg_size);

  MTLK_ASSERT(NULL != dot11d_cfg);
  MTLK_ASSERT(sizeof(*dot11d_cfg) == dot11d_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
   MTLK_CFG_CHECK_ITEM_AND_CALL(dot11d_cfg, is_dot11d, _mtlk_core_set_is_dot11d,
                               (core, dot11d_cfg->is_dot11d), res);
   MTLK_CFG_CHECK_ITEM_AND_CALL(dot11d_cfg, should_reset_tx_limits, mtlk_reset_tx_limit_tables,
                               (&core->slow_ctx->tx_limits), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_mac_wdog_cfg (mtlk_handle_t hcore,
                             const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_mac_wdog_cfg_t mac_wdog_cfg;

  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&mac_wdog_cfg, 0, sizeof(mac_wdog_cfg));

  MTLK_CFG_SET_ITEM(&mac_wdog_cfg, mac_watchdog_timeout_ms,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_TIMEOUT_MS));
  MTLK_CFG_SET_ITEM(&mac_wdog_cfg, mac_watchdog_period_ms,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_PERIOD_MS));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &mac_wdog_cfg, sizeof(mac_wdog_cfg));
  }

  return res;
}

static int
_mtlk_core_set_mac_wdog_timeout(mtlk_core_t *core, uint16 value)
{
  if (value < 1000) {
    return MTLK_ERR_PARAMS;
  }
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_TIMEOUT_MS, value);
  return MTLK_ERR_OK;
}

static int
_mtlk_core_set_mac_wdog_period(mtlk_core_t *core, uint32 value)
{
  if (0 == value) {
    return MTLK_ERR_PARAMS;
  }
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_PERIOD_MS, value);
  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_core_set_mac_wdog_cfg (mtlk_handle_t hcore,
                             const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_mac_wdog_cfg_t *mac_wdog_cfg = NULL;
  uint32 mac_wdog_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  mac_wdog_cfg = mtlk_clpb_enum_get_next(clpb, &mac_wdog_cfg_size);

  MTLK_ASSERT(NULL != mac_wdog_cfg);
  MTLK_ASSERT(sizeof(*mac_wdog_cfg) == mac_wdog_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
   MTLK_CFG_CHECK_ITEM_AND_CALL(mac_wdog_cfg, mac_watchdog_timeout_ms, _mtlk_core_set_mac_wdog_timeout,
                               (core, mac_wdog_cfg->mac_watchdog_timeout_ms), res);
   MTLK_CFG_CHECK_ITEM_AND_CALL(mac_wdog_cfg, mac_watchdog_period_ms, _mtlk_core_set_mac_wdog_period,
                               (core, mac_wdog_cfg->mac_watchdog_period_ms), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_stadb_cfg (mtlk_handle_t hcore,
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_stadb_cfg_t stadb_cfg;

  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&stadb_cfg, 0, sizeof(stadb_cfg));

  MTLK_CFG_SET_ITEM(&stadb_cfg, sta_keepalive_timeout, core->slow_ctx->stadb.sta_keepalive_timeout);
  MTLK_CFG_SET_ITEM(&stadb_cfg, keepalive_interval, core->slow_ctx->stadb.keepalive_interval);
  MTLK_CFG_SET_ITEM(&stadb_cfg, aggr_open_threshold, core->slow_ctx->stadb.aggr_open_threshold);

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &stadb_cfg, sizeof(stadb_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_stadb_cfg (mtlk_handle_t hcore,
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_stadb_cfg_t *stadb_cfg = NULL;
  uint32 stadb_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  stadb_cfg = mtlk_clpb_enum_get_next(clpb, &stadb_cfg_size);

  MTLK_ASSERT(NULL != stadb_cfg);
  MTLK_ASSERT(sizeof(*stadb_cfg) == stadb_cfg_size);

  MTLK_CFG_GET_ITEM(stadb_cfg, sta_keepalive_timeout,
                    core->slow_ctx->stadb.sta_keepalive_timeout);
  MTLK_CFG_GET_ITEM(stadb_cfg, keepalive_interval,
                    core->slow_ctx->stadb.keepalive_interval);
  MTLK_CFG_GET_ITEM(stadb_cfg, aggr_open_threshold,
                    core->slow_ctx->stadb.aggr_open_threshold);

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_sq_cfg (mtlk_handle_t hcore,
                       const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_sq_cfg_t sq_cfg;

  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&sq_cfg, 0, sizeof(sq_cfg));

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&sq_cfg, sq_limit, sq_get_limits,
                                (core, sq_cfg.sq_limit, NTS_PRIORITIES));
  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&sq_cfg, peer_queue_limit, sq_get_peer_limits,
                                (core, sq_cfg.peer_queue_limit, NTS_PRIORITIES));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &sq_cfg, sizeof(sq_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_sq_cfg (mtlk_handle_t hcore,
                       const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_sq_cfg_t *sq_cfg = NULL;
  uint32 sq_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  sq_cfg = mtlk_clpb_enum_get_next(clpb, &sq_cfg_size);

  MTLK_ASSERT(NULL != sq_cfg);
  MTLK_ASSERT(sizeof(*sq_cfg) == sq_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(sq_cfg, sq_limit, sq_set_limits,
                              (core, sq_cfg->sq_limit, NTS_PRIORITIES), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL(sq_cfg, peer_queue_limit, sq_set_peer_limits,
                              (core, sq_cfg->peer_queue_limit, NTS_PRIORITIES), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static uint8
_mtlk_core_get_user_spectrum_mode(mtlk_core_t *core)
{
  if (mtlk_vap_is_ap(core->vap_handle)) {
    return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_USER_SPECTRUM_MODE);
  }

  return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_STA_FORCE_SPECTRUM_MODE);
}

static int
_mtlk_core_get_force_rate (mtlk_core_t          *core,
                           mtlk_core_rate_cfg_t *rate_cfg,
                           uint16                object_id)
{
  uint16 forced_rate_idx = 0; /* prevent compiler from complaining */
  uint32 short_cyclic_prefix;

  switch (object_id) {
  case MIB_HT_FORCE_RATE:
    forced_rate_idx = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_HT_FORCED_RATE_SET);
    break;
  case MIB_LEGACY_FORCE_RATE:
    forced_rate_idx = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_LEGACY_FORCED_RATE_SET);
    break;
  default:
    MTLK_ASSERT(0);
    break;
  }

  short_cyclic_prefix = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_TX);
  if(forced_rate_idx == BITRATE_LAST)
  {
    short_cyclic_prefix |= MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_RATE31);
  }

  return mtlk_bitrate_idx_to_rates(forced_rate_idx,
                                   MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE),
                                   short_cyclic_prefix,
                                   &rate_cfg->int_rate,
                                   &rate_cfg->frac_rate);
}

static int
_mtlk_core_get_n_rate_bo (mtlk_core_t           *core,
                          mtlk_core_n_rate_bo_t *n_rate_bo)
{
  n_rate_bo->qam16     = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_N_RATE_BO_QAM16);
  n_rate_bo->qam64_2_3 = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_N_RATE_BO_QAM64_2_3);
  n_rate_bo->qam64_3_4 = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_N_RATE_BO_QAM64_3_4);
  n_rate_bo->qam64_5_6 = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_N_RATE_BO_QAM64_5_6);

  return MTLK_ERR_OK;
}

static void
_mtlk_core_get_countries_supported(mtlk_core_t *core, mtlk_gen_core_country_name_t *countries)
{
  MTLK_ASSERT(NULL != core);
  MTLK_ASSERT(NULL != countries);

  memset(countries, 0, sizeof(mtlk_gen_core_country_name_t)*MAX_COUNTRIES);

  get_all_countries_for_domain(country_code_to_domain(mtlk_core_get_country_code(core)), countries, MAX_COUNTRIES);
}

static int
_mtlk_core_get_channel (mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  /* Retrieve PARAM_DB_CORE_CHANNEL_CUR channel in case if there are active VAPs
   * Master VAP can be in NET_STATE_READY, but Slave VAP can be in NET_STATE_CONNECTED,
   * therefore PARAM_DB_CORE_CHANNEL_CUR channel, belonged to Master VAP has correct value */
  if ((NET_STATE_CONNECTED == mtlk_core_get_net_state(core)) || (0 != mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(core->vap_handle))))
    return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_CHANNEL_CUR);
  else
    return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_CHANNEL_CFG);
}

static int
_mtlk_core_set_up_rescan_exemption_time_sec (mtlk_core_t *core, uint32 value)
{
  if (value == MAX_UINT32) {
    ;
  }
  else if (value < MAX_UINT32/ MSEC_PER_SEC) {
    value *= MSEC_PER_SEC;
  }
  else {
    /* In this case, the TS (which is measured in ms) can wrap around. */
    return MTLK_ERR_PARAMS;
  }

  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_UP_RESCAN_EXEMPTION_TIME, value);

  return MTLK_ERR_OK;
}

static uint32
_mtlk_core_get_up_rescan_exemption_time_sec (mtlk_core_t *core)
{
  uint32 res = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_UP_RESCAN_EXEMPTION_TIME);

  if (res != MAX_UINT32) {
    res /= MSEC_PER_SEC;
  }

  return res;
}

static void
_mtlk_core_get_agg_rate_limit (mtlk_core_t *core, mtlk_agg_rate_limit_t *limit)
{
    limit->mode    = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_AGG_RATE_LIMIT_MODE);
    limit->maxRate = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_AGG_RATE_LIMIT_MAXRATE);
}

static void
_mtlk_core_get_rx_duty_cycle (mtlk_core_t *core, mtlk_rx_duty_cycle_t *rx_duty_cycle)
{
    rx_duty_cycle->onTime  = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_RX_DUTY_CYCLE_ONTIME);
    rx_duty_cycle->offTime = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_RX_DUTY_CYCLE_OFFTIME);
}

static void
_mtlk_core_get_lna_gains (mtlk_core_t *core, mtlk_lna_gain_t *lna_gains)
{
uint32 lna_gains_pdb;
mtlk_eeprom_data_t *ee_data;

  lna_gains_pdb = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_LNA_GAIN);

  /* if it was set by iwpriv read from PDB also from eeprom */
  if(MTLK_BFIELD_GET(lna_gains_pdb, BF_LNA_GAIN_IS_SET)){

    lna_gains->lna_gain_bypass = MTLK_BFIELD_GET(lna_gains_pdb, BF_LNA_GAIN_BYPASS);
    lna_gains->lna_gain_high   = MTLK_BFIELD_GET(lna_gains_pdb, BF_LNA_GAIN_HIGH);
  }
  else{

    ee_data = mtlk_core_get_eeprom(core);
    lna_gains->lna_gain_bypass = mtlk_eeprom_get_lna_gain_bypass(ee_data);
    lna_gains->lna_gain_high = mtlk_eeprom_get_lna_gain_high(ee_data);
  }
}

static void
_mtlk_master_core_get_core_cfg (mtlk_core_t *core,
                         mtlk_gen_core_cfg_t* pCore_Cfg)
{
  MTLK_CFG_SET_ITEM(pCore_Cfg, dbg_sw_wd_enable,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_MAC_SOFT_RESET_ENABLE));
  MTLK_CFG_SET_ITEM(pCore_Cfg, up_rescan_exemption_time,
                    _mtlk_core_get_up_rescan_exemption_time_sec(core));
  MTLK_CFG_SET_ITEM(pCore_Cfg, spectrum_mode,
                    _mtlk_core_get_user_spectrum_mode(core));
  MTLK_CFG_SET_ITEM(pCore_Cfg, channel,
                    _mtlk_core_get_channel(core));
  MTLK_CFG_SET_ITEM(pCore_Cfg, bonding, mtlk_core_get_user_bonding(core));
  MTLK_CFG_SET_ITEM(pCore_Cfg, frequency_band_cur, mtlk_core_get_freq_band_cur(core));
}

static void
_mtlk_slave_core_get_core_cfg (mtlk_core_t *core,
                         mtlk_gen_core_cfg_t* pCore_Cfg)
{
  _mtlk_master_core_get_core_cfg(mtlk_core_get_master(core), pCore_Cfg);
}

static int __MTLK_IFUNC
_mtlk_core_get_core_cfg (mtlk_handle_t hcore,
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_gen_core_cfg_t* pcore_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  uint32 num_macs = 0;
  uint32 i;
  uint32 str_size;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  pcore_cfg = mtlk_osal_mem_alloc(sizeof(mtlk_gen_core_cfg_t), MTLK_MEM_TAG_CORE_CFG);
  if(NULL == pcore_cfg) {
    ELOG_D("CID-%04x: Cannot allocate memory for core configuration data", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NO_MEM;
    goto err_return;
  }
  memset(pcore_cfg, 0, sizeof(*pcore_cfg));

  MTLK_CFG_SET_ITEM(pcore_cfg, bridge_mode,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_BRIDGE_MODE));

  MTLK_CFG_SET_ITEM(pcore_cfg, ap_forwarding,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_AP_FORWARDING));

  MTLK_CFG_SET_ITEM(pcore_cfg, reliable_multicast,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_RELIABLE_MCAST));

  if (NET_STATE_CONNECTED == mtlk_core_get_net_state(core)) {
    MTLK_CFG_SET_ITEM(pcore_cfg, net_mode,
                      mtlk_core_get_network_mode_cur(core));
    MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC_VOID(pcore_cfg, bssid, mtlk_pdb_get_mac,
        (mtlk_vap_get_param_db(core->vap_handle), PARAM_DB_CORE_BSSID, pcore_cfg->bssid) );
  } else {
    MTLK_CFG_SET_ITEM(pcore_cfg, net_mode,
                      mtlk_core_get_network_mode_cfg(core));
    MTLK_CFG_SET_ARRAY_ITEM_BY_FUNC_VOID(pcore_cfg, bssid, memset,
                      (pcore_cfg->bssid, 0, sizeof(pcore_cfg->bssid)) );
  }

  MTLK_CFG_SET_ITEM(pcore_cfg, basic_rates,
                    get_basic_rateset(core, mtlk_core_get_network_mode_cur(core), FALSE));

  MTLK_CFG_SET_ITEM(pcore_cfg, supported_rates,
                    get_supported_rateset(core, mtlk_core_get_network_mode_cur(core), FALSE));

  MTLK_CFG_SET_ITEM(pcore_cfg, extended_rates,
                    get_extended_rateset(core, mtlk_core_get_network_mode_cur(core), FALSE));

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(pcore_cfg, countries_supported, _mtlk_core_get_countries_supported,
                                (core, pcore_cfg->countries_supported));

  for (i = 0, num_macs = 0; i < MAX_ADDRESSES_IN_ACL; i++) {
    if (mtlk_osal_is_zero_address(core->slow_ctx->acl[i].au8Addr)) {
      continue;
    }
    MTLK_CFG_SET_ITEM_BY_FUNC_VOID(pcore_cfg, macs_to_set, memcpy,
                                   (pcore_cfg->macs_to_set[num_macs].au8Addr, core->slow_ctx->acl[i].au8Addr, sizeof(IEEE_ADDR)))
    MTLK_CFG_SET_ITEM_BY_FUNC_VOID(pcore_cfg, mac_mask, memcpy,
                                   (pcore_cfg->mac_mask[num_macs++].au8Addr, core->slow_ctx->acl_mask[i].au8Addr, sizeof(IEEE_ADDR)))
  }

  MTLK_CFG_SET_ITEM(pcore_cfg, num_macs_to_set, num_macs);

  str_size = sizeof(pcore_cfg->nickname);
  MTLK_CFG_SET_ITEM_BY_FUNC(pcore_cfg, nickname, mtlk_pdb_get_string,
                            (mtlk_vap_get_param_db(core->vap_handle),
                            PARAM_DB_CORE_NICK_NAME,
                            pcore_cfg->nickname, &str_size), res);

  MTLK_CFG_SET_ITEM_BY_FUNC(pcore_cfg, essid, mtlk_pdb_get_string,
                            (mtlk_vap_get_param_db(core->vap_handle),
                            PARAM_DB_CORE_ESSID,
                            pcore_cfg->essid, &str_size), res );

  MTLK_CFG_SET_ITEM(pcore_cfg, is_hidden_ssid, core->slow_ctx->cfg.is_hidden_ssid);

  if (!mtlk_vap_is_slave_ap(core->vap_handle)) {
    _mtlk_master_core_get_core_cfg(core, pcore_cfg);
  } else {
    _mtlk_slave_core_get_core_cfg(core, pcore_cfg);
  }

err_return:
  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, pcore_cfg, sizeof(*pcore_cfg));
  }
  if(NULL != pcore_cfg) {
    mtlk_osal_mem_free(pcore_cfg);
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_master_specific_cfg (mtlk_handle_t hcore,
                                    const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_master_core_cfg_t master_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&master_cfg, 0, sizeof(master_cfg));

  MTLK_CFG_SET_ITEM_BY_FUNC(&master_cfg, legacy_force_rate, _mtlk_core_get_force_rate,
                             (core, &master_cfg.legacy_force_rate, MIB_LEGACY_FORCE_RATE), res);
  if (res != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Cannot get Legacy force rate (err=%d)", mtlk_vap_get_oid(core->vap_handle), res);
    goto err_return;
  }

  MTLK_CFG_SET_ITEM_BY_FUNC(&master_cfg, ht_force_rate, _mtlk_core_get_force_rate,
                             (core, &master_cfg.ht_force_rate, MIB_HT_FORCE_RATE), res);
  if (res != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Cannot get HT force rate (err=%d)", mtlk_vap_get_oid(core->vap_handle), res);
    goto err_return;
  }

  MTLK_CFG_SET_ITEM(&master_cfg, power_selection, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_POWER_SELECTION));

  MTLK_CFG_SET_ITEM(&master_cfg, debug_tpc, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_DEBUG_TPC));

  MTLK_CFG_SET_ITEM(&master_cfg, rx_high_threshold, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_RX_HIGH_THRESHOLD));

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&master_cfg, rx_duty_cycle, _mtlk_core_get_rx_duty_cycle,
                                  (core, &master_cfg.rx_duty_cycle));

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&master_cfg, agg_rate_limit, _mtlk_core_get_agg_rate_limit,
                                  (core, &master_cfg.agg_rate_limit));

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&master_cfg, lna_gain, _mtlk_core_get_lna_gains,
                                  (core, &master_cfg.lna_gain));

  MTLK_CFG_SET_ITEM(&master_cfg, cca_threshold,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_CCA_THRESHOLD));

  MTLK_CFG_SET_ITEM(&master_cfg, ra_protection,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_RA_PROTECTION));

  MTLK_CFG_SET_ITEM(&master_cfg, force_ncb,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_FORCE_NCB));

  MTLK_CFG_SET_ITEM_BY_FUNC(&master_cfg, n_rate_bo, _mtlk_core_get_n_rate_bo,
                            (core, &master_cfg.n_rate_bo), res);

err_return:
  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &master_cfg, sizeof(master_cfg));
  }

  return res;
}

static int
_mtlk_core_set_bridge_mode(mtlk_core_t *core, uint8 mode)
{
  /* allow bridge mode change only if not connected */
  if (NET_STATE_CONNECTED == core->net_state) {
    ELOG_DD("CID-%04x: Cannot change bridge mode to (%d) while connected.", mtlk_vap_get_oid(core->vap_handle), mode);
    return MTLK_ERR_PARAMS;
  }

  /* check for only allowed values */
  if (mode >= BR_MODE_LAST) {
    ELOG_DD("CID-%04x: Unsupported bridge mode value: %d.", mtlk_vap_get_oid(core->vap_handle), mode);
    return MTLK_ERR_PARAMS;
  }

  /* on AP only NONE and WDS allowed */
  if (mtlk_vap_is_ap(core->vap_handle) && mode != BR_MODE_NONE && mode != BR_MODE_WDS) {
    ELOG_DD("CID-%04x: Unsupported (on AP) bridge mode value: %d.", mtlk_vap_get_oid(core->vap_handle), mode);
    return MTLK_ERR_PARAMS;
  }

  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_BRIDGE_MODE, mode);

  ILOG1_DD("CID-%04x: bridge_mode set to %u", mtlk_vap_get_oid(core->vap_handle), mode);

  if (mtlk_vap_is_ap(core->vap_handle)){
    if (mode == BR_MODE_WDS){
      /* Enable WDS abilities */
      wds_enable_abilities(&core->slow_ctx->wds_mng);
    }else{
      /* Disable WDS abilities */
      wds_disable_abilities(&core->slow_ctx->wds_mng);
    }
  }

  return MTLK_ERR_OK;
}

static int
_mtlk_core_set_spectrum_mode(mtlk_core_t *core, uint8 mode)
{
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  /* mode:
   * for AP: 20, 40;
   * for STA: 20, 40,
   *          auto - use 20MHz mode for 2.4G band and
   *                 use 40MHz mode for 5G band */
  if (!(mode == SPECTRUM_20MHZ || mode == SPECTRUM_40MHZ || (mode == SPECTRUM_AUTO && !mtlk_vap_is_ap(core->vap_handle)))) {
    ELOG_D("CID-%04x: Invalid value", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_PARAMS;
  }

  if (mtlk_vap_is_ap(core->vap_handle)) {
    MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_USER_SPECTRUM_MODE, mode);
  } else {
    MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_STA_FORCE_SPECTRUM_MODE, mode);
  }
  return MTLK_ERR_OK;
}

static int
_mtlk_core_update_network_mode(mtlk_core_t *core, uint8 mode)
{
  if(mtlk_core_scan_is_running(core)) {
    ELOG_D("CID-%04x: Cannot set network mode while scan is running", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_BUSY;
  }

  return mtlk_core_update_network_mode(core, mode);
}

static int
_mtlk_core_set_force_rate (mtlk_core_t *core, mtlk_core_rate_cfg_t *rate_cfg, uint16 u16ObjectID)
{
  uint16 forced_rate_idx;
  int res = MTLK_ERR_OK;
  int tmp_int_rate = 0;
  int tmp_frac_rate = 0;
  uint16 array_index = BITRATE_LAST+1;
  mtlk_core_t **core_array = NULL;
  mtlk_core_t *core_other = NULL;
  uint32 core_cnt = 0;
  uint32 core_idx = 0;

  MTLK_CFG_GET_ITEM(rate_cfg, int_rate, tmp_int_rate);
  MTLK_CFG_GET_ITEM(rate_cfg, frac_rate, tmp_frac_rate);
  MTLK_CFG_GET_ITEM(rate_cfg, array_idx, array_index);

  /* check is fate requested by index in array */
  if (array_index <= BITRATE_LAST) {
    forced_rate_idx = array_index;
  }
  else {
    res = mtlk_bitrate_rates_to_idx(tmp_int_rate,
                                    tmp_frac_rate,
                                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE),
                                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_TX),
                                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_RATE31),
                                    &forced_rate_idx);
  }

  if (res != MTLK_ERR_OK) {
    return res;
  }
  if (forced_rate_idx == NO_RATE) {
    goto apply;
  }

  /* Check new forced rate against all configured network modes */

  core_array = mtlk_vap_get_core_array(mtlk_vap_get_manager(core->vap_handle), &core_cnt);
  for(core_idx = 0; core_idx < core_cnt; core_idx++)
  {
    core_other = core_array[core_idx];
    if ((!core_other->is_stopped) && !((1 << forced_rate_idx) & mtlk_core_get_available_bitrates(core_other))) {
      ILOG0_DD("CID-%04x: Rate doesn't fall into list of supported rates for current network mode of CID-%04x",
               mtlk_vap_get_oid(core->vap_handle), mtlk_vap_get_oid(core_other->vap_handle));
      return MTLK_ERR_PARAMS;
    }
  }

apply:

  switch (u16ObjectID) {
    case MIB_LEGACY_FORCE_RATE:
      MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_LEGACY_FORCED_RATE_SET, forced_rate_idx);
      break;
    case MIB_HT_FORCE_RATE:
      MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_HT_FORCED_RATE_SET, forced_rate_idx);
      break;
    default:
      break;
  }

  mtlk_mib_set_forced_rates(core);

  return MTLK_ERR_OK;
}

static void
_mtlk_core_set_power_selection (mtlk_core_t *core, int8 power_selection)
{
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_POWER_SELECTION, power_selection);

  mtlk_mib_set_power_selection(core);
}

static int
mtlk_core_set_ra_protection (mtlk_core_t *core)
{
  int result = MTLK_ERR_OK;
  mtlk_txmm_data_t *man_entry = NULL;
  mtlk_txmm_msg_t man_msg;
  UMI_RA_PROTECTION *pRAprotection;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), &result);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send UMI_RA_PROTECTION to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
    goto FINISH;
  }

  man_entry->id           = UM_MAN_RA_PROTECTION_REQ;
  man_entry->payload_size = sizeof(*pRAprotection);

  memset(man_entry->payload, 0, man_entry->payload_size);
  pRAprotection = (UMI_RA_PROTECTION*)(man_entry->payload);

  pRAprotection->Mode = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_RA_PROTECTION);

  ILOG2_D("CID-%04x: Sending UMI_RA_PROTECTION", mtlk_vap_get_oid(core->vap_handle));
  result = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't send UMI_RA_PROTECTION request to MAC (err=%d)", mtlk_vap_get_oid(core->vap_handle), result);
    goto FINISH;
  }

  if (pRAprotection->Status != UMI_OK) {
    ELOG_DD("CID-%04x: Error returned for UMI_RA_PROTECTION request to MAC (err=%d)", mtlk_vap_get_oid(core->vap_handle), pRAprotection->Status);
    result = MTLK_ERR_UMI;
    goto FINISH;
  }

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return result;
}

static void
_mtlk_core_set_ra_protection (mtlk_core_t *core, uint8 ra_protection)
{
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_RA_PROTECTION, ra_protection);

  (void) mtlk_core_set_ra_protection(core);
}


static int
mtlk_core_set_force_ncb (mtlk_core_t *core)
{
  int result = MTLK_ERR_OK;
  mtlk_txmm_data_t *man_entry = NULL;
  mtlk_txmm_msg_t man_msg;
  UMI_FORCE_NCB *pForceNcb;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), &result);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send UMI_FORCE_NCB to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
    goto FINISH;
  }

  man_entry->id           = UM_MAN_FORCE_NCB_REQ;
  man_entry->payload_size = sizeof(*pForceNcb);

  memset(man_entry->payload, 0, man_entry->payload_size);
  pForceNcb = (UMI_FORCE_NCB*)(man_entry->payload);

  pForceNcb->Mode = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_FORCE_NCB);

  ILOG2_D("CID-%04x: Sending UMI_FORCE_NCB", mtlk_vap_get_oid(core->vap_handle));
  result = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't send UMI_FORCE_NCB request to MAC (err=%d)", mtlk_vap_get_oid(core->vap_handle), result);
    goto FINISH;
  }

//NOTICE 2 == UMI_BAD_PARAMETER
  if (pForceNcb->Status != UMI_OK) {
    ELOG_DD("CID-%04x: Error returned for UMI_FORCE_NCB request to MAC (err=%d)", mtlk_vap_get_oid(core->vap_handle), pForceNcb->Status);
    result = MTLK_ERR_UMI;
    goto FINISH;
  }

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return result;
}

static void
_mtlk_core_set_force_ncb (mtlk_core_t *core, uint8 force_ncb)
{
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_FORCE_NCB, force_ncb);

  (void) mtlk_core_set_force_ncb(core);
}

static int
mtlk_core_set_n_rate_bo (mtlk_core_t *core)
{
  int result = MTLK_ERR_OK;
  mtlk_txmm_data_t *man_entry = NULL;
  mtlk_txmm_msg_t man_msg;
  UMI_N_RATES_BO *pNratesBo;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), &result);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send UMI_N_RATES_BO to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
    goto FINISH;
  }

  man_entry->id           = UM_MAN_SET_N_RATES_POWER_BO_REQ;
  man_entry->payload_size = sizeof(*pNratesBo);

  memset(man_entry->payload, 0, man_entry->payload_size);
  pNratesBo = (UMI_N_RATES_BO*)(man_entry->payload);

  pNratesBo->QAM16backoff      = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_N_RATE_BO_QAM16);
  pNratesBo->QAM64_2_3_backoff = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_N_RATE_BO_QAM64_2_3);
  pNratesBo->QAM64_3_4_backoff = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_N_RATE_BO_QAM64_3_4);
  pNratesBo->QAM64_5_6_backoff = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_N_RATE_BO_QAM64_5_6);

  ILOG2_D("CID-%04x: Sending UMI_N_RATES_BO", mtlk_vap_get_oid(core->vap_handle));
  result = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't send UMI_N_RATES_BO request to MAC (err=%d)", mtlk_vap_get_oid(core->vap_handle), result);
  }

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return result;
}

static void
_mtlk_core_set_n_rate_bo (mtlk_core_t *core, mtlk_core_n_rate_bo_t *n_rate_bo)
{
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_N_RATE_BO_QAM16, n_rate_bo->qam16);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_N_RATE_BO_QAM64_2_3, n_rate_bo->qam64_2_3);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_N_RATE_BO_QAM64_3_4, n_rate_bo->qam64_3_4);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_N_RATE_BO_QAM64_5_6, n_rate_bo->qam64_5_6);

  (void) mtlk_core_set_n_rate_bo(core);
}

static int
_mtlk_core_set_debug_tpc (mtlk_core_t *core, uint8 debug_tpc)
{
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_DEBUG_TPC, debug_tpc);

  return _mtlk_core_send_current_debug_tpc(core);
}

BOOL
_mtlk_core_is_master_on_g35(mtlk_core_t *core)
{
    mtlk_vap_handle_t   vap_handle  = core->vap_handle;
    mtlk_card_type_t    hw_type     = MTLK_CARD_UNKNOWN;

    MTLK_ASSERT(NULL != core);

    if (!mtlk_vap_is_master(vap_handle)) {
        return FALSE;   /* is not master VAP */
    }

    (void)mtlk_vap_get_hw_vft(vap_handle)->get_prop(vap_handle,
                                MTLK_HW_PROP_CARD_TYPE, &hw_type, sizeof(&hw_type));

    CARD_SELECTOR_START(hw_type)
      IF_CARD_G3 (
        /* nothing */
      );
      IF_CARD_AHBG35 (
        return TRUE;
      );
    CARD_SELECTOR_END();

    return FALSE; /* is not g35 */
}

static int
_mtlk_core_send_current_rx_high_threshold (mtlk_core_t *core)
{
    return mtlk_mmb_send_rx_high_threshold(mtlk_vap_get_hw(core->vap_handle),
                MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_RX_HIGH_THRESHOLD));
}

static int
_mtlk_core_set_rx_high_threshold (mtlk_core_t *core, int8 rx_high_threshold)
{
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_RX_HIGH_THRESHOLD, rx_high_threshold);

  return mtlk_mmb_send_rx_high_threshold(mtlk_vap_get_hw(core->vap_handle), rx_high_threshold);
}

static int
_mtlk_core_set_rx_duty_cycle (mtlk_core_t *core, mtlk_rx_duty_cycle_t *rx_duty_cycle)
{
    MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_RX_DUTY_CYCLE_ONTIME,  rx_duty_cycle->onTime);
    MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_RX_DUTY_CYCLE_OFFTIME, rx_duty_cycle->offTime);

    return mtlk_mmb_send_rx_duty_cycle(mtlk_vap_get_hw(core->vap_handle),
                                        rx_duty_cycle->onTime, rx_duty_cycle->offTime);
}

static int
_mtlk_core_set_agg_rate_limit (mtlk_core_t *core, mtlk_agg_rate_limit_t *limit)
{
    MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_AGG_RATE_LIMIT_MODE, limit->mode);
    MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_AGG_RATE_LIMIT_MAXRATE, limit->maxRate);

    return mtlk_mmb_send_agg_rate_limit(mtlk_vap_get_hw(core->vap_handle), limit->mode, limit->maxRate);
}

static int
_mtlk_core_send_lna_gains_on_preactivate (mtlk_core_t *core)
{
mtlk_lna_gain_t  lna_gains;

  _mtlk_core_get_lna_gains(core, &lna_gains);

  return mtlk_mmb_send_lna_gains(mtlk_vap_get_hw(core->vap_handle), lna_gains.lna_gain_bypass, lna_gains.lna_gain_high);
}

static int
_mtlk_core_set_lna_gain (mtlk_core_t *core, mtlk_lna_gain_t *lna_gains)
{
uint32 lna_gains_pdb = 0;

  MTLK_BFIELD_SET(lna_gains_pdb, BF_LNA_GAIN_BYPASS, lna_gains->lna_gain_bypass);
  MTLK_BFIELD_SET(lna_gains_pdb, BF_LNA_GAIN_HIGH, lna_gains->lna_gain_high);
  MTLK_BFIELD_SET(lna_gains_pdb, BF_LNA_GAIN_IS_SET, (uint32)TRUE);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_LNA_GAIN, lna_gains_pdb);

  return mtlk_mmb_send_lna_gains(mtlk_vap_get_hw(core->vap_handle), lna_gains->lna_gain_bypass, lna_gains->lna_gain_high);
}

static int
_mtlk_core_send_current_cca_threshold (mtlk_core_t *core)
{
    return mtlk_mmb_send_cca_threshold(mtlk_vap_get_hw(core->vap_handle),
                MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_CCA_THRESHOLD));
}

static int
_mtlk_core_set_cca_threshold (mtlk_core_t *core, int8 cca_threshold)
{
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_CCA_THRESHOLD, cca_threshold);

  return mtlk_mmb_send_cca_threshold(mtlk_vap_get_hw(core->vap_handle), cca_threshold);
}

static __INLINE int
_mtlk_core_set_nickname_by_cfg(mtlk_core_t *core, mtlk_gen_core_cfg_t *core_cfg)
{
  int res = mtlk_pdb_set_string(mtlk_vap_get_param_db(core->vap_handle),
                                PARAM_DB_CORE_NICK_NAME,
                                core_cfg->nickname);
  if (MTLK_ERR_OK == res) {
    ILOG2_DS("CID-%04x: Set NICKNAME to \"%s\"", mtlk_vap_get_oid(core->vap_handle),
        core_cfg->nickname);
  }
  return res;
}

static __INLINE int
_mtlk_core_set_essid_by_cfg(mtlk_core_t *core, mtlk_gen_core_cfg_t *core_cfg)
{
  int res = mtlk_pdb_set_string(mtlk_vap_get_param_db(core->vap_handle),
                            PARAM_DB_CORE_ESSID, core_cfg->essid);
  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: Can't store ESSID (err=%d)", mtlk_vap_get_oid(core->vap_handle), res);
  } else {
    ILOG2_DS("CID-%04x: Set ESSID to \"%s\"", mtlk_vap_get_oid(core->vap_handle), core_cfg->essid);
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_add_acl (mtlk_core_t *core, mtlk_gen_core_cfg_t *core_cfg)
{
  int i, res = MTLK_ERR_OK;

  for (i=0; i < core_cfg->num_macs_to_set; i++) {
    MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, macs_to_set, mtlk_core_set_acl,
                                (core, &core_cfg->macs_to_set[i], core_cfg->mac_mask_filled ? &core_cfg->mac_mask[i] : NULL), res);
  }
  if (MTLK_ERR_OK == res) {
    res = mtlk_set_mib_acl(mtlk_vap_get_txmm(core->vap_handle), core->slow_ctx->acl, core->slow_ctx->acl_mask);
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_remove_acl (mtlk_core_t *core, mtlk_gen_core_cfg_t *core_cfg)
{
  int i, res = MTLK_ERR_OK;

  for (i=0; i < core_cfg->num_macs_to_del; i++) {
    MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, macs_to_del, mtlk_core_del_acl,
                                (core, &core_cfg->macs_to_del[i]), res);
  }
  if (MTLK_ERR_OK == res) {
    res = mtlk_set_mib_acl(mtlk_vap_get_txmm(core->vap_handle), core->slow_ctx->acl, core->slow_ctx->acl_mask);
  }

  return res;
}


static int __MTLK_IFUNC
_mtlk_core_set_core_cfg (mtlk_handle_t hcore,
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_gen_core_cfg_t *core_cfg = NULL;
  uint32 core_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  core_cfg = mtlk_clpb_enum_get_next(clpb, &core_cfg_size);

  MTLK_ASSERT(NULL != core_cfg);
  MTLK_ASSERT(sizeof(*core_cfg) == core_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()

  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, bridge_mode, _mtlk_core_set_bridge_mode,
                              (core, core_cfg->bridge_mode), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(core_cfg, reliable_multicast, mtlk_pdb_set_int,
                                    (mtlk_vap_get_param_db(core->vap_handle),
                                    PARAM_DB_CORE_RELIABLE_MCAST,
                                    !!core_cfg->reliable_multicast));

  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, up_rescan_exemption_time, _mtlk_core_set_up_rescan_exemption_time_sec,
                               (core, core_cfg->up_rescan_exemption_time), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(core_cfg, ap_forwarding, mtlk_pdb_set_int,
                                    (mtlk_vap_get_param_db(core->vap_handle),
                                    PARAM_DB_CORE_AP_FORWARDING,
                                    !!core_cfg->ap_forwarding));
  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, spectrum_mode, _mtlk_core_set_spectrum_mode,
                              (mtlk_core_get_master(core), core_cfg->spectrum_mode), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, net_mode, _mtlk_core_update_network_mode,
                              (core, core_cfg->net_mode), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, basic_rates, _mtlk_core_set_bss_rateset,
                              (core, core_cfg->basic_rates, PARAM_DB_CORE_BASIC_RATE_SET), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, supported_rates, _mtlk_core_set_bss_rateset,
                              (core, core_cfg->supported_rates, PARAM_DB_CORE_SUPPORTED_RATE_SET), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, extended_rates, _mtlk_core_set_bss_rateset,
                              (core, core_cfg->extended_rates, PARAM_DB_CORE_EXTENDED_RATE_SET), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, num_macs_to_set, _mtlk_core_add_acl, (core, core_cfg), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, num_macs_to_del, _mtlk_core_remove_acl, (core, core_cfg), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, nickname, _mtlk_core_set_nickname_by_cfg, (core, core_cfg), res);
  MTLK_CFG_GET_ITEM_BY_FUNC_VOID(core_cfg, essid, _mtlk_core_set_essid_by_cfg, (core, core_cfg));
  MTLK_CFG_CHECK_ITEM_AND_CALL(core_cfg, channel, _mtlk_core_set_channel,
                              (mtlk_core_get_master(core), core_cfg->channel, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_USER_SPECTRUM_MODE), TRUE), res);
  MTLK_CFG_GET_ITEM(core_cfg, is_hidden_ssid, core->slow_ctx->cfg.is_hidden_ssid);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_set_master_specific_cfg (mtlk_handle_t hcore,
                                    const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_master_core_cfg_t *master_cfg = NULL;
  uint32 master_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  master_cfg = mtlk_clpb_enum_get_next(clpb, &master_cfg_size);

  MTLK_ASSERT(NULL != master_cfg);
  MTLK_ASSERT(sizeof(*master_cfg) == master_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(master_cfg, legacy_force_rate, _mtlk_core_set_force_rate,
                                (core, &master_cfg->legacy_force_rate, MIB_LEGACY_FORCE_RATE), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(master_cfg, ht_force_rate, _mtlk_core_set_force_rate,
                                (core, &master_cfg->ht_force_rate, MIB_HT_FORCE_RATE), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(master_cfg, power_selection, _mtlk_core_set_power_selection,
                                     (core, master_cfg->power_selection));

  MTLK_CFG_CHECK_ITEM_AND_CALL(master_cfg, debug_tpc, _mtlk_core_set_debug_tpc,
                                (core, master_cfg->debug_tpc), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(master_cfg, rx_high_threshold, _mtlk_core_set_rx_high_threshold,
                                (core, master_cfg->rx_high_threshold), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(master_cfg, rx_duty_cycle, _mtlk_core_set_rx_duty_cycle,
                                (core, &master_cfg->rx_duty_cycle), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(master_cfg, agg_rate_limit, _mtlk_core_set_agg_rate_limit,
                                (core, &master_cfg->agg_rate_limit), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(master_cfg, lna_gain, _mtlk_core_set_lna_gain,
                                (core, &master_cfg->lna_gain), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL(master_cfg, cca_threshold, _mtlk_core_set_cca_threshold,
                                (core, master_cfg->cca_threshold), res);

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(master_cfg, ra_protection, _mtlk_core_set_ra_protection,
                                   (core, master_cfg->ra_protection));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(master_cfg, force_ncb, _mtlk_core_set_force_ncb,
                                   (core, master_cfg->force_ncb));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(master_cfg, n_rate_bo, _mtlk_core_set_n_rate_bo,
                                   (core, &master_cfg->n_rate_bo));

MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_eeprom_cfg (mtlk_handle_t hcore,
                           const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_eeprom_cfg_t* eeprom_cfg = mtlk_osal_mem_alloc(sizeof(mtlk_eeprom_cfg_t), MTLK_MEM_TAG_EEPROM);
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if(NULL == eeprom_cfg) {
    return MTLK_ERR_NO_MEM;
  }

  memset(eeprom_cfg, 0, sizeof(mtlk_eeprom_cfg_t));

  if (core->is_stopped) {
    MTLK_CFG_SET_ITEM(eeprom_cfg, is_if_stopped, TRUE);

    MTLK_CFG_SET_ITEM_BY_FUNC_VOID(eeprom_cfg, eeprom_data, mtlk_eeprom_get_cfg ,
                                   (mtlk_core_get_eeprom(core), &eeprom_cfg->eeprom_data));

    MTLK_CFG_SET_ITEM_BY_FUNC_VOID(eeprom_cfg, eeprom_raw_data, mtlk_eeprom_get_raw_cfg ,
                                   (core->vap_handle,
                                    eeprom_cfg->eeprom_raw_data,
                                    sizeof(eeprom_cfg->eeprom_raw_data)));
  }
  else {
    MTLK_CFG_SET_ITEM(eeprom_cfg, is_if_stopped, FALSE)
  }

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, eeprom_cfg, sizeof(mtlk_eeprom_cfg_t));
  }

  mtlk_osal_mem_free(eeprom_cfg);

  return res;
}

#ifdef EEPROM_DATA_VALIDATION

static int __MTLK_IFUNC
_mtlk_core_get_eeprom_fw (mtlk_handle_t hcore,
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_eeprom_cfg_t* eeprom_cfg = mtlk_osal_mem_alloc(sizeof(mtlk_eeprom_cfg_t), MTLK_MEM_TAG_EEPROM);
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if(NULL == eeprom_cfg) {
    return MTLK_ERR_NO_MEM;
  }

  memset(eeprom_cfg, 0, sizeof(mtlk_eeprom_cfg_t));

  if (core->is_stopped) {
    MTLK_CFG_SET_ITEM(eeprom_cfg, is_if_stopped, TRUE);

    MTLK_CFG_SET_ITEM_BY_FUNC_VOID(eeprom_cfg, eeprom_raw_data, mtlk_eeprom_get_raw_fw ,
                                   (core->vap_handle,
                                    eeprom_cfg->eeprom_raw_data,
                                    sizeof(eeprom_cfg->eeprom_raw_data)));
  }
  else {
    MTLK_CFG_SET_ITEM(eeprom_cfg, is_if_stopped, FALSE)
  }

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, eeprom_cfg, sizeof(mtlk_eeprom_cfg_t));
  }

  mtlk_osal_mem_free(eeprom_cfg);

  return res;
}

#endif /* EEPROM_DATA_VALIDATION */

static int __MTLK_IFUNC
_mtlk_core_get_hstdb_cfg (mtlk_handle_t hcore,
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_hstdb_cfg_t hstdb_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&hstdb_cfg, 0, sizeof(hstdb_cfg));

  MTLK_CFG_SET_ITEM(&hstdb_cfg, wds_host_timeout, core->slow_ctx->hstdb.wds_host_timeout);
  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&hstdb_cfg, address, mtlk_hstdb_get_local_mac,
                                 (&core->slow_ctx->hstdb, hstdb_cfg.address.au8Addr));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &hstdb_cfg, sizeof(hstdb_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_hstdb_cfg (mtlk_handle_t hcore,
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_hstdb_cfg_t *hstdb_cfg = NULL;
  uint32 hstdb_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  hstdb_cfg = mtlk_clpb_enum_get_next(clpb, &hstdb_cfg_size);

  MTLK_ASSERT(NULL != hstdb_cfg);
  MTLK_ASSERT(sizeof(*hstdb_cfg) == hstdb_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_GET_ITEM(hstdb_cfg, wds_host_timeout, core->slow_ctx->hstdb.wds_host_timeout);
  MTLK_CFG_CHECK_ITEM_AND_CALL(hstdb_cfg, address, mtlk_hstdb_set_local_mac,
                               (&core->slow_ctx->hstdb, hstdb_cfg->address.au8Addr), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}


int __MTLK_IFUNC
_mtlk_core_simple_cli (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  mtlk_txmm_msg_t         man_msg;
  mtlk_txmm_data_t*       man_entry = NULL;
  UmiDbgCliReq_t          *mac_msg;
  mtlk_dbg_cli_cfg_t      *UmiDbgCliReq;
  int                     res = MTLK_ERR_OK;

  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  uint32 clpb_data_size;
  void* clpb_data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  clpb_data = mtlk_clpb_enum_get_next(clpb, &clpb_data_size);
  MTLK_ASSERT(NULL != clpb_data);
  MTLK_ASSERT(sizeof(mtlk_dbg_cli_cfg_t) == clpb_data_size);
  UmiDbgCliReq = (mtlk_dbg_cli_cfg_t*)clpb_data;

  ILOG2_DDDDD("Simple CLI: Action %d, data1 %d, data2 %d, data3 %d, numOfArgs %d",
    UmiDbgCliReq->DbgCliReq.action, UmiDbgCliReq->DbgCliReq.data1, UmiDbgCliReq->DbgCliReq.data2,
    UmiDbgCliReq->DbgCliReq.data3, UmiDbgCliReq->DbgCliReq.numOfArgumets);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txdm(core->vap_handle), NULL);
  if (!man_entry) {
    ELOG_V("Can not get TXMM slot");
    return MTLK_ERR_NO_RESOURCES;
  }

  man_entry->id = UM_DBG_CLI_REQ;
  man_entry->payload_size = sizeof(UmiDbgCliReq_t);
  mac_msg = (UmiDbgCliReq_t *)man_entry->payload;

  mac_msg->action         = HOST_TO_MAC32(UmiDbgCliReq->DbgCliReq.action);
  mac_msg->numOfArgumets = HOST_TO_MAC32(UmiDbgCliReq->DbgCliReq.numOfArgumets);
  mac_msg->data1          = HOST_TO_MAC32(UmiDbgCliReq->DbgCliReq.data1);
  mac_msg->data2          = HOST_TO_MAC32(UmiDbgCliReq->DbgCliReq.data2);
  mac_msg->data3          = HOST_TO_MAC32(UmiDbgCliReq->DbgCliReq.data3);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  mtlk_txmm_msg_cleanup(&man_msg);

  if (MTLK_ERR_OK != res) {
    ELOG_D("DBG_CLI failed (res=%d)", res);
  }

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

int __MTLK_IFUNC
_mtlk_core_fw_debug (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  mtlk_txmm_msg_t     man_msg;
  mtlk_txmm_data_t*   man_entry = NULL;
  UMI_FW_DBG_REQ      *mac_msg;
  mtlk_fw_debug_cfg_t *UmiFWDebugReq;
  int                 res = MTLK_ERR_OK;

  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  uint32 clpb_data_size;
  void* clpb_data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  clpb_data = mtlk_clpb_enum_get_next(clpb, &clpb_data_size);
  MTLK_ASSERT(NULL != clpb_data);
  MTLK_ASSERT(sizeof(mtlk_fw_debug_cfg_t) == clpb_data_size);
  UmiFWDebugReq = (mtlk_fw_debug_cfg_t*)clpb_data;

  ILOG0_DD("CID-%04x: FW debug type: %d",
           mtlk_vap_get_oid(core->vap_handle),
           UmiFWDebugReq->FWDebugReq.debugType);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txdm(core->vap_handle), NULL);
  if (!man_entry) {
    ELOG_V("Can not get TXMM slot");
    return MTLK_ERR_NO_RESOURCES;
  }

  man_entry->id = UM_DBG_FW_DBG_REQ;
  man_entry->payload_size = sizeof(UMI_FW_DBG_REQ);
  mac_msg = (UMI_FW_DBG_REQ *)man_entry->payload;

  mac_msg->debugType = HOST_TO_MAC32(UmiFWDebugReq->FWDebugReq.debugType);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  mtlk_txmm_msg_cleanup(&man_msg);

  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: FW Debug message failed (res=%d)",
            mtlk_vap_get_oid(core->vap_handle),
            res);
  }

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

int __MTLK_IFUNC
_mtlk_core_get_mc_ps_size_cfg (mtlk_handle_t hcore,
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_mc_ps_size_cfg_t mc_ps_size;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&mc_ps_size, 0, sizeof(mc_ps_size));

  MTLK_CFG_SET_ITEM(&mc_ps_size, maxNumberOfFsdus,
                    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_FW_MC_PS_MAX_FSDUS));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &mc_ps_size, sizeof(mc_ps_size));
  }

  return res;
}

int __MTLK_IFUNC
mtlk_core_set_mc_ps_size (struct nic *core, uint32 maxNumberOfFsdus)
{
  mtlk_txmm_msg_t         man_msg;
  mtlk_txmm_data_t        *man_entry;
  UMI_MULTICAST_PS_SIZE   *mac_msg;
  uint32                  res;

  ILOG2_DD("CID-%04x: Set Multicast PS size: %d", mtlk_vap_get_oid(core->vap_handle), maxNumberOfFsdus);

  if ((1 > maxNumberOfFsdus) || (MAX_MPS_FSDUS_PER_VAP < maxNumberOfFsdus)) {
    ELOG_D("Parameter must be in range (1..%d)", MAX_MPS_FSDUS_PER_VAP);
    return MTLK_ERR_PARAMS;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (!man_entry) {
    ELOG_V("Can not get TXMM slot");
    return MTLK_ERR_NO_RESOURCES;
  }

  man_entry->id = UM_MAN_MULTICAST_PS_SIZE_REQ;
  man_entry->payload_size = sizeof(UMI_MULTICAST_PS_SIZE);
  mac_msg = (UMI_MULTICAST_PS_SIZE *)man_entry->payload;

  mac_msg->maxNumberOfFsdus = (uint8)maxNumberOfFsdus;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_mc_ps_size_cfg (mtlk_handle_t hcore,
                             const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;

  mtlk_mc_ps_size_cfg_t *mc_ps_size_cfg;
  uint32 mtlk_mc_ps_size_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  mc_ps_size_cfg = mtlk_clpb_enum_get_next(clpb, &mtlk_mc_ps_size_cfg_size);

  MTLK_ASSERT(NULL != mc_ps_size_cfg);
  MTLK_ASSERT(sizeof(*mc_ps_size_cfg) == mtlk_mc_ps_size_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
   MTLK_CFG_CHECK_ITEM_AND_CALL(mc_ps_size_cfg, maxNumberOfFsdus, mtlk_core_set_mc_ps_size,
                               (core, mc_ps_size_cfg->maxNumberOfFsdus), res);
   MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mc_ps_size_cfg, maxNumberOfFsdus, MTLK_CORE_PDB_SET_INT,
                               (core, PARAM_DB_FW_MC_PS_MAX_FSDUS, mc_ps_size_cfg->maxNumberOfFsdus));
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_scan_cfg (mtlk_handle_t hcore,
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_scan_cfg_t scan_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&scan_cfg, 0, sizeof(scan_cfg));

  MTLK_CFG_SET_ITEM(&scan_cfg, cache_expire,
                    mtlk_cache_get_expiration_time(&core->slow_ctx->cache));
  MTLK_CFG_SET_ITEM(&scan_cfg, channels_per_chunk_limit,
                    mtlk_scan_get_per_chunk_limit(&core->slow_ctx->scan));
  MTLK_CFG_SET_ITEM(&scan_cfg, pause_between_chunks,
                    mtlk_scan_get_pause_between_chunks(&core->slow_ctx->scan));
  MTLK_CFG_SET_ITEM(&scan_cfg, is_background_scan,
                    mtlk_scan_is_background_scan_enabled(&core->slow_ctx->scan));

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&scan_cfg, essid, mtlk_scan_get_essid,
                                (&core->slow_ctx->scan, scan_cfg.essid));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &scan_cfg, sizeof(scan_cfg));
  }

  return res;
}

static int
_mtlk_core_set_scan_cfg (mtlk_handle_t hcore,
                         const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_scan_cfg_t *scan_cfg = NULL;
  uint32 scan_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  scan_cfg = mtlk_clpb_enum_get_next(clpb, &scan_cfg_size);

  MTLK_ASSERT(NULL != scan_cfg);
  MTLK_ASSERT(sizeof(*scan_cfg) == scan_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_GET_ITEM_BY_FUNC(scan_cfg, cache_expire, mtlk_cache_modify_expiration_time,
                         (&core->slow_ctx->cache, scan_cfg->cache_expire, mtlk_core_get_net_state(core) != NET_STATE_CONNECTED),
                         res);
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(scan_cfg, channels_per_chunk_limit, mtlk_scan_set_per_chunk_limit,
                              (&core->slow_ctx->scan, scan_cfg->channels_per_chunk_limit));
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(scan_cfg, pause_between_chunks, mtlk_scan_set_pause_between_chunks,
                              (&core->slow_ctx->scan, scan_cfg->pause_between_chunks));
  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(scan_cfg, is_background_scan, mtlk_scan_set_is_background_scan_enabled,
                              (&core->slow_ctx->scan, scan_cfg->is_background_scan));

  MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(scan_cfg, essid, mtlk_scan_set_essid,
                                   (&core->slow_ctx->scan, scan_cfg->essid));
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_tx_power_limit_cfg (mtlk_handle_t hcore,
                                   const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_tx_power_limit_cfg_t tx_power_limit_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&tx_power_limit_cfg, 0, sizeof(tx_power_limit_cfg));

  MTLK_CFG_SET_ITEM(&tx_power_limit_cfg, field_01, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_POWER_LIMIT_11B_BOOST));
  MTLK_CFG_SET_ITEM(&tx_power_limit_cfg, field_02, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_POWER_LIMIT_BPSK_BOOST));
  MTLK_CFG_SET_ITEM(&tx_power_limit_cfg, field_03, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_POWER_LIMIT_AUTO_RESPONCE));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &tx_power_limit_cfg, sizeof(tx_power_limit_cfg));
  }

  return res;
}

static int
_mtlk_core_set_hw_data_cfg (mtlk_handle_t hcore,
                            const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_hw_data_cfg_t *hw_data_cfg = NULL;
  uint32 hw_data_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  hw_data_cfg = mtlk_clpb_enum_get_next(clpb, &hw_data_cfg_size);

  MTLK_ASSERT(NULL != hw_data_cfg);
  MTLK_ASSERT(sizeof(*hw_data_cfg) == hw_data_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(hw_data_cfg, hw_cfg, mtlk_set_hw_limit,
                              (&core->slow_ctx->tx_limits, &hw_data_cfg->hw_cfg), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL(hw_data_cfg, ant, mtlk_set_ant_gain,
                              (&core->slow_ctx->tx_limits, &hw_data_cfg->ant), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL(hw_data_cfg, power_limit, _mtlk_core_save_tx_power_limit,
                              (core, &hw_data_cfg->power_limit), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
_mtlk_core_get_qos_cfg (mtlk_handle_t hcore,
                        const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_qos_cfg_t qos_cfg;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&qos_cfg, 0, sizeof(qos_cfg));

  MTLK_CFG_SET_ITEM(&qos_cfg, map, mtlk_qos_get_map());

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &qos_cfg, sizeof(qos_cfg));
  }

  return res;
}

static int
_mtlk_core_set_qos_cfg (mtlk_handle_t hcore,
                        const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_qos_cfg_t *qos_cfg = NULL;
  uint32 qos_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  qos_cfg = mtlk_clpb_enum_get_next(clpb, &qos_cfg_size);

  MTLK_ASSERT(NULL != qos_cfg);
  MTLK_ASSERT(sizeof(*qos_cfg) == qos_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(qos_cfg, map, mtlk_qos_set_map,
                              (qos_cfg->map), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int
_mtlk_core_get_coc_antenna_params (mtlk_core_t *core, mtlk_coc_antenna_cfg_t *antenna_params)
{
  mtlk_coc_antenna_cfg_t *current_params;

  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(antenna_params != NULL);

  current_params = mtlk_coc_get_current_params(core->slow_ctx->coc_mngmt);
  memcpy(antenna_params, current_params, sizeof(mtlk_coc_antenna_cfg_t));

  return MTLK_ERR_OK;
}

static int
_mtlk_core_get_coc_auto_params (mtlk_core_t *core, mtlk_coc_auto_cfg_t *auto_params)
{
  mtlk_coc_auto_cfg_t *configured_params;

  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(auto_params != NULL);

  configured_params = mtlk_coc_get_auto_params(core->slow_ctx->coc_mngmt);
  memcpy(auto_params, configured_params, sizeof(mtlk_coc_auto_cfg_t));

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_core_get_coc_cfg (mtlk_handle_t hcore,
                        const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_coc_mode_cfg_t coc_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  memset(&coc_cfg, 0, sizeof(coc_cfg));

  MTLK_CFG_SET_ITEM(&coc_cfg, is_auto_mode, mtlk_coc_is_auto_mode(core->slow_ctx->coc_mngmt));
  MTLK_CFG_SET_ITEM_BY_FUNC(&coc_cfg, antenna_params,
                            _mtlk_core_get_coc_antenna_params, (core, &coc_cfg.antenna_params), res);
  MTLK_CFG_SET_ITEM_BY_FUNC(&coc_cfg, auto_params,
                            _mtlk_core_get_coc_auto_params, (core, &coc_cfg.auto_params), res);

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &coc_cfg, sizeof(coc_cfg));
  }

  return res;
}

static int
_mtlk_core_set_coc_power_mode (mtlk_core_t *core, BOOL is_auto_mode)
{
  int res = MTLK_ERR_OK;

  res = mtlk_coc_set_power_mode(core->slow_ctx->coc_mngmt, is_auto_mode);

  return res;
}

static int
_mtlk_core_set_antenna_params (mtlk_core_t *core, mtlk_coc_antenna_cfg_t *antenna_params)
{
  int res = MTLK_ERR_OK;

  res = mtlk_coc_set_antenna_params(core->slow_ctx->coc_mngmt, antenna_params);

  return res;
}

static int
_mtlk_core_set_auto_params (mtlk_core_t *core, mtlk_coc_auto_cfg_t *auto_params)
{
  int res = MTLK_ERR_OK;

  res = mtlk_coc_set_auto_params(core->slow_ctx->coc_mngmt, auto_params);

  return res;
}

static int
_mtlk_core_set_coc_cfg (mtlk_handle_t hcore,
                        const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_coc_mode_cfg_t *coc_cfg = NULL;
  uint32 coc_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  coc_cfg = mtlk_clpb_enum_get_next(clpb, &coc_cfg_size);

  MTLK_ASSERT(NULL != coc_cfg);
  MTLK_ASSERT(sizeof(*coc_cfg) == coc_cfg_size);

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(coc_cfg, auto_params, _mtlk_core_set_auto_params,
                               (core, &coc_cfg->auto_params), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL(coc_cfg, antenna_params, _mtlk_core_set_antenna_params,
                               (core, &coc_cfg->antenna_params), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL(coc_cfg, is_auto_mode, _mtlk_core_set_coc_power_mode,
                               (core, coc_cfg->is_auto_mode), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

#ifdef MTCFG_PMCU_SUPPORT

static int
_mtlk_core_get_pcoc_params (mtlk_core_t *core, mtlk_pcoc_params_t *params)
{
  mtlk_pcoc_params_t *configured_params;

  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(params != NULL);

  configured_params = mtlk_pcoc_get_params(core->slow_ctx->pcoc_mngmt);
  memcpy(params, configured_params, sizeof(mtlk_pcoc_params_t));

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_core_get_pcoc_cfg (mtlk_handle_t hcore,
                        const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_pcoc_mode_cfg_t pcoc_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  mtlk_pcoc_t *pcoc_obj = core->slow_ctx->pcoc_mngmt;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  if(!pcoc_obj)
  {
    res = MTLK_ERR_NOT_SUPPORTED;
    goto end;
  }

  memset(&pcoc_cfg, 0, sizeof(pcoc_cfg));

  MTLK_CFG_SET_ITEM(&pcoc_cfg, is_enabled,    mtlk_pcoc_is_enabled_adm(pcoc_obj));
  MTLK_CFG_SET_ITEM(&pcoc_cfg, is_active,     mtlk_pcoc_is_active(pcoc_obj));
  MTLK_CFG_SET_ITEM(&pcoc_cfg, traffic_state, mtlk_pcoc_get_traffic_state(pcoc_obj));
  MTLK_CFG_SET_ITEM_BY_FUNC(&pcoc_cfg, params,
                            _mtlk_core_get_pcoc_params, (core, &pcoc_cfg.params), res);

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &pcoc_cfg, sizeof(pcoc_cfg));
  }

end:

  return res;
}

static int
_mtlk_core_set_pcoc_enabled (mtlk_core_t *core, BOOL is_enabled)
{
  int res = MTLK_ERR_OK;

  res = mtlk_pcoc_set_enabled_adm(core->slow_ctx->pcoc_mngmt, is_enabled);

  return res;
}

static int
_mtlk_core_set_pcoc_pmcu_debug (mtlk_core_t *core, uint32 pmcu_debug)
{
  int res = MTLK_ERR_OK;

  res = mtlk_pcoc_test_pmcu_cb(core->slow_ctx->pcoc_mngmt, pmcu_debug);

  return res;
}

static int
_mtlk_core_set_pcoc_params (mtlk_core_t *core, mtlk_pcoc_params_t *params)
{
  int res = MTLK_ERR_OK;

  res = mtlk_pcoc_set_params(core->slow_ctx->pcoc_mngmt, params);

  return res;
}

static int
_mtlk_core_set_pcoc_cfg (mtlk_handle_t hcore,
                        const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_pcoc_mode_cfg_t *pcoc_cfg = NULL;
  uint32 pcoc_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  mtlk_pcoc_t *pcoc_obj = core->slow_ctx->pcoc_mngmt;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  pcoc_cfg = mtlk_clpb_enum_get_next(clpb, &pcoc_cfg_size);

  MTLK_ASSERT(NULL != pcoc_cfg);
  MTLK_ASSERT(sizeof(*pcoc_cfg) == pcoc_cfg_size);

  if(!pcoc_obj)
  {
    res = MTLK_ERR_NOT_SUPPORTED;
    goto end;
  }

MTLK_CFG_START_CHEK_ITEM_AND_CALL()
  MTLK_CFG_CHECK_ITEM_AND_CALL(pcoc_cfg, params, _mtlk_core_set_pcoc_params,
                               (core, &pcoc_cfg->params), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL(pcoc_cfg, is_enabled, _mtlk_core_set_pcoc_enabled,
                               (core, pcoc_cfg->is_enabled), res);
  MTLK_CFG_CHECK_ITEM_AND_CALL(pcoc_cfg, pmcu_debug, _mtlk_core_set_pcoc_pmcu_debug,
                               (core, pcoc_cfg->pmcu_debug), res);
MTLK_CFG_END_CHEK_ITEM_AND_CALL()

end:

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

#endif /*MTCFG_PMCU_SUPPORT*/

static int __MTLK_IFUNC
_mtlk_core_mbss_add_vap (mtlk_handle_t hcore, uint32 vap_index)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_vap_handle_t slave_vap_handle;
  mtlk_card_type_t hw_type = MTLK_CARD_UNKNOWN;
  uint32 max_vaps_count;

  ILOG0_D("CID-%04x: Got PRM_ID_VAP_ADD", mtlk_vap_get_oid(core->vap_handle));

  max_vaps_count = mtlk_vap_manager_get_max_vaps_count(mtlk_vap_get_manager(core->vap_handle));
  if (vap_index >= max_vaps_count) {
    ELOG_DD("CID-%04x: VAP#%d index invalid", mtlk_vap_get_oid(core->vap_handle), vap_index);
    res = MTLK_ERR_PARAMS;
  }
  else if (mtlk_vap_get_hw_vft(core->vap_handle)->get_prop(core->vap_handle, MTLK_HW_PROP_CARD_TYPE, &hw_type, sizeof(&hw_type))) {
    ELOG_D("CID-%04x: Can't determine HW type", mtlk_vap_get_oid(core->vap_handle));
  }
  else if ((res = mtlk_vap_manager_create_vap(mtlk_vap_get_manager(core->vap_handle), &slave_vap_handle, hw_type, vap_index)) != MTLK_ERR_OK) {
    ELOG_D("CID-%04x: Can't add VAP", mtlk_vap_get_oid(core->vap_handle));
  }
  else if ((res = mtlk_vap_start(slave_vap_handle, TRUE)) != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't start VAP#%d", mtlk_vap_get_oid(core->vap_handle), vap_index);
    mtlk_vap_delete(slave_vap_handle);
  }
  else {
    ILOG0_DD("CID-%04x: VAP#%d added", mtlk_vap_get_oid(core->vap_handle), vap_index);
  }

  return res;
}

static int
_mtlk_core_mbss_del_vap (mtlk_handle_t hcore, uint32 vap_index)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_vap_handle_t vap_handle;
  int target_core_state;
  uint32 max_vaps_count;

  max_vaps_count = mtlk_vap_manager_get_max_vaps_count(mtlk_vap_get_manager(core->vap_handle));
  if (vap_index >= max_vaps_count) {
    ELOG_DD("CID-%04x: VAP#%d index invalid", mtlk_vap_get_oid(core->vap_handle), vap_index);
    res = MTLK_ERR_PARAMS;
    goto func_ret;
  }

  res = mtlk_vap_manager_get_vap_handle(mtlk_vap_get_manager(core->vap_handle), vap_index, &vap_handle);
  if (MTLK_ERR_OK != res ) {
    ELOG_DD("CID-%04x: VAP#%d doesn't exist", mtlk_vap_get_oid(core->vap_handle), vap_index);
    res = MTLK_ERR_PARAMS;
    goto func_ret;
  }

  if (mtlk_vap_is_master(vap_handle)) {
    ELOG_D("CID-%04x: Can't remove Master VAP", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_PARAMS;
    goto func_ret;
  }

  target_core_state = mtlk_core_get_net_state(mtlk_vap_get_core(vap_handle));
  if ( 0 == ((NET_STATE_READY|NET_STATE_IDLE|NET_STATE_HALTED) & target_core_state) ) {
    ILOG1_D("CID-%04x:: Invalid card state - request rejected", mtlk_vap_get_oid(vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto func_ret;
  }

  ILOG0_DD("CID-%04x: Deleting VAP#%d", mtlk_vap_get_oid(core->vap_handle), vap_index);
  mtlk_vap_stop(vap_handle, MTLK_VAP_SLAVE_INTERFACE);
  mtlk_vap_delete(vap_handle);
  res = MTLK_ERR_OK;

func_ret:
  return res;
}

static int __MTLK_IFUNC
_mtlk_core_add_vap (mtlk_handle_t hcore,
                    const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_mbss_cfg_t *mbss_cfg;
  uint32 mbss_cfg_size = sizeof(mtlk_mbss_cfg_t);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  mbss_cfg = mtlk_clpb_enum_get_next(clpb, &mbss_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_CHECK_ITEM_AND_CALL(mbss_cfg, added_vap_index, _mtlk_core_mbss_add_vap,
                                 (hcore, mbss_cfg->added_vap_index), res);
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_del_vap (mtlk_handle_t hcore,
                    const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_mbss_cfg_t *mbss_cfg;
  uint32 mbss_cfg_size = sizeof(mtlk_mbss_cfg_t);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  mbss_cfg = mtlk_clpb_enum_get_next(clpb, &mbss_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_CHECK_ITEM_AND_CALL(mbss_cfg, deleted_vap_index, _mtlk_core_mbss_del_vap,
                                 (hcore, mbss_cfg->deleted_vap_index), res);
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));

  return res;
}

static int
_mtlk_core_set_mbss_vap_limits (mtlk_core_t *core, uint32 min_limit, uint32 max_limit)
{
  uint32 res = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry;
  UMI_LIMITS_VAP_OPERATE *vap_operate;

  if (min_limit != MTLK_MBSS_VAP_LIMIT_DEFAULT &&
    max_limit != MTLK_MBSS_VAP_LIMIT_DEFAULT &&
    min_limit > max_limit) {
      ELOG_V("maximum limit is lower than minimum limit value");
      res = MTLK_ERR_PARAMS;
      goto FINISH;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto FINISH;
  }

  MTLK_ASSERT(man_entry->payload != NULL);

  man_entry->id = UM_MAN_VAP_LIMITS_REQ;
  man_entry->payload_size = sizeof(UMI_LIMITS_VAP_OPERATE);
  vap_operate = (UMI_LIMITS_VAP_OPERATE *)(man_entry->payload);
  vap_operate->u8MaxLimit = max_limit;
  vap_operate->u8MinLimit = min_limit;
  vap_operate->u8OperationCode = VAP_OPERATIONS_LIMITS_SET;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Can't set limits, timed-out", mtlk_vap_get_oid(core->vap_handle));
    goto RELEASE_MAN_ENTRY;
  }

  switch (vap_operate->u8Status) {
  case UMI_OK:
    res = MTLK_ERR_OK;
    break;
  case UMI_NOT_SUPPORTED:
    res = MTLK_ERR_NOT_SUPPORTED;
    break;
  case UMI_BAD_PARAMETER:
    res = MTLK_ERR_PARAMS;
    break;
  case UMI_BSS_ALREADY_ACTIVE:
    res = MTLK_ERR_NOT_READY;
    break;
  default:
    MTLK_ASSERT(FALSE);
  }

RELEASE_MAN_ENTRY:
  mtlk_txmm_msg_cleanup(&man_msg);

FINISH:
  return res;
}


static int
mtlk_core_get_mbss_vap_limits (mtlk_core_t *core, const void *data)
{
  int res = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry;
  UMI_LIMITS_VAP_OPERATE *vap_operate;
  mtlk_mbss_vap_limit_data_cfg_t *vap_limits = (mtlk_mbss_vap_limit_data_cfg_t *)data;

  MTLK_ASSERT(vap_limits != NULL);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto FINISH;
  }

  MTLK_ASSERT(man_entry->payload != NULL);

  man_entry->id = UM_MAN_VAP_LIMITS_REQ;
  man_entry->payload_size = sizeof(UMI_LIMITS_VAP_OPERATE);
  vap_operate = (UMI_LIMITS_VAP_OPERATE *)man_entry->payload;
  vap_operate->u8OperationCode = VAP_OPERATION_LIMITS_QUERY;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Can't set limits, timed-out", mtlk_vap_get_oid(core->vap_handle));
    goto SEND_COMPLETED;
  }

  switch (vap_operate->u8Status) {
  case UMI_OK:
    res = MTLK_ERR_OK;
    break;
  case UMI_NOT_SUPPORTED:
    res = MTLK_ERR_NOT_SUPPORTED;
    goto SEND_COMPLETED;
    break;
  case UMI_BAD_PARAMETER:
    res = MTLK_ERR_PARAMS;
    goto SEND_COMPLETED;
    break;
  default:
    MTLK_ASSERT(FALSE);
  }
  vap_limits->lower_limit = vap_operate->u8MinLimit;
  vap_limits->upper_limit = vap_operate->u8MaxLimit;


SEND_COMPLETED:
  mtlk_txmm_msg_cleanup(&man_msg);
FINISH:
  return res;
}

static int __MTLK_IFUNC
_mtlk_core_get_mbss_vars (mtlk_handle_t hcore,
                          const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_mbss_cfg_t mbss_cfg;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  mtlk_core_t *core = (mtlk_core_t*)hcore;

  memset(&mbss_cfg, 0, sizeof(mbss_cfg));

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_SET_ITEM_BY_FUNC(&mbss_cfg, vap_limits, mtlk_core_get_mbss_vap_limits,
                              (core, &mbss_cfg.vap_limits), res);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &mbss_cfg, sizeof(mbss_cfg));
  }

  return res;
}

typedef struct
{
  int          res;
  mtlk_clpb_t *clpb;
} mtlk_core_get_serializer_info_enum_ctx_t;

static BOOL __MTLK_IFUNC
__mtlk_core_get_serializer_info_enum_clb (mtlk_serializer_t    *szr,
                                          const mtlk_command_t *command,
                                          BOOL                  is_current,
                                          mtlk_handle_t         enum_ctx)
{
  mtlk_core_get_serializer_info_enum_ctx_t *ctx =
    HANDLE_T_PTR(mtlk_core_get_serializer_info_enum_ctx_t, enum_ctx);
  mtlk_serializer_command_info_t cmd_info;

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_SET_ITEM(&cmd_info, is_current, is_current);
    MTLK_CFG_SET_ITEM(&cmd_info, priority, mtlk_command_get_priority(command));
    MTLK_CFG_SET_ITEM(&cmd_info, issuer_slid, mtlk_command_get_issuer_slid(command));
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  ctx->res = mtlk_clpb_push(ctx->clpb, &cmd_info, sizeof(cmd_info));

  return (ctx->res == MTLK_ERR_OK)?TRUE:FALSE;
}

static int __MTLK_IFUNC
_mtlk_core_get_serializer_info (mtlk_handle_t hcore,
                                const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_core_get_serializer_info_enum_ctx_t ctx;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  ctx.clpb = *(mtlk_clpb_t **) data;
  ctx.res  = MTLK_ERR_OK;

  mtlk_serializer_enum_commands(&nic->slow_ctx->serializer, __mtlk_core_get_serializer_info_enum_clb, HANDLE_T(&ctx));

  return ctx.res;
}

static int __MTLK_IFUNC
_mtlk_core_set_mbss_vars (mtlk_handle_t hcore,
                          const void* data, uint32 data_size)
{

  uint32 res = MTLK_ERR_OK;
  mtlk_mbss_cfg_t *mbss_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  uint32 mbss_cfg_size = sizeof(mtlk_mbss_cfg_t);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  mbss_cfg = mtlk_clpb_enum_get_next(clpb, &mbss_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_CHECK_ITEM_AND_CALL(mbss_cfg, vap_limits, _mtlk_core_set_mbss_vap_limits,
                                 (core, mbss_cfg->vap_limits.lower_limit, mbss_cfg->vap_limits.upper_limit), res);
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mbss_cfg, vap_limits, MTLK_CORE_PDB_SET_INT,
                                 (core, PARAM_DB_CORE_STA_LIMIT_MIN, mbss_cfg->vap_limits.lower_limit));
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(mbss_cfg, vap_limits, MTLK_CORE_PDB_SET_INT,
                                 (core, PARAM_DB_CORE_STA_LIMIT_MAX, mbss_cfg->vap_limits.upper_limit));
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()


  res = mtlk_clpb_push(clpb, &res, sizeof(res));

  return res;
}
/* 20/40 coexistence */
static int __MTLK_IFUNC
_mtlk_core_get_coex_20_40_mode_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_coex_20_40_mode_cfg_t coex_cfg;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&coex_cfg, 0, sizeof(coex_cfg));


  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_SET_ITEM(&coex_cfg, coexistence_mode,
      MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_COEX_CFG));

    MTLK_CFG_SET_ITEM(&coex_cfg, rssi_threshold,
      mtlk_20_40_get_rssi_threshold(mtlk_core_get_coex_sm(core)));

    MTLK_CFG_SET_ITEM(&coex_cfg, intolerance_mode,
      mtlk_20_40_is_intolerance_declared(mtlk_core_get_coex_sm(core)));

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &coex_cfg, sizeof(coex_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
  _mtlk_core_get_coex_20_40_ap_force_params_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_coex_20_40_ap_force_params_cfg_t coex_cfg;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&coex_cfg, 0, sizeof(coex_cfg));


  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_SET_ITEM(&coex_cfg, ap_force_scan_params_on_assoc_sta,
      mtlk_20_40_get_ap_force_scan_params_on_assoc_sta(mtlk_core_get_coex_sm(core)));

    MTLK_CFG_SET_ITEM(&coex_cfg, wait_for_scan_results_interval,
      mtlk_20_40_get_wait_for_scan_results_interval(mtlk_core_get_coex_sm(core)));

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

    res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &coex_cfg, sizeof(coex_cfg));
  }

  return res;
}

//*******1
static int __MTLK_IFUNC
  _mtlk_core_get_coex_20_40_exm_req_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_coex_20_40_exm_req_cfg_t coex_cfg;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&coex_cfg, 0, sizeof(coex_cfg));

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_SET_ITEM(&coex_cfg, exemption_req,
      mtlk_20_40_sta_is_scan_exemption_request_forced(mtlk_core_get_coex_sm(core)));

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &coex_cfg, sizeof(coex_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
  _mtlk_core_get_coex_20_40_min_num_exm_sta_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_coex_20_40_min_num_exm_sta_cfg_t coex_cfg;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&coex_cfg, 0, sizeof(coex_cfg));

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

  MTLK_CFG_SET_ITEM(&coex_cfg, min_non_exempted_sta,
    mtlk_20_40_get_min_non_exempted_sta(mtlk_core_get_coex_sm(core)));

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
     res = mtlk_clpb_push(clpb, &coex_cfg, sizeof(coex_cfg));
  }

  return res;
}



//*****2
static int __MTLK_IFUNC
  _mtlk_core_get_coex_20_40_times_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_coex_20_40_times_cfg_t coex_cfg;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&coex_cfg, 0, sizeof(coex_cfg));

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_SET_ITEM(&coex_cfg, delay_factor,
      mtlk_20_40_get_transition_delay_factor(mtlk_core_get_coex_sm(core)));

    MTLK_CFG_SET_ITEM(&coex_cfg, obss_scan_interval,
      mtlk_20_40_get_scan_interval(mtlk_core_get_coex_sm(core)));

    MTLK_CFG_SET_ITEM(&coex_cfg, scan_activity_threshold,
      mtlk_20_40_get_scan_activity_threshold(mtlk_core_get_coex_sm(core)));

    MTLK_CFG_SET_ITEM(&coex_cfg, scan_passive_dwell,
      mtlk_20_40_get_scan_passive_dwell(mtlk_core_get_coex_sm(core)));

    MTLK_CFG_SET_ITEM(&coex_cfg, scan_active_dwell,
      mtlk_20_40_get_scan_active_dwell(mtlk_core_get_coex_sm(core)));

    MTLK_CFG_SET_ITEM(&coex_cfg, scan_passive_total_per_channel,
      mtlk_20_40_get_passive_total_per_channel(mtlk_core_get_coex_sm(core)));

    MTLK_CFG_SET_ITEM(&coex_cfg, scan_active_total_per_channel,
      mtlk_20_40_get_active_total_per_channel(mtlk_core_get_coex_sm(core)));

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &coex_cfg, sizeof(coex_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
  _mtlk_core_set_coex_20_40_mode_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_coex_20_40_mode_cfg_t *coex_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  uint32 coex_cfg_size = sizeof(mtlk_coex_20_40_mode_cfg_t);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  coex_cfg = mtlk_clpb_enum_get_next(clpb, &coex_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_GET_ITEM_BY_FUNC_VOID(coex_cfg, coexistence_mode,
      MTLK_CORE_PDB_SET_INT, (core, PARAM_DB_CORE_COEX_CFG, coex_cfg->coexistence_mode));
    MTLK_CFG_GET_ITEM_BY_FUNC_VOID(coex_cfg, rssi_threshold,
      mtlk_20_40_set_rssi_threshold, (mtlk_core_get_coex_sm(core), coex_cfg->rssi_threshold));
    MTLK_CFG_GET_ITEM_BY_FUNC(coex_cfg, intolerance_mode,
      mtlk_20_40_declare_intolerance, (mtlk_core_get_coex_sm(core), coex_cfg->intolerance_mode), res);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
  _mtlk_core_set_coex_20_40_ap_force_params_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_coex_20_40_ap_force_params_cfg_t *coex_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  uint32 coex_cfg_size = sizeof(mtlk_coex_20_40_ap_force_params_cfg_t);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  coex_cfg = mtlk_clpb_enum_get_next(clpb, &coex_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_GET_ITEM_BY_FUNC(coex_cfg, ap_force_scan_params_on_assoc_sta,
      mtlk_20_40_set_ap_force_scan_params_on_assoc_sta, (mtlk_core_get_coex_sm(core), coex_cfg->ap_force_scan_params_on_assoc_sta), res);

    MTLK_CFG_GET_ITEM_BY_FUNC(coex_cfg, wait_for_scan_results_interval,
      mtlk_20_40_set_wait_for_scan_results_interval, (mtlk_core_get_coex_sm(core), coex_cfg->wait_for_scan_results_interval), res);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

    return mtlk_clpb_push(clpb, &res, sizeof(res));
}

//*****1s
static int __MTLK_IFUNC
  _mtlk_core_set_coex_20_40_exm_req_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_coex_20_40_exm_req_cfg_t *coex_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  uint32 coex_cfg_size = sizeof(mtlk_coex_20_40_exm_req_cfg_t);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  coex_cfg = mtlk_clpb_enum_get_next(clpb, &coex_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_GET_ITEM_BY_FUNC(coex_cfg, exemption_req,
        mtlk_20_40_sta_force_scan_exemption_request, (mtlk_core_get_coex_sm(core), coex_cfg->exemption_req), res);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int __MTLK_IFUNC
  _mtlk_core_set_coex_20_40_min_num_exm_sta_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_coex_20_40_min_num_exm_sta_cfg_t *coex_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  uint32 coex_cfg_size = sizeof(mtlk_coex_20_40_min_num_exm_sta_cfg_t);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  coex_cfg = mtlk_clpb_enum_get_next(clpb, &coex_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

  MTLK_CFG_GET_ITEM_BY_FUNC(coex_cfg, min_non_exempted_sta,
    mtlk_20_40_set_min_non_exempted_sta, (mtlk_core_get_coex_sm(core), coex_cfg->min_non_exempted_sta), res);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}



//*****2s
static int __MTLK_IFUNC
  _mtlk_core_set_coex_20_40_times_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_coex_20_40_times_cfg_t *coex_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  uint32 coex_cfg_size = sizeof(mtlk_coex_20_40_times_cfg_t);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  coex_cfg = mtlk_clpb_enum_get_next(clpb, &coex_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_GET_ITEM_BY_FUNC(coex_cfg, delay_factor,
      mtlk_20_40_set_transition_delay_factor, (mtlk_core_get_coex_sm(core), coex_cfg->delay_factor), res);

    MTLK_CFG_GET_ITEM_BY_FUNC(coex_cfg, obss_scan_interval,
      mtlk_20_40_set_scan_interval, (mtlk_core_get_coex_sm(core), coex_cfg->obss_scan_interval), res);

    MTLK_CFG_GET_ITEM_BY_FUNC(coex_cfg, scan_activity_threshold,
      mtlk_20_40_set_scan_activity_threshold, (mtlk_core_get_coex_sm(core), coex_cfg->scan_activity_threshold), res);

    MTLK_CFG_GET_ITEM_BY_FUNC(coex_cfg, scan_passive_dwell,
      mtlk_20_40_set_scan_passive_dwell, (mtlk_core_get_coex_sm(core), coex_cfg->scan_passive_dwell), res);

    MTLK_CFG_GET_ITEM_BY_FUNC(coex_cfg, scan_active_dwell,
      mtlk_20_40_set_scan_active_dwell, (mtlk_core_get_coex_sm(core), coex_cfg->scan_active_dwell), res);

    MTLK_CFG_GET_ITEM_BY_FUNC(coex_cfg, scan_passive_total_per_channel,
      mtlk_20_40_set_passive_total_per_channel, (mtlk_core_get_coex_sm(core), coex_cfg->scan_passive_total_per_channel), res);

    MTLK_CFG_GET_ITEM_BY_FUNC(coex_cfg, scan_active_total_per_channel,
      mtlk_20_40_set_active_total_per_channel, (mtlk_core_get_coex_sm(core), coex_cfg->scan_active_total_per_channel), res);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

/* End of: 20/40 coexistence */

static int
_mtlk_core_set_interfdet_do_params (mtlk_core_t *core)
{
  BOOL interf_det_enabled, is_spectrum_40;

  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(mtlk_vap_is_master_ap(core->vap_handle));

  interf_det_enabled = FALSE;
  interf_det_enabled |= (0 != MTLK_CORE_PDB_GET_INT(core, PARAM_DB_INTERFDET_20MHZ_DETECTION_THRESHOLD));
  interf_det_enabled |= (0 != MTLK_CORE_PDB_GET_INT(core, PARAM_DB_INTERFDET_20MHZ_NOTIFICATION_THRESHOLD));

  MTLK_CORE_PDB_SET_INT(mtlk_core_get_master(core), PARAM_DB_INTERFDET_MODE, interf_det_enabled);

  if (!core->is_stopped){
    if (MTLK_HW_BAND_2_4_GHZ == mtlk_core_get_freq_band_cfg(core)) {
      is_spectrum_40 = (SPECTRUM_40MHZ == _mtlk_core_get_user_spectrum_mode(core));

      if (MTLK_ERR_OK == _mtlk_core_set_fw_interfdet_req(core, interf_det_enabled, is_spectrum_40, FALSE)) {
        ILOG0_DS("CID-%04x: Interference detection is %s", mtlk_vap_get_oid(core->vap_handle),
                  (interf_det_enabled ? "activated" : "deactivated"));
        mtlk_core_interfdet_enable(core, interf_det_enabled);
      }
      else {
        ELOG_DS("CID-%04x: Interference detection cannot be %s", mtlk_vap_get_oid(core->vap_handle),
                (interf_det_enabled ? "activated" : "deactivated"));
        mtlk_core_interfdet_enable(core, FALSE);
      }
    }
  }

  return MTLK_ERR_OK;
}

static int
_mtlk_core_set_interfdet_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_interfdet_cfg_t *interfdet_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  uint32 interfdet_cfg_size = sizeof(mtlk_interfdet_cfg_t);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **)data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  interfdet_cfg = mtlk_clpb_enum_get_next(clpb, &interfdet_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(interfdet_cfg, req_timeouts, MTLK_CORE_PDB_SET_INT,
                            (core, PARAM_DB_INTERFDET_ACTIVE_POLLING_TIMEOUT, interfdet_cfg->req_timeouts.active_polling_timeout));
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(interfdet_cfg, req_timeouts, MTLK_CORE_PDB_SET_INT,
                            (core, PARAM_DB_INTERFDET_SHORT_SCAN_POLLING_TIMEOUT, interfdet_cfg->req_timeouts.short_scan_polling_timeout));
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(interfdet_cfg, req_timeouts, MTLK_CORE_PDB_SET_INT,
                            (core, PARAM_DB_INTERFDET_LONG_SCAN_POLLING_TIMEOUT, interfdet_cfg->req_timeouts.long_scan_polling_timeout));
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(interfdet_cfg, req_timeouts, MTLK_CORE_PDB_SET_INT,
                            (core, PARAM_DB_INTERFDET_ACTIVE_NOTIFICATION_TIMEOUT, interfdet_cfg->req_timeouts.active_notification_timeout));
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(interfdet_cfg, req_timeouts, MTLK_CORE_PDB_SET_INT,
                            (core, PARAM_DB_INTERFDET_SHORT_SCAN_NOTIFICATION_TIMEOUT, interfdet_cfg->req_timeouts.short_scan_notification_timeout));
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(interfdet_cfg, req_timeouts, MTLK_CORE_PDB_SET_INT,
                            (core, PARAM_DB_INTERFDET_LONG_SCAN_NOTIFICATION_TIMEOUT, interfdet_cfg->req_timeouts.long_scan_notification_timeout));

    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(interfdet_cfg, req_thresh, MTLK_CORE_PDB_SET_INT,
                            (core, PARAM_DB_INTERFDET_20MHZ_DETECTION_THRESHOLD, interfdet_cfg->req_thresh.detection_threshold_20mhz));
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(interfdet_cfg, req_thresh, MTLK_CORE_PDB_SET_INT,
                            (core, PARAM_DB_INTERFDET_20MHZ_NOTIFICATION_THRESHOLD, interfdet_cfg->req_thresh.notification_threshold_20mhz));
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(interfdet_cfg, req_thresh, MTLK_CORE_PDB_SET_INT,
                            (core, PARAM_DB_INTERFDET_40MHZ_DETECTION_THRESHOLD, interfdet_cfg->req_thresh.detection_threshold_40mhz));
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(interfdet_cfg, req_thresh, MTLK_CORE_PDB_SET_INT,
                            (core, PARAM_DB_INTERFDET_40MHZ_NOTIFICATION_THRESHOLD, interfdet_cfg->req_thresh.notification_threshold_40mhz));
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(interfdet_cfg, req_thresh, MTLK_CORE_PDB_SET_INT,
                            (core, PARAM_DB_INTERFDET_SCAN_NOISE_THRESHOLD, interfdet_cfg->req_thresh.scan_noise_threshold));
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(interfdet_cfg, req_thresh, MTLK_CORE_PDB_SET_INT,
                            (core, PARAM_DB_INTERFDET_SCAN_MINIMUM_NOISE, interfdet_cfg->req_thresh.scan_minimum_noise));

    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(interfdet_cfg, req_scantimes, mtlk_scan_set_scan_time,
              (&core->slow_ctx->scan, interfdet_cfg->req_scantimes.long_scan_max_time, interfdet_cfg->req_scantimes.short_scan_max_time));

    _mtlk_core_set_interfdet_do_params(core);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int
_mtlk_core_get_interfdet_mode_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  mtlk_interfdet_mode_cfg_t interfdet_mode_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **)data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(mtlk_vap_is_master_ap(core->vap_handle));
  memset(&interfdet_mode_cfg, 0, sizeof(interfdet_mode_cfg));

  MTLK_CFG_SET_ITEM(&interfdet_mode_cfg, interfdet_mode, mtlk_core_is_interfdet_enabled(core));

  return mtlk_clpb_push(clpb, &interfdet_mode_cfg, sizeof(interfdet_mode_cfg));
}

static int
_mtlk_core_set_fw_interfdet_req (mtlk_core_t *core, BOOL interfdet_mode, BOOL is_spectrum_40, BOOL is_long_scan)
{
  int res = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_INTERFERER_DETECTION *umi_interfdet = NULL;

  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(mtlk_vap_is_master_ap(core->vap_handle));

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto END;
  }

  man_entry->id = UM_MAN_ENABLE_DETECTION_REQ;
  man_entry->payload_size = sizeof(*umi_interfdet);

  umi_interfdet = (UMI_INTERFERER_DETECTION *)man_entry->payload;
  umi_interfdet->mode = (uint8)interfdet_mode;

  if (is_spectrum_40){
    umi_interfdet->threshold = (int8)MTLK_CORE_PDB_GET_INT(core, PARAM_DB_INTERFDET_40MHZ_NOTIFICATION_THRESHOLD);
  }
  else {
    umi_interfdet->threshold = (int8)MTLK_CORE_PDB_GET_INT(core, PARAM_DB_INTERFDET_20MHZ_NOTIFICATION_THRESHOLD);
  }

  umi_interfdet->ActivePollingTimeout      = HOST_TO_MAC32(MTLK_CORE_PDB_GET_INT(core, PARAM_DB_INTERFDET_ACTIVE_POLLING_TIMEOUT));
  umi_interfdet->ActiveNotificationTimeout = HOST_TO_MAC32(MTLK_CORE_PDB_GET_INT(core, PARAM_DB_INTERFDET_ACTIVE_NOTIFICATION_TIMEOUT));

  if (is_long_scan){
    umi_interfdet->ScanPollingTimeout      = HOST_TO_MAC32(MTLK_CORE_PDB_GET_INT(core, PARAM_DB_INTERFDET_LONG_SCAN_POLLING_TIMEOUT));
    umi_interfdet->ScanNotificationTimeout = HOST_TO_MAC32(MTLK_CORE_PDB_GET_INT(core, PARAM_DB_INTERFDET_LONG_SCAN_NOTIFICATION_TIMEOUT));
  }
  else {
    umi_interfdet->ScanPollingTimeout      = HOST_TO_MAC32(MTLK_CORE_PDB_GET_INT(core, PARAM_DB_INTERFDET_SHORT_SCAN_POLLING_TIMEOUT));
    umi_interfdet->ScanNotificationTimeout = HOST_TO_MAC32(MTLK_CORE_PDB_GET_INT(core, PARAM_DB_INTERFDET_SHORT_SCAN_NOTIFICATION_TIMEOUT));
  }

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Cannot send UM_MAN_ENABLE_DETECTION_REQ to the FW (err=%d)", res);
    goto END;
  }

  ILOG0_DSDSSDDDDD("CID-%04x: UMI_INTERFDET: %s (%d) %s %s: Scan Poll/Notif:%d/%d, Active Poll/Notif: %d/%d, Thresh: %d",
    mtlk_vap_get_oid(core->vap_handle),
    umi_interfdet->mode ? "EN":"DIS",
    umi_interfdet->mode,
    is_long_scan ? "Long":"Short",
    is_spectrum_40 ? "40MHz":"20MHz",
    MAC_TO_HOST32(umi_interfdet->ScanPollingTimeout),
    MAC_TO_HOST32(umi_interfdet->ScanNotificationTimeout),
    MAC_TO_HOST32(umi_interfdet->ActivePollingTimeout),
    MAC_TO_HOST32(umi_interfdet->ActiveNotificationTimeout),
    umi_interfdet->threshold);

END:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return res;
};

static void
_mtlk_core_store_fw_led_gpio_cfg (mtlk_core_t *core, const mtlk_fw_led_gpio_cfg_t *fw_led_gpio_cfg)
{
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_FW_LED_GPIO_DISABLE_TESTBUS, fw_led_gpio_cfg->disable_testbus);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_FW_LED_GPIO_ACTIVE_GPIOs,    fw_led_gpio_cfg->active_gpios);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_FW_LED_GPIO_LED_POLARITY,    fw_led_gpio_cfg->led_polatity);
}

static int
_mtlk_core_set_fw_led_gpio_cfg (mtlk_core_t *core, const mtlk_fw_led_gpio_cfg_t *fw_led_gpio_cfg)
{
  int res = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_CONFIG_GPIO  *umi_gpio_cfg = NULL;

  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(fw_led_gpio_cfg != NULL);
  MTLK_ASSERT(mtlk_vap_is_slave_ap(core->vap_handle) == FALSE);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto END;
  }

  man_entry->id = UM_MAN_CONFIG_GPIO_REQ;
  man_entry->payload_size = sizeof(*umi_gpio_cfg);

  umi_gpio_cfg = (UMI_CONFIG_GPIO *)man_entry->payload;

  umi_gpio_cfg->uDisableTestbus = fw_led_gpio_cfg->disable_testbus;
  umi_gpio_cfg->uActiveGpios    = fw_led_gpio_cfg->active_gpios;
  umi_gpio_cfg->bLedPolarity    = fw_led_gpio_cfg->led_polatity;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Cannot send UM_MAN_CONFIG_GPIO_REQ to the FW (err=%d)", res);
    goto END;
  }

END:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}

static void
_mtlk_core_get_fw_led_gpio_cfg (mtlk_core_t *core, mtlk_fw_led_gpio_cfg_t *fw_led_gpio_cfg)
{
  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(fw_led_gpio_cfg != NULL);

  fw_led_gpio_cfg->disable_testbus = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_FW_LED_GPIO_DISABLE_TESTBUS);
  fw_led_gpio_cfg->active_gpios    = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_FW_LED_GPIO_ACTIVE_GPIOs);
  fw_led_gpio_cfg->led_polatity    = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_FW_LED_GPIO_LED_POLARITY);
}

static void
_mtlk_core_store_fw_led_state (mtlk_core_t *core, const mtlk_fw_led_state_t *fw_led_state)
{
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_FW_LED_STATE_BASEB_LED, fw_led_state->baseb_led);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_FW_LED_STATE_LED_STATE, fw_led_state->led_state);
}

static int
_mtlk_core_set_fw_led_state (mtlk_core_t *core, const mtlk_fw_led_state_t *fw_led_state)
{
  int res = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_SET_LED      *umi_led_state = NULL;

  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(fw_led_state != NULL);
  MTLK_ASSERT(mtlk_vap_is_slave_ap(core->vap_handle) == FALSE);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto END;
  }

  man_entry->id = UM_MAN_SET_LED_REQ;
  man_entry->payload_size = sizeof(*umi_led_state);

  umi_led_state = (UMI_SET_LED *)man_entry->payload;

  umi_led_state->u8BasebLed  = fw_led_state->baseb_led;
  umi_led_state->u8LedStatus = fw_led_state->led_state;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Cannot send UM_MAN_SET_LED_REQ to the FW (err=%d)", res);
    goto END;
  }

END:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return res;
}

static void
_mtlk_core_get_fw_led_state (mtlk_core_t *core, mtlk_fw_led_state_t *fw_led_state)
{
  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(fw_led_state != NULL);

  fw_led_state->baseb_led = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_FW_LED_STATE_BASEB_LED);
  fw_led_state->led_state = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_FW_LED_STATE_LED_STATE);
}

static int
_mtlk_core_set_fw_led_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_UNKNOWN;
  mtlk_fw_led_cfg_t *fw_led_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  uint32 fw_led_cfg_size = sizeof(*fw_led_cfg);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  fw_led_cfg = mtlk_clpb_enum_get_next(clpb, &fw_led_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_CHECK_ITEM_AND_CALL(fw_led_cfg, gpio_cfg,
                                _mtlk_core_set_fw_led_gpio_cfg,
                                (core, &fw_led_cfg->gpio_cfg), res);
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(fw_led_cfg, gpio_cfg,
                                      _mtlk_core_store_fw_led_gpio_cfg,
                                      (core, &fw_led_cfg->gpio_cfg));

    MTLK_CFG_CHECK_ITEM_AND_CALL(fw_led_cfg, led_state,
                                _mtlk_core_set_fw_led_state,
                                (core, &fw_led_cfg->led_state), res);
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(fw_led_cfg, led_state,
                                      _mtlk_core_store_fw_led_state,
                                      (core, &fw_led_cfg->led_state));

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int
_mtlk_core_get_fw_led_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_UNKNOWN;
  mtlk_fw_led_cfg_t fw_led_cfg;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  memset(&fw_led_cfg, 0, sizeof(fw_led_cfg));

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_SET_ITEM_BY_FUNC_VOID(&fw_led_cfg, gpio_cfg,
                                   _mtlk_core_get_fw_led_gpio_cfg, (core, &fw_led_cfg.gpio_cfg));
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &fw_led_cfg, sizeof(fw_led_cfg));
  }

  return res;
}

static int
_mtlk_core_set_fw_log_severity (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_UNKNOWN;
  mtlk_fw_log_severity_t  *fw_log_severity;
  mtlk_core_t  *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t  *clpb = *(mtlk_clpb_t **) data;
  void         *clpb_data;
  uint32        clpb_data_size;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  clpb_data = mtlk_clpb_enum_get_next(clpb, &clpb_data_size);
  MTLK_ASSERT(NULL != clpb_data);
  MTLK_ASSERT(sizeof(mtlk_fw_log_severity_t) == clpb_data_size);

  fw_log_severity = (mtlk_fw_log_severity_t *) clpb_data;

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    /* need to check two parameters */
    /* 1st: check only */
    MTLK_CFG_CHECK_ITEM_VOID(fw_log_severity, newLevel);

    /* 2nd: check and call */
    MTLK_CFG_CHECK_ITEM_AND_CALL(fw_log_severity, targetCPU,
        _mtlk_mmb_send_fw_log_severity,
        (mtlk_vap_get_hw(core->vap_handle), fw_log_severity->newLevel, fw_log_severity->targetCPU), res);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int
_mtlk_core_set_enhanced11b_th (mtlk_core_t *core, const mtlk_enhanced11b_th_t *thresholds)
{
  int res = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_Enhanced11bTH_Req *umi_11b_threshold = NULL;

  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(thresholds != NULL);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto END;
  }

  man_entry->id = UM_MAN_CHANGE_11B_THRESHOLD_REQ;
  man_entry->payload_size = sizeof(*umi_11b_threshold);

  umi_11b_threshold = (UMI_Enhanced11bTH_Req *)man_entry->payload;

  umi_11b_threshold->Consecutive11bTH = thresholds->Consecutive11bTH;
  umi_11b_threshold->Consecutive11nTH = thresholds->Consecutive11nTH;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Cannot send UM_MAN_CHANGE_11B_THRESHOLD_REQ to the FW (err=%d)", res);
    goto END;
  }

END:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return res;
}

static void
_mtlk_core_store_enhanced11b_cfg (mtlk_core_t *core, const mtlk_enhanced11b_th_t *thresholds)
{
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CONSECUTIVE_11B_TH, thresholds->Consecutive11bTH);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CONSECUTIVE_11N_TH, thresholds->Consecutive11nTH);
}

static int
_mtlk_core_set_enhanced11b_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_enhanced11b_cfg_t *enhanced11b_cfg = NULL;
  uint32 enhanced11b_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  enhanced11b_cfg = mtlk_clpb_enum_get_next(clpb, &enhanced11b_cfg_size);

  MTLK_ASSERT(NULL != enhanced11b_cfg);
  MTLK_ASSERT(sizeof(*enhanced11b_cfg) == enhanced11b_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_CHECK_ITEM_AND_CALL(enhanced11b_cfg, thresholds, _mtlk_core_set_enhanced11b_th,
                                 (core, &enhanced11b_cfg->thresholds), res);
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(enhanced11b_cfg, thresholds, _mtlk_core_store_enhanced11b_cfg,
                                      (core, &enhanced11b_cfg->thresholds));
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int
_mtlk_core_get_enhanced11b_th (mtlk_core_t *core, mtlk_enhanced11b_th_t *thresholds)
{
  int res = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_Enhanced11bTH_Req *umi_11b_threshold = NULL;

  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(thresholds != NULL);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto END;
  }

  man_entry->id = UM_MAN_GET_11B_THRESHOLD_REQ;
  man_entry->payload_size = sizeof(*umi_11b_threshold);
  umi_11b_threshold = (UMI_Enhanced11bTH_Req *)man_entry->payload;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Cannot send UM_MAN_GET_11B_THRESHOLD_REQ to the FW (err=%d)", res);
    goto END;
  }

  thresholds->Consecutive11bTH = umi_11b_threshold->Consecutive11bTH;
  thresholds->Consecutive11nTH = umi_11b_threshold->Consecutive11nTH;

END:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return res;
}

static int
_mtlk_core_get_enhanced11b_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_enhanced11b_cfg_t enhanced11b_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&enhanced11b_cfg, 0, sizeof(enhanced11b_cfg));

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_SET_ITEM_BY_FUNC(&enhanced11b_cfg, thresholds,
                              _mtlk_core_get_enhanced11b_th, (core, &enhanced11b_cfg.thresholds), res);
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &enhanced11b_cfg, sizeof(enhanced11b_cfg));
  }

  return res;
}

static void
_mtlk_core_store_11b_antsel (mtlk_core_t *core, const mtlk_11b_antsel_t *antsel)
{
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_11B_ANTSEL_RATE, antsel->rate);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_11B_ANTSEL_RXANT, antsel->rxAnt);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_11B_ANTSEL_TXANT, antsel->txAnt);
}

static int
_mtlk_core_set_11b_antsel (mtlk_core_t *core, const mtlk_11b_antsel_t *antsel)
{
#define _11B_ANTSEL_ROUNDROBIN    3 /* TODO: move this define to the mac shared files */

  int res = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_ANT_SELECTION_11B *umi_11b_antsel = NULL;

  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(antsel != NULL);

  /* rate field is set only when txAnt and rxAnt set to Round Robin mode,
     otherwise the rate value should be 0 */
  if (((antsel->txAnt != _11B_ANTSEL_ROUNDROBIN) && (antsel->rxAnt != _11B_ANTSEL_ROUNDROBIN) &&
       (antsel->rate != 0)) ||
      ((antsel->txAnt == _11B_ANTSEL_ROUNDROBIN) && (antsel->rxAnt == _11B_ANTSEL_ROUNDROBIN) &&
       (antsel->rate == 0)) ||
      ((antsel->txAnt == _11B_ANTSEL_ROUNDROBIN) && (antsel->rxAnt != _11B_ANTSEL_ROUNDROBIN)) ||
      ((antsel->txAnt != _11B_ANTSEL_ROUNDROBIN) && (antsel->rxAnt == _11B_ANTSEL_ROUNDROBIN))) {
    ELOG_DDD("Incorrect configuration: txAnt=%d, rxAnt=%d, rate=%d", antsel->txAnt, antsel->rxAnt, antsel->rate);
    res = MTLK_ERR_PARAMS;
    goto END;
  }

  if ((antsel->txAnt >= core->slow_ctx->tx_limits.num_tx_antennas) &&
      (antsel->txAnt != _11B_ANTSEL_ROUNDROBIN)) {
    ELOG_D("Incorrect configuration: txAnt=%d", antsel->txAnt);
    res = MTLK_ERR_PARAMS;
    goto END;
  }

  if ((antsel->rxAnt >= core->slow_ctx->tx_limits.num_rx_antennas) &&
      (antsel->rxAnt != _11B_ANTSEL_ROUNDROBIN)) {
    ELOG_D("Incorrect configuration: rxAnt=%d", antsel->rxAnt);
    res = MTLK_ERR_PARAMS;
    goto END;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto END;
  }

  man_entry->id = UM_MAN_SEND_11B_SET_ANT_REQ;
  man_entry->payload_size = sizeof(*umi_11b_antsel);

  umi_11b_antsel = (UMI_ANT_SELECTION_11B *)man_entry->payload;

  umi_11b_antsel->txAnt = antsel->txAnt;
  umi_11b_antsel->rxAnt = antsel->rxAnt;
  umi_11b_antsel->rate  = antsel->rate;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Cannot send UM_MAN_SEND_11B_SET_ANT_REQ to the FW (err=%d)", res);
    goto END;
  }

END:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return res;
}

static int
_mtlk_core_set_11b_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_11b_cfg_t *_11b_cfg = NULL;
  uint32 _11b_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  _11b_cfg = mtlk_clpb_enum_get_next(clpb, &_11b_cfg_size);
  MTLK_ASSERT(NULL != _11b_cfg);
  MTLK_ASSERT(sizeof(*_11b_cfg) == _11b_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_CHECK_ITEM_AND_CALL(_11b_cfg, antsel, _mtlk_core_set_11b_antsel,
                                 (core, &_11b_cfg->antsel), res);
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(_11b_cfg, antsel, _mtlk_core_store_11b_antsel,
                                      (core, &_11b_cfg->antsel));
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int
_mtlk_core_get_11b_antsel (mtlk_core_t *core, mtlk_11b_antsel_t *antsel)
{
  int res = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_ANT_SELECTION_11B *umi_11b_antsel = NULL;

  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(antsel != NULL);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto END;
  }

  man_entry->id = UM_MAN_SEND_11B_GET_ANT_REQ;
  man_entry->payload_size = sizeof(*umi_11b_antsel);
  umi_11b_antsel = (UMI_ANT_SELECTION_11B *)man_entry->payload;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Cannot send UM_MAN_SEND_11B_GET_ANT_REQ to the FW (err=%d)", res);
    goto END;
  }

  antsel->txAnt = umi_11b_antsel->txAnt;
  antsel->rxAnt = umi_11b_antsel->rxAnt;
  antsel->rate = umi_11b_antsel->rate;

END:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return res;
}

static int
_mtlk_core_get_11b_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_11b_cfg_t _11b_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&_11b_cfg, 0, sizeof(_11b_cfg));

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_SET_ITEM_BY_FUNC(&_11b_cfg, antsel,
                              _mtlk_core_get_11b_antsel, (core, &_11b_cfg.antsel), res);
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &_11b_cfg, sizeof(_11b_cfg));
  }

  return res;
}

/* This line must be the last in this file */
#include "core_ext.c"

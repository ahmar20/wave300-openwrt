/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MTLK_WSS_ID_H__
#define __MTLK_WSS_ID_H__

#define MTLK_WWSS_WLAN_STAT_ID_FIRST 0x80000000

/* Separate statistic counters by group */

#ifndef FALSE
#define FALSE (0)
#endif
#ifndef TRUE
#define TRUE  (1)
#endif

/* DEBUG statistic */
#ifdef  MTCFG_STAT_CNTRS_DEBUG
#define MTLK_WWSS_WLAN_STAT_DEBUG   TRUE
#else  /* MTCFG_STAT_CNTRS_DEBUG */
#define MTLK_WWSS_WLAN_STAT_DEBUG   FALSE
#endif /* MTCFG_STAT_CNTRS_DEBUG */

/* Note: might be configured individually in the future */
#define MTLK_WWSS_WLAN_STAT_GROUP_ALLWAYS_ALLOWED       TRUE
#define MTLK_WWSS_WLAN_STAT_GROUP_HOTP_MIN_ALLOWED      MTLK_WWSS_WLAN_STAT_DEBUG
#define MTLK_WWSS_WLAN_STAT_GROUP_HOTP_OPT_ALLOWED      MTLK_WWSS_WLAN_STAT_DEBUG
#define MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED        MTLK_WWSS_WLAN_STAT_DEBUG
#define MTLK_WWSS_WLAN_STAT_GROUP_HW_SOURCE_ALLOWED     MTLK_WWSS_WLAN_STAT_DEBUG
#define MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED        MTLK_WWSS_WLAN_STAT_DEBUG
        /* Some Analyzer pocessess */
#define MTLK_WWSS_WLAN_STAT_GROUP_ANALYZER_TX_ALLOWED        MTLK_WWSS_WLAN_STAT_GROUP_HOTP_MIN_ALLOWED
#define MTLK_WWSS_WLAN_STAT_GROUP_ANALYZER_RX_ALLOWED        MTLK_WWSS_WLAN_STAT_GROUP_ALLWAYS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_GROUP_ANALYZER_RX_LONG_ALLOWED   MTLK_WWSS_WLAN_STAT_GROUP_HOTP_MIN_ALLOWED

/* MTIDL statistic: Minimal or Full */
#define MTLK_MTIDL_PEER_STAT_FULL                       MTLK_WWSS_WLAN_STAT_DEBUG
#define MTLK_MTIDL_WLAN_STAT_FULL                       MTLK_WWSS_WLAN_STAT_DEBUG
#define MTLK_MTIDL_HW_STAT_FULL                         MTLK_WWSS_WLAN_STAT_DEBUG

typedef enum
{
  MTLK_WWSS_WLAN_STAT_ID_FCS_ERROR_COUNT = MTLK_WWSS_WLAN_STAT_ID_FIRST,
  MTLK_WWSS_WLAN_STAT_ID_RTS_SUCCESS_COUNT,
  MTLK_WWSS_WLAN_STAT_ID_RTS_FAILURE_COUNT,
  MTLK_WWSS_WLAN_STAT_ID_FREE_TX_MSDUs,
  MTLK_WWSS_WLAN_STAT_ID_TX_MSDUs_USAGE_PEAK,
  MTLK_WWSS_WLAN_STAT_ID_BIST_CHECK_PASSED,
  MTLK_WWSS_WLAN_STAT_ID_MAN_MSG_SENT,
  MTLK_WWSS_WLAN_STAT_ID_MAN_MSG_CONFIRMED,
  MTLK_WWSS_WLAN_STAT_ID_MAN_MSG_PEAK,
  MTLK_WWSS_WLAN_STAT_ID_DBG_MSG_SENT,
  MTLK_WWSS_WLAN_STAT_ID_DBG_MSG_CONFIRMED,
  MTLK_WWSS_WLAN_STAT_ID_DBG_MSG_PEAK,
  MTLK_WWSS_WLAN_STAT_ID_FW_LOGGER_PACKETS_PROCESSED,
  MTLK_WWSS_WLAN_STAT_ID_FW_LOGGER_PACKETS_DROPPED,
  MTLK_WWSS_WLAN_STAT_ID_DAT_FRAMES_RECEIVED,
  MTLK_WWSS_WLAN_STAT_ID_CTL_FRAMES_RECEIVED,
  MTLK_WWSS_WLAN_STAT_ID_MAN_FRAMES_RECEIVED,

  MTLK_WWSS_WLAN_STAT_ID_ISRS_TOTAL,
  MTLK_WWSS_WLAN_STAT_ID_ISRS_FOREIGN,
  MTLK_WWSS_WLAN_STAT_ID_ISRS_NOT_PENDING, 
  MTLK_WWSS_WLAN_STAT_ID_ISRS_HALTED,
  MTLK_WWSS_WLAN_STAT_ID_ISRS_INIT,
  MTLK_WWSS_WLAN_STAT_ID_ISRS_TO_DPC,
  MTLK_WWSS_WLAN_STAT_ID_ISRS_UNKNOWN,
  MTLK_WWSS_WLAN_STAT_ID_POST_ISR_DPCS,
  MTLK_WWSS_WLAN_STAT_ID_FW_MSGS_HANDLED,
  MTLK_WWSS_WLAN_STAT_ID_SQ_DPCS_SCHEDULED,
  MTLK_WWSS_WLAN_STAT_ID_SQ_DPCS_ARRIVED,
  MTLK_WWSS_WLAN_STAT_ID_RX_BUF_ALLOC_FAILED,
  MTLK_WWSS_WLAN_STAT_ID_RX_BUF_REALLOC_FAILED,
  MTLK_WWSS_WLAN_STAT_ID_RX_BUF_REALLOCATED,

  MTLK_WWSS_WLAN_STAT_ID_BYTES_SENT,
  MTLK_WWSS_WLAN_STAT_ID_BYTES_RECEIVED,
  MTLK_WWSS_WLAN_STAT_ID_UNICAST_PACKETS_SENT,
  MTLK_WWSS_WLAN_STAT_ID_UNICAST_PACKETS_RECEIVED,
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_PACKETS_SENT,
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_PACKETS_RECEIVED,
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_PACKETS_SENT,
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_PACKETS_RECEIVED,
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_BYTES_SENT,
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_BYTES_RECEIVED,
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_BYTES_SENT,
  MTLK_WWSS_WLAN_STAT_ID_BROADCAST_BYTES_RECEIVED,
  MTLK_WWSS_WLAN_STAT_ID_NACKed_PACKETS_SENT,
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_NO_PEERS,
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_ACM,
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_EAPOL_CLONED,
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_DIRECTED,
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_MCAST,
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_NO_RESOURCES,
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_SQ_OVERFLOW,
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_EAPOL_FILTER,
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_DROP_ALL_FILTER,
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_TX_QUEUE_OVERFLOW,
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_FOREIGN,
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_LOOPBACK,
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_TOO_OLD,
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_DUPLICATE,
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_NO_RESOURCES,

  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_FW,
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_FW,
  MTLK_WWSS_WLAN_STAT_ID_PAIRWISE_MIC_FAILURE_PACKETS,
  MTLK_WWSS_WLAN_STAT_ID_GROUP_MIC_FAILURE_PACKETS,
  MTLK_WWSS_WLAN_STAT_ID_UNICAST_REPLAYED_PACKETS,
  MTLK_WWSS_WLAN_STAT_ID_MULTICAST_REPLAYED_PACKETS,
  MTLK_WWSS_WLAN_STAT_ID_RETRY_COUNT,
  MTLK_WWSS_WLAN_STAT_ID_MULTIPLE_RETRY_COUNT,
  MTLK_WWSS_WLAN_STAT_ID_FWD_RX_PACKETS,
  MTLK_WWSS_WLAN_STAT_ID_FWD_RX_BYTES,
  MTLK_WWSS_WLAN_STAT_ID_BAR_FRAMES_COUNT,
  MTLK_WWSS_WLAN_STAT_ID_MSDU_RECEIVED_AC_BE,
  MTLK_WWSS_WLAN_STAT_ID_MSDU_RECEIVED_AC_BK,
  MTLK_WWSS_WLAN_STAT_ID_MSDU_RECEIVED_AC_VI,
  MTLK_WWSS_WLAN_STAT_ID_MSDU_RECEIVED_AC_VO,
  MTLK_WWSS_WLAN_STAT_ID_MSDU_TRANSMITTED_AC_BE,
  MTLK_WWSS_WLAN_STAT_ID_MSDU_TRANSMITTED_AC_BK,
  MTLK_WWSS_WLAN_STAT_ID_MSDU_TRANSMITTED_AC_VI,
  MTLK_WWSS_WLAN_STAT_ID_MSDU_TRANSMITTED_AC_VO,
  MTLK_WWSS_WLAN_STAT_ID_MSDU_USED_AC_BE,
  MTLK_WWSS_WLAN_STAT_ID_MSDU_USED_AC_BK,
  MTLK_WWSS_WLAN_STAT_ID_MSDU_USED_AC_VI,
  MTLK_WWSS_WLAN_STAT_ID_MSDU_USED_AC_VO,
  MTLK_WWSS_WLAN_STAT_ID_MAC_STATS,
  MTLK_WWSS_WLAN_STAT_ID_SEND_QUEUE_STATS,
  MTLK_WWSS_WLAN_STAT_ID_REORDERING_STATS,

  MTLK_WWSS_WLAN_STAT_ID_LAST_DATA_UPLINK_RATE,
  MTLK_WWSS_WLAN_STAT_ID_LAST_DATA_DOWNLINK_RATE,
  MTLK_WWSS_WLAN_STAT_ID_SIGNAL_STRENGTH_SHORT,
  MTLK_WWSS_WLAN_STAT_ID_SIGNAL_STRENGTH_HISTORY,
  MTLK_WWSS_WLAN_STAT_ID_DISCARDED_TX_PACKETS_FW,
  MTLK_WWSS_WLAN_STAT_ID_PACKETS_SENT,
  MTLK_WWSS_WLAN_STAT_ID_PACKETS_RECEIVED,

  MTLK_WWSS_WLAN_STAT_ID_802_1X_PACKETS_SENT,
  MTLK_WWSS_WLAN_STAT_ID_802_1X_PACKETS_RECEIVED,

  MTLK_WWSS_WLAN_STAT_ID_LAST_ACTIVITY_TIMESTAMP,
  MTLK_WWSS_WLAN_STAT_ID_TX_THROUGHPUT_SHORT,
  MTLK_WWSS_WLAN_STAT_ID_TX_THROUGHPUT_HISTORY,
  MTLK_WWSS_WLAN_STAT_ID_RX_THROUGHPUT_SHORT,
  MTLK_WWSS_WLAN_STAT_ID_RX_THROUGHPUT_HISTORY,
  MTLK_WWSS_WLAN_STAT_ID_PS_MODE_ENTRANCES,

  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_RECEIVED,                           /* received coexistence element */
  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_REQUESTED,           /* received coexistence element with SCAN_EXEMPTION_REQUEST bit = 1 */
  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_GRANTED,             /* sent coexistence element with SCAN_EXEMPTION_GRANT bit = 1 */
  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED,     /* sent coexistence element with SCAN_EXEMPTION_GRANT bit = 0 */
  MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_20_TO_40,                    /* switch channel message sent to FW (20MHz to 40MHz) */
  MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_40_TO_20,                    /* switch channel message sent to FW (40MHz to 20MHz) */
  MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_40_TO_40,                    /* switch channel message sent to FW (40MHz to 40MHz) */

  MTLK_WWSS_WLAN_STAT_ID_AGGR_ACTIVE,
  MTLK_WWSS_WLAN_STAT_ID_REORD_ACTIVE,
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_REQ_RECEIVED,
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_CFMD_FAIL,
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_CFMD_SUCCESS,
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_LOST,
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_NEGATIVE_SENT,
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_NOT_CFMD,
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_POSITIVE_SENT,
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_REACHED,
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_RETRANSMISSIONS,
  MTLK_WWSS_WLAN_STAT_ID_RX_BAR_WITHOUT_REORDERING,
  MTLK_WWSS_WLAN_STAT_ID_RX_AGGR_PKT_WITHOUT_REORDERING,
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_CFMD_FAIL,
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_CFMD_SUCCESS,
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_LOST,
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_NOT_CFMD,
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_RCV,
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_REACHED,
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_RETRANSMISSIONS,
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_SENT,
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_SENT_BY_TIMEOUT,
  MTLK_WWSS_WLAN_STAT_ID_TX_ACK_ON_BAR_DETECTED,
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_CFMD_FAIL,
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_CFMD_SUCCESS,
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_NOT_CFMD,
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_SENT,
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_RCV_NEGATIVE,
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_RCV_POSITIVE,
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_TIMEOUT,
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_CFMD_FAIL,
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_CFMD_SUCCESS,
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_NOT_CFMD,
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_SENT,
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_CFMD_FAIL,
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_CFMD_SUCCESS,
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_LOST,
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_NOT_CFMD,
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_RCV,
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_REACHED,
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_RETRANSMISSIONS,
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_SENT,
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_CFMD_FAIL,
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_CFMD_SUCCESS,
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_NOT_CFMD,
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_SENT,

  MTLK_WWSS_WLAN_STAT_ID_NOF_FAST_RCVRY_PROCESSED,
  MTLK_WWSS_WLAN_STAT_ID_NOF_FULL_RCVRY_PROCESSED,
  MTLK_WWSS_WLAN_STAT_ID_NOF_FAST_RCVRY_FAILED,
  MTLK_WWSS_WLAN_STAT_ID_NOF_FULL_RCVRY_FAILED,
  MTLK_WWSS_WLAN_STAT_ID_LAST
} mtlk_wss_stat_id_e;

#define MTLK_ASSERT_STAT_ID(id)                         \
  do {                                                  \
    MTLK_ASSERT((id) >= MTLK_WWSS_WLAN_STAT_ID_FIRST); \
    MTLK_ASSERT((id) < MTLK_WWSS_WLAN_STAT_ID_LAST);   \
  } while (0)

/*** HOT PATH group ***/

/* Allways allowed */
#define MTLK_WWSS_WLAN_STAT_ID_BYTES_SENT_ALLOWED                                   MTLK_WWSS_WLAN_STAT_GROUP_ALLWAYS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_BYTES_RECEIVED_ALLOWED                               MTLK_WWSS_WLAN_STAT_GROUP_ALLWAYS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_PACKETS_SENT_ALLOWED                                 MTLK_WWSS_WLAN_STAT_GROUP_ALLWAYS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_PACKETS_RECEIVED_ALLOWED                             MTLK_WWSS_WLAN_STAT_GROUP_ALLWAYS_ALLOWED

/* HotPath minimal */
#define MTLK_WWSS_WLAN_STAT_ID_UNICAST_PACKETS_SENT_ALLOWED                         MTLK_WWSS_WLAN_STAT_GROUP_ALLWAYS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_UNICAST_PACKETS_RECEIVED_ALLOWED                     MTLK_WWSS_WLAN_STAT_GROUP_ALLWAYS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MULTICAST_PACKETS_SENT_ALLOWED                       MTLK_WWSS_WLAN_STAT_GROUP_ALLWAYS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MULTICAST_PACKETS_RECEIVED_ALLOWED                   MTLK_WWSS_WLAN_STAT_GROUP_ALLWAYS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_BROADCAST_PACKETS_SENT_ALLOWED                       MTLK_WWSS_WLAN_STAT_GROUP_ALLWAYS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_BROADCAST_PACKETS_RECEIVED_ALLOWED                   MTLK_WWSS_WLAN_STAT_GROUP_ALLWAYS_ALLOWED

/* HotPath optional */
#define MTLK_WWSS_WLAN_STAT_ID_SQ_DPCS_SCHEDULED_ALLOWED                            MTLK_WWSS_WLAN_STAT_GROUP_HOTP_OPT_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_SQ_DPCS_ARRIVED_ALLOWED                              MTLK_WWSS_WLAN_STAT_GROUP_HOTP_OPT_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_DAT_FRAMES_RECEIVED_ALLOWED                          MTLK_WWSS_WLAN_STAT_GROUP_HOTP_OPT_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_CTL_FRAMES_RECEIVED_ALLOWED                          MTLK_WWSS_WLAN_STAT_GROUP_HOTP_OPT_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MAN_FRAMES_RECEIVED_ALLOWED                          MTLK_WWSS_WLAN_STAT_GROUP_HOTP_OPT_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MULTICAST_BYTES_SENT_ALLOWED                         MTLK_WWSS_WLAN_STAT_GROUP_HOTP_OPT_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MULTICAST_BYTES_RECEIVED_ALLOWED                     MTLK_WWSS_WLAN_STAT_GROUP_HOTP_OPT_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_BROADCAST_BYTES_SENT_ALLOWED                         MTLK_WWSS_WLAN_STAT_GROUP_HOTP_OPT_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_BROADCAST_BYTES_RECEIVED_ALLOWED                     MTLK_WWSS_WLAN_STAT_GROUP_HOTP_OPT_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_NACKed_PACKETS_SENT_ALLOWED                          MTLK_WWSS_WLAN_STAT_GROUP_HOTP_OPT_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_NO_PEERS_ALLOWED            MTLK_WWSS_WLAN_STAT_GROUP_HOTP_OPT_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_802_1X_PACKETS_SENT_ALLOWED                          MTLK_WWSS_WLAN_STAT_GROUP_HOTP_OPT_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_802_1X_PACKETS_RECEIVED_ALLOWED                      MTLK_WWSS_WLAN_STAT_GROUP_HOTP_OPT_ALLOWED

/* Aggregation group */
#define MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_REQ_RECEIVED_ALLOWED                        MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_CFMD_FAIL_ALLOWED                       MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_CFMD_SUCCESS_ALLOWED                    MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_LOST_ALLOWED                            MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_NEGATIVE_SENT_ALLOWED                   MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_NOT_CFMD_ALLOWED                        MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_POSITIVE_SENT_ALLOWED                   MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_REACHED_ALLOWED                         MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_RETRANSMISSIONS_ALLOWED                 MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_BAR_WITHOUT_REORDERING_ALLOWED                    MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_AGGR_PKT_WITHOUT_REORDERING_ALLOWED               MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_CFMD_FAIL_ALLOWED                       MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_CFMD_SUCCESS_ALLOWED                    MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_LOST_ALLOWED                            MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_NOT_CFMD_ALLOWED                        MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_RCV_ALLOWED                             MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_REACHED_ALLOWED                         MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_RETRANSMISSIONS_ALLOWED                 MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_SENT_ALLOWED                            MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_SENT_BY_TIMEOUT_ALLOWED                     MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_ACK_ON_BAR_DETECTED_ALLOWED                       MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_CFMD_FAIL_ALLOWED                       MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_CFMD_SUCCESS_ALLOWED                    MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_NOT_CFMD_ALLOWED                        MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_SENT_ALLOWED                            MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_RCV_NEGATIVE_ALLOWED                    MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_RCV_POSITIVE_ALLOWED                    MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_TIMEOUT_ALLOWED                         MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_CFMD_FAIL_ALLOWED                      MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_CFMD_SUCCESS_ALLOWED                   MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_NOT_CFMD_ALLOWED                       MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_SENT_ALLOWED                           MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_CFMD_FAIL_ALLOWED                       MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_CFMD_SUCCESS_ALLOWED                    MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_LOST_ALLOWED                            MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_NOT_CFMD_ALLOWED                        MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_RCV_ALLOWED                             MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_REACHED_ALLOWED                         MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_RETRANSMISSIONS_ALLOWED                 MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_SENT_ALLOWED                            MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_CFMD_FAIL_ALLOWED                       MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_CFMD_SUCCESS_ALLOWED                    MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_NOT_CFMD_ALLOWED                        MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_SENT_ALLOWED                            MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_AGGR_ACTIVE_ALLOWED                                  MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_REORD_ACTIVE_ALLOWED                                 MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED

/*** HW_SOURCE group ***/
#define MTLK_WWSS_WLAN_STAT_ID_FW_LOGGER_PACKETS_PROCESSED_ALLOWED                  MTLK_WWSS_WLAN_STAT_GROUP_HW_SOURCE_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_FW_LOGGER_PACKETS_DROPPED_ALLOWED                    MTLK_WWSS_WLAN_STAT_GROUP_HW_SOURCE_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_ISRS_TOTAL_ALLOWED                                   MTLK_WWSS_WLAN_STAT_GROUP_HW_SOURCE_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_ISRS_FOREIGN_ALLOWED                                 MTLK_WWSS_WLAN_STAT_GROUP_HW_SOURCE_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_ISRS_NOT_PENDING_ALLOWED                             MTLK_WWSS_WLAN_STAT_GROUP_HW_SOURCE_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_ISRS_HALTED_ALLOWED                                  MTLK_WWSS_WLAN_STAT_GROUP_HW_SOURCE_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_ISRS_INIT_ALLOWED                                    MTLK_WWSS_WLAN_STAT_GROUP_HW_SOURCE_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_ISRS_TO_DPC_ALLOWED                                  MTLK_WWSS_WLAN_STAT_GROUP_HW_SOURCE_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_ISRS_UNKNOWN_ALLOWED                                 MTLK_WWSS_WLAN_STAT_GROUP_HW_SOURCE_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_POST_ISR_DPCS_ALLOWED                                MTLK_WWSS_WLAN_STAT_GROUP_HW_SOURCE_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_FW_MSGS_HANDLED_ALLOWED                              MTLK_WWSS_WLAN_STAT_GROUP_HW_SOURCE_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_BUF_ALLOC_FAILED_ALLOWED                          MTLK_WWSS_WLAN_STAT_GROUP_HW_SOURCE_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_BUF_REALLOC_FAILED_ALLOWED                        MTLK_WWSS_WLAN_STAT_GROUP_HW_SOURCE_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_BUF_REALLOCATED_ALLOWED                           MTLK_WWSS_WLAN_STAT_GROUP_HW_SOURCE_ALLOWED

/*** Others group ***/
#define MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_ACM_ALLOWED                 MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_EAPOL_CLONED_ALLOWED        MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_DIRECTED_ALLOWED    MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_MCAST_ALLOWED       MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_NO_RESOURCES_ALLOWED        MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_SQ_OVERFLOW_ALLOWED         MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_EAPOL_FILTER_ALLOWED        MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_DROP_ALL_FILTER_ALLOWED     MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_TX_QUEUE_OVERFLOW_ALLOWED   MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_FOREIGN_ALLOWED             MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_LOOPBACK_ALLOWED            MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_TOO_OLD_ALLOWED             MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_DUPLICATE_ALLOWED           MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_NO_RESOURCES_ALLOWED        MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_FW_ALLOWED                      MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_FW_ALLOWED                      MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_PAIRWISE_MIC_FAILURE_PACKETS_ALLOWED                 MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_GROUP_MIC_FAILURE_PACKETS_ALLOWED                    MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_UNICAST_REPLAYED_PACKETS_ALLOWED                     MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MULTICAST_REPLAYED_PACKETS_ALLOWED                   MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RETRY_COUNT_ALLOWED                                  MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MULTIPLE_RETRY_COUNT_ALLOWED                         MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_FWD_RX_PACKETS_ALLOWED                               MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_FWD_RX_BYTES_ALLOWED                                 MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_BAR_FRAMES_COUNT_ALLOWED                             MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED

#define MTLK_WWSS_WLAN_STAT_ID_MSDU_RECEIVED_AC_BE_ALLOWED                          MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MSDU_RECEIVED_AC_BK_ALLOWED                          MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MSDU_RECEIVED_AC_VI_ALLOWED                          MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MSDU_RECEIVED_AC_VO_ALLOWED                          MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MSDU_TRANSMITTED_AC_BE_ALLOWED                       MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MSDU_TRANSMITTED_AC_BK_ALLOWED                       MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MSDU_TRANSMITTED_AC_VI_ALLOWED                       MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MSDU_TRANSMITTED_AC_VO_ALLOWED                       MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MSDU_USED_AC_BE_ALLOWED                              MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MSDU_USED_AC_BK_ALLOWED                              MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MSDU_USED_AC_VI_ALLOWED                              MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MSDU_USED_AC_VO_ALLOWED                              MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED

#define MTLK_WWSS_WLAN_STAT_ID_MAC_STATS_ALLOWED                                    MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_SEND_QUEUE_STATS_ALLOWED                             MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_REORDERING_STATS_ALLOWED                             MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_LAST_DATA_UPLINK_RATE_ALLOWED                        MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_LAST_DATA_DOWNLINK_RATE_ALLOWED                      MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_SIGNAL_STRENGTH_SHORT_ALLOWED                        MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_SIGNAL_STRENGTH_HISTORY_ALLOWED                      MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_DISCARDED_TX_PACKETS_FW_ALLOWED                      MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_LAST_ACTIVITY_TIMESTAMP_ALLOWED                      MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_THROUGHPUT_SHORT_ALLOWED                          MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_THROUGHPUT_HISTORY_ALLOWED                        MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_THROUGHPUT_SHORT_ALLOWED                          MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RX_THROUGHPUT_HISTORY_ALLOWED                        MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED

#define MTLK_WWSS_WLAN_STAT_ID_PS_MODE_ENTRANCES_ALLOWED                            MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED

#define MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_RECEIVED_ALLOWED                         MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_REQUESTED_ALLOWED         MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_GRANTED_ALLOWED           MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED_ALLOWED   MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_20_TO_40_ALLOWED                  MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_40_TO_20_ALLOWED                  MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_40_TO_40_ALLOWED                  MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED

#define MTLK_WWSS_WLAN_STAT_ID_NOF_FAST_RCVRY_PROCESSED_ALLOWED                     MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_NOF_FULL_RCVRY_PROCESSED_ALLOWED                     MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_NOF_FAST_RCVRY_FAILED_ALLOWED                        MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_NOF_FULL_RCVRY_FAILED_ALLOWED                        MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED

#define MTLK_WWSS_WLAN_STAT_ID_FCS_ERROR_COUNT_ALLOWED                              MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RTS_SUCCESS_COUNT_ALLOWED                            MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_RTS_FAILURE_COUNT_ALLOWED                            MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_FREE_TX_MSDUs_ALLOWED                                MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_TX_MSDUs_USAGE_PEAK_ALLOWED                          MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_BIST_CHECK_PASSED_ALLOWED                            MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MAN_MSG_SENT_ALLOWED                                 MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MAN_MSG_CONFIRMED_ALLOWED                            MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_MAN_MSG_PEAK_ALLOWED                                 MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_DBG_MSG_SENT_ALLOWED                                 MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_DBG_MSG_CONFIRMED_ALLOWED                            MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED
#define MTLK_WWSS_WLAN_STAT_ID_DBG_MSG_PEAK_ALLOWED                                 MTLK_WWSS_WLAN_STAT_GROUP_OTHERS_ALLOWED

#endif /* __MTLK_WSS_ID_H__ */


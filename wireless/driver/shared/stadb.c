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
* Written by: Dmitry Fleytman
*
*/
#include "mtlkinc.h"

#include "rod.h"
#include "core.h"
#include "stadb.h"
#include "mtlk_osal.h"
#include "core.h"
#include "mtlk_coreui.h"
#include "sq.h"
#include "mtlk_param_db.h"
#include "mtlkwlanirbdefs.h"
#include "mtlk_snprintf.h"
#include "mtlk_wssd.h"
#include "mtlk_df_nbuf.h"
#include "mtlk_df.h"

#define LOG_LOCAL_GID   GID_STADB
#define LOG_LOCAL_FID   1

#define STADB_FLAGS_STOPPED      0x20000000

struct eq_evt_aggr_closed_data
{
  IEEE_ADDR sta_addr;
  uint16    prior_class;
};

static int __MTLK_IFUNC
_mtlk_stadb_on_addba_revive_aggregation_tid(mtlk_handle_t sta_db_object, const void *payload, uint32 size);

static void __MTLK_IFUNC
_mtlk_sta_ps_mode_cleanup(mtlk_sq_peer_ctx_t *ppeer, mtlk_vap_handle_t vap_handle);

/******************************************************************************************
 * STA API
 ******************************************************************************************/
// Timeout change adaptation interval
#define DEFAULT_KEEPALIVE_TIMEOUT   1000 /* ms */
#define DEFAULT_AGGR_OPEN_THRESHOLD 5    /* packets*/
#define HOST_INACTIVE_TIMEOUT       1800 /* sec */

static const uint32 _mtlk_sta_wss_id_map[] = 
{
  MTLK_WWSS_WLAN_STAT_ID_BYTES_SENT,                                   /* MTLK_STAI_CNT_TX_BYTES_SENT */
  MTLK_WWSS_WLAN_STAT_ID_BYTES_RECEIVED,                               /* MTLK_STAI_CNT_BYTES_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_PACKETS_SENT,                                 /* MTLK_STAI_CNT_PACKETS_SENT */
  MTLK_WWSS_WLAN_STAT_ID_PACKETS_RECEIVED,                             /* MTLK_STAI_CNT_PACKETS_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_UNICAST_PACKETS_SENT,                         /* MTLK_STAI_CNT_UNICAST_PACKETS_SENT */
  MTLK_WWSS_WLAN_STAT_ID_UNICAST_PACKETS_RECEIVED,                     /* MTLK_STAI_CNT_UNICAST_PACKETS_RECEIVED */

#if MTLK_MTIDL_PEER_STAT_FULL
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_FW,                      /* MTLK_STAI_CNT_TX_PACKETS_DISCARDED_FW */
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_TOO_OLD,             /* MTLK_STAI_CNT_RX_PACKETS_DISCARDED_DRV_TOO_OLD */
  MTLK_WWSS_WLAN_STAT_ID_RX_PACKETS_DISCARDED_DRV_DUPLICATE,           /* MTLK_STAI_CNT_RX_PACKETS_DISCARDED_DRV_DUPLICATE */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_NO_RESOURCES,        /* MTLK_STAI_CNT_TX_PACKETS_DISCARDED_DRV_NO_RESOURCES */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_SQ_OVERFLOW,         /* MTLK_STAI_CNT_TX_PACKETS_DISCARDED_SQ_OVERFLOW */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_EAPOL_FILTER,        /* MTLK_STAI_CNT_TX_PACKETS_DISCARDED_EAPOL_FILTER */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_DROP_ALL_FILTER,     /* MTLK_STAI_CNT_TX_PACKETS_DISCARDED_DROP_ALL_FILTER */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_TX_QUEUE_OVERFLOW,   /* MTLK_STAI_CNT_TX_PACKETS_DISCARDED_TX_QUEUE_OVERFLOW */

  MTLK_WWSS_WLAN_STAT_ID_802_1X_PACKETS_RECEIVED,                      /* MTLK_STAI_CNT_802_1X_PACKETS_RECEIVED                                         */
  MTLK_WWSS_WLAN_STAT_ID_802_1X_PACKETS_SENT,                          /* MTLK_STAI_CNT_802_1X_PACKETS_SENT                                             */

  MTLK_WWSS_WLAN_STAT_ID_FWD_RX_PACKETS,                               /* MTLK_STAI_CNT_FWD_RX_PACKETS */
  MTLK_WWSS_WLAN_STAT_ID_FWD_RX_BYTES,                                 /* MTLK_STAI_CNT_FWD_RX_BYTES */
  MTLK_WWSS_WLAN_STAT_ID_PS_MODE_ENTRANCES,                            /* MTLK_STAI_CNT_PS_MODE_ENTRANCES */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_ACM,                 /* MTLK_STAI_CNT_TX_PACKETS_DISCARDED_ACM */
  MTLK_WWSS_WLAN_STAT_ID_TX_PACKETS_DISCARDED_DRV_EAPOL_CLONED,        /* MTLK_STAI_CNT_TX_PACKETS_DISCARDED_EAPOL_CLONED */
  MTLK_WWSS_WLAN_STAT_ID_AGGR_ACTIVE,                                  /* MTLK_STAI_CNT_AGGR_ACTIVE */
  MTLK_WWSS_WLAN_STAT_ID_REORD_ACTIVE,                                 /* MTLK_STAI_CNT_REORD_ACTIVE */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_REQ_RECEIVED,         /* MTLK_STAI_CNT_RX_ADDBA_REQ_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_CFMD_FAIL,        /* MTLK_STAI_CNT_RX_ADDBA_RES_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_CFMD_SUCCESS,     /* MTLK_STAI_CNT_RX_ADDBA_RES_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_LOST,             /* MTLK_STAI_CNT_RX_ADDBA_RES_LOST */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_NEGATIVE_SENT,    /* MTLK_STAI_CNT_RX_ADDBA_RES_NEGATIVE_SENT */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_NOT_CFMD,         /* MTLK_STAI_CNT_RX_ADDBA_RES_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_POSITIVE_SENT,    /* MTLK_STAI_CNT_RX_ADDBA_RES_POSITIVE_SENT */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_REACHED,          /* MTLK_STAI_CNT_RX_ADDBA_RES_REACHED */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_RETRANSMISSIONS,  /* MTLK_STAI_CNT_RX_ADDBA_RES_RETRANSMISSIONS */
  MTLK_WWSS_WLAN_STAT_ID_RX_BAR_WITHOUT_REORDERING,     /* MTLK_STAI_CNT_RX_BAR_WITHOUT_REORDERING */
  MTLK_WWSS_WLAN_STAT_ID_RX_AGGR_PKT_WITHOUT_REORDERING, /* MTLK_STAI_CNT_RX_AGGR_PKT_WITHOUT_REORDERING */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_CFMD_FAIL,        /* MTLK_STAI_CNT_RX_DELBA_REQ_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_CFMD_SUCCESS,     /* MTLK_STAI_CNT_RX_DELBA_REQ_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_LOST,             /* MTLK_STAI_CNT_RX_DELBA_REQ_LOST */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_NOT_CFMD,         /* MTLK_STAI_CNT_RX_DELBA_REQ_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_RCV,              /* MTLK_STAI_CNT_RX_DELBA_REQ_RCV */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_REACHED,          /* MTLK_STAI_CNT_RX_DELBA_REQ_REACHED */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_RETRANSMISSIONS,  /* MTLK_STAI_CNT_RX_DELBA_REQ_RETRANSMISSIONS */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_SENT,             /* MTLK_STAI_CNT_RX_DELBA_REQ_SENT */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_SENT_BY_TIMEOUT,      /* MTLK_STAI_CNT_RX_DELBA_SENT_BY_TIMEOUT */
  MTLK_WWSS_WLAN_STAT_ID_TX_ACK_ON_BAR_DETECTED,        /* MTLK_STAI_CNT_TX_ACK_ON_BAR_DETECTED */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_CFMD_FAIL,        /* MTLK_STAI_CNT_TX_ADDBA_REQ_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_CFMD_SUCCESS,     /* MTLK_STAI_CNT_TX_ADDBA_REQ_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_NOT_CFMD,         /* MTLK_STAI_CNT_TX_ADDBA_REQ_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_SENT,             /* MTLK_STAI_CNT_TX_ADDBA_REQ_SENT */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_RCV_NEGATIVE,     /* MTLK_STAI_CNT_TX_ADDBA_RES_RCV_NEGATIVE */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_RCV_POSITIVE,     /* MTLK_STAI_CNT_TX_ADDBA_RES_RCV_POSITIVE */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_TIMEOUT,          /* MTLK_STAI_CNT_TX_ADDBA_RES_TIMEOUT */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_CFMD_FAIL,       /* MTLK_STAI_CNT_TX_CLOSE_AGGR_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_CFMD_SUCCESS,    /* MTLK_STAI_CNT_TX_CLOSE_AGGR_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_NOT_CFMD,        /* MTLK_STAI_CNT_TX_CLOSE_AGGR_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_SENT,            /* MTLK_STAI_CNT_TX_CLOSE_AGGR_SENT */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_CFMD_FAIL,        /* MTLK_STAI_CNT_TX_DELBA_REQ_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_CFMD_SUCCESS,     /* MTLK_STAI_CNT_TX_DELBA_REQ_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_LOST,             /* MTLK_STAI_CNT_TX_DELBA_REQ_LOST */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_NOT_CFMD,         /* MTLK_STAI_CNT_TX_DELBA_REQ_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_RCV,              /* MTLK_STAI_CNT_TX_DELBA_REQ_RCV */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_REACHED,          /* MTLK_STAI_CNT_TX_DELBA_REQ_REACHED */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_RETRANSMISSIONS,  /* MTLK_STAI_CNT_TX_DELBA_REQ_RETRANSMISSIONS */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_SENT,             /* MTLK_STAI_CNT_TX_DELBA_REQ_SENT */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_CFMD_FAIL,        /* MTLK_STAI_CNT_TX_OPEN_AGGR_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_CFMD_SUCCESS,     /* MTLK_STAI_CNT_TX_OPEN_AGGR_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_NOT_CFMD,         /* MTLK_STAI_CNT_TX_OPEN_AGGR_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_SENT,             /* MTLK_STAI_CNT_TX_OPEN_AGGR_SENT */
#endif /* MTLK_MTIDL_PEER_STAT_FULL */
};

uint16 __MTLK_IFUNC
mtlk_sta_get_rx_rate (const sta_entry *sta)
{
  uint16 res;

  MTLK_ASSERT(sta != NULL);

  res = mtlk_hw_rate_params_to_rate(sta->info.rx_rate, sta->info.scp);

  MTLK_ASSERT(res != MTLK_HW_RATE_INVALID);

  return res;
}

// Timers
static uint32 __MTLK_IFUNC
_mtlk_sta_keepalive_tmr (mtlk_osal_timer_t *timer, 
                         mtlk_handle_t      clb_usr_data)
{
  sta_entry *sta = HANDLE_T_PTR(sta_entry, clb_usr_data);

  mtlk_osal_timestamp_diff_t diff;
  mtlk_osal_msec_t msecs, timeout = sta->paramdb->keepalive_interval;

  if (timeout == 0)
    return DEFAULT_KEEPALIVE_TIMEOUT; /* to adopt possible timeout change */

  diff  = mtlk_osal_timestamp_diff(mtlk_osal_timestamp(), sta->timestamp);
  msecs = mtlk_osal_timestamp_to_ms(diff);

  if (msecs >= timeout) {
    sta->paramdb->cfg.api.on_sta_keepalive(sta->paramdb->cfg.api.usr_data, sta);
    return timeout; /* restart with same timeout */
  } else {
    return (timeout - msecs); /* restart with adjusted timeout */
  }
}

static uint32 __MTLK_IFUNC
mtlk_sta_data_disconnect_tmr (mtlk_osal_timer_t *timer, 
                              mtlk_handle_t      clb_usr_data)
{
  sta_entry *sta = HANDLE_T_PTR(sta_entry, clb_usr_data);

  mtlk_osal_timestamp_diff_t diff;
  mtlk_osal_msec_t msecs, timeout = sta->paramdb->sta_keepalive_timeout;

  if (timeout == 0)
    return DEFAULT_KEEPALIVE_TIMEOUT; /* to adopt possible timeout change */

  if (!sta->paramdb->cfg.api.sta_inactivity_on(sta->paramdb->cfg.api.usr_data, sta)) {
    sta->timestamp = mtlk_osal_timestamp();
    ILOG3_YD("IDLE_TIMER: Update STA %Y - %lu", mtlk_sta_get_addr(sta), sta->timestamp);
    return timeout; /* restart with same timeout */
  }

  diff  = mtlk_osal_timestamp_diff(mtlk_osal_timestamp(), sta->timestamp);
  msecs = mtlk_osal_timestamp_to_ms(diff);

  if (msecs >= timeout) {
    sta->paramdb->cfg.api.on_sta_inactive(sta->paramdb->cfg.api.usr_data, sta);
    return 0; /* do not restart timer */
  } else {
    return (timeout - msecs); /* restart with adjusted timeout */
  }
}

static void
_mtlk_sta_destroy_rod_queue (sta_entry *sta, unsigned int prio_id)
{
  ASSERT(sta != NULL);
  ASSERT(prio_id < NTS_TIDS);

  if (mtlk_is_used_rod_queue(&sta->rod_queue[prio_id])) {
    ILOG3_YD("Destroying TS for STA %Y priority %d",
          mtlk_sta_get_addr(sta), prio_id);
    mtlk_clear_rod_queue(&sta->rod_queue[prio_id]);
  } else {
    ILOG3_YD("TS for STA %Y priority %d is not used",
      mtlk_sta_get_addr(sta), prio_id);
  }
}

static void
_mtlk_sta_create_rod_queue (sta_entry *sta, int prio_id, int win_size, int ssn)
{
  ASSERT(sta != NULL);
  if (sta == NULL)
    return;

  ASSERT(prio_id < NTS_TIDS);
  if (prio_id >= NTS_TIDS)
    return;

  ILOG3_YDDD("Creating TS for STA %Y priority %d: window = %d SSN = %d",
    mtlk_sta_get_addr(sta), prio_id, win_size, ssn);

  if (MTLK_ERR_OK != mtlk_create_rod_queue(&sta->rod_queue[prio_id], sta->vap_handle, sta->wss, win_size, ssn)) {
    return;
  }
}

static void
_mtlk_sta_flush (sta_entry *sta)
{
  int prio_id;

  for (prio_id = 0; prio_id < ARRAY_SIZE(sta->rod_queue); prio_id++) {
    mtlk_handle_rod_queue_timer(&sta->rod_queue[prio_id]);
  }
}

static void
_mtlk_sta_reset_tx_cntrs (sta_entry *sta)
{
  int prio_id;

  for (prio_id = 0; prio_id < ARRAY_SIZE(sta->tx_packets); prio_id++) {
    mtlk_osal_atomic_set(&sta->tx_packets[prio_id], 0);
  }
}

static void
_mtlk_sta_reset_aggr_inited (sta_entry *sta)
{
  int prio_id;

  for (prio_id = 0; prio_id < ARRAY_SIZE(sta->aggr_inited); prio_id++) {
    sta->aggr_inited[prio_id] = FALSE;
  }
}

static void
_mtlk_sta_reset_cnts (sta_entry *sta)
{
  int i = 0;
  for (; i < MTLK_STAI_CNT_LAST; i++) {
    mtlk_wss_reset_stat(sta->wss, i);
  }
}

static void
_mtlk_sta_set_packets_filter_default (sta_entry *sta)
{
  sta->info.filter = MTLK_PCKT_FLTR_ALLOW_802_1X;
  _mtlk_sta_reset_tx_cntrs(sta);
}

static void __MTLK_IFUNC
_mtlk_sta_schedule_aggregation_revival_tid (sta_entry *sta,
                                            uint16     prior_class)
{
  int                            res;
  struct eq_evt_aggr_closed_data data;

  memset(&data, 0, sizeof(data));

  MTLK_ASSERT(sta != NULL);

  data.sta_addr    = *mtlk_sta_get_addr(sta);
  data.prior_class =  prior_class;

  res = mtlk_core_schedule_internal_task(mtlk_vap_get_core(sta->vap_handle),
                                         HANDLE_T(sta->paramdb),
                                         _mtlk_stadb_on_addba_revive_aggregation_tid,
                                         &data, sizeof(data));
  if (res != MTLK_ERR_OK) {
    ELOG_DD("Can't schedule revival (TID=%d, err=%d)", prior_class, res);
  }
}

/***********************
* ADDBA module wrapper *
***********************/
static uint32 __MTLK_IFUNC 
_mtlk_sta_get_addba_last_rx_timestamp (mtlk_handle_t usr_data, 
                                       uint16        tid)
{
  sta_entry *sta = HANDLE_T_PTR(sta_entry, usr_data);

  MTLK_ASSERT(tid < ARRAY_SIZE(sta->rod_queue));

  return mtlk_rod_queue_get_last_rx_time(&sta->rod_queue[tid]);
}

static void __MTLK_IFUNC
_mtlk_sta_addba_do_nothing (mtlk_handle_t usr_data,
                            uint16        prior_class)
{
  sta_entry *sta = HANDLE_T_PTR(sta_entry, usr_data);

  MTLK_UNREFERENCED_PARAM(prior_class);
  MTLK_UNREFERENCED_PARAM(sta);
}

static void __MTLK_IFUNC
_mtlk_sta_addba_revive_aggregation_tid (mtlk_handle_t usr_data,
                                        uint16        prior_class)
{
  _mtlk_sta_schedule_aggregation_revival_tid(HANDLE_T_PTR(sta_entry, usr_data), prior_class);
}

static void __MTLK_IFUNC
_mtlk_sta_addba_restart_tx_count_tid (mtlk_handle_t usr_data,
                                      uint16        prior_class)
{
  sta_entry *sta = HANDLE_T_PTR(sta_entry, usr_data);

  MTLK_ASSERT(sta->aggr_inited[prior_class] == TRUE);

  ILOG0_DYD("CID-%04x: STA:%Y TID=%d: Re-starting TX counter",
            mtlk_vap_get_oid(sta->vap_handle),
            mtlk_sta_get_addr(sta), prior_class);

  mtlk_osal_atomic_set(&sta->tx_packets[prior_class], 0);

  sta->aggr_inited[prior_class] = FALSE;
}

static void __MTLK_IFUNC
_mtlk_sta_addba_exit_ps (sta_entry *sta)
{
  int prio_id;

  for (prio_id = 0; prio_id < ARRAY_SIZE(sta->aggr_inited); prio_id++) {
    if (sta->aggr_inited[prio_id] == TRUE)
    {
       if(mtlk_addba_peer_reset_power_save(&sta->addba_peer, prio_id))
       {
          mtlk_osal_atomic_set(&sta->tx_packets[prio_id], 0);
          sta->aggr_inited[prio_id] = FALSE;
       }
    }
  }
}


static void __MTLK_IFUNC
_mtlk_sta_addba_start_reordering (mtlk_handle_t usr_data,
                                  uint16        prior_class,
                                  uint16        ssn,
                                  uint8         win_size)
{
  sta_entry *sta = HANDLE_T_PTR(sta_entry, usr_data);

  ILOG2_V("Starting reordering");
  _mtlk_sta_create_rod_queue(sta, prior_class, win_size, ssn);
}

static void __MTLK_IFUNC 
_mtlk_sta_addba_stop_reordering (mtlk_handle_t usr_data,
                                 uint16        prior_class)
{
  sta_entry *sta = HANDLE_T_PTR(sta_entry, usr_data);

  ILOG2_V("Finishing reordering");

  _mtlk_sta_destroy_rod_queue(sta, prior_class);
}

/******************************************************************************************/

static void
_mtlk_sta_get_peer_stats(const sta_entry* sta, mtlk_wssa_drv_peer_stats_t* stats)
{
  int i;

  /* minimal statistic level */
  stats->TxBytesSucceeded                     = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_BYTES_SENT);
  stats->RxBytesSucceeded                     = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_BYTES_RECEIVED);
  stats->TxPacketsSucceeded                   = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_PACKETS_SENT);
  stats->RxPacketsSucceeded                   = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_PACKETS_RECEIVED);
  stats->UnicastPacketsSent                   = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_UNICAST_PACKETS_SENT);
  stats->UnicastPacketsReceived               = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_UNICAST_PACKETS_RECEIVED);

  stats->LastUplinkRate                       = mtlk_sta_get_tx_rate(sta);
  stats->LastDownlinkRate                     = mtlk_sta_get_rx_rate(sta);
  for (i = 0; i < NUMBER_OF_RX_ANTENNAS; ++i) {
    stats->ShortTermRSSIAverage[i] = MTLK_NORMALIZED_RSSI(mtlk_sta_get_short_term_rssi(sta, i));
  }
}

#if MTLK_MTIDL_PEER_STAT_FULL
static void
_mtlk_sta_get_debug_peer_stats(const sta_entry* sta, mtlk_wssa_drv_debug_peer_stats_t* stats)
{
  int i;

  /* minimal statistic level */
  stats->TxBytesSucceeded                     = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_BYTES_SENT);
  stats->RxBytesSucceeded                     = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_BYTES_RECEIVED);
  stats->TxPacketsSucceeded                   = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_PACKETS_SENT);
  stats->RxPacketsSucceeded                   = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_PACKETS_RECEIVED);
  stats->UnicastPacketsSent                   = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_UNICAST_PACKETS_SENT);
  stats->UnicastPacketsReceived               = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_UNICAST_PACKETS_RECEIVED);

  stats->LastUplinkRate                       = mtlk_sta_get_tx_rate(sta);
  stats->LastDownlinkRate                     = mtlk_sta_get_rx_rate(sta);
  for (i = 0; i < NUMBER_OF_RX_ANTENNAS; ++i) {
    stats->ShortTermRSSIAverage[i] = MTLK_NORMALIZED_RSSI(mtlk_sta_get_short_term_rssi(sta, i));
    stats->LongTermRSSIAverage[i] = MTLK_NORMALIZED_RSSI(mtlk_sta_get_long_term_rssi(sta, i));
  }

  /* add FULL statistic level */
  stats->RxPackets802_1x                      = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_802_1X_PACKETS_RECEIVED);
  stats->TxPackets802_1x                      = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_802_1X_PACKETS_SENT);
  stats->DiscardedTxPacketsFw                 = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_PACKETS_DISCARDED_FW);
  stats->TxPacketsDiscardedACM                = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_PACKETS_DISCARDED_ACM);
  stats->TxPacketsDiscardedEAPOLCloned        = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_PACKETS_DISCARDED_EAPOL_CLONED);
  stats->RxPacketsDiscardedDrvTooOld          = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_PACKETS_DISCARDED_DRV_TOO_OLD);
  stats->RxPacketsDiscardedDrvDuplicate       = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_PACKETS_DISCARDED_DRV_DUPLICATE);
  stats->TxPacketsDiscardedDrvNoResources     = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_PACKETS_DISCARDED_DRV_NO_RESOURCES);
  stats->TxPacketsDiscardedDrvSQOverflow      = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_PACKETS_DISCARDED_SQ_OVERFLOW);
  stats->TxPacketsDiscardedDrvEAPOLFilter     = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_PACKETS_DISCARDED_EAPOL_FILTER);
  stats->TxPacketsDiscardedDrvDropAllFilter   = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_PACKETS_DISCARDED_DROP_ALL_FILTER);
  stats->TxPacketsDiscardedDrvTXQueueOverflow = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_PACKETS_DISCARDED_TX_QUEUE_OVERFLOW);
  stats->FwdRxPackets                         = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_FWD_RX_PACKETS);
  stats->FwdRxBytes                           = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_FWD_RX_BYTES);
  stats->PSModeEntrances                      = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_PS_MODE_ENTRANCES);

  stats->LastActivityTimestamp                = mtlk_osal_time_passed_ms(sta->timestamp);

  stats->ShortTermTXAverage                   = mtlk_sta_get_short_term_tx(sta);
  stats->LongTermTXAverage                    = mtlk_sta_get_long_term_tx(sta);
  stats->ShortTermRXAverage                   = mtlk_sta_get_short_term_rx(sta);
  stats->LongTermRXAverage                    = mtlk_sta_get_long_term_rx(sta);
  stats->RetransmissionsNumberShort           = mtlk_sta_get_retransmissions_number_short(sta);
}
#endif /* MTLK_MTIDL_PEER_STAT_FULL */

static void
_mtlk_sta_get_peer_capabilities(const sta_entry* sta, mtlk_wssa_drv_peer_capabilities_t* capabilities)
{
  capabilities->NetModesSupported           = sta->info.capabilities.NetModesSupported;
  capabilities->MIMOConfigTX                = sta->info.capabilities.MIMOConfigTX;
  capabilities->MIMOConfigRX                = sta->info.capabilities.MIMOConfigRX;
  capabilities->WMMSupported                = sta->info.capabilities.WMMSupported;
  capabilities->CBSupported                 = sta->info.capabilities.CBSupported;
  capabilities->SGI20Supported              = sta->info.capabilities.SGI20Supported;
  capabilities->SGI40Supported              = sta->info.capabilities.SGI40Supported;
  capabilities->AMPDUMaxLengthExp           = sta->info.capabilities.AMPDUMaxLengthExp;
  capabilities->AMPDUMinStartSpacing        = sta->info.capabilities.AMPDUMinStartSpacing;
  capabilities->STBCSupported               = sta->info.capabilities.STBCSupported;
  capabilities->LDPCSupported               = sta->info.capabilities.LDPCSupported;
  capabilities->Vendor                      = sta->info.capabilities.Vendor;
  capabilities->LQLDPCEnabled               = sta->info.capabilities.LQLDPCEnabled;
  capabilities->BFSupported                 = sta->info.capabilities.BFSupported;

  capabilities->AssociationTimestamp        = mtlk_osal_time_passed_ms(sta->connection_timestamp);
}

void __MTLK_IFUNC
mtlk_sta_get_peer_aggregation (const sta_entry* sta, mtlk_wssa_drv_peer_aggregation_t *aggregation)
{
  MTLK_ASSERT(sta != NULL);
  MTLK_ASSERT(aggregation != NULL);

#if MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
  aggregation->AggrActive                 = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_AGGR_ACTIVE);
  aggregation->ReordActive                = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_REORD_ACTIVE);
  aggregation->RxAddbaReqReceived         = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_ADDBA_REQ_RECEIVED);
  aggregation->RxAddbaResCfmdFail         = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_ADDBA_RES_CFMD_FAIL);
  aggregation->RxAddbaResCfmdSuccess      = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_ADDBA_RES_CFMD_SUCCESS);
  aggregation->RxAddbaResLost             = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_ADDBA_RES_LOST);
  aggregation->RxAddbaResNegativeSent     = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_ADDBA_RES_NEGATIVE_SENT);
  aggregation->RxAddbaResNotCfmd          = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_ADDBA_RES_NOT_CFMD);
  aggregation->RxAddbaResPositiveSent     = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_ADDBA_RES_POSITIVE_SENT);
  aggregation->RxAddbaResReached          = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_ADDBA_RES_REACHED);
  aggregation->RxAddbaResRetransmissions  = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_ADDBA_RES_RETRANSMISSIONS);
  aggregation->RxBarWithoutReordering     = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_BAR_WITHOUT_REORDERING);
  aggregation->RxPktWithoutReordering     = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_AGGR_PKT_WITHOUT_REORDERING);
  aggregation->RxDelbaReqCfmdFail         = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_DELBA_REQ_CFMD_FAIL);
  aggregation->RxDelbaReqCfmdSuccess      = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_DELBA_REQ_CFMD_SUCCESS);
  aggregation->RxDelbaReqLost             = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_DELBA_REQ_LOST);
  aggregation->RxDelbaReqNotCfmd          = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_DELBA_REQ_NOT_CFMD);
  aggregation->RxDelbaReqRcv              = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_DELBA_REQ_RCV);
  aggregation->RxDelbaReqReached          = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_DELBA_REQ_REACHED);
  aggregation->RxDelbaReqRetransmissions  = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_DELBA_REQ_RETRANSMISSIONS);
  aggregation->RxDelbaReqSent             = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_DELBA_REQ_SENT);
  aggregation->RxDelbaSentByTimeout       = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_RX_DELBA_SENT_BY_TIMEOUT);

  aggregation->TxAckOnBarDetected         = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_ACK_ON_BAR_DETECTED);
  aggregation->TxAddbaReqCfmdFail         = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_ADDBA_REQ_CFMD_FAIL);
  aggregation->TxAddbaReqCfmdSuccess      = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_ADDBA_REQ_CFMD_SUCCESS);
  aggregation->TxAddbaReqNotCfmd          = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_ADDBA_REQ_NOT_CFMD);
  aggregation->TxAddbaReqSent             = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_ADDBA_REQ_SENT);
  aggregation->TxAddbaResRcvNegative      = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_ADDBA_RES_RCV_NEGATIVE);
  aggregation->TxAddbaResRcvPositive      = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_ADDBA_RES_RCV_POSITIVE);
  aggregation->TxAddbaResTimeout          = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_ADDBA_RES_TIMEOUT);
  aggregation->TxCloseAggrCfmdFail        = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_CLOSE_AGGR_CFMD_FAIL);
  aggregation->TxCloseAggrCfmdSuccess     = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_CLOSE_AGGR_CFMD_SUCCESS);
  aggregation->TxCloseAggrNotCfmd         = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_CLOSE_AGGR_NOT_CFMD);
  aggregation->TxCloseAggrSent            = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_CLOSE_AGGR_SENT);
  aggregation->TxDelbaReqCfmdFail         = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_DELBA_REQ_CFMD_FAIL);
  aggregation->TxDelbaReqCfmdSuccess      = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_DELBA_REQ_CFMD_SUCCESS);
  aggregation->TxDelbaReqLost             = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_DELBA_REQ_LOST);
  aggregation->TxDelbaReqNotCfmd          = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_DELBA_REQ_NOT_CFMD);
  aggregation->TxDelbaReqRcv              = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_DELBA_REQ_RCV);
  aggregation->TxDelbaReqReached          = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_DELBA_REQ_REACHED);
  aggregation->TxDelbaReqRetransmissions  = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_DELBA_REQ_RETRANSMISSIONS);
  aggregation->TxDelbaReqSent             = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_DELBA_REQ_SENT);
  aggregation->TxOpenAggrCfmdFail         = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_OPEN_AGGR_CFMD_FAIL);
  aggregation->TxOpenAggrCfmdSuccess      = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_OPEN_AGGR_CFMD_SUCCESS);
  aggregation->TxOpenAggrNotCfmd          = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_OPEN_AGGR_NOT_CFMD);
  aggregation->TxOpenAggrSent             = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_TX_OPEN_AGGR_SENT);

#else  /* MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED */

  memset(aggregation, 0, sizeof(*aggregation));

#endif /* MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED */
}

static void __MTLK_IFUNC
_mtlk_sta_stat_handle_request(mtlk_irbd_t       *irbd,
                              mtlk_handle_t      context,
                              const mtlk_guid_t *evt,
                              void              *buffer,
                              uint32            *size)
{
  sta_entry            *sta  = HANDLE_T_PTR(sta_entry, context);
  mtlk_wssa_info_hdr_t *hdr = (mtlk_wssa_info_hdr_t *) buffer;

  MTLK_UNREFERENCED_PARAM(evt);

  if(sizeof(mtlk_wssa_info_hdr_t) > *size)
    return;

  if(MTIDL_SRC_DRV == hdr->info_source)
  {
    switch(hdr->info_id)
    {
    case MTLK_WSSA_DRV_STATUS_PEER:
      {
        if(sizeof(mtlk_wssa_drv_peer_stats_t) + sizeof(mtlk_wssa_info_hdr_t) > *size)
        {
          hdr->processing_result = MTLK_ERR_BUF_TOO_SMALL;
        }
        else
        {
          _mtlk_sta_get_peer_stats(sta, (mtlk_wssa_drv_peer_stats_t*) &hdr[1]);
          hdr->processing_result = MTLK_ERR_OK;
          *size = sizeof(mtlk_wssa_drv_peer_stats_t) + sizeof(mtlk_wssa_info_hdr_t);
        }
      }
      break;
#if MTLK_MTIDL_PEER_STAT_FULL
    case MTLK_WSSA_DRV_DEBUG_STATUS_PEER:
      {
        if(sizeof(mtlk_wssa_drv_debug_peer_stats_t) + sizeof(mtlk_wssa_info_hdr_t) > *size)
        {
          hdr->processing_result = MTLK_ERR_BUF_TOO_SMALL;
        }
        else
        {
          _mtlk_sta_get_debug_peer_stats(sta, (mtlk_wssa_drv_debug_peer_stats_t*) &hdr[1]);
          hdr->processing_result = MTLK_ERR_OK;
          *size = sizeof(mtlk_wssa_drv_debug_peer_stats_t) + sizeof(mtlk_wssa_info_hdr_t);
        }
      }
      break;
#endif /* MTLK_MTIDL_PEER_STAT_FULL */
    case MTLK_WSSA_DRV_CAPABILITIES_PEER:
      {
        if(sizeof(mtlk_wssa_drv_peer_capabilities_t) + sizeof(mtlk_wssa_info_hdr_t) > *size)
        {
          hdr->processing_result = MTLK_ERR_BUF_TOO_SMALL;
        }
        else
        {
          _mtlk_sta_get_peer_capabilities(sta, (mtlk_wssa_drv_peer_capabilities_t*) &hdr[1]);
          hdr->processing_result = MTLK_ERR_OK;
          *size = sizeof(mtlk_wssa_drv_peer_capabilities_t) + sizeof(mtlk_wssa_info_hdr_t);
        }
      }
      break;

/* with DEBUG only */
#if MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
    case MTLK_WSSA_DRV_PEER_AGGREGATION:
      {
        if(sizeof(mtlk_wssa_drv_peer_aggregation_t) + sizeof(mtlk_wssa_info_hdr_t) > *size)
        {
          hdr->processing_result = MTLK_ERR_BUF_TOO_SMALL;
        }
        else
        {
          mtlk_sta_get_peer_aggregation(sta, (mtlk_wssa_drv_peer_aggregation_t*) &hdr[1]);
          hdr->processing_result = MTLK_ERR_OK;
          *size = sizeof(mtlk_wssa_drv_peer_aggregation_t) + sizeof(mtlk_wssa_info_hdr_t);
        }
      }
      break;
#endif /* MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED */
    default:
      {
        hdr->processing_result = MTLK_ERR_NO_ENTRY;
        *size = sizeof(mtlk_wssa_info_hdr_t);
      }
    }
  }
  else
  {
    hdr->processing_result = MTLK_ERR_NO_ENTRY;
    *size = sizeof(mtlk_wssa_info_hdr_t);
  }
}

MTLK_INIT_STEPS_LIST_BEGIN(sta_entry)
  MTLK_INIT_STEPS_LIST_ENTRY(sta_entry, STA_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(sta_entry, KALV_TMR)
  MTLK_INIT_STEPS_LIST_ENTRY(sta_entry, IDLE_TMR)
  MTLK_INIT_STEPS_LIST_ENTRY(sta_entry, IRBD_NODE)
  MTLK_INIT_STEPS_LIST_ENTRY(sta_entry, WSS_NODE)
  MTLK_INIT_STEPS_LIST_ENTRY(sta_entry, WSS_HCNTRs)
  MTLK_INIT_STEPS_LIST_ENTRY(sta_entry, ADDBA_PEER)
  MTLK_INIT_STEPS_LIST_ENTRY(sta_entry, PEER_ANALYZER)
MTLK_INIT_INNER_STEPS_BEGIN(sta_entry)
MTLK_INIT_STEPS_LIST_END(sta_entry);

MTLK_START_STEPS_LIST_BEGIN(sta_entry)
  MTLK_START_STEPS_LIST_ENTRY(sta_entry, ROD_QUEs)
  MTLK_START_STEPS_LIST_ENTRY(sta_entry, TX_CNTs)
  MTLK_START_STEPS_LIST_ENTRY(sta_entry, AGGR_INITEDs)
  MTLK_START_STEPS_LIST_ENTRY(sta_entry, SQ_PEER)
  MTLK_START_STEPS_LIST_ENTRY(sta_entry, PS_MODE)
  MTLK_START_STEPS_LIST_ENTRY(sta_entry, BA_PEER)
  MTLK_START_STEPS_LIST_ENTRY(sta_entry, KALV_TMR)
  MTLK_START_STEPS_LIST_ENTRY(sta_entry, IDLE_TMR)
  MTLK_START_STEPS_LIST_ENTRY(sta_entry, IRBD_NODE)
  MTLK_START_STEPS_LIST_ENTRY(sta_entry, STA_STAT_REQ_HANDLER)
  MTLK_START_STEPS_LIST_ENTRY(sta_entry, ENABLE_FILTER)
MTLK_START_INNER_STEPS_BEGIN(sta_entry)
MTLK_START_STEPS_LIST_END(sta_entry);

static void
_mtlk_sta_cleanup (sta_entry *sta)
{
  MTLK_CLEANUP_BEGIN(sta_entry, MTLK_OBJ_PTR(sta))
    MTLK_CLEANUP_STEP(sta_entry, PEER_ANALYZER, MTLK_OBJ_PTR(sta),
                      mtlk_peer_analyzer_cleanup, (&sta->info.sta_analyzer));
    MTLK_CLEANUP_STEP(sta_entry, ADDBA_PEER, MTLK_OBJ_PTR(sta),
                      mtlk_addba_peer_cleanup, (&sta->addba_peer));
    MTLK_CLEANUP_STEP(sta_entry, WSS_HCNTRs, MTLK_OBJ_PTR(sta),
                      mtlk_wss_cntrs_close, (sta->wss, sta->wss_hcntrs, ARRAY_SIZE(sta->wss_hcntrs)));
    MTLK_CLEANUP_STEP(sta_entry, WSS_NODE, MTLK_OBJ_PTR(sta),
                      mtlk_wss_delete, (sta->wss));
    MTLK_CLEANUP_STEP(sta_entry, IRBD_NODE, MTLK_OBJ_PTR(sta),
                      mtlk_irbd_free, (sta->irbd));
    MTLK_CLEANUP_STEP(sta_entry, IDLE_TMR, MTLK_OBJ_PTR(sta),
                      mtlk_osal_timer_cleanup, (&sta->idle_timer))
    MTLK_CLEANUP_STEP(sta_entry, KALV_TMR, MTLK_OBJ_PTR(sta),
                      mtlk_osal_timer_cleanup, (&sta->keepalive_timer))
    MTLK_CLEANUP_STEP(sta_entry, STA_LOCK, MTLK_OBJ_PTR(sta),
                      mtlk_osal_lock_cleanup, (&sta->lock))
  MTLK_CLEANUP_END(sta_entry, MTLK_OBJ_PTR(sta))
}

static int
_mtlk_sta_init (sta_entry *sta, mtlk_vap_handle_t vap_handle, sta_db *paramdb, mtlk_addba_t *addba)
{
  mtlk_addba_wrap_api_t   addba_api;

  MTLK_ASSERT(ARRAY_SIZE(_mtlk_sta_wss_id_map)  == MTLK_STAI_CNT_LAST);
  MTLK_ASSERT(ARRAY_SIZE(sta->wss_hcntrs) == MTLK_STAI_CNT_LAST);

  memset(sta, 0, sizeof(*sta));

  sta->paramdb = paramdb;
  sta->vap_handle = vap_handle;

  mtlk_osal_atomic_set(&sta->ref_cnt, 0);

  memset(&addba_api, 0, sizeof(addba_api));

  /************************************************************************
   * STA object is the ADDBA Peer object owner, so the usr_data to be
   * filled with the parent STA object.
   * Then, all the ADDBA Peer Outward API functions will receive the STA
   * pointer according to ADDBA Peer Outward API specification (see addba.h)
   ************************************************************************/

  addba_api.usr_data              = HANDLE_T(sta);
  addba_api.get_last_rx_timestamp = _mtlk_sta_get_addba_last_rx_timestamp;
  addba_api.on_start_aggregation  = _mtlk_sta_addba_do_nothing; 
  addba_api.on_stop_aggregation   = _mtlk_sta_addba_restart_tx_count_tid;
  addba_api.on_start_reordering   = _mtlk_sta_addba_start_reordering;
  addba_api.on_stop_reordering    = _mtlk_sta_addba_stop_reordering;
  addba_api.on_ba_req_rejected    = _mtlk_sta_addba_revive_aggregation_tid;
  addba_api.on_ba_req_unconfirmed = _mtlk_sta_addba_restart_tx_count_tid;

  MTLK_INIT_TRY(sta_entry, MTLK_OBJ_PTR(sta))
    MTLK_INIT_STEP(sta_entry, STA_LOCK, MTLK_OBJ_PTR(sta),
                   mtlk_osal_lock_init, (&sta->lock));
    MTLK_INIT_STEP(sta_entry, KALV_TMR, MTLK_OBJ_PTR(sta),
                   mtlk_osal_timer_init, (&sta->keepalive_timer,
                                          _mtlk_sta_keepalive_tmr,
                                          HANDLE_T(sta)));
    MTLK_INIT_STEP(sta_entry, IDLE_TMR, MTLK_OBJ_PTR(sta),
                   mtlk_osal_timer_init, (&sta->idle_timer,
                                          mtlk_sta_data_disconnect_tmr,
                                          HANDLE_T(sta)));
    MTLK_INIT_STEP_EX(sta_entry, IRBD_NODE, MTLK_OBJ_PTR(sta),
                      mtlk_irbd_alloc, (),
                      sta->irbd, sta->irbd != NULL, MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP_EX(sta_entry, WSS_NODE, MTLK_OBJ_PTR(sta),
                      mtlk_wss_create, (paramdb->wss, _mtlk_sta_wss_id_map, ARRAY_SIZE(_mtlk_sta_wss_id_map)),
                      sta->wss, sta->wss != NULL, MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP(sta_entry, WSS_HCNTRs, MTLK_OBJ_PTR(sta),
                   mtlk_wss_cntrs_open, (sta->wss, _mtlk_sta_wss_id_map, sta->wss_hcntrs, MTLK_STAI_CNT_LAST));
    MTLK_INIT_STEP(sta_entry, ADDBA_PEER, MTLK_OBJ_PTR(sta),
                   mtlk_addba_peer_init, (&sta->addba_peer, &addba_api, addba, sta->wss));
    MTLK_INIT_STEP(sta_entry, PEER_ANALYZER, MTLK_OBJ_PTR(sta),
                   mtlk_peer_analyzer_init, (&sta->info.sta_analyzer));
  MTLK_INIT_FINALLY(sta_entry, MTLK_OBJ_PTR(sta))
  MTLK_INIT_RETURN(sta_entry, MTLK_OBJ_PTR(sta), _mtlk_sta_cleanup, (sta))
}

static __INLINE void
_mtlk_sta_stop (sta_entry *sta, const IEEE_ADDR *addr)
{
  int i;

  MTLK_STOP_BEGIN(sta_entry, MTLK_OBJ_PTR(sta))
    MTLK_STOP_STEP(sta_entry, ENABLE_FILTER, MTLK_OBJ_PTR(sta),
                   MTLK_NOACTION, ());
    MTLK_STOP_STEP(sta_entry, STA_STAT_REQ_HANDLER, MTLK_OBJ_PTR(sta), 
                   mtlk_wssd_unregister_request_handler, (sta->irbd, sta->stat_irb_handle));
    MTLK_STOP_STEP(sta_entry, IRBD_NODE, MTLK_OBJ_PTR(sta),
                   mtlk_irbd_cleanup, (sta->irbd));
    MTLK_STOP_STEP(sta_entry, IDLE_TMR, MTLK_OBJ_PTR(sta),
                   mtlk_osal_timer_cancel_sync, (&sta->idle_timer));
    MTLK_STOP_STEP(sta_entry, KALV_TMR, MTLK_OBJ_PTR(sta),
                   mtlk_osal_timer_cancel_sync, (&sta->keepalive_timer));
    MTLK_STOP_STEP(sta_entry, BA_PEER, MTLK_OBJ_PTR(sta),
                   mtlk_addba_peer_stop, (&sta->addba_peer, sta->info.dot11n_mode));
    MTLK_STOP_STEP(sta_entry, PS_MODE, MTLK_OBJ_PTR(sta),
                   _mtlk_sta_ps_mode_cleanup, (&sta->sq_peer_ctx, sta->vap_handle));
    MTLK_STOP_STEP(sta_entry, SQ_PEER, MTLK_OBJ_PTR(sta),
                   mtlk_sq_peer_ctx_cleanup, (sta->paramdb->cfg.sq, &sta->sq_peer_ctx));
    MTLK_STOP_STEP(sta_entry, AGGR_INITEDs, MTLK_OBJ_PTR(sta),
                   MTLK_NOACTION, ());
    MTLK_STOP_STEP(sta_entry, TX_CNTs, MTLK_OBJ_PTR(sta),
                   MTLK_NOACTION, ());
    for (i = 0; MTLK_STOP_ITERATIONS_LEFT(MTLK_OBJ_PTR(sta), ROD_QUEs) > 0; i++) {
      _mtlk_sta_destroy_rod_queue(sta, i);
      MTLK_STOP_STEP_LOOP(sta_entry, ROD_QUEs, MTLK_OBJ_PTR(sta),
                          mtlk_release_rod_queue, (&sta->rod_queue[i]));
    }
  MTLK_STOP_END(sta_entry, MTLK_OBJ_PTR(sta))
}

static __INLINE int
_mtlk_sta_start (sta_entry *sta, BOOL dot11n_mode, const IEEE_ADDR *addr, uint32 sta_id)
{
  int i;
  char irbd_name[sizeof(MTLK_IRB_STA_NAME) + sizeof("XX:XX:XX:XX:XX:XX")];

  /* This call cannot fail since the MAC addr string size is constant */
  mtlk_snprintf(irbd_name, sizeof(irbd_name), MTLK_IRB_STA_NAME "%Y", addr->au8Addr);

  MTLK_START_TRY(sta_entry, MTLK_OBJ_PTR(sta))
    for (i = 0; i < ARRAY_SIZE(sta->rod_queue); i++) {
      MTLK_START_STEP_LOOP(sta_entry, ROD_QUEs, MTLK_OBJ_PTR(sta),
                           mtlk_init_rod_queue, (&sta->rod_queue[i]));
    }

    sta->sta_id               = sta_id;
    sta->connection_timestamp = mtlk_osal_timestamp();
    sta->timestamp            = sta->connection_timestamp;
    sta->info.dot11n_mode     = dot11n_mode;
#ifdef MTCFG_RF_MANAGEMENT_MTLK
    sta->info.rf_mgmt_data = MTLK_RF_MGMT_DATA_DEFAULT;
#endif
    sta->info.cipher       = IW_ENCODE_ALG_NONE;

    sta->beacon_interval   = DEFAULT_BEACON_INTERVAL;
    sta->beacon_timestamp  = 0;

    MTLK_START_STEP_VOID(sta_entry, TX_CNTs, MTLK_OBJ_PTR(sta),
                         _mtlk_sta_reset_tx_cntrs, (sta));
    MTLK_START_STEP_VOID(sta_entry, AGGR_INITEDs, MTLK_OBJ_PTR(sta),
                         _mtlk_sta_reset_aggr_inited, (sta));
    MTLK_START_STEP_VOID(sta_entry, SQ_PEER, MTLK_OBJ_PTR(sta),
                         mtlk_sq_peer_ctx_init, (sta->paramdb->cfg.sq, 
                                                 &sta->sq_peer_ctx, 
                                                 dot11n_mode ? MTLK_SQ_TX_LIMIT_DEFAULT : MTLK_SQ_TX_LIMIT_LEGACY,
                                                 NULL));
    MTLK_START_STEP_VOID(sta_entry, PS_MODE, MTLK_OBJ_PTR(sta),
                         MTLK_NOACTION, ());
    MTLK_START_STEP_VOID(sta_entry, BA_PEER, MTLK_OBJ_PTR(sta),
                            mtlk_addba_peer_start, (&sta->addba_peer, addr, sta->info.dot11n_mode));
    MTLK_START_STEP(sta_entry, KALV_TMR, MTLK_OBJ_PTR(sta),
                    mtlk_osal_timer_set, (&sta->keepalive_timer,
                                           sta->paramdb->keepalive_interval ? 
                                           sta->paramdb->keepalive_interval : 
                                           DEFAULT_KEEPALIVE_TIMEOUT));
    MTLK_START_STEP(sta_entry, IDLE_TMR, MTLK_OBJ_PTR(sta),
                    mtlk_osal_timer_set, (&sta->idle_timer,
                                          sta->paramdb->sta_keepalive_timeout ? 
                                          sta->paramdb->sta_keepalive_timeout : 
                                          DEFAULT_KEEPALIVE_TIMEOUT));
    MTLK_START_STEP(sta_entry, IRBD_NODE, MTLK_OBJ_PTR(sta),
                    mtlk_irbd_init, (sta->irbd, mtlk_vap_get_irbd(sta->vap_handle), irbd_name));
    MTLK_START_STEP_EX(sta_entry, STA_STAT_REQ_HANDLER, MTLK_OBJ_PTR(sta),
                       mtlk_wssd_register_request_handler, (sta->irbd, _mtlk_sta_stat_handle_request, HANDLE_T(sta)),
                       sta->stat_irb_handle, sta->stat_irb_handle != NULL, MTLK_ERR_UNKNOWN);
    MTLK_START_STEP_VOID(sta_entry, ENABLE_FILTER, MTLK_OBJ_PTR(sta),
                         _mtlk_sta_set_packets_filter_default, (sta));
  MTLK_START_FINALLY(sta_entry, MTLK_OBJ_PTR(sta))
  MTLK_START_RETURN(sta_entry, MTLK_OBJ_PTR(sta), _mtlk_sta_stop, (sta, addr))
}

/* counters will be modified with checking ALLOWED option */
#define _mtlk_sta_inc_cnt(sta, id)         { if (id##_ALLOWED) __mtlk_sta_inc_cnt(sta, id); }
#define _mtlk_sta_add_cnt(sta, id, val)    { if (id##_ALLOWED) __mtlk_sta_add_cnt(sta, id, val); }


static __INLINE void
__mtlk_sta_inc_cnt (sta_entry        *sta,
                   sta_info_cnt_id_e cnt_id)
{
  MTLK_ASSERT(cnt_id >= 0 && cnt_id < MTLK_STAI_CNT_LAST);

  mtlk_wss_cntr_inc(sta->wss_hcntrs[cnt_id]);
}

static __INLINE void
__mtlk_sta_add_cnt (sta_entry        *sta,
                   sta_info_cnt_id_e cnt_id,
                   uint32 val)
{
  MTLK_ASSERT(cnt_id >= 0 && cnt_id < MTLK_STAI_CNT_LAST);

  mtlk_wss_cntr_add(sta->wss_hcntrs[cnt_id], val);
}


void __MTLK_IFUNC
mtlk_sta_on_packet_sent (sta_entry *sta, mtlk_nbuf_t *nbuf, uint32 retransmissions)
{
  ASSERT(sta != NULL);
  sta->timestamp = mtlk_osal_timestamp();
  if (0 != mtlk_df_nbuf_get_data_length(nbuf)) { /*skip null packets */
    uint32 data_length = mtlk_df_nbuf_get_data_length(nbuf);

    _mtlk_sta_inc_cnt(sta, MTLK_STAI_CNT_PACKETS_SENT);
    _mtlk_sta_add_cnt(sta, MTLK_STAI_CNT_BYTES_SENT, data_length);

    if (mtlk_nbuf_priv_check_flags(mtlk_nbuf_priv(nbuf), MTLK_NBUFF_UNICAST)) {
      _mtlk_sta_inc_cnt(sta, MTLK_STAI_CNT_UNICAST_PACKETS_SENT);
    }

#if MTLK_WWSS_WLAN_STAT_GROUP_ANALYZER_TX_ALLOWED
    mtlk_peer_analyzer_process_tx_packet(&sta->info.sta_analyzer, data_length, retransmissions);
#endif
  }

  ILOG3_YD("TX: Update STA %Y - %lu", mtlk_sta_get_addr(sta), sta->timestamp);
}

/* with DEBUG statistic */
#if MTLK_MTIDL_PEER_STAT_FULL

void __MTLK_IFUNC
mtlk_sta_on_packet_dropped(sta_entry *sta, mtlk_tx_drop_reasons_e reason)
{
  __mtlk_sta_inc_cnt(sta, reason);
}

#endif /* MTLK_MTIDL_PEER_STAT_FULL */


void __MTLK_IFUNC
mtlk_sta_on_frame_arrived(sta_entry *sta, const uint8 rssi[])
{
  ASSERT(sta != NULL);
  sta->timestamp = mtlk_osal_timestamp();

#if MTLK_WWSS_WLAN_STAT_GROUP_ANALYZER_RX_ALLOWED
  mtlk_peer_analyzer_process_rssi_sample(&sta->info.sta_analyzer, rssi);
#endif

  ILOG3_YD("RX: Update STA %Y - %lu", mtlk_sta_get_addr(sta), sta->timestamp);
}

void __MTLK_IFUNC
mtlk_sta_on_packet_indicated(sta_entry *sta, mtlk_nbuf_t *nbuf)
{
  uint32 data_length = mtlk_df_nbuf_get_data_length(nbuf);

  ASSERT(sta != NULL);
  _mtlk_sta_inc_cnt(sta, MTLK_STAI_CNT_PACKETS_RECEIVED);
  _mtlk_sta_add_cnt(sta, MTLK_STAI_CNT_BYTES_RECEIVED, data_length);

  if (mtlk_nbuf_priv_check_flags(mtlk_nbuf_priv(nbuf), MTLK_NBUFF_UNICAST)) {
    _mtlk_sta_inc_cnt(sta, MTLK_STAI_CNT_UNICAST_PACKETS_RECEIVED);
  }

#if MTLK_WWSS_WLAN_STAT_GROUP_ANALYZER_RX_ALLOWED
  mtlk_peer_analyzer_process_rx_packet(&sta->info.sta_analyzer, data_length);
#endif
}

void __MTLK_IFUNC
mtlk_sta_on_rx_packet_802_1x(sta_entry *sta)
{
  ASSERT(sta != NULL);
  _mtlk_sta_inc_cnt(sta, MTLK_STAI_CNT_802_1X_PACKETS_RECEIVED);
}

void __MTLK_IFUNC
mtlk_sta_on_tx_packet_802_1x(sta_entry *sta)
{
  ASSERT(sta != NULL);
  _mtlk_sta_inc_cnt(sta, MTLK_STAI_CNT_802_1X_PACKETS_SENT);
}

void __MTLK_IFUNC
mtlk_sta_on_rx_packet_forwarded(sta_entry *sta, mtlk_nbuf_t *nbuf)
{
  ASSERT(sta != NULL);
  _mtlk_sta_inc_cnt(sta, MTLK_STAI_CNT_FWD_RX_PACKETS);
  _mtlk_sta_add_cnt(sta, MTLK_STAI_CNT_FWD_RX_BYTES, mtlk_df_nbuf_get_data_length(nbuf));
}

void __MTLK_IFUNC 
mtlk_sta_update_tx (sta_entry *sta, uint16 tid) 
{ 
  ASSERT(sta != NULL); 
  ASSERT(tid < ARRAY_SIZE(sta->tx_packets)); 

  if (mtlk_sta_is_dot11n(sta) &&
      sta->aggr_inited[tid] == FALSE &&
      mtlk_sta_get_packets_filter(sta) == MTLK_PCKT_FLTR_ALLOW_ALL) {
    uint32 nof_tx_packets = mtlk_osal_atomic_inc(&sta->tx_packets[tid]);
    if (nof_tx_packets == sta->paramdb->aggr_open_threshold) {
      ILOG1_DYDD("CID-%04x: STA:%Y TID=%d TX Cntr=%d: Initiating aggregation agreement",
                 mtlk_vap_get_oid(sta->vap_handle),
                 mtlk_sta_get_addr(sta), tid, nof_tx_packets);
      mtlk_osal_atomic_set(&sta->tx_packets[tid], 0);
      sta->aggr_inited[tid] = TRUE;
      _mtlk_sta_schedule_aggregation_revival_tid(sta, tid);
    }
  }
}

int __MTLK_IFUNC
mtlk_sta_update_beacon_interval (sta_entry *sta, uint16 beacon_interval)
{
  int lost_beacons;

  if (beacon_interval) {
    sta->beacon_interval = beacon_interval;
  }

  /* if sta->beacon_timestamp is zero - we receive first beacon from AP */
  if (sta->beacon_timestamp) {
    mtlk_osal_timestamp_diff_t diff;
    diff = mtlk_osal_timestamp_diff(mtlk_osal_timestamp(), sta->beacon_timestamp);
    lost_beacons = mtlk_osal_timestamp_to_ms(diff) / sta->beacon_interval;
  } else {
    lost_beacons = 0;
  }

  sta->beacon_timestamp = mtlk_osal_timestamp();

  ILOG3_YDD("Update AP %Y - %lu (beacon interval %d)",
    mtlk_sta_get_addr(sta), sta->timestamp, beacon_interval);

  return lost_beacons;
}

void __MTLK_IFUNC
mtlk_sta_set_pm_enabled (sta_entry *sta, BOOL enabled)
{
  mtlk_core_t *core;

  MTLK_ASSERT(NULL != sta);

  core = mtlk_vap_get_core(sta->vap_handle);
  MTLK_ASSERT(NULL != core);

  mtlk_sq_set_pm_enabled(&sta->sq_peer_ctx, enabled);

  if (enabled) {
    _mtlk_sta_inc_cnt(sta, MTLK_STAI_CNT_PS_MODE_ENTRANCES);
  }
  else {
    _mtlk_sta_addba_exit_ps(sta);
  }
}

void __MTLK_IFUNC
mtlk_sta_set_capabilities (sta_entry *sta, const sta_capabilities *capabilities)
{
  MTLK_ASSERT(sta != NULL);
  MTLK_ASSERT(capabilities != NULL);

  mtlk_osal_lock_acquire(&sta->lock);
  memcpy(&sta->info.capabilities, capabilities, sizeof(sta->info.capabilities));
  mtlk_osal_lock_release(&sta->lock);
}

void __MTLK_IFUNC
mtlk_sta_update_fw_related_info (sta_entry *sta, const DEVICE_STATUS *fw_info)
{
  MTLK_ASSERT(sizeof(sta->info.rssi) == sizeof(fw_info->au8RSSI));
  
  mtlk_osal_lock_acquire(&sta->lock);
  memcpy(sta->info.rssi, fw_info->au8RSSI, sizeof(sta->info.rssi));
  sta->info.net_mode = fw_info->u8NetworkMode;
  sta->info.tx_rate  = MAC_TO_HOST16(fw_info->u16TxRate);
  mtlk_osal_lock_release(&sta->lock);
}

void __MTLK_IFUNC
mtlk_sta_zero_rod_reply_counters (sta_entry *sta)
{
  int i;
  for (i = 0; i < ARRAY_SIZE(sta->rod_queue); i++) {
    mtlk_rod_queue_clear_reply_counter(&sta->rod_queue[i]);
  }
}

void __MTLK_IFUNC
mtlk_sta_zero_pmf_replay_counters (sta_entry *sta)
{
  memset(sta->pmf_rsc, 0, sizeof(sta->pmf_rsc));
}

void __MTLK_IFUNC
mtlk_sta_set_packets_filter (sta_entry         *sta, 
                             mtlk_pckt_filter_e filter_type)
{
  if (sta->info.filter != filter_type) {
    ILOG1_YDD("STA (%Y) filter: %d => %d", mtlk_sta_get_addr(sta), sta->info.filter, filter_type); 
    sta->info.filter = filter_type;
    _mtlk_sta_reset_tx_cntrs(sta); 
  }
}

void __MTLK_IFUNC
mtlk_sta_get_peer_stats (const sta_entry* sta, mtlk_wssa_drv_peer_stats_t* stats)
{
  MTLK_ASSERT(sta != NULL);
  MTLK_ASSERT(stats != NULL);

  _mtlk_sta_get_peer_stats(sta, stats);
}

void __MTLK_IFUNC
mtlk_sta_get_peer_capabilities (const sta_entry* sta, mtlk_wssa_drv_peer_capabilities_t* capabilities)
{
  MTLK_ASSERT(sta != NULL);
  MTLK_ASSERT(capabilities != NULL);

  _mtlk_sta_get_peer_capabilities(sta, capabilities);
}

/*
  Close TX aggregation for special station

  \param sta  - handle to the sta [I]
  \param tid  - Access protocol index, '-1' for all TIDs [I]

  \return
    none
*/
void __MTLK_IFUNC
mtlk_sta_delba_req(sta_entry* sta, int tid)
{
  uint16 tid_max;
  mtlk_addba_peer_t *addba_peer;

  MTLK_ASSERT(sta != NULL);
  MTLK_ASSERT(tid < NTS_TIDS);

  if (tid == -1)
  {
    tid     = 0;
    tid_max = NTS_TIDS;
  }
  else
  {
    tid_max = tid + 1;
  }

  addba_peer = mtlk_sta_get_addb_peer(sta);

  for (; tid < tid_max; tid++)
  {
    mtlk_addba_delba_req(addba_peer, (uint16)tid);
  }
}

void __MTLK_IFUNC
mtlk_sta_on_qos_without_reord(sta_entry* sta, uint16 tid)
{
  mtlk_addba_peer_t *addba_peer;

  MTLK_ASSERT(sta != NULL);
  MTLK_ASSERT(tid < NTS_TIDS);

  addba_peer = mtlk_sta_get_addb_peer(sta);
  mtlk_addba_on_qos_without_reord(addba_peer, tid);
}

/******************************************************************************************/

/******************************************************************************************
 * SPA DB API
 ******************************************************************************************/
// STA DB flash interval
#define MTLK_STADB_DEFAULT_FLUSH_PERIOD  100  /* ms */
#define MTLK_STADB_ITER_ADDBA_PERIOD     1000 /* msec */

#define MTLK_STADB_HASH_NOF_BUCKETS      16

void __MTLK_IFUNC
__mtlk_sta_on_unref_private (sta_entry *sta)
{
  MTLK_ASSERT(sta != NULL);
  MTLK_ASSERT(sta->paramdb != NULL);

  ILOG2_Y("No more references of STA (%Y)!",
          mtlk_sta_get_addr(sta));

  _mtlk_sta_cleanup(sta);
  mtlk_osal_mem_free(sta);
}


// Timers
static uint32 __MTLK_IFUNC
_mtlk_stadb_flush_clb (mtlk_osal_timer_t *timer, 
                       mtlk_handle_t      clb_usr_data)
{
  sta_db                       *stadb = HANDLE_T_PTR(sta_db, clb_usr_data);
  mtlk_hash_enum_t              e;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  MTLK_ASSERT(stadb != NULL);

  MTLK_UNREFERENCED_PARAM(timer);

  mtlk_osal_lock_acquire(&stadb->lock);
  h = mtlk_hash_enum_first_ieee_addr(&stadb->hash, &e);
  while (h) {
    sta_entry *sta = MTLK_CONTAINER_OF(h, sta_entry, hentry);

    _mtlk_sta_flush(sta);

    h = mtlk_hash_enum_next_ieee_addr(&stadb->hash, &e);
  }
  mtlk_osal_lock_release(&stadb->lock);

  return MTLK_STADB_DEFAULT_FLUSH_PERIOD;
}

static uint32 _mtlk_stadb_addba_iterator(mtlk_osal_timer_t *timer, mtlk_handle_t data)
{
  mtlk_stadb_iterate_addba((sta_db*)data);

  /* re-arm timer */
  return MTLK_STADB_ITER_ADDBA_PERIOD;
}

static int __MTLK_IFUNC
_mtlk_stadb_on_addba_revive_aggregation_tid (mtlk_handle_t sta_db_object, const void *payload, uint32 size)
{
  sta_db                               *stadb = HANDLE_T_PTR(sta_db, sta_db_object);
  sta_entry                            *sta   = NULL;
  const struct eq_evt_aggr_closed_data *data  = 
    (const struct eq_evt_aggr_closed_data *)payload;

  MTLK_ASSERT(sizeof(*data) == size);
  
  sta = mtlk_stadb_find_sta(stadb, data->sta_addr.au8Addr);
  if (sta) {
    ILOG2_YD("Reviving Aggr (%Y,tid=%d)", data->sta_addr.au8Addr, data->prior_class);
    mtlk_addba_peer_start_aggr_negotiation(&sta->addba_peer,
                                           data->prior_class, mtlk_osal_atomic_get(&sta->sq_peer_ctx.ps_mode_enabled));
    mtlk_sta_decref(sta); /* De-reference of find */
  }

  return MTLK_ERR_OK;
}

sta_entry * __MTLK_IFUNC
mtlk_stadb_add_sta (sta_db *stadb, const unsigned char *mac,
                    BOOL dot11n_mode, uint32 sta_id)
{
  int                           res = MTLK_ERR_UNKNOWN;
  sta_entry                    *sta = NULL;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  sta = mtlk_osal_mem_alloc(sizeof(*sta), MTLK_MEM_TAG_STADB_ITER);
  if (!sta) {
    res = MTLK_ERR_NO_MEM;
    WLOG_V("Can't alloc STA");
    goto err_alloc;
  }

  res = _mtlk_sta_init(sta, stadb->vap_handle, stadb, stadb->cfg.addba);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Can't init STA (err#%d)", res);
    goto err_init;
  }

  mtlk_sta_incref(sta); /* Reference by a caller */

  MTLK_ASSERT(stadb->aggr_open_threshold != 0);

  res = _mtlk_sta_start(sta, dot11n_mode, (IEEE_ADDR *)mac, sta_id);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Can't start STA (err#%d)", res);
    goto err_start;
  }

  mtlk_osal_lock_acquire(&stadb->lock);
  h = mtlk_hash_insert_ieee_addr(&stadb->hash, (IEEE_ADDR *)mac, &sta->hentry);
  if (h == NULL) {
    ++stadb->hash_cnt;
    mtlk_sta_incref(sta); /* Reference by STA DB hash */
  }
  mtlk_osal_lock_release(&stadb->lock);

  if (h != NULL) {
    res = MTLK_ERR_PROHIB;
    WLOG_Y("%Y already connected", mac);
    goto err_insert;
  }

  ILOG3_YP("PEER %Y added (%p)", mac, sta);

  return sta;

err_insert:
  _mtlk_sta_stop(sta, (IEEE_ADDR *)mac);
err_start:
  _mtlk_sta_cleanup(sta);
err_init:
  mtlk_osal_mem_free(sta);
err_alloc:
  return NULL;
}

static void
_mtlk_stadb_remove_sta_internal(sta_db* stadb, sta_entry *sta)
{
  IEEE_ADDR addr = *mtlk_sta_get_addr(sta);

  --stadb->hash_cnt;
  mtlk_hash_remove_ieee_addr(&stadb->hash, &sta->hentry);

  mtlk_mc_drop_sta(mtlk_vap_get_core(stadb->vap_handle), addr.au8Addr);

  _mtlk_sta_stop(sta, &addr);

  mtlk_sta_decref(sta); /* De-reference by STA DB hash */

  ILOG3_Y("PEER %Y removed", &addr);
}

void __MTLK_IFUNC
mtlk_stadb_remove_sta (sta_db* stadb, sta_entry *sta)
{
  mtlk_osal_lock_acquire(&stadb->lock);

  _mtlk_stadb_remove_sta_internal(stadb, sta);

  mtlk_osal_lock_release(&stadb->lock);
}

MTLK_INIT_STEPS_LIST_BEGIN(sta_db)
  MTLK_INIT_STEPS_LIST_ENTRY(sta_db, STADB_FLUSH_TIMER)
  MTLK_INIT_STEPS_LIST_ENTRY(sta_db, STADB_ITERATE_ADDBA_TIMER)
  MTLK_INIT_STEPS_LIST_ENTRY(sta_db, STADB_HASH)
  MTLK_INIT_STEPS_LIST_ENTRY(sta_db, STADB_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(sta_db, REG_ABILITIES)
  MTLK_INIT_STEPS_LIST_ENTRY(sta_db, EN_ABILITIES)
MTLK_INIT_INNER_STEPS_BEGIN(sta_db)
MTLK_INIT_STEPS_LIST_END(sta_db);

MTLK_START_STEPS_LIST_BEGIN(sta_db)
  MTLK_START_STEPS_LIST_ENTRY(sta_db, STADB_WSS_CREATE)
  MTLK_START_STEPS_LIST_ENTRY(sta_db, STADB_FLUSH_TIMER)
  MTLK_START_STEPS_LIST_ENTRY(sta_db, STADB_ITERATE_ADDBA_TIMER)
MTLK_START_INNER_STEPS_BEGIN(sta_db)
MTLK_START_STEPS_LIST_END(sta_db);

static const mtlk_ability_id_t _stadb_abilities[] = {
  MTLK_CORE_REQ_GET_STADB_CFG,
  MTLK_CORE_REQ_SET_STADB_CFG,
  MTLK_CORE_REQ_GET_STADB_STATUS
};

int __MTLK_IFUNC
mtlk_stadb_init (sta_db *stadb, mtlk_vap_handle_t vap_handle)
{
  MTLK_ASSERT(stadb != NULL);

  stadb->vap_handle = vap_handle;

  // Set default configuration
  stadb->sta_keepalive_timeout = 60000;
  stadb->keepalive_interval    = 1000;
  stadb->aggr_open_threshold   = DEFAULT_AGGR_OPEN_THRESHOLD;

  stadb->hash_cnt              = 0;
  stadb->flags                |= STADB_FLAGS_STOPPED;

  MTLK_INIT_TRY(sta_db, MTLK_OBJ_PTR(stadb))
    MTLK_INIT_STEP(sta_db, STADB_FLUSH_TIMER, MTLK_OBJ_PTR(stadb),
                   mtlk_osal_timer_init, (&stadb->flush_timer, _mtlk_stadb_flush_clb, HANDLE_T(stadb)));
    MTLK_INIT_STEP(sta_db, STADB_ITERATE_ADDBA_TIMER, MTLK_OBJ_PTR(stadb),
                   mtlk_osal_timer_init, (&stadb->iter_addba_timer, _mtlk_stadb_addba_iterator, HANDLE_T(stadb)));
    MTLK_INIT_STEP(sta_db, STADB_HASH, MTLK_OBJ_PTR(stadb),
                   mtlk_hash_init_ieee_addr, (&stadb->hash, MTLK_STADB_HASH_NOF_BUCKETS))
    MTLK_INIT_STEP(sta_db, STADB_LOCK, MTLK_OBJ_PTR(stadb),
                   mtlk_osal_lock_init, (&stadb->lock));
    MTLK_INIT_STEP(sta_db, REG_ABILITIES, MTLK_OBJ_PTR(stadb),
                    mtlk_abmgr_register_ability_set,
                    (mtlk_vap_get_abmgr(stadb->vap_handle), _stadb_abilities, ARRAY_SIZE(_stadb_abilities)));
    MTLK_INIT_STEP_VOID(sta_db, EN_ABILITIES, MTLK_OBJ_PTR(stadb),
                        mtlk_abmgr_enable_ability_set,
                        (mtlk_vap_get_abmgr(stadb->vap_handle), _stadb_abilities, ARRAY_SIZE(_stadb_abilities)));
  MTLK_INIT_FINALLY(sta_db, MTLK_OBJ_PTR(stadb))
  MTLK_INIT_RETURN(sta_db, MTLK_OBJ_PTR(stadb), mtlk_stadb_cleanup, (stadb))
}

void __MTLK_IFUNC
mtlk_stadb_configure (sta_db *stadb, const sta_db_cfg_t *cfg)
{
  MTLK_ASSERT(stadb != NULL);
  MTLK_ASSERT(cfg != NULL);
  MTLK_ASSERT(cfg->addba != NULL);
  MTLK_ASSERT(cfg->sq != NULL);
  MTLK_ASSERT(cfg->max_nof_stas != 0);
  MTLK_ASSERT(cfg->parent_wss != NULL);
  MTLK_ASSERT(cfg->api.on_sta_inactive != NULL);
  MTLK_ASSERT(cfg->api.on_sta_keepalive != NULL);
  MTLK_ASSERT(cfg->api.sta_inactivity_on != NULL);

  stadb->cfg = *cfg;
}

int __MTLK_IFUNC
mtlk_stadb_start (sta_db *stadb)
{
  MTLK_ASSERT(stadb != NULL);

  MTLK_START_TRY(sta_db, MTLK_OBJ_PTR(stadb))
    MTLK_START_STEP_EX(sta_db, STADB_WSS_CREATE, MTLK_OBJ_PTR(stadb),
                       mtlk_wss_create, (stadb->cfg.parent_wss, NULL, 0),
                       stadb->wss, stadb->wss != NULL, MTLK_ERR_OK);
    MTLK_START_STEP(sta_db, STADB_FLUSH_TIMER, MTLK_OBJ_PTR(stadb),
                    mtlk_osal_timer_set, (&stadb->flush_timer, MTLK_STADB_DEFAULT_FLUSH_PERIOD));
    MTLK_START_STEP(sta_db, STADB_ITERATE_ADDBA_TIMER, MTLK_OBJ_PTR(stadb),
                    mtlk_osal_timer_set, (&stadb->iter_addba_timer, MTLK_STADB_ITER_ADDBA_PERIOD));

    stadb->flags &= ~STADB_FLAGS_STOPPED;
  MTLK_START_FINALLY(sta_db, MTLK_OBJ_PTR(stadb))
  MTLK_START_RETURN(sta_db, MTLK_OBJ_PTR(stadb), mtlk_stadb_stop, (stadb))
}

void __MTLK_IFUNC
mtlk_stadb_stop (sta_db *stadb)
{
  if (stadb->flags & STADB_FLAGS_STOPPED) {
    return;
  }

  MTLK_STOP_BEGIN(sta_db, MTLK_OBJ_PTR(stadb))
    MTLK_STOP_STEP(sta_db, STADB_ITERATE_ADDBA_TIMER, MTLK_OBJ_PTR(stadb),
                   mtlk_osal_timer_cancel_sync, (&stadb->iter_addba_timer));
    MTLK_STOP_STEP(sta_db, STADB_FLUSH_TIMER, MTLK_OBJ_PTR(stadb),
                   mtlk_osal_timer_cancel_sync, (&stadb->flush_timer));
    MTLK_STOP_STEP(sta_db, STADB_WSS_CREATE, MTLK_OBJ_PTR(stadb),
                   mtlk_wss_delete, (stadb->wss));
    stadb->flags |= STADB_FLAGS_STOPPED;
  MTLK_STOP_END(sta_db, MTLK_OBJ_PTR(stadb))
}

void __MTLK_IFUNC
mtlk_stadb_cleanup (sta_db *stadb)
{
  MTLK_CLEANUP_BEGIN(sta_db, MTLK_OBJ_PTR(stadb))
    MTLK_CLEANUP_STEP(sta_db, EN_ABILITIES, MTLK_OBJ_PTR(stadb),
                      mtlk_abmgr_disable_ability_set,
                      (mtlk_vap_get_abmgr(stadb->vap_handle), _stadb_abilities, ARRAY_SIZE(_stadb_abilities)));
    MTLK_CLEANUP_STEP(sta_db, REG_ABILITIES, MTLK_OBJ_PTR(stadb),
                      mtlk_abmgr_unregister_ability_set,
                      (mtlk_vap_get_abmgr(stadb->vap_handle), _stadb_abilities, ARRAY_SIZE(_stadb_abilities)));
    MTLK_CLEANUP_STEP(sta_db, STADB_LOCK, MTLK_OBJ_PTR(stadb),
                      mtlk_osal_lock_cleanup, (&stadb->lock));
    MTLK_CLEANUP_STEP(sta_db, STADB_HASH, MTLK_OBJ_PTR(stadb),
                      mtlk_hash_cleanup_ieee_addr, (&stadb->hash));
    MTLK_CLEANUP_STEP(sta_db, STADB_ITERATE_ADDBA_TIMER, MTLK_OBJ_PTR(stadb),
                      mtlk_osal_timer_cleanup, (&stadb->iter_addba_timer));
    MTLK_CLEANUP_STEP(sta_db, STADB_FLUSH_TIMER, MTLK_OBJ_PTR(stadb),
                      mtlk_osal_timer_cleanup, (&stadb->flush_timer));
  MTLK_CLEANUP_END(sta_db, MTLK_OBJ_PTR(stadb))
}

const sta_entry * __MTLK_IFUNC
mtlk_stadb_iterate_first (sta_db *stadb, mtlk_stadb_iterator_t *iter)
{
  int                           err = MTLK_ERR_UNKNOWN;
  const sta_entry *             res = NULL;
  uint32                        idx = 0;
  mtlk_hash_enum_t              e;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;
  
  MTLK_ASSERT(stadb != NULL);
  MTLK_ASSERT(iter != NULL);

  mtlk_osal_lock_acquire(&stadb->lock);

  iter->size = stadb->hash_cnt;

  if (!iter->size) {
    err = MTLK_ERR_NOT_IN_USE;
    goto end;
  }

  iter->arr = 
    (sta_entry**)mtlk_osal_mem_alloc(iter->size * sizeof(sta_entry *),
                                     MTLK_MEM_TAG_STADB_ITER);
  if (!iter->arr) {
    ELOG_D("Can't allocate iteration array of %d entries", iter->size);
    err = MTLK_ERR_NO_MEM;
    goto end;
  }

  memset(iter->arr, 0, iter->size * sizeof(sta_entry *));

  h = mtlk_hash_enum_first_ieee_addr(&stadb->hash, &e);
  while (idx < iter->size && h) {
    sta_entry *sta = MTLK_CONTAINER_OF(h, sta_entry, hentry);
    iter->arr[idx] = sta;
    mtlk_sta_incref(sta); /* Reference by iterator */
    ++idx;
    h = mtlk_hash_enum_next_ieee_addr(&stadb->hash, &e);
  }

  err = MTLK_ERR_OK;

end:
  mtlk_osal_lock_release(&stadb->lock);

  if (err == MTLK_ERR_OK) {
    iter->stadb = stadb;
    iter->idx   = 0;

    res = mtlk_stadb_iterate_next(iter);
    if (!res) {
      mtlk_stadb_iterate_done(iter);
    }
  }

  return res;
}

const sta_entry * __MTLK_IFUNC
mtlk_stadb_iterate_next (mtlk_stadb_iterator_t *iter)
{
  const sta_entry *sta = NULL;
  
  MTLK_ASSERT(iter != NULL);
  MTLK_ASSERT(iter->stadb != NULL);
  MTLK_ASSERT(iter->arr != NULL);

  if (iter->idx < iter->size) {
    sta = iter->arr[iter->idx];
    ++iter->idx;
  }

  return sta;
}

void __MTLK_IFUNC
mtlk_stadb_iterate_done (mtlk_stadb_iterator_t *iter)
{
  uint32 idx = 0;

  MTLK_ASSERT(iter != NULL);
  MTLK_ASSERT(iter->stadb != NULL);
  MTLK_ASSERT(iter->arr != NULL);

  for (idx = 0; idx < iter->size; idx++) {
    mtlk_sta_decref(iter->arr[idx]); /* De-reference by iterator */
  }
  mtlk_osal_mem_free(iter->arr);
  memset(iter, 0, sizeof(*iter));
}

void __MTLK_IFUNC
mtlk_stadb_reset_cnts (sta_db *stadb)
{
  mtlk_hash_enum_t              e;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  MTLK_ASSERT(stadb != NULL);

  mtlk_osal_lock_acquire(&stadb->lock);
  h = mtlk_hash_enum_first_ieee_addr(&stadb->hash, &e);
  while (h) {
    sta_entry *sta = MTLK_CONTAINER_OF(h, sta_entry, hentry);
    _mtlk_sta_reset_cnts(sta);
    h = mtlk_hash_enum_next_ieee_addr(&stadb->hash, &e);
  }
  mtlk_osal_lock_release(&stadb->lock);
}

void __MTLK_IFUNC
mtlk_stadb_disconnect_all (sta_db *stadb,
    mtlk_stadb_disconnect_sta_clb_f clb,
    mtlk_handle_t usr_ctx,
    BOOL wait_all_packets_confirmed)
{
  mtlk_hash_enum_t              e;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;
  sta_entry                  *sta;

  MTLK_ASSERT(stadb != NULL);
  MTLK_ASSERT(clb != NULL);

  mtlk_osal_lock_acquire(&stadb->lock);
  h = mtlk_hash_enum_first_ieee_addr(&stadb->hash, &e);
  while (h) {
    sta = MTLK_CONTAINER_OF(h, sta_entry, hentry);
    mtlk_sta_set_packets_filter(sta, MTLK_PCKT_FLTR_DISCARD_ALL);
    h = mtlk_hash_enum_next_ieee_addr(&stadb->hash, &e);
  }
  mtlk_osal_lock_release(&stadb->lock);

  if (wait_all_packets_confirmed) {
    h = mtlk_hash_enum_first_ieee_addr(&stadb->hash, &e);
    while (h) {
      int res;
      sta = MTLK_CONTAINER_OF(h, sta_entry, hentry);
      /* wait till all messages to MAC to be confirmed */
      res = mtlk_sq_wait_all_packets_confirmed(mtlk_sta_get_sq(sta));
      if (MTLK_ERR_OK != res) {
        WLOG_DY("CID-%04x: STA:%Y wait time for all MSDUs confirmation expired",
                mtlk_vap_get_oid(sta->vap_handle),
                mtlk_sta_get_addr(sta));

        mtlk_vap_get_hw_vft(sta->vap_handle)->set_prop(sta->vap_handle, MTLK_HW_RESET, NULL, 0);
        break;
      }
      h = mtlk_hash_enum_next_ieee_addr(&stadb->hash, &e);
    }
  }

  mtlk_osal_lock_acquire(&stadb->lock);
  h = mtlk_hash_enum_first_ieee_addr(&stadb->hash, &e);
  while (h) {
    sta = MTLK_CONTAINER_OF(h, sta_entry, hentry);

    clb(usr_ctx, sta); // replace with remove_sta below

    _mtlk_stadb_remove_sta_internal(stadb, sta);

    h = mtlk_hash_enum_next_ieee_addr(&stadb->hash, &e);
  }
  mtlk_osal_lock_release(&stadb->lock);
}

void __MTLK_IFUNC
mtlk_stadb_iterate_addba (sta_db *stadb)
{
  mtlk_hash_enum_t              e;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  MTLK_ASSERT(stadb != NULL);

  mtlk_osal_lock_acquire(&stadb->lock);
  h = mtlk_hash_enum_first_ieee_addr(&stadb->hash, &e);
  while (h) {
    sta_entry *sta = MTLK_CONTAINER_OF(h, sta_entry, hentry);

    mtlk_addba_peer_iterate(mtlk_sta_get_addb_peer(sta));

    h = mtlk_hash_enum_next_ieee_addr(&stadb->hash, &e);
  }
  mtlk_osal_lock_release(&stadb->lock);
}

int __MTLK_IFUNC
mtlk_stadb_get_stat(sta_db *stadb, hst_db *hstdb, mtlk_clpb_t *clpb, uint8 group_cipher)
{
  int                   res = MTLK_ERR_OK;
  const sta_entry       *sta;
  mtlk_stadb_iterator_t iter;
  mtlk_stadb_stat_t     stadb_stat;
  uint8                 tid;
  mtlk_hstdb_iterator_t hiter;
  int bridge_mode;

  bridge_mode = mtlk_pdb_get_int(mtlk_vap_get_param_db(stadb->vap_handle), PARAM_DB_CORE_BRIDGE_MODE);

  sta = mtlk_stadb_iterate_first(stadb, &iter);
  if (sta) {
    do {

      /* Omit Open-mode STAs if security enabled */
      if (!group_cipher || mtlk_sta_get_cipher(sta)) {

        /* Get general STA statistic */
        memset(&stadb_stat, 0, sizeof(stadb_stat));

        stadb_stat.type = STAT_ID_STADB;
        stadb_stat.u.general_stat.addr = *(mtlk_sta_get_addr(sta));
        stadb_stat.u.general_stat.sta_rx_packets = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_PACKETS_RECEIVED);
        stadb_stat.u.general_stat.sta_tx_packets = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_PACKETS_SENT);
        stadb_stat.u.general_stat.sta_rx_dropped = 0;
        stadb_stat.u.general_stat.network_mode = mtlk_sta_get_net_mode(sta);
        stadb_stat.u.general_stat.tx_rate = mtlk_sta_get_tx_rate(sta);

        for (tid = 0; tid < NTS_TIDS; tid++) {
          if (!mtlk_sta_get_rod_stats(sta, tid,
                  &stadb_stat.u.general_stat.reordering_stats[tid].reord_stat)) {
            stadb_stat.u.general_stat.reordering_stats[tid].used = FALSE;
            continue;
          } else {
            stadb_stat.u.general_stat.reordering_stats[tid].used = TRUE;
          }
        }

        /* push stadb statistic to clipboard */
        res = mtlk_clpb_push(clpb, &stadb_stat, sizeof(stadb_stat));
        if (MTLK_ERR_OK != res) {
          goto err_push_sta;
        }

        /* Get HostDB statistic related to current STA */
        if ((NULL != hstdb) && (BR_MODE_WDS == bridge_mode)) {

          const IEEE_ADDR *host_addr = mtlk_hstdb_iterate_first(hstdb, sta, &hiter);
          if (host_addr) {
            do {
              memset(&stadb_stat, 0, sizeof(stadb_stat));
              stadb_stat.type = STAT_ID_HSTDB;
              stadb_stat.u.hstdb_stat.addr = *host_addr;

              /* push to clipboard*/
              res = mtlk_clpb_push(clpb, &stadb_stat, sizeof(stadb_stat));
              if (MTLK_ERR_OK != res) {
                goto err_push_host;
              }

              host_addr = mtlk_hstdb_iterate_next(&hiter);
            } while (host_addr);
            mtlk_hstdb_iterate_done(&hiter);
          }
        }
      }

      sta = mtlk_stadb_iterate_next(&iter);
    } while (sta);
    mtlk_stadb_iterate_done(&iter);
  }

  return res;

err_push_host:
  mtlk_hstdb_iterate_done(&hiter);
err_push_sta:
  mtlk_stadb_iterate_done(&iter);
  mtlk_clpb_purge(clpb);
  return res;
}

BOOL __MTLK_IFUNC
mtlk_stadb_is_empty(sta_db *stadb)
{
  BOOL res;

  mtlk_osal_lock_acquire(&stadb->lock);
  res = (stadb->hash_cnt == 0);
  mtlk_osal_lock_release(&stadb->lock);

  return res;
}

/******************************************************************************************
 * HST API
 ******************************************************************************************/
typedef struct _host_entry {
  MTLK_HASH_ENTRY_T(ieee_addr) hentry;
  mtlk_osal_timer_t            idle_timer;
  mtlk_osal_timestamp_t        timestamp;
  sta_entry                   *sta;
  IEEE_ADDR                    sta_addr;
  struct _hst_db              *paramdb;
  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_START_STATUS;
} host_entry;

static __INLINE const IEEE_ADDR *
_mtlk_hst_get_addr (const host_entry *hst)
{
  return MTLK_HASH_VALUE_GET_KEY(hst, hentry);
}

static __INLINE BOOL
_mtlk_is_hst_behind_sta (const host_entry *host, const sta_entry *sta)
{
  return (host->sta == sta);
}

MTLK_INIT_STEPS_LIST_BEGIN(host_entry)
  MTLK_INIT_STEPS_LIST_ENTRY(host_entry, WDS_TIMER)
MTLK_INIT_INNER_STEPS_BEGIN(host_entry)
MTLK_INIT_STEPS_LIST_END(host_entry);

MTLK_START_STEPS_LIST_BEGIN(host_entry)
  MTLK_START_STEPS_LIST_ENTRY(host_entry, STA_REF)
  MTLK_START_STEPS_LIST_ENTRY(host_entry, WDS_TIMER)
MTLK_START_INNER_STEPS_BEGIN(host_entry)
MTLK_START_STEPS_LIST_END(host_entry);

static void
_mtlk_hst_cleanup (host_entry *host)
{
  MTLK_CLEANUP_BEGIN(host_entry, MTLK_OBJ_PTR(host))
    MTLK_CLEANUP_STEP(host_entry, WDS_TIMER, MTLK_OBJ_PTR(host),
                      mtlk_osal_timer_cleanup, (&host->idle_timer));
  MTLK_CLEANUP_END(host_entry, MTLK_OBJ_PTR(host))
}

static int
_mtlk_hst_init (host_entry *host, hst_db *paramdb)
{
  memset(host, 0, sizeof(host_entry));

  host->paramdb = paramdb;

  MTLK_INIT_TRY(host_entry, MTLK_OBJ_PTR(host))
    MTLK_INIT_STEP(host_entry, WDS_TIMER, MTLK_OBJ_PTR(host),
                   mtlk_osal_timer_init, (&host->idle_timer,
                                          mtlk_core_wds_on_host_disconnect_tmr,
                                          HANDLE_T(host)));
  MTLK_INIT_FINALLY(host_entry, MTLK_OBJ_PTR(host))
  MTLK_INIT_RETURN(host_entry, MTLK_OBJ_PTR(host), _mtlk_hst_cleanup, (host))
}

static __INLINE void
_mtlk_hst_stop (host_entry *host)
{

  MTLK_ASSERT(host->sta);

  MTLK_STOP_BEGIN(host_entry, MTLK_OBJ_PTR(host))
    MTLK_STOP_STEP(host_entry, WDS_TIMER, MTLK_OBJ_PTR(host),
                   mtlk_osal_timer_set, (&host->idle_timer, HOST_INACTIVE_TIMEOUT * 1000 ));
    MTLK_STOP_STEP(host_entry, STA_REF, MTLK_OBJ_PTR(host),
                   mtlk_sta_decref, (host->sta)); /* De-reference by HST */
  MTLK_STOP_END(host_entry, MTLK_OBJ_PTR(host))
  host->sta = NULL;
}

static __INLINE int
_mtlk_hst_start (host_entry *host, sta_entry *sta)
{
  host->timestamp = mtlk_osal_timestamp();
  host->sta       = sta;
  memcpy(&host->sta_addr, mtlk_sta_get_addr(sta), sizeof(host->sta_addr));

  MTLK_START_TRY(host_entry, MTLK_OBJ_PTR(host))
    MTLK_START_STEP_VOID(host_entry, STA_REF, MTLK_OBJ_PTR(host),
                         mtlk_sta_incref, (host->sta)); /* Reference by HST */
    MTLK_START_STEP(host_entry, WDS_TIMER, MTLK_OBJ_PTR(host),
                    mtlk_osal_timer_set, (&host->idle_timer,
                                          (host->paramdb->wds_host_timeout * 1000) ? : DEFAULT_KEEPALIVE_TIMEOUT));
  MTLK_START_FINALLY(host_entry, MTLK_OBJ_PTR(host))
  MTLK_START_RETURN(host_entry, MTLK_OBJ_PTR(host), _mtlk_hst_stop, (host))
}

static __INLINE void
_mtlk_hst_update (host_entry *host, sta_entry *sta)
{

  ILOG3_YD("Update HOST %Y - %lu", _mtlk_hst_get_addr(host), host->timestamp);

  if (!_mtlk_is_hst_behind_sta(host, sta)){

    if (host->sta){
      /* Host was associated previously with another STA */
      ILOG2_YY("HOST %Y is stopped with STA %Y on UPDATE", _mtlk_hst_get_addr(host)->au8Addr, host->sta_addr.au8Addr);
      _mtlk_hst_stop(host);
    }

    ILOG2_YY("HOST %Y is started with STA %Y on UPDATE", _mtlk_hst_get_addr(host)->au8Addr, mtlk_sta_get_addr(sta)->au8Addr);
    _mtlk_hst_start(host, sta);
  }
  else{
    host->timestamp = mtlk_osal_timestamp();
  }

}
/******************************************************************************************/

/******************************************************************************************
 * HST DB API
 ******************************************************************************************/
#define MTLK_HSTDB_HASH_NOF_BUCKETS 16

static void
_mtlk_hstdb_recalc_default_host_unsafe (hst_db *hstdb)
{
  mtlk_hash_enum_t              e;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;
  host_entry                   *newest_hst = NULL;
  uint32                        smallest_diff = (uint32)-1;
    
  h = mtlk_hash_enum_first_ieee_addr(&hstdb->hash, &e);
  while (h)
  {
    host_entry *hst  = MTLK_CONTAINER_OF(h, host_entry, hentry);
    uint32      diff = mtlk_osal_timestamp_diff(mtlk_osal_timestamp(), hst->timestamp);

    if (hst->sta != NULL){
      if (diff < smallest_diff) {
        smallest_diff = diff;
        newest_hst    = hst;
      }
    }

    h = mtlk_hash_enum_next_ieee_addr(&hstdb->hash, &e);
  }

  if (newest_hst == NULL) {
    memset(hstdb->default_host, 0, sizeof(hstdb->default_host));
  }
  else {
    mtlk_osal_copy_eth_addresses(hstdb->default_host, _mtlk_hst_get_addr(newest_hst)->au8Addr);
  }

  ILOG3_Y("default_host %Y", hstdb->default_host);
}

static void
_mtlk_hstdb_add_host (hst_db* hstdb, const unsigned char *mac,
                      sta_entry *sta)
{
  int         res = MTLK_ERR_UNKNOWN;
  host_entry *hst = NULL;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  hst = mtlk_osal_mem_alloc(sizeof(*hst), MTLK_MEM_TAG_STADB_ITER);
  if (!hst) {
    ELOG_V("Can't allocate host!");
    res = MTLK_ERR_NO_MEM;
    goto err_alloc;
  }

  res = _mtlk_hst_init(hst, hstdb);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Can't start Host (err#%d)", res);
    goto err_init;
  }

  ILOG2_YY("HOST %Y started with STA %Y on CREATE", ((IEEE_ADDR *)mac)->au8Addr, mtlk_sta_get_addr(sta)->au8Addr);
  res = _mtlk_hst_start(hst, sta);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Can't start Host (err#%d)", res);
    goto err_start;
  }

  mtlk_osal_lock_acquire(&hstdb->lock);
  h = mtlk_hash_insert_ieee_addr(&hstdb->hash, (IEEE_ADDR *)mac, &hst->hentry);
  if (h == NULL) {
    ++hstdb->hash_cnt;
  }
  mtlk_osal_lock_release(&hstdb->lock);

  if (h != NULL) {
    res = MTLK_ERR_PROHIB;
    WLOG_V("Already registered");
    goto err_insert;
  }

  mtlk_hstdb_update_default_host(hstdb, mac);

  ILOG2_YPY("HOST %Y added (%p), belongs to STA %Y", mac, hst, hst->sta_addr.au8Addr);

  return;

err_insert:
  _mtlk_hst_stop(hst);
err_start:
  _mtlk_hst_cleanup(hst);
err_init:
  mtlk_osal_mem_free(hst);
err_alloc:
  return;
}

void __MTLK_IFUNC
mtlk_hstdb_update_default_host (hst_db* hstdb, const unsigned char *mac)
{
  if (!mtlk_osal_is_valid_ether_addr(hstdb->default_host) ||
       0 == mtlk_osal_compare_eth_addresses(hstdb->default_host, hstdb->local_mac)) {
    mtlk_osal_copy_eth_addresses(hstdb->default_host, mac);
    ILOG3_Y("default_host %Y", hstdb->default_host);
  }
}

void __MTLK_IFUNC
mtlk_hstdb_update_host (hst_db* hstdb, const unsigned char *mac, sta_entry *sta)
{
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  mtlk_osal_lock_acquire(&hstdb->lock);
  h = mtlk_hash_find_ieee_addr(&hstdb->hash, (IEEE_ADDR *)mac);
  mtlk_osal_lock_release(&hstdb->lock);
  if (h) {
    host_entry *host = MTLK_CONTAINER_OF(h, host_entry, hentry);
    /* Station already associated with HOST - just update timestamp */
    _mtlk_hst_update(host, sta);
  }
  else {

    ILOG2_V("Can't find peer (HOST), adding...");
    _mtlk_hstdb_add_host(hstdb, mac, sta);
  }
}

static void
_mtlk_hstdb_remove_host_unsafe (hst_db *hstdb, host_entry *host, const IEEE_ADDR *mac)
{
  MTLK_ASSERT(NULL != hstdb);
  MTLK_ASSERT(NULL != host);
  MTLK_ASSERT(NULL != mac);

  if (host->sta){
    ILOG2_YY("HOST %Y stopped on STA %Y on REMOVE", _mtlk_hst_get_addr(host)->au8Addr, host->sta_addr.au8Addr);
    _mtlk_hst_stop(host);
  }

  _mtlk_hst_cleanup(host);
  mtlk_osal_mem_free(host);

  ILOG2_Y("HOST %Y removed", mac->au8Addr);
}

/* Function should be used with HSTDB lock acquired by caller */
static void
_mtlk_hstdb_hash_remove_ieee_addr_internal(hst_db *hstdb, host_entry *host)
{
  MTLK_ASSERT(NULL != hstdb);
  MTLK_ASSERT(NULL != host);

  --hstdb->hash_cnt;
  mtlk_hash_remove_ieee_addr(&hstdb->hash, &host->hentry);
}

static void
_mtlk_hstdb_remove_host (hst_db *hstdb, host_entry *host)
{
  /* WARNING: We store the MAC address here since it may
  *           become corrupted after mtlk_hash_remove_hstdb()
  */
  const IEEE_ADDR mac = *_mtlk_hst_get_addr(host);

  mtlk_osal_lock_acquire(&hstdb->lock);
  _mtlk_hstdb_hash_remove_ieee_addr_internal(hstdb, host);
  /* are we deleting default host? */
  if (0 == mtlk_osal_compare_eth_addresses(hstdb->default_host, mac.au8Addr)) {
    _mtlk_hstdb_recalc_default_host_unsafe(hstdb);
  }
  mtlk_osal_lock_release(&hstdb->lock);

  /* The host isn't in the DB anymore, so nobody can access it =>
   * we can remove it with no locking
   */
  _mtlk_hstdb_remove_host_unsafe(hstdb, host, &mac);
}

int __MTLK_IFUNC
mtlk_hstdb_remove_all_by_sta (hst_db *hstdb, const sta_entry *sta)
{
  /* For L2NAT only! not applicable for WDS */

  mtlk_hash_enum_t              e;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;
  BOOL                          recalc_def_host = FALSE;

  mtlk_osal_lock_acquire(&hstdb->lock);
  h = mtlk_hash_enum_first_ieee_addr(&hstdb->hash, &e);
  while (h)
  {
    host_entry *hst  = MTLK_CONTAINER_OF(h, host_entry, hentry);
    if (_mtlk_is_hst_behind_sta(hst, sta)) {
      /* WARNING: We store the MAC address here since it may
      *           become corrupted after mtlk_hash_remove_hstdb()
      */
      const IEEE_ADDR mac = *_mtlk_hst_get_addr(hst);

      /* Call "internal" functions to remove host, they don't use locks */
      _mtlk_hstdb_hash_remove_ieee_addr_internal(hstdb, hst);
      _mtlk_hstdb_remove_host_unsafe(hstdb, hst, &mac);

      /* are we deleting default host? */
      if (FALSE == recalc_def_host && 
          0 == mtlk_osal_compare_eth_addresses(hstdb->default_host, mac.au8Addr)) {
        recalc_def_host = TRUE;
      }

    }
    h = mtlk_hash_enum_next_ieee_addr(&hstdb->hash, &e);
  }

  if (recalc_def_host) {
    _mtlk_hstdb_recalc_default_host_unsafe(hstdb);
  }
  mtlk_osal_lock_release(&hstdb->lock);

  return MTLK_ERR_OK;
}

void __MTLK_IFUNC
mtlk_hstdb_remove_host_by_addr (hst_db *hstdb, const IEEE_ADDR *mac)
{
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  mtlk_osal_lock_acquire(&hstdb->lock);
  h = mtlk_hash_find_ieee_addr(&hstdb->hash, mac);
  if (h) {
    host_entry *host = MTLK_CONTAINER_OF(h, host_entry, hentry);
    _mtlk_hstdb_hash_remove_ieee_addr_internal(hstdb, host);
    mtlk_osal_lock_release(&hstdb->lock);

    /* The host isn't in the DB anymore, so nobody can access it =>
     * we can remove it with no locking
     */
    _mtlk_hstdb_remove_host_unsafe(hstdb, host, mac);
  }
  else {
    mtlk_osal_lock_release(&hstdb->lock);
  }
}

static int
_mtlk_hstdb_cleanup_all_hst_unsafe (hst_db *hstdb)
{
  /* Remove all existing entries from DB. Called on slow context cleanup only.
     All entries must be already stopped at this point */

  mtlk_hash_enum_t              e;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  h = mtlk_hash_enum_first_ieee_addr(&hstdb->hash, &e);
  while (h)
  {
    host_entry *hst  = MTLK_CONTAINER_OF(h, host_entry, hentry);
    MTLK_ASSERT(hst->sta == NULL);
    ILOG2_Y("WDS HOST Clean up on DESTROY %Y ", _mtlk_hst_get_addr(hst)->au8Addr);
    _mtlk_hstdb_hash_remove_ieee_addr_internal(hstdb, hst);
    _mtlk_hst_cleanup(hst);
    mtlk_osal_mem_free(hst);

    h = mtlk_hash_enum_next_ieee_addr(&hstdb->hash, &e);
  }

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_hstdb_stop_all_by_sta (hst_db *hstdb, const sta_entry *sta)
{
  /* For WDS only! not applicable for L2NAT */

  mtlk_hash_enum_t              e;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  mtlk_osal_lock_acquire(&hstdb->lock);
  h = mtlk_hash_enum_first_ieee_addr(&hstdb->hash, &e);
  while (h)
  {
    host_entry *hst  = MTLK_CONTAINER_OF(h, host_entry, hentry);
    if (_mtlk_is_hst_behind_sta(hst, sta)){
      ILOG2_YY("HOST %Y on STA %Y is stopped", _mtlk_hst_get_addr(hst)->au8Addr, mtlk_sta_get_addr(sta)->au8Addr);
      _mtlk_hst_stop(hst);
    }

    h = mtlk_hash_enum_next_ieee_addr(&hstdb->hash, &e);

  }

  mtlk_osal_lock_release(&hstdb->lock);

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_hstdb_start_all_by_sta (hst_db *hstdb, sta_entry *sta)
{
  /* For WDS only! not applicable for L2NAT.
     Associate existing HOST entries with just connected STA
     if previous STA address is the same */

  mtlk_hash_enum_t              e;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;
  const IEEE_ADDR              *sta_addr;

  sta_addr = mtlk_sta_get_addr(sta);
  mtlk_osal_lock_acquire(&hstdb->lock);
  h = mtlk_hash_enum_first_ieee_addr(&hstdb->hash, &e);
  while (h)
  {
    host_entry *hst  = MTLK_CONTAINER_OF(h, host_entry, hentry);

    if (0 == mtlk_osal_compare_eth_addresses(hst->sta_addr.au8Addr, sta_addr->au8Addr)){
      ILOG2_YY("HOST Entry reassociated %Y to STA %Y on CONNECT", _mtlk_hst_get_addr(hst)->au8Addr, hst->sta_addr.au8Addr);
      _mtlk_hst_start(hst, sta);
    }

    h = mtlk_hash_enum_next_ieee_addr(&hstdb->hash, &e);

  }
  mtlk_osal_lock_release(&hstdb->lock);

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_hstdb_ppa_remove_all_by_sta(hst_db *hstdb, const sta_entry *sta)
{
  mtlk_hash_enum_t              e;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  ILOG3_Y("Remove all HOST addresses from switch table on STA %Y", mtlk_sta_get_addr(sta)->au8Addr);

  mtlk_osal_lock_acquire(&hstdb->lock);
  h = mtlk_hash_enum_first_ieee_addr(&hstdb->hash, &e);
  while (h)
  {
    host_entry *hst  = MTLK_CONTAINER_OF(h, host_entry, hentry);

    /* Remove host MAC addr from switch MAC table */
    mtlk_df_user_ppa_remove_mac_addr(mtlk_vap_get_df(hstdb->vap_handle), _mtlk_hst_get_addr(hst)->au8Addr);

    h = mtlk_hash_enum_next_ieee_addr(&hstdb->hash, &e);
  }

  mtlk_osal_lock_release(&hstdb->lock);

  return MTLK_ERR_OK;
}

mtlk_vap_handle_t __MTLK_IFUNC
mtlk_host_get_vap_handle (mtlk_handle_t host_handle)
{
  host_entry  *host;

  MTLK_ASSERT(host_handle);

  host = HANDLE_T_PTR(host_entry, host_handle);
  return (host->paramdb->vap_handle);
}

int __MTLK_IFUNC
mtlk_hstdb_dbg (hst_db *hstdb)
{
  /* Printout host DB entries with STA associated with it
     as well as STA's MAC address and time since last incoming
     packet from that host */

  mtlk_hash_enum_t              e;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  mtlk_osal_timestamp_diff_t diff;
  mtlk_osal_msec_t msecs;
  mtlk_osal_timestamp_t timestamp;

  timestamp = mtlk_osal_timestamp();

  mtlk_osal_lock_acquire(&hstdb->lock);
  h = mtlk_hash_enum_first_ieee_addr(&hstdb->hash, &e);
  WLOG_V(" | Host              | STA mac           |  STA       | T.diff (ms)");
  WLOG_V(" |===================|===================|============|============");
/*WLOG_V(" | 11:22:33:44:55:66 | 11:22:33:44:55:66 | 0x12345678 | 123...     ");*/
  while (h)
  {
    host_entry *hst  = MTLK_CONTAINER_OF(h, host_entry, hentry);

    diff = mtlk_osal_timestamp_diff(timestamp, hst->timestamp);
    msecs = mtlk_osal_timestamp_to_ms(diff);

    if (hst->sta){
      WLOG_YYDD(" | %Y | %Y | 0x%08X | %u", _mtlk_hst_get_addr(hst)->au8Addr, hst->sta_addr.au8Addr, (uint32)hst->sta, msecs);
    }
    else {
      WLOG_YYD (" | %Y | %Y |    NULL    | %u", _mtlk_hst_get_addr(hst)->au8Addr, hst->sta_addr.au8Addr, msecs);
    }
    h = mtlk_hash_enum_next_ieee_addr(&hstdb->hash, &e);
  }

  mtlk_osal_lock_release(&hstdb->lock);

  return MTLK_ERR_OK;
}

int mtlk_host_wds_disconnect_tmr (mtlk_handle_t host_handle, const void *data,  uint32 data_size)
{

  /* Note: Called within Serializer context
           Serialized by the core when wds host timer triggers */

  host_entry  *host;
  mtlk_core_t *core;
  mtlk_osal_timestamp_diff_t diff;
  mtlk_osal_msec_t msecs, timeout;

  host = HANDLE_T_PTR(host_entry, host_handle);
  core = mtlk_vap_get_core(host->paramdb->vap_handle);

  timeout = host->paramdb->wds_host_timeout * 1000;

  /* Remove HOST from entry if not associated too long */
  if (host->sta == NULL){
    ILOG2_Y("HOST %Y Inactive timeout", _mtlk_hst_get_addr(host));
    _mtlk_hstdb_remove_host(host->paramdb, host);
    return MTLK_ERR_OK;
  }

  if (timeout == 0){
    mtlk_osal_timer_set (&host->idle_timer, DEFAULT_KEEPALIVE_TIMEOUT);
    return MTLK_ERR_OK;
  }

  diff = mtlk_osal_timestamp_diff(mtlk_osal_timestamp(), host->timestamp);
  msecs = mtlk_osal_timestamp_to_ms(diff);

  if (msecs >= timeout) {
    ILOG2_YDD("WDS host %Y lost, idle time %u msecs (>%u)",
              _mtlk_hst_get_addr(host), msecs, timeout);
    _mtlk_hstdb_remove_host(host->paramdb, host);
    /* do not restart timer */
    return MTLK_ERR_OK;

  } else {
    /* restart with adjusted timeout */
    mtlk_osal_timer_set(&host->idle_timer, (timeout - msecs));
    return MTLK_ERR_OK;
  }
}

MTLK_INIT_STEPS_LIST_BEGIN(hst_db)
  MTLK_INIT_STEPS_LIST_ENTRY(hst_db, HSTDB_HASH)
  MTLK_INIT_STEPS_LIST_ENTRY(hst_db, HSTDB_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(hst_db, REG_ABILITIES)
  MTLK_INIT_STEPS_LIST_ENTRY(hst_db, EN_ABILITIES)
MTLK_INIT_INNER_STEPS_BEGIN(hst_db)
MTLK_INIT_STEPS_LIST_END(hst_db);

static const mtlk_ability_id_t _hstdb_abilities[] = {
  MTLK_CORE_REQ_GET_HSTDB_CFG,
  MTLK_CORE_REQ_SET_HSTDB_CFG
};

int __MTLK_IFUNC
mtlk_hstdb_init (hst_db *hstdb, mtlk_vap_handle_t vap_handle)
{
  hstdb->wds_host_timeout = 0;

  hstdb->vap_handle = vap_handle;

  MTLK_INIT_TRY(hst_db, MTLK_OBJ_PTR(hstdb))
    MTLK_INIT_STEP(hst_db, HSTDB_HASH, MTLK_OBJ_PTR(hstdb),
                   mtlk_hash_init_ieee_addr, (&hstdb->hash, MTLK_HSTDB_HASH_NOF_BUCKETS));
    MTLK_INIT_STEP(hst_db, HSTDB_LOCK, MTLK_OBJ_PTR(hstdb),
                   mtlk_osal_lock_init, (&hstdb->lock));
    MTLK_INIT_STEP(hst_db, REG_ABILITIES, MTLK_OBJ_PTR(hstdb),
                   mtlk_abmgr_register_ability_set,
                   (mtlk_vap_get_abmgr(hstdb->vap_handle), _hstdb_abilities, ARRAY_SIZE(_hstdb_abilities)));
    MTLK_INIT_STEP_VOID(hst_db, EN_ABILITIES, MTLK_OBJ_PTR(hstdb),
                        mtlk_abmgr_enable_ability_set,
                        (mtlk_vap_get_abmgr(hstdb->vap_handle), _hstdb_abilities, ARRAY_SIZE(_hstdb_abilities)));
  MTLK_INIT_FINALLY(hst_db, MTLK_OBJ_PTR(hstdb))
  MTLK_INIT_RETURN(hst_db, MTLK_OBJ_PTR(hstdb), mtlk_hstdb_cleanup, (hstdb))
}

void __MTLK_IFUNC
mtlk_hstdb_cleanup (hst_db *hstdb)
{

  _mtlk_hstdb_cleanup_all_hst_unsafe(hstdb);

  MTLK_CLEANUP_BEGIN(hst_db, MTLK_OBJ_PTR(hstdb))
    MTLK_CLEANUP_STEP(hst_db, EN_ABILITIES, MTLK_OBJ_PTR(hstdb),
                       mtlk_abmgr_disable_ability_set,
                       (mtlk_vap_get_abmgr(hstdb->vap_handle), _hstdb_abilities, ARRAY_SIZE(_hstdb_abilities)));
    MTLK_CLEANUP_STEP(hst_db, REG_ABILITIES, MTLK_OBJ_PTR(hstdb),
                       mtlk_abmgr_unregister_ability_set,
                       (mtlk_vap_get_abmgr(hstdb->vap_handle), _hstdb_abilities, ARRAY_SIZE(_hstdb_abilities)));
    MTLK_CLEANUP_STEP(hst_db, HSTDB_LOCK, MTLK_OBJ_PTR(hstdb),
                      mtlk_osal_lock_cleanup, (&hstdb->lock));
    MTLK_CLEANUP_STEP(hst_db, HSTDB_HASH, MTLK_OBJ_PTR(hstdb),
                      mtlk_hash_cleanup_ieee_addr, (&hstdb->hash));
  MTLK_CLEANUP_END(hst_db, MTLK_OBJ_PTR(hstdb))
}

sta_entry * __MTLK_IFUNC
mtlk_hstdb_find_sta (hst_db* hstdb, const unsigned char *mac)
{
  sta_entry                    *sta = NULL;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  mtlk_osal_lock_acquire(&hstdb->lock);
  h = mtlk_hash_find_ieee_addr(&hstdb->hash, (IEEE_ADDR *)mac);
  if (h) {
    host_entry *host = MTLK_CONTAINER_OF(h, host_entry, hentry);

    if (host->sta){
      ILOG3_YPP("Found host %Y, STA %p (%p)",
        _mtlk_hst_get_addr(host), host->sta, host);
      sta = host->sta;
      mtlk_sta_incref(sta); /* Reference by caller */
    }
  }
  mtlk_osal_lock_release(&hstdb->lock);

  return sta;
}

int __MTLK_IFUNC mtlk_hstdb_set_local_mac (hst_db              *hstdb,
                                           const unsigned char *mac)
{
  int res = MTLK_ERR_OK;

  if (mtlk_osal_is_valid_ether_addr(mac))
    mtlk_osal_copy_eth_addresses(hstdb->local_mac, mac);
  else
    res = MTLK_ERR_PARAMS;
  return res;
}

void __MTLK_IFUNC mtlk_hstdb_get_local_mac (hst_db        *hstdb,
                                            unsigned char *mac)
{
  mtlk_osal_copy_eth_addresses(mac, hstdb->local_mac);
}

const IEEE_ADDR * __MTLK_IFUNC
mtlk_hstdb_iterate_first (hst_db *hstdb, const sta_entry *sta, mtlk_hstdb_iterator_t *iter)
{
  int                           err  = MTLK_ERR_OK;
  const IEEE_ADDR              *addr = NULL;
  uint32                        idx  = 0;
  mtlk_hash_enum_t              e;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  MTLK_ASSERT(hstdb != NULL);
  MTLK_ASSERT(sta != NULL);
  MTLK_ASSERT(iter != NULL);

  mtlk_osal_lock_acquire(&hstdb->lock);

  iter->size = hstdb->hash_cnt;

  if (!iter->size) {
    err = MTLK_ERR_NOT_IN_USE;
    goto end;
  }

  iter->addr = (IEEE_ADDR *)mtlk_osal_mem_alloc(iter->size * sizeof(IEEE_ADDR), MTLK_MEM_TAG_STADB_ITER);
  if (!iter->addr) {
    ELOG_D("Can't allocate iteration array of %d entries", iter->size);
    err = MTLK_ERR_NO_MEM;
    goto end;
  }

  memset(iter->addr, 0, iter->size * sizeof(IEEE_ADDR));

  h = mtlk_hash_enum_first_ieee_addr(&hstdb->hash, &e);
  while (idx < iter->size && h) {
    const host_entry *host = MTLK_CONTAINER_OF(h, host_entry, hentry);
    if (_mtlk_is_hst_behind_sta(host, sta)) {
      iter->addr[idx] = *_mtlk_hst_get_addr(host);
      ++idx;
    }
    h = mtlk_hash_enum_next_ieee_addr(&hstdb->hash, &e);
  }

  err = MTLK_ERR_OK;

end:
  mtlk_osal_lock_release(&hstdb->lock);

  if (err == MTLK_ERR_OK) {
    iter->idx   = 0;

    addr = mtlk_hstdb_iterate_next(iter);
    if (!addr) {
      mtlk_osal_mem_free(iter->addr);
    }
  }

  return addr;
}

const IEEE_ADDR * __MTLK_IFUNC
mtlk_hstdb_iterate_next (mtlk_hstdb_iterator_t *iter)
{
  const IEEE_ADDR *addr = NULL;

  MTLK_ASSERT(iter != NULL);
  MTLK_ASSERT(iter->addr != NULL);

  if (iter->idx < iter->size &&
      !mtlk_osal_is_zero_address(iter->addr[iter->idx].au8Addr)) {
    addr = &iter->addr[iter->idx];
    ++iter->idx;
  }

  return addr;
}

void __MTLK_IFUNC
mtlk_hstdb_iterate_done (mtlk_hstdb_iterator_t *iter)
{
  MTLK_ASSERT(iter != NULL);
  MTLK_ASSERT(iter->addr != NULL);

  mtlk_osal_mem_free(iter->addr);
  memset(iter, 0, sizeof(*iter));
}
/******************************************************************************************/

int __MTLK_IFUNC
mtlk_stadb_addba_status(sta_db *stadb, mtlk_clpb_t *clpb)
{
  int res = MTLK_ERR_OK;
  mtlk_hash_enum_t e;
  MTLK_HASH_ENTRY_T(ieee_addr) *h;

  MTLK_ASSERT(stadb != NULL);

  mtlk_osal_lock_acquire(&stadb->lock);
  h = mtlk_hash_enum_first_ieee_addr(&stadb->hash, &e);
  while (h) {
    sta_entry *sta = MTLK_CONTAINER_OF(h, sta_entry, hentry);

    res = mtlk_addba_peer_status(mtlk_sta_get_addb_peer(sta), clpb);
    if (MTLK_ERR_OK != res)
      break;

    h = mtlk_hash_enum_next_ieee_addr(&stadb->hash, &e);
  }
  mtlk_osal_lock_release(&stadb->lock);

  return res;
}

static void __MTLK_IFUNC
_mtlk_sta_ps_mode_cleanup(mtlk_sq_peer_ctx_t *ppeer, mtlk_vap_handle_t vap_handle)
{
  if (NULL == ppeer->vap_handle && mtlk_osal_atomic_get(&ppeer->ps_mode_enabled)) {
    mtlk_core_set_pm_enabled(mtlk_vap_get_core(vap_handle), FALSE);
  }
}

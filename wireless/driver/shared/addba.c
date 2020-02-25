/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/

#include "mtlkinc.h"
#include "addba.h"
#include "mtlkqos.h"
#include "mtlkaux.h"
#include "mtlk_core_iface.h"
#include "mtlk_coreui.h"

#define LOG_LOCAL_GID   GID_ADDBA
#define LOG_LOCAL_FID   1

/**********************************************************************
 * Local definitions
 ************************************************************************/

#define BA_DIR_ORIGINATOR  1
#define BA_DIR_RECIPIENT   0

static const uint32 _mtlk_addba_peer_cntr_wss_id_map[] = 
{
  MTLK_WWSS_WLAN_STAT_ID_AGGR_ACTIVE,                   /* MTLK_ADDBAPI_CNT_AGGR_ACTIVE */
  MTLK_WWSS_WLAN_STAT_ID_REORD_ACTIVE,                  /* MTLK_ADDBAPI_CNT_REORD_ACTIVE */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_REQ_RECEIVED,         /* MTLK_ADDBAPI_CNT_RX_ADDBA_REQ_RECEIVED */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_CFMD_FAIL,        /* MTLK_ADDBAPI_CNT_RX_ADDBA_RES_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_CFMD_SUCCESS,     /* MTLK_ADDBAPI_CNT_RX_ADDBA_RES_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_LOST,             /* MTLK_ADDBAPI_CNT_RX_ADDBA_RES_LOST */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_NEGATIVE_SENT,    /* MTLK_ADDBAPI_CNT_RX_ADDBA_RES_NEGATIVE_SENT */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_NOT_CFMD,         /* MTLK_ADDBAPI_CNT_RX_ADDBA_RES_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_POSITIVE_SENT,    /* MTLK_ADDBAPI_CNT_RX_ADDBA_RES_POSITIVE_SENT */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_REACHED,          /* MTLK_ADDBAPI_CNT_RX_ADDBA_RES_REACHED */
  MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_RETRANSMISSIONS,  /* MTLK_ADDBAPI_CNT_RX_ADDBA_RES_RETRANSMISSIONS */
  MTLK_WWSS_WLAN_STAT_ID_RX_BAR_WITHOUT_REORDERING,     /* MTLK_ADDBAPI_CNT_RX_BAR_WITHOUT_REORDERING */
  MTLK_WWSS_WLAN_STAT_ID_RX_AGGR_PKT_WITHOUT_REORDERING,/* MTLK_ADDBAPI_CNT_RX_AGGR_PKT_WITHOUT_REORDERING */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_CFMD_FAIL,        /* MTLK_ADDBAPI_CNT_RX_DELBA_REQ_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_CFMD_SUCCESS,     /* MTLK_ADDBAPI_CNT_RX_DELBA_REQ_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_LOST,             /* MTLK_ADDBAPI_CNT_RX_DELBA_REQ_LOST */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_NOT_CFMD,         /* MTLK_ADDBAPI_CNT_RX_DELBA_REQ_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_RCV,              /* MTLK_ADDBAPI_CNT_RX_DELBA_REQ_RCV */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_REACHED,          /* MTLK_ADDBAPI_CNT_RX_DELBA_REQ_REACHED */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_RETRANSMISSIONS,  /* MTLK_ADDBAPI_CNT_RX_DELBA_REQ_RETRANSMISSIONS */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_SENT,             /* MTLK_ADDBAPI_CNT_RX_DELBA_REQ_SENT */
  MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_SENT_BY_TIMEOUT,      /* MTLK_ADDBAPI_CNT_RX_DELBA_SENT_BY_TIMEOUT */
  MTLK_WWSS_WLAN_STAT_ID_TX_ACK_ON_BAR_DETECTED,        /* MTLK_ADDBAPI_CNT_TX_ACK_ON_BAR_DETECTED */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_CFMD_FAIL,        /* MTLK_ADDBAPI_CNT_TX_ADDBA_REQ_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_CFMD_SUCCESS,     /* MTLK_ADDBAPI_CNT_TX_ADDBA_REQ_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_NOT_CFMD,         /* MTLK_ADDBAPI_CNT_TX_ADDBA_REQ_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_SENT,             /* MTLK_ADDBAPI_CNT_TX_ADDBA_REQ_SENT */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_RCV_NEGATIVE,     /* MTLK_ADDBAPI_CNT_TX_ADDBA_RES_RCV_NEGATIVE */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_RCV_POSITIVE,     /* MTLK_ADDBAPI_CNT_TX_ADDBA_RES_RCV_POSITIVE */
  MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_TIMEOUT,          /* MTLK_ADDBAPI_CNT_TX_ADDBA_RES_TIMEOUT */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_CFMD_FAIL,       /* MTLK_ADDBAPI_CNT_TX_CLOSE_AGGR_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_CFMD_SUCCESS,    /* MTLK_ADDBAPI_CNT_TX_CLOSE_AGGR_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_NOT_CFMD,        /* MTLK_ADDBAPI_CNT_TX_CLOSE_AGGR_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_SENT,            /* MTLK_ADDBAPI_CNT_TX_CLOSE_AGGR_SENT */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_CFMD_FAIL,        /* MTLK_ADDBAPI_CNT_TX_DELBA_REQ_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_CFMD_SUCCESS,     /* MTLK_ADDBAPI_CNT_TX_DELBA_REQ_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_LOST,             /* MTLK_ADDBAPI_CNT_TX_DELBA_REQ_LOST */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_NOT_CFMD,         /* MTLK_ADDBAPI_CNT_TX_DELBA_REQ_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_RCV,              /* MTLK_ADDBAPI_CNT_TX_DELBA_REQ_RCV */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_REACHED,          /* MTLK_ADDBAPI_CNT_TX_DELBA_REQ_REACHED */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_RETRANSMISSIONS,  /* MTLK_ADDBAPI_CNT_TX_DELBA_REQ_RETRANSMISSIONS */
  MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_SENT,             /* MTLK_ADDBAPI_CNT_TX_DELBA_REQ_SENT */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_CFMD_FAIL,        /* MTLK_ADDBAPI_CNT_TX_OPEN_AGGR_CFMD_FAIL */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_CFMD_SUCCESS,     /* MTLK_ADDBAPI_CNT_TX_OPEN_AGGR_CFMD_SUCCESS */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_NOT_CFMD,         /* MTLK_ADDBAPI_CNT_TX_OPEN_AGGR_NOT_CFMD */
  MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_SENT,             /* MTLK_ADDBAPI_CNT_TX_OPEN_AGGR_SENT */
};

/* Statistic ALLOWED flags */
#define MTLK_ADDBAPI_CNT_AGGR_ACTIVE_ALLOWED                    MTLK_WWSS_WLAN_STAT_ID_AGGR_ACTIVE_ALLOWED
#define MTLK_ADDBAPI_CNT_REORD_ACTIVE_ALLOWED                   MTLK_WWSS_WLAN_STAT_ID_REORD_ACTIVE_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_ADDBA_REQ_RECEIVED_ALLOWED          MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_REQ_RECEIVED_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_ADDBA_RES_CFMD_FAIL_ALLOWED         MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_CFMD_FAIL_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_ADDBA_RES_CFMD_SUCCESS_ALLOWED      MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_CFMD_SUCCESS_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_ADDBA_RES_LOST_ALLOWED              MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_LOST_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_ADDBA_RES_NEGATIVE_SENT_ALLOWED     MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_NEGATIVE_SENT_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_ADDBA_RES_NOT_CFMD_ALLOWED          MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_NOT_CFMD_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_ADDBA_RES_POSITIVE_SENT_ALLOWED     MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_POSITIVE_SENT_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_ADDBA_RES_REACHED_ALLOWED           MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_REACHED_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_ADDBA_RES_RETRANSMISSIONS_ALLOWED   MTLK_WWSS_WLAN_STAT_ID_RX_ADDBA_RES_RETRANSMISSIONS_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_BAR_WITHOUT_REORDERING_ALLOWED      MTLK_WWSS_WLAN_STAT_ID_RX_BAR_WITHOUT_REORDERING_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_AGGR_PKT_WITHOUT_REORDERING_ALLOWED MTLK_WWSS_WLAN_STAT_ID_RX_AGGR_PKT_WITHOUT_REORDERING_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_DELBA_REQ_CFMD_FAIL_ALLOWED         MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_CFMD_FAIL_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_DELBA_REQ_CFMD_SUCCESS_ALLOWED      MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_CFMD_SUCCESS_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_DELBA_REQ_LOST_ALLOWED              MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_LOST_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_DELBA_REQ_NOT_CFMD_ALLOWED          MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_NOT_CFMD_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_DELBA_REQ_RCV_ALLOWED               MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_RCV_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_DELBA_REQ_REACHED_ALLOWED           MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_REACHED_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_DELBA_REQ_RETRANSMISSIONS_ALLOWED   MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_RETRANSMISSIONS_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_DELBA_REQ_SENT_ALLOWED              MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_REQ_SENT_ALLOWED
#define MTLK_ADDBAPI_CNT_RX_DELBA_SENT_BY_TIMEOUT_ALLOWED       MTLK_WWSS_WLAN_STAT_ID_RX_DELBA_SENT_BY_TIMEOUT_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_ACK_ON_BAR_DETECTED_ALLOWED         MTLK_WWSS_WLAN_STAT_ID_TX_ACK_ON_BAR_DETECTED_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_ADDBA_REQ_CFMD_FAIL_ALLOWED         MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_CFMD_FAIL_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_ADDBA_REQ_CFMD_SUCCESS_ALLOWED      MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_CFMD_SUCCESS_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_ADDBA_REQ_NOT_CFMD_ALLOWED          MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_NOT_CFMD_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_ADDBA_REQ_SENT_ALLOWED              MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_REQ_SENT_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_ADDBA_RES_RCV_NEGATIVE_ALLOWED      MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_RCV_NEGATIVE_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_ADDBA_RES_RCV_POSITIVE_ALLOWED      MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_RCV_POSITIVE_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_ADDBA_RES_TIMEOUT_ALLOWED           MTLK_WWSS_WLAN_STAT_ID_TX_ADDBA_RES_TIMEOUT_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_CLOSE_AGGR_CFMD_FAIL_ALLOWED        MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_CFMD_FAIL_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_CLOSE_AGGR_CFMD_SUCCESS_ALLOWED     MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_CFMD_SUCCESS_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_CLOSE_AGGR_NOT_CFMD_ALLOWED         MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_NOT_CFMD_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_CLOSE_AGGR_SENT_ALLOWED             MTLK_WWSS_WLAN_STAT_ID_TX_CLOSE_AGGR_SENT_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_DELBA_REQ_CFMD_FAIL_ALLOWED         MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_CFMD_FAIL_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_DELBA_REQ_CFMD_SUCCESS_ALLOWED      MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_CFMD_SUCCESS_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_DELBA_REQ_LOST_ALLOWED              MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_LOST_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_DELBA_REQ_NOT_CFMD_ALLOWED          MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_NOT_CFMD_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_DELBA_REQ_RCV_ALLOWED               MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_RCV_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_DELBA_REQ_REACHED_ALLOWED           MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_REACHED_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_DELBA_REQ_RETRANSMISSIONS_ALLOWED   MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_RETRANSMISSIONS_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_DELBA_REQ_SENT_ALLOWED              MTLK_WWSS_WLAN_STAT_ID_TX_DELBA_REQ_SENT_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_OPEN_AGGR_CFMD_FAIL_ALLOWED         MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_CFMD_FAIL_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_OPEN_AGGR_CFMD_SUCCESS_ALLOWED      MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_CFMD_SUCCESS_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_OPEN_AGGR_NOT_CFMD_ALLOWED          MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_NOT_CFMD_ALLOWED
#define MTLK_ADDBAPI_CNT_TX_OPEN_AGGR_SENT_ALLOWED              MTLK_WWSS_WLAN_STAT_ID_TX_OPEN_AGGR_SENT_ALLOWED

#define ADDBA_TU_TO_MS(nof_tu) ((nof_tu) * MTLK_ADDBA_BA_TIMEOUT_UNIT_US / 1000) /* 1TU = 1024 us */

#define INVALID_TAG_ID ((uint8)(-1))

/* counters will be modified with checking DEBUG option */
#if MTLK_WWSS_WLAN_STAT_GROUP_AGGREG_ALLOWED
#define _mtlk_addba_wss_cntr_dec(hcntr)  mtlk_wss_cntr_dec(hcntr)
#define _mtlk_addba_wss_cntr_inc(hcntr)  mtlk_wss_cntr_inc(hcntr)
#else
#define _mtlk_addba_wss_cntr_dec(hcntr)  /* nothing */
#define _mtlk_addba_wss_cntr_inc(hcntr)  /* nothing */
#endif

static void
_mtlk_addba_peer_send_delba_req (mtlk_addba_peer_t  *peer,
                                    uint16          tid_idx,
                                    uint16          dir);

static __INLINE int
_mtlk_addba_is_allowed_rate (uint16 rate)
{
  int res = 1;
  if (rate != MTLK_ADDBA_RATE_ADAPTIVE &&
      !mtlk_aux_is_11n_rate((uint8)rate)) {
    res = 0;
  }

  return res;
}

static __INLINE void
_mtlk_addba_correct_res_win_size (uint8 *win_size)
{
  uint8 res = *win_size;

  if (!res)
  {
    res = MTLK_ADDBA_MAX_REORD_WIN_SIZE;
  }

  if (*win_size != res)
  {
    ILOG2_DD("WinSize correction: %d=>%d", (int)*win_size, (int)res);
    *win_size = res;
  }
}

#define _mtlk_addba_correct_res_timeout(t) /* VOID, We accept any timeout */

static __INLINE BOOL
_mtlk_addba_is_aggr_on (mtlk_addba_peer_tx_state_e state)
{
  return (state == MTLK_ADDBA_TX_AGGR_OPENED ||
          state == MTLK_ADDBA_TX_DEL_AGGR_REQ_SENT);
}

/*
  Start reordering on RX side

  \param peer       - Handle of addba peer
  \param tid_idx    - Access protocol index
  \param ssn        - Starting sequence number
  \param win_size   - Maximum transmission window size

  \return
    MTLK_ERR_OK - on success
    MTLK_ERR_NO_RESOURCES - no reordering resources
 */
static int
_mtlk_addba_peer_reordering_start(mtlk_addba_peer_t* peer, uint16 tid_idx, uint16 ssn, uint8 win_size)
{
  if (FALSE == mtlk_reflim_try_ref(peer->addba->reord_reflim))
  {
    ILOG3_YDDD("RX %Y TID=%d Reordering limit reached %d >= %d)",
               &peer->addr, (int)tid_idx,
               mtlk_reflim_get_cur(peer->addba->reord_reflim),
               mtlk_reflim_get_max(peer->addba->reord_reflim));
    return MTLK_ERR_NO_RESOURCES;
  }

  if (FALSE == peer->tid[tid_idx].rx.reorder_is_active)
  {
    peer->api.on_start_reordering(peer->api.usr_data, tid_idx, ssn, win_size);

    peer->tid[tid_idx].rx.reorder_is_active = TRUE;

    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_REORD_ACTIVE]);

    ILOG2_YD("RX %Y TID=%d reordering opened", &peer->addr, tid_idx);
  }

  return MTLK_ERR_OK;
}

/*
  Reset state for RX side

  \param peer       - Handle of addba peer
  \param tid_idx    - Access protocol index

  \return
    Boolean indication of reordering state
      TRUE  - reordering is active
      FALSE - reordering is closed

*/
static __INLINE void
_mtlk_addba_peer_reset_rx_state (mtlk_addba_peer_t *peer,
                                 uint16             tid_idx)
{
  peer->tid[tid_idx].rx.state = MTLK_ADDBA_RX_NONE;

  peer->tid[tid_idx].rx.delba_req_attempt   = 0; /* retransmission stopped */
  peer->tid[tid_idx].rx.addba_res_attempt   = 0; /* retransmission stopped */
}

/*
  Stop reordering on RX side

  \param peer       - Handle of addba peer
  \param tid_idx    - Access protocol index

  \return
    none
 */
static void
_mtlk_addba_peer_reordering_stop(mtlk_addba_peer_t* peer, uint16 tid_idx)
{
  _mtlk_addba_peer_reset_rx_state(peer, tid_idx);

  if (TRUE == peer->tid[tid_idx].rx.reorder_is_active)
  {
    peer->tid[tid_idx].rx.reorder_is_active = FALSE;

    peer->api.on_stop_reordering(peer->api.usr_data, tid_idx);

    mtlk_reflim_unref(peer->addba->reord_reflim);

    _mtlk_addba_wss_cntr_dec(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_REORD_ACTIVE]);

    ILOG2_YD("RX %Y TID=%d reordering closed", &peer->addr, tid_idx);
  }
}

/*
  Retrieve reordering status

  \param peer       - Handle of addba peer

  \return
    TRUE  - reordering in progress
    FALSE - reordering disabled
 */
static __INLINE BOOL
_mtlk_addba_is_reord_on (mtlk_addba_peer_t* peer, uint16 tid_idx)
{
  return peer->tid[tid_idx].rx.reorder_is_active;
}

static __INLINE BOOL
_mtlk_addba_peer_reset_tx_state (mtlk_addba_peer_t *peer,
                                 uint16             tid_idx)
{
  BOOL res = FALSE;

  MTLK_ASSERT(NULL != peer);

  if (peer->tid[tid_idx].tx.state != MTLK_ADDBA_TX_NONE)
  {
    if (peer->tid[tid_idx].tx.state != MTLK_ADDBA_TX_STA_POWER_SAVE)
    {
      mtlk_reflim_unref(peer->addba->aggr_reflim);
      _mtlk_addba_wss_cntr_dec(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_AGGR_ACTIVE]);
    }
    res = _mtlk_addba_is_aggr_on(peer->tid[tid_idx].tx.state);
    peer->tid[tid_idx].tx.state = MTLK_ADDBA_TX_NONE;
    peer->tid[tid_idx].tx.addba_req_dlgt = INVALID_TAG_ID;
    peer->tid[tid_idx].tx.ts_id = MTLK_TS_ID_INVALID;
  }

  return res;
}

/******************************************************************************************************
 * CFM callbacks for TXMM
*******************************************************************************************************/
static mtlk_txmm_clb_action_e __MTLK_IFUNC
_mtlk_addba_peer_on_delba_req_tx_cfm_clb (mtlk_handle_t          clb_usr_data,
                                          mtlk_txmm_data_t      *data,
                                          mtlk_txmm_clb_reason_e reason)
{
  mtlk_addba_peer_t  *peer      = (mtlk_addba_peer_t*)clb_usr_data;
  UMI_DELBA_REQ_SEND* delba_req = (UMI_DELBA_REQ_SEND*)data->payload;
  uint16              tid_idx   = MAC_TO_HOST16(delba_req->u16AccessProtocol);
  uint16              status    = MAC_TO_HOST16(delba_req->u16Status);
  uint16              dir       = (MAC_TO_HOST16(delba_req->u16Intiator) == 0) ? BA_DIR_RECIPIENT : BA_DIR_ORIGINATOR;
  char *sdir_rx = "RX";
  char *sdir_tx = "TX";
  char *sdir;
  int stat_rx[3] = {
      MTLK_ADDBAPI_CNT_RX_DELBA_REQ_NOT_CFMD,
      MTLK_ADDBAPI_CNT_RX_DELBA_REQ_CFMD_FAIL,
      MTLK_ADDBAPI_CNT_RX_DELBA_REQ_CFMD_SUCCESS
    };
  int stat_tx[3] = {
      MTLK_ADDBAPI_CNT_TX_DELBA_REQ_NOT_CFMD,
      MTLK_ADDBAPI_CNT_TX_DELBA_REQ_CFMD_FAIL,
      MTLK_ADDBAPI_CNT_TX_DELBA_REQ_CFMD_SUCCESS
    };
  int *stat;
  uint8 *attempt_ptr;

  if (BA_DIR_ORIGINATOR == dir)
  {
    sdir = sdir_tx;
    stat = stat_tx;
    attempt_ptr = &peer->tid[tid_idx].tx.delba_req_attempt;
  }
  else
  {
    sdir = sdir_rx;
    stat = stat_rx;
    attempt_ptr = &peer->tid[tid_idx].rx.delba_req_attempt;
  }

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
  {
    ELOG_SYD("%s %Y TID=%d DELBA isn't confirmed",
            sdir, delba_req->sDA.au8Addr, (int)tid_idx);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][stat[0]]);
  }
  else if (status != UMI_OK)
  {
    ELOG_SYDD("%s %Y TID=%d DELBA failed in FW (%u)",
             sdir, delba_req->sDA.au8Addr, (int)tid_idx, status);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][stat[1]]);
  }
  else
  {
    ILOG2_SYD("%s %Y TID=%d DELBA sent",
             sdir, delba_req->sDA.au8Addr, (int)tid_idx);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][stat[2]]);
  }

  if (0 == *attempt_ptr)
  {
    /* activate retransmission counter */
    *attempt_ptr = 1;
  }

  return MTLK_TXMM_CLBA_FREE;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
_mtlk_addba_peer_on_close_aggr_cfm_clb (mtlk_handle_t          clb_usr_data,
                                        mtlk_txmm_data_t      *data,
                                        mtlk_txmm_clb_reason_e reason)
{
  mtlk_addba_peer_t  *peer           = (mtlk_addba_peer_t*)clb_usr_data;
  UMI_CLOSE_AGGR_REQ *close_aggr_req = (UMI_CLOSE_AGGR_REQ*)data->payload;
  uint16              tid_idx        = MAC_TO_HOST16(close_aggr_req->u16AccessProtocol);
  uint16              status         = MAC_TO_HOST16(close_aggr_req->u16Status);
  BOOL                stop           = FALSE;

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
  {
    ELOG_YD("TX %Y TID=%d aggregation closing isn't confirmed",
          close_aggr_req->sDA.au8Addr, tid_idx);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_CLOSE_AGGR_NOT_CFMD]);
  }
  else if (status != UMI_OK)
  {
    ELOG_YDD("TX %Y TID=%d aggregation closing failed in FW (%u)",
      close_aggr_req->sDA.au8Addr, tid_idx, status);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_CLOSE_AGGR_CFMD_FAIL]);
  }
  else
  {
    ILOG2_YD("TX %Y TID=%d aggregation closed",
         close_aggr_req->sDA.au8Addr, tid_idx);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_CLOSE_AGGR_CFMD_SUCCESS]);
  }

  mtlk_osal_lock_acquire(&peer->addba->lock);
  stop = _mtlk_addba_peer_reset_tx_state(peer, tid_idx);
  mtlk_osal_lock_release(&peer->addba->lock);

  if (TRUE == stop)
    peer->api.on_stop_aggregation(peer->api.usr_data, tid_idx);

  return MTLK_TXMM_CLBA_FREE;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
_mtlk_addba_peer_on_open_aggr_cfm_clb (mtlk_handle_t          clb_usr_data,
                                       mtlk_txmm_data_t      *data,
                                       mtlk_txmm_clb_reason_e reason)
{
  mtlk_addba_peer_t *peer         = (mtlk_addba_peer_t*)clb_usr_data;
  UMI_OPEN_AGGR_REQ *add_aggr_req = (UMI_OPEN_AGGR_REQ*)data->payload;
  uint16             tid_idx      = MAC_TO_HOST16(add_aggr_req->u16AccessProtocol);
  uint16             status       = MAC_TO_HOST16(add_aggr_req->u16Status);

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
  {
    ELOG_YD("TX %Y TID=%d aggregation opening isn't confirmed",
          add_aggr_req->sDA.au8Addr, tid_idx);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_OPEN_AGGR_NOT_CFMD]);
  }
  else if (status != UMI_OK)
  {
    ELOG_YDD("TX %Y TID=%d aggregation opening failed in FW (%u)",
          add_aggr_req->sDA.au8Addr, tid_idx, status);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_OPEN_AGGR_CFMD_FAIL]);
  }
  else
  {
    ILOG2_YD("TX %Y TID=%d aggregation opened", add_aggr_req->sDA.au8Addr, tid_idx);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_OPEN_AGGR_CFMD_SUCCESS]);
    peer->tid[tid_idx].tx.ts_id = add_aggr_req->u8TsId;
  }

  mtlk_osal_lock_acquire(&peer->addba->lock);
  MTLK_ASSERT(peer->tid[tid_idx].tx.state == MTLK_ADDBA_TX_ADD_AGGR_REQ_SENT);
  peer->tid[tid_idx].tx.state = MTLK_ADDBA_TX_AGGR_OPENED;
  mtlk_osal_lock_release(&peer->addba->lock);

  peer->api.on_start_aggregation(peer->api.usr_data, tid_idx);

  return MTLK_TXMM_CLBA_FREE;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
_mtlk_addba_peer_on_addba_req_tx_cfm_clb (mtlk_handle_t          clb_usr_data,
                                          mtlk_txmm_data_t      *data,
                                          mtlk_txmm_clb_reason_e reason)
{
  mtlk_addba_peer_t *peer       = (mtlk_addba_peer_t*)clb_usr_data;
  UMI_ADDBA_REQ_SEND* addba_req = (UMI_ADDBA_REQ_SEND*)data->payload;
  uint16              tid_idx   = MAC_TO_HOST16(addba_req->u16AccessProtocol);
  uint16              status    = MAC_TO_HOST16(addba_req->u16Status);

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
  {
    ELOG_YDD("TX %Y TID=%d TAG=%d request isn't confirmed",
          addba_req->sDA.au8Addr, tid_idx, addba_req->u8DialogToken);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_ADDBA_REQ_NOT_CFMD]);
  }
  else if (status != UMI_OK)
  {
    ELOG_YDDD("TX %Y TID=%d TAG=%d request failed in FW (%u)",
      addba_req->sDA.au8Addr, tid_idx, addba_req->u8DialogToken, status);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_ADDBA_REQ_CFMD_FAIL]);
  }
  else
  {
    ILOG2_YDD("TX %Y TID=%d TAG=%d request sent",
         addba_req->sDA.au8Addr, tid_idx, addba_req->u8DialogToken);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_ADDBA_REQ_CFMD_SUCCESS]);
  }

  mtlk_osal_lock_acquire(&peer->addba->lock);
  peer->tid[tid_idx].tx.state               = MTLK_ADDBA_TX_ADDBA_REQ_CFMD;
  peer->tid[tid_idx].tx.addba_req_cfmd_time = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
  mtlk_osal_lock_release(&peer->addba->lock);

  return MTLK_TXMM_CLBA_FREE;
}

static mtlk_txmm_clb_action_e __MTLK_IFUNC
_mtlk_addba_peer_on_addba_res_tx_cfm_clb (mtlk_handle_t          clb_usr_data,
                                          mtlk_txmm_data_t*      data,
                                          mtlk_txmm_clb_reason_e reason)
{
  mtlk_addba_peer_t   *peer     = (mtlk_addba_peer_t*)clb_usr_data;
  UMI_ADDBA_RES_SEND* addba_res = (UMI_ADDBA_RES_SEND*)data->payload;
  uint16              tid_idx   = MAC_TO_HOST16(addba_res->u16AccessProtocol);
  uint16              status    = MAC_TO_HOST16(addba_res->u16Status);

  if (reason != MTLK_TXMM_CLBR_CONFIRMED)
  {
    ELOG_YDD("RX %Y TID=%d TAG=%d response isn't confirmed",
          addba_res->sDA.au8Addr, tid_idx, addba_res->u8DialogToken);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_ADDBA_RES_NOT_CFMD]);
  }
  else if (status != UMI_OK)
  {
    ELOG_YDDD("RX %Y TID=%d TAG=%d response failed in FW (%u)",
      addba_res->sDA.au8Addr, tid_idx, addba_res->u8DialogToken, status);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_ADDBA_RES_CFMD_FAIL]);
  }
  else
  {
    ILOG2_YDD("RX %Y TID=%d TAG=%d response sent",
         addba_res->sDA.au8Addr, tid_idx, addba_res->u8DialogToken);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_ADDBA_RES_CFMD_SUCCESS]);
  }

  if (0 == peer->tid[tid_idx].rx.addba_res_attempt)
  {
    /* activate retransmission counter */
    peer->tid[tid_idx].rx.addba_res_attempt = 1;
  }

  return MTLK_TXMM_CLBA_FREE;
}

/******************************************************************************************************/

static void
_mtlk_addba_peer_on_addba_req_sent (mtlk_addba_peer_t *peer,
                                    uint16             tid_idx)
{
  peer->tid[tid_idx].tx.state          = MTLK_ADDBA_TX_ADDBA_REQ_SENT;
  peer->tid[tid_idx].tx.addba_req_dlgt = peer->addba->next_dlg_token;

  if (++peer->addba->next_dlg_token == INVALID_TAG_ID) {
  	++peer->addba->next_dlg_token;
  }

  _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_AGGR_ACTIVE]);
  _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_ADDBA_REQ_SENT]);
}

static void
_mtlk_addba_peer_fill_addba_req (mtlk_addba_peer_t  *peer,
                                 uint16              tid_idx,
                                 mtlk_txmm_data_t   *tx_data)
{
  uint8 win_size_req = peer->addba->cfg.tid[tid_idx].aggr_win_size;
  UMI_ADDBA_REQ_SEND *addba_req = (UMI_ADDBA_REQ_SEND *)tx_data->payload;

  memset(addba_req, 0, sizeof(*addba_req));

  tx_data->id           = UM_MAN_ADDBA_REQ_TX_REQ;
  tx_data->payload_size = sizeof(*addba_req);

  /* Limit requested window size to sane value */
  if(win_size_req == 0)
  {
    /* This is special case: we do not propose any win size to our peer */
  }
  else if(win_size_req > MTLK_ADDBA_MAX_REORD_WIN_SIZE)
  {
    win_size_req = MTLK_ADDBA_MAX_REORD_WIN_SIZE;
  }
  else if(win_size_req < MTLK_ADDBA_MIN_REORD_WIN_SIZE)
  {
    win_size_req = MTLK_ADDBA_MIN_REORD_WIN_SIZE;
  }

  addba_req->sDA               = peer->addr;
  addba_req->u8DialogToken     = peer->addba->next_dlg_token;
  addba_req->u16AccessProtocol = HOST_TO_MAC16(tid_idx);
  addba_req->u8BA_WinSize_O    = win_size_req;
  addba_req->u16BATimeout      = HOST_TO_MAC16(peer->addba->cfg.tid[tid_idx].addba_timeout);
  addba_req->u16Status         = HOST_TO_MAC16(UMI_OK);

  /* Later we will compare this value with the value in ADDBA response */
  peer->tid[tid_idx].tx.win_size_req = win_size_req;

  ILOG2_YDDDD("TX %Y TID=%d TAG=%d request WSIZE=%d TM=%d",
       &peer->addr,
       (int)tid_idx,
       (int)peer->addba->next_dlg_token,
       win_size_req,
       (int)peer->addba->cfg.tid[tid_idx].addba_timeout);
}

static void
_mtlk_addba_peer_tx_addba_req (mtlk_addba_peer_t *peer,
                               uint16             tid_idx, BOOL ps_mode)
{

  if (peer->tid[tid_idx].tx.state != MTLK_ADDBA_TX_NONE)
  {
    ILOG2_YD("TX %Y TID=%d: duplicate ADDBA request? - ignored", &peer->addr, tid_idx);
    return;
  }

  if (ps_mode) {
    peer->tid[tid_idx].tx.state = MTLK_ADDBA_TX_STA_POWER_SAVE;
    ILOG2_YD("TX %Y TID=%d: STA in power save - ignoring ADDBA request", &peer->addr, tid_idx);
    return;
  }

  if (mtlk_reflim_try_ref(peer->addba->aggr_reflim))
  { /* format and send ADDBA request */
    int               res     = MTLK_ERR_UNKNOWN;
    mtlk_txmm_data_t* tx_data =
      mtlk_txmm_msg_get_empty_data(&peer->tid[tid_idx].tx.man_msg,
                                   peer->addba->txmm);
    if (tx_data)
    {
      _mtlk_addba_peer_fill_addba_req(peer, tid_idx, tx_data);

      res = mtlk_txmm_msg_send(&peer->tid[tid_idx].tx.man_msg, _mtlk_addba_peer_on_addba_req_tx_cfm_clb,
                                HANDLE_T(peer), 0);

      if (res == MTLK_ERR_OK)
      {
        _mtlk_addba_peer_on_addba_req_sent(peer, tid_idx);
      }
      else
      {
        ELOG_D("Can't send ADDBA req due to TXMM err#%d", res);
      }
    }
    else
    {
      ELOG_V("Can't send ADDBA req due to lack of MAN_MSG");
    }

    if (res != MTLK_ERR_OK) {
      mtlk_reflim_unref(peer->addba->aggr_reflim);
    }
  }
  else
  {
    WLOG_DD("TX: ADDBA won't be sent (aggregations limit reached: %d >= %d)",
            mtlk_reflim_get_cur(peer->addba->aggr_reflim), mtlk_reflim_get_max(peer->addba->aggr_reflim));
  }
}

static void
_mtlk_addba_peer_close_aggr_req (mtlk_addba_peer_t *peer,
                                 uint16             tid_idx)
{
  mtlk_txmm_data_t* tx_data;
  UMI_CLOSE_AGGR_REQ* close_aggr_req;
  int state = peer->tid[tid_idx].tx.state;
  int sres;

  if (state != MTLK_ADDBA_TX_ADD_AGGR_REQ_SENT &&
      state != MTLK_ADDBA_TX_AGGR_OPENED)
  {
    WLOG_YD("TX %Y TID=%d: trying to close not opened aggregation", &peer->addr, tid_idx);
    return;
  }

  if (MTLK_TS_ID_INVALID == peer->tid[tid_idx].tx.ts_id)
  {
    WLOG_YD("TX %Y TID=%d: trying to close not confirmed aggregation", &peer->addr, tid_idx);
    return;
  }

  tx_data = mtlk_txmm_msg_get_empty_data(&peer->tid[tid_idx].tx.man_msg,
                                         peer->addba->txmm);

  if (!tx_data)
  {
    ELOG_V("Can't close Aggr due to lack of MAN_MSG");
    return;
  }

  tx_data->id           = UM_MAN_CLOSE_AGGR_REQ;
  tx_data->payload_size = sizeof(*close_aggr_req);

  close_aggr_req = (UMI_CLOSE_AGGR_REQ*)tx_data->payload;
  close_aggr_req->sDA               = peer->addr;
  close_aggr_req->u16AccessProtocol = HOST_TO_MAC16(tid_idx);
  close_aggr_req->u16Status         = HOST_TO_MAC16(UMI_OK);
  close_aggr_req->u8TsId            = peer->tid[tid_idx].tx.ts_id;

  ILOG2_YD("TX %Y TID=%d closing aggregation", &peer->addr, (int)tid_idx);

  sres = mtlk_txmm_msg_send(&peer->tid[tid_idx].tx.man_msg, _mtlk_addba_peer_on_close_aggr_cfm_clb,
                            HANDLE_T(peer), 0);

  if (sres == MTLK_ERR_OK) {
    peer->tid[tid_idx].tx.state = MTLK_ADDBA_TX_DEL_AGGR_REQ_SENT;
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_CLOSE_AGGR_SENT]);
  }
  else
  {
    ELOG_D("Can't close Aggr due to TXMM err#%d", sres);
  }
}

static void
_mtlk_addba_peer_send_delba_req (mtlk_addba_peer_t  *peer,
                                    uint16          tid_idx,
                                    uint16          dir)
{
  mtlk_txmm_msg_t  *tx_msg = &peer->tid[tid_idx].rx.man_msg;
  mtlk_txmm_data_t *tx_data;

  tx_data = mtlk_txmm_msg_get_empty_data(tx_msg, peer->addba->txmm);
  if (tx_data)
  {
    UMI_DELBA_REQ_SEND *delba_req = (UMI_DELBA_REQ_SEND*)tx_data->payload;
    int                 sres;

    tx_data->id           = UM_MAN_DELBA_REQ;
    tx_data->payload_size = sizeof(*delba_req);

    delba_req->sDA               = peer->addr;
    delba_req->u16AccessProtocol = HOST_TO_MAC16(tid_idx);
    delba_req->u16ResonCode      = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_SUCCESS);
    delba_req->u16Status         = HOST_TO_MAC16(UMI_OK);

    if (BA_DIR_ORIGINATOR == dir)
    {
      delba_req->u16Intiator = HOST_TO_MAC16(1);
      ILOG2_YD("TX %Y TID=%d send DELBA", &peer->addr, tid_idx);
    }
    else
    {
      delba_req->u16Intiator = HOST_TO_MAC16(0);
      ILOG2_YD("RX %Y TID=%d send DELBA", &peer->addr, tid_idx);
    }

    sres = mtlk_txmm_msg_send(tx_msg, _mtlk_addba_peer_on_delba_req_tx_cfm_clb,
                              HANDLE_T(peer), 0);
    if (sres != MTLK_ERR_OK)
    {
      ELOG_D("Can't send DELBA req due to TXMM err#%d", sres);
    }
    else if (BA_DIR_RECIPIENT == dir)
    {
      if(_mtlk_addba_is_reord_on(peer, tid_idx))
        peer->tid[tid_idx].rx.state = MTLK_ADDBA_RX_DELBA_REQ_SENT;
      else
        peer->tid[tid_idx].rx.state = MTLK_ADDBA_RX_DELBA_REQ_SENT_WITHOUT_REORDERING;

      _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_DELBA_REQ_SENT]);
    }
    else if (BA_DIR_ORIGINATOR == dir)
    {
      /* \note: do not touch 'state' for originator */
      _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_DELBA_REQ_SENT]);
    }
  }
  else
  {
    ELOG_V("no msg available");
  }
}

static void
_mtlk_addba_peer_stop (mtlk_addba_peer_t *peer)
{
  uint16 tid_idx = 0;
  BOOL   stop    = FALSE;

  /* Close opened aggregations & reorderings */
  for (tid_idx = 0; tid_idx < SIZEOF(peer->tid); tid_idx++)
  {
    mtlk_osal_lock_acquire(&peer->addba->lock);
    stop = _mtlk_addba_peer_reset_tx_state(peer, tid_idx);
    mtlk_osal_lock_release(&peer->addba->lock);

    /* After _mtlk_addba_peer_reset_tx_state invocation state machine */
    /* is in state MTLK_ADDBA_TX_NONE so no new retries will be sent  */
    /* by timer. There still may be outstanding TXMM message with     */
    /* pending callback. It is not a problem if it is called now,     */
    /* but since we don't want it to be called later, we have to      */
    /* cancel the message.                                            */
    mtlk_txmm_msg_cancel(&peer->tid[tid_idx].tx.man_msg);

    if (stop)
    {
      peer->api.on_stop_aggregation(peer->api.usr_data, tid_idx);
      ILOG2_YD("TX %Y TID=%d aggregation closed", &peer->addr, tid_idx);
    }

    mtlk_osal_lock_acquire(&peer->addba->lock);
    _mtlk_addba_peer_reset_rx_state(peer, tid_idx);
    mtlk_osal_lock_release(&peer->addba->lock);

    /* After _mtlk_addba_peer_reset_rx_state invocation state machine */
    /* is in state MTLK_ADDBA_RX_NONE so no new retries will be sent  */
    /* by timer. There still may be outstanding TXMM message with     */
    /* pending callback. It is not a problem if it is called now,     */
    /* but since we don't want it to be called later, we have to      */
    /* cancel the message.                                            */
    mtlk_txmm_msg_cancel(&peer->tid[tid_idx].rx.man_msg);

    mtlk_osal_lock_acquire(&peer->addba->lock);
    _mtlk_addba_peer_reordering_stop(peer, tid_idx);
    mtlk_osal_lock_release(&peer->addba->lock);

    /* Reset excluding the man_msg's */
    memset(&peer->tid[tid_idx].tx, 0, MTLK_OFFSET_OF(mtlk_addba_peer_tx_t, man_msg));
    memset(&peer->tid[tid_idx].rx, 0, MTLK_OFFSET_OF(mtlk_addba_peer_rx_t, man_msg));

    peer->tid[tid_idx].rx.delba_on_rx_time = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
  }
}

#if 0
static void
_mtlk_addba_peer_start_negotiation (mtlk_addba_peer_t *peer)
{
  uint16 tid, i = 0;

  for (i = 0; i < NTS_PRIORITIES; i++)
  {
    tid = mtlk_qos_get_tid_by_ac(i);
    ILOG2_DDD("use_aggr[%d (ac=%d)]=%d", (int)tid, (int)i, peer->addba->cfg.tid[tid].use_aggr);
    if (peer->addba->cfg.tid[tid].use_aggr)
      _mtlk_addba_peer_tx_addba_req(peer, tid);
  }
}
#endif /* 0 */

static __INLINE void
_mtlk_addba_peer_check_addba_res_rx_timeouts (mtlk_addba_peer_t *peer)
{
  uint8 tid_idx = 0;

  for (tid_idx = 0; tid_idx < SIZEOF(peer->tid); tid_idx++)
  {
    BOOL notify = FALSE;
    mtlk_osal_lock_acquire(&peer->addba->lock);
    if (peer->tid[tid_idx].tx.state == MTLK_ADDBA_TX_ADDBA_REQ_CFMD)
    {
      mtlk_osal_msec_t now_time  = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
      mtlk_osal_ms_diff_t diff_time = mtlk_osal_ms_time_diff(now_time, peer->tid[tid_idx].tx.addba_req_cfmd_time);
      if (diff_time >= MTLK_DOT11_ADDBA_RESPONSE_TIMEOUT_MS)
      {
        BOOL stop = _mtlk_addba_peer_reset_tx_state(peer, tid_idx);

        MTLK_ASSERT(stop == FALSE);    /* Aggregations cannot be started at this phase */
        MTLK_UNREFERENCED_PARAM(stop); /* For release compilation */

        ILOG2_YDDD("TX %Y TID=%d request timeout expired (%u >= %u)",
            &peer->addr, tid_idx, diff_time,
            MTLK_DOT11_ADDBA_RESPONSE_TIMEOUT_MS);

        notify = TRUE;

        _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_ADDBA_RES_TIMEOUT]);
      }
    }
    else if(peer->tid[tid_idx].tx.state == MTLK_ADDBA_TX_NONE)
    {
      if(peer->tid[tid_idx].tx.addba_req_reject_state == TRUE)
      {
        mtlk_osal_msec_t now_time  = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
        mtlk_osal_ms_diff_t diff_time = mtlk_osal_ms_time_diff(now_time, peer->tid[tid_idx].tx.addba_req_reject_time);
        if(diff_time >= peer->tid[tid_idx].tx.addba_req_reject_interval)
        {
          ILOG2_YDDD("TX %Y TID=%d ADDBA reject limit timeout expired (%u >= %u)",
            &peer->addr, tid_idx, diff_time,
            peer->tid[tid_idx].tx.addba_req_reject_interval);

          notify = TRUE;

          peer->tid[tid_idx].tx.addba_req_reject_state = FALSE;
          peer->tid[tid_idx].tx.addba_req_reject_interval *= 2;
          if(peer->tid[tid_idx].tx.addba_req_reject_interval > ADDBA_REJECT_INTERVAL_MAX)
          {
            peer->tid[tid_idx].tx.addba_req_reject_interval = ADDBA_REJECT_INTERVAL_MAX;
          }
        }
      }
    }
    mtlk_osal_lock_release(&peer->addba->lock);

    if (notify)
    {
      peer->api.on_ba_req_unconfirmed(peer->api.usr_data, tid_idx);
    }
  }
}

static __INLINE void
_mtlk_addba_peer_check_delba_timeouts (mtlk_addba_peer_t *peer)
{
  uint16 tid_idx = 0;

  for (tid_idx = 0; tid_idx < SIZEOF(peer->tid); tid_idx++)
  {
    mtlk_osal_lock_acquire(&peer->addba->lock);
    /* check DELBA send timeout */
    if (peer->tid[tid_idx].rx.state == MTLK_ADDBA_RX_REORD_IN_PROCESS)
    {
      if (peer->tid[tid_idx].rx.delba_timeout)
      {
        uint32 last_rx, now, diff;

        last_rx = peer->api.get_last_rx_timestamp(peer->api.usr_data, tid_idx);
        now  = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
        diff = mtlk_osal_ms_time_diff(now, last_rx);

        if (diff >= peer->tid[tid_idx].rx.delba_timeout)
        {
          _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_DELBA_SENT_BY_TIMEOUT]);

          ILOG2_YDDD("RX %Y TID=%d DELBA timeout expired (%u >= %u)",
              &peer->addr, tid_idx,
              diff, peer->tid[tid_idx].rx.delba_timeout);
          _mtlk_addba_peer_send_delba_req(peer, tid_idx, BA_DIR_RECIPIENT);
        }
      }
    }
    mtlk_osal_lock_release(&peer->addba->lock);
  }
}

MTLK_INIT_STEPS_LIST_BEGIN(addba)
  MTLK_INIT_STEPS_LIST_ENTRY(addba, INIT_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(addba, REG_ABILITIES)
  MTLK_INIT_STEPS_LIST_ENTRY(addba, EN_ABILITIES)
MTLK_INIT_INNER_STEPS_BEGIN(addba)
MTLK_INIT_STEPS_LIST_END(addba);

static const mtlk_ability_id_t _addba_abilities[] = {
  MTLK_CORE_REQ_GET_ADDBA_STATE,
  MTLK_CORE_REQ_SET_DELBA_REQ,
  MTLK_CORE_REQ_GET_ADDBA_CFG,
  MTLK_CORE_REQ_SET_ADDBA_CFG
};

int __MTLK_IFUNC
mtlk_addba_init (mtlk_addba_t           *obj,
                 mtlk_txmm_t            *txmm,
                 mtlk_reflim_t          *aggr_reflim,
                 mtlk_reflim_t          *reord_reflim,
                 const mtlk_addba_cfg_t *cfg,
                 mtlk_vap_handle_t      vap_handle)
{
  MTLK_ASSERT(obj != NULL);
  MTLK_ASSERT(cfg != NULL);
  MTLK_ASSERT(txmm != NULL);
  MTLK_ASSERT(aggr_reflim != NULL);
  MTLK_ASSERT(reord_reflim != NULL);

  MTLK_INIT_TRY(addba, MTLK_OBJ_PTR(obj))
    MTLK_INIT_STEP(addba, INIT_LOCK, MTLK_OBJ_PTR(obj),
                   mtlk_osal_lock_init,  (&obj->lock));
    obj->vap_handle     = vap_handle;
    obj->cfg            = *cfg;
    obj->txmm           = txmm;
    obj->aggr_reflim    = aggr_reflim;
    obj->reord_reflim   = reord_reflim;
    obj->next_dlg_token = 0;

    MTLK_INIT_STEP(addba, REG_ABILITIES, MTLK_OBJ_PTR(obj),
                   mtlk_abmgr_register_ability_set,
                   (mtlk_vap_get_abmgr(obj->vap_handle), _addba_abilities, ARRAY_SIZE(_addba_abilities)));
    MTLK_INIT_STEP_VOID(addba, EN_ABILITIES, MTLK_OBJ_PTR(obj),
                        mtlk_abmgr_enable_ability_set,
                        (mtlk_vap_get_abmgr(obj->vap_handle), _addba_abilities, ARRAY_SIZE(_addba_abilities)));
  MTLK_INIT_FINALLY(addba, MTLK_OBJ_PTR(obj))
  MTLK_INIT_RETURN(addba, MTLK_OBJ_PTR(obj), mtlk_addba_cleanup, (obj))
}

void __MTLK_IFUNC
mtlk_addba_cleanup (mtlk_addba_t *obj)
{
  MTLK_CLEANUP_BEGIN(addba, MTLK_OBJ_PTR(obj))
    MTLK_CLEANUP_STEP(addba, EN_ABILITIES, MTLK_OBJ_PTR(obj),
                      mtlk_abmgr_disable_ability_set,
                      (mtlk_vap_get_abmgr(obj->vap_handle), _addba_abilities, ARRAY_SIZE(_addba_abilities)));
    MTLK_CLEANUP_STEP(addba, REG_ABILITIES, MTLK_OBJ_PTR(obj),
                      mtlk_abmgr_unregister_ability_set,
                      (mtlk_vap_get_abmgr(obj->vap_handle), _addba_abilities, ARRAY_SIZE(_addba_abilities)));
    MTLK_CLEANUP_STEP(addba, INIT_LOCK, MTLK_OBJ_PTR(obj),
                      mtlk_osal_lock_cleanup, (&obj->lock));
  MTLK_CLEANUP_END(addba, MTLK_OBJ_PTR(obj));
}

int  __MTLK_IFUNC
mtlk_addba_reconfigure (mtlk_addba_t           *obj,
                        const mtlk_addba_cfg_t *cfg)
{
  mtlk_osal_lock_acquire(&obj->lock);
  obj->cfg = *cfg;
  mtlk_osal_lock_release(&obj->lock);

  return MTLK_ERR_OK;
}

MTLK_INIT_STEPS_LIST_BEGIN(addba_peer)
  MTLK_INIT_STEPS_LIST_ENTRY(addba_peer, TXMM_MSG_INIT_TX)
  MTLK_INIT_STEPS_LIST_ENTRY(addba_peer, TXMM_MSG_INIT_RX)
  MTLK_INIT_STEPS_LIST_ENTRY(addba_peer, WSS_OBJ)
  MTLK_INIT_STEPS_LIST_ENTRY(addba_peer, WSS_CNTRs)
MTLK_INIT_INNER_STEPS_BEGIN(addba_peer)
MTLK_INIT_STEPS_LIST_END(addba_peer);

int  __MTLK_IFUNC
mtlk_addba_peer_init (mtlk_addba_peer_t           *peer,
                      const mtlk_addba_wrap_api_t *api,
                      mtlk_addba_t                *addba,
                      mtlk_wss_t                  *parent_wss)
{
  uint8 i;

  MTLK_ASSERT(peer != NULL);
  MTLK_ASSERT(addba != NULL);
  MTLK_ASSERT(api != NULL);
  MTLK_ASSERT(parent_wss != NULL);
  MTLK_ASSERT(api->get_last_rx_timestamp != NULL);
  MTLK_ASSERT(api->on_start_aggregation != NULL);
  MTLK_ASSERT(api->on_stop_aggregation != NULL);
  MTLK_ASSERT(api->on_start_reordering != NULL);
  MTLK_ASSERT(api->on_stop_reordering != NULL);
  MTLK_ASSERT(api->on_ba_req_rejected != NULL);
  MTLK_ASSERT(api->on_ba_req_unconfirmed != NULL);

  MTLK_ASSERT(ARRAY_SIZE(_mtlk_addba_peer_cntr_wss_id_map) == MTLK_ADDBAPI_CNT_LAST);

  peer->addba     = addba;
  peer->is_active = FALSE;
  peer->api       = *api;

  for (i = 0; i < ARRAY_SIZE(peer->tid); i++) {
    peer->tid[i].tx.ts_id = MTLK_TS_ID_INVALID;
  }

  MTLK_INIT_TRY(addba_peer, MTLK_OBJ_PTR(peer))
    for (i = 0; i < ARRAY_SIZE(peer->tid); i++) {
      MTLK_INIT_STEP_LOOP(addba_peer, TXMM_MSG_INIT_TX, MTLK_OBJ_PTR(peer),
                          mtlk_txmm_msg_init, (&peer->tid[i].tx.man_msg));
    }
    for (i = 0; i < ARRAY_SIZE(peer->tid); i++) {
      MTLK_INIT_STEP_LOOP(addba_peer, TXMM_MSG_INIT_RX, MTLK_OBJ_PTR(peer),
                           mtlk_txmm_msg_init, (&peer->tid[i].rx.man_msg));
    }
    MTLK_INIT_STEP_EX(addba_peer, WSS_OBJ, MTLK_OBJ_PTR(peer),
                      mtlk_wss_create, (parent_wss, NULL, 0),
                      peer->wss, peer->wss != NULL, MTLK_ERR_NO_MEM);
    for (i = 0; i < ARRAY_SIZE(peer->tid); i++) {
      MTLK_INIT_STEP_LOOP(addba_peer, WSS_CNTRs, MTLK_OBJ_PTR(peer),
                          mtlk_wss_cntrs_open, (peer->wss, _mtlk_addba_peer_cntr_wss_id_map, peer->wss_hcntrs[i], MTLK_ADDBAPI_CNT_LAST));
    }
  MTLK_INIT_FINALLY(addba_peer, MTLK_OBJ_PTR(peer))
  MTLK_INIT_RETURN(addba_peer, MTLK_OBJ_PTR(peer), mtlk_addba_peer_cleanup, (peer))
}

void __MTLK_IFUNC
mtlk_addba_peer_cleanup (mtlk_addba_peer_t *peer)
{
  uint8 i;

  MTLK_ASSERT(peer != NULL);
  MTLK_ASSERT(peer->is_active == FALSE);

  MTLK_CLEANUP_BEGIN(addba_peer, MTLK_OBJ_PTR(peer))
    for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(peer), WSS_CNTRs) > 0; i++) {
      MTLK_CLEANUP_STEP_LOOP(addba_peer, WSS_CNTRs, MTLK_OBJ_PTR(peer),
                             mtlk_wss_cntrs_close, (peer->wss, peer->wss_hcntrs[i], MTLK_ADDBAPI_CNT_LAST));
    }
    MTLK_CLEANUP_STEP(addba_peer, WSS_OBJ, MTLK_OBJ_PTR(peer),
                      mtlk_wss_delete, (peer->wss));
    for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(peer), TXMM_MSG_INIT_RX) > 0; i++) {
      MTLK_CLEANUP_STEP_LOOP(addba_peer, TXMM_MSG_INIT_RX, MTLK_OBJ_PTR(peer),
                             mtlk_txmm_msg_cleanup, (&peer->tid[i].rx.man_msg));
    }

    for (i = 0; MTLK_CLEANUP_ITERATONS_LEFT(MTLK_OBJ_PTR(peer), TXMM_MSG_INIT_TX) > 0; i++) {
      MTLK_CLEANUP_STEP_LOOP(addba_peer, TXMM_MSG_INIT_TX, MTLK_OBJ_PTR(peer),
                             mtlk_txmm_msg_cleanup, (&peer->tid[i].tx.man_msg));
    }
  peer->addba = NULL;
  MTLK_CLEANUP_END(addba_peer, MTLK_OBJ_PTR(peer))
}

void __MTLK_IFUNC
mtlk_addba_peer_on_delba_req_rx (mtlk_addba_peer_t* peer,
                                 uint16             tid_idx,
                                 uint16             res_code,
                                 uint16             initiator)
{
  MTLK_ASSERT(peer != NULL);

  /* Check for TID and drop message if it isn't correct */
  if (tid_idx >= SIZEOF(peer->tid))
  {
    ELOG_YDDDDD("TX %Y TID=%d RES=%d IN=%d: wrong priority (%d >= %ld)",
        &peer->addr, (int)tid_idx, (int)res_code, (int)initiator,
        (int)tid_idx, SIZEOF(peer->tid));

    return;
  }

  ILOG2_YDDD("TX %Y TID=%d DELBA recvd, RES=%d IN=%d", &peer->addr,
       (int)tid_idx, (int)res_code, (int)initiator);

  if (peer->is_active)
  {
    mtlk_osal_lock_acquire(&peer->addba->lock);

    if (initiator)
    {
      _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_DELBA_REQ_RCV]);

      _mtlk_addba_peer_reordering_stop(peer, tid_idx);
    }
    else
    {
      _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_DELBA_REQ_RCV]);
      /*
         Sent by recipient of the data => our TX-related =>
         we should stop the aggregations transmission.
      */
      _mtlk_addba_peer_close_aggr_req(peer, tid_idx);
    }

    mtlk_osal_lock_release(&peer->addba->lock);
  }
}

void __MTLK_IFUNC
mtlk_addba_peer_on_addba_res_rx (mtlk_addba_peer_t *peer,
                                 uint16             res_code,
                                 uint16             tid_idx,
                                 uint8              win_size_rsp,
                                 uint8              dlgt)
{
  int notify_reject = 0;

  MTLK_ASSERT(peer != NULL);

  /* Check for TID and drop message if it isn't correct */
  if (tid_idx >= SIZEOF(peer->tid))
  {
    ELOG_YDDDD("TX %Y TID=%d TAG=%d: wrong priority (%d >= %ld)",
        &peer->addr, (int)tid_idx, (int)dlgt,
        (int)tid_idx, SIZEOF(peer->tid));

    return;
  }

  ILOG2_YDDD("TX %Y TID=%d TAG=%d response recvd RES=%d",
       &peer->addr, (int)tid_idx, (int)dlgt, (int)res_code);

  if (peer->is_active)
  {
    mtlk_osal_lock_acquire(&peer->addba->lock);

    /* Check the TID and drop message if it isn't belongs to the
     * current ADDBA session */
    if (peer->tid[tid_idx].tx.addba_req_dlgt != dlgt) {
      WLOG_YDDD("TX: %Y TID=%u TAG=%u invalid TAG: currentTAG=%u - ignoring response",
              &peer->addr, (int)tid_idx, (int)dlgt,
              (int)peer->tid[tid_idx].tx.addba_req_dlgt);

      goto out;
    };

    if (peer->tid[tid_idx].tx.state != MTLK_ADDBA_TX_ADDBA_REQ_SENT &&
        peer->tid[tid_idx].tx.state != MTLK_ADDBA_TX_ADDBA_REQ_CFMD) {
      WLOG_YDDD("TX: %Y TID=%d TAG=%d invalid state: %d - ignoring response",
              &peer->addr, (int)tid_idx, (int)dlgt,
              (int)peer->tid[tid_idx].tx.state);
      goto out;
    }

    if (res_code == MTLK_ADDBA_RES_CODE_SUCCESS)
    {
      mtlk_txmm_data_t  *tx_data;
      UMI_OPEN_AGGR_REQ *add_aggr_req;
      int                sres;
      uint8              win_size_req = peer->tid[tid_idx].tx.win_size_req;
      uint8              win_size_fw = 0;

      _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_ADDBA_RES_RCV_POSITIVE]);

      ILOG2_YDDD("TX %Y TID=%d TAG=%d WSIZE=%d accepted by peer",
          &peer->addr, (int)tid_idx, (int)dlgt, win_size_rsp);

      peer->tid[tid_idx].tx.addba_res_rejects = 0;
      peer->tid[tid_idx].tx.addba_req_reject_interval = ADDBA_REJECT_INTERVAL_MIN;

      tx_data = mtlk_txmm_msg_get_empty_data(&peer->tid[tid_idx].tx.man_msg,
                                             peer->addba->txmm);
      if (!tx_data)
      {
        ELOG_V("no msg available");
        goto out;
      }

      /* Select window size */

      if(win_size_req == 0)
      {
        /* We don't know what we want. Just accept what our peer wants. */
        win_size_fw = win_size_rsp;
      }
      else
      {
        /* Choose smallest of what is supported by us and by peer */
        win_size_fw = MIN(win_size_req, win_size_rsp);
      }

      if(win_size_fw > MTLK_ADDBA_MAX_REORD_WIN_SIZE)
      {
        win_size_fw = MTLK_ADDBA_MAX_REORD_WIN_SIZE;
      }
      else if(win_size_fw < MTLK_ADDBA_MIN_REORD_WIN_SIZE)
      {
        WLOG2_YDDD("TX: %Y TID=%d TAG=%d too small wsize: %d - ignoring response",
                &peer->addr, (int)tid_idx, (int)dlgt,
                win_size_fw);
        goto out;
      }

      add_aggr_req = (UMI_OPEN_AGGR_REQ*)tx_data->payload;

      tx_data->id           = UM_MAN_OPEN_AGGR_REQ;
      tx_data->payload_size = sizeof(*add_aggr_req);

      add_aggr_req->sDA                      = peer->addr;
      add_aggr_req->u16AccessProtocol        = HOST_TO_MAC16(tid_idx);
      add_aggr_req->u16MaxNumOfPackets       = HOST_TO_MAC16(peer->addba->cfg.tid[tid_idx].max_nof_packets);
      add_aggr_req->u32MaxNumOfBytes         = HOST_TO_MAC32(peer->addba->cfg.tid[tid_idx].max_nof_bytes);
      add_aggr_req->u32TimeoutInterval       = HOST_TO_MAC32(peer->addba->cfg.tid[tid_idx].timeout_interval);
      add_aggr_req->u32MinSizeOfPacketInAggr = HOST_TO_MAC32(peer->addba->cfg.tid[tid_idx].min_packet_size_in_aggr);
      add_aggr_req->u16Status                = HOST_TO_MAC16(UMI_OK);
      add_aggr_req->windowSize               = win_size_fw;

      ILOG2_YD("TX %Y TID=%d opening aggregation",
           add_aggr_req->sDA.au8Addr, (int)tid_idx);

      sres = mtlk_txmm_msg_send(&peer->tid[tid_idx].tx.man_msg, _mtlk_addba_peer_on_open_aggr_cfm_clb,
                                HANDLE_T(peer), 0);
      if (sres == MTLK_ERR_OK)
      {
        peer->tid[tid_idx].tx.state = MTLK_ADDBA_TX_ADD_AGGR_REQ_SENT;
        _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_OPEN_AGGR_SENT]);
      }
      else
      {
        ELOG_D("Can't open AGGR due to TXMM err#%d", sres);
      }
    }
    else
    {
      BOOL stop = _mtlk_addba_peer_reset_tx_state(peer, tid_idx);

      MTLK_ASSERT(stop == FALSE);    /* Aggregations cannot be started at this phase */
      MTLK_UNREFERENCED_PARAM(stop); /* For Release compilation */

      _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_ADDBA_RES_RCV_NEGATIVE]);

      ILOG2_YDD("TX %Y TID=%d TAG=%d rejected by peer",
          &peer->addr, (int)tid_idx, (int)dlgt);

      ++peer->tid[tid_idx].tx.addba_res_rejects;
      if (peer->tid[tid_idx].tx.addba_res_rejects < MTLK_ADDBA_MAX_REJECTS)
      {
        notify_reject = 1;
      }
      else
      {
        ILOG1_YD("ADDBA rejects limit reached for peer %Y, TID=%d. "
                "No more notifications will be sent to upper layer",
                 &peer->addr, tid_idx);
        peer->tid[tid_idx].tx.addba_res_rejects = 0;
        peer->tid[tid_idx].tx.addba_req_reject_state = TRUE;
        peer->tid[tid_idx].tx.addba_req_reject_time = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
      }
    }
out:
    mtlk_osal_lock_release(&peer->addba->lock);
    if (notify_reject)
    {
      peer->api.on_ba_req_rejected(peer->api.usr_data, tid_idx);
    }
  }
}

static int
_mtlk_addba_peer_send_rx_addba_res(mtlk_addba_peer_t *peer, uint16 tid_idx)
{
  int err;
  mtlk_txmm_data_t* tx_data = NULL;

  tx_data = mtlk_txmm_msg_get_empty_data(&peer->tid[tid_idx].rx.man_msg,
                                         peer->addba->txmm);
  if (NULL == tx_data)
  {
    ELOG_V("Can't send ADDBA resp due to lack of MAN_MSG");
    return MTLK_ERR_NO_RESOURCES;
  }

  tx_data->id           = UM_MAN_ADDBA_RES_TX_REQ;
  tx_data->payload_size = sizeof(peer->tid[tid_idx].rx.addba_res);

  /* take response from cash */
  memcpy(tx_data->payload, &peer->tid[tid_idx].rx.addba_res, tx_data->payload_size);

  err = mtlk_txmm_msg_send(&peer->tid[tid_idx].rx.man_msg, _mtlk_addba_peer_on_addba_res_tx_cfm_clb,
                            HANDLE_T(peer), 0);
  if (err != MTLK_ERR_OK)
  {
    ELOG_D("Can't send ADDBA response due to TXMM err#%d", err);
  }
  else if (peer->tid[tid_idx].rx.addba_res.u16ResultCode == HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_SUCCESS))
  {
    peer->tid[tid_idx].rx.state         = MTLK_ADDBA_RX_ADDBA_POSITIVE_RES_SENT;
    peer->tid[tid_idx].rx.req_tstamp    = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());

    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_ADDBA_RES_POSITIVE_SENT]);
  }
  else
  {
    peer->tid[tid_idx].rx.state         = MTLK_ADDBA_RX_ADDBA_NEGATIVE_RES_SENT;

    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_ADDBA_RES_NEGATIVE_SENT]);
  }

  return err;
}

/*
  Handler for ADDBA request

  \param peer       - Handle of addba peer
  \param ssn        - Starting sequence number
  \param tid_idx    - Access protocol index
  \param win_size   - Maximum transmission window size
  \param dlgt       - Dialog token
  \param tmout      - Block Ack timeout value
  \param ack_policy - Block Ack policy
  \param rate       - TODO !!!!!!

  \return
    none
 */
void __MTLK_IFUNC
mtlk_addba_peer_on_addba_req_rx (mtlk_addba_peer_t *peer,
                                 uint16             ssn,
                                 uint16             tid_idx,
                                 uint8              win_size,
                                 uint8              dlgt,
                                 uint16             tmout,
                                 uint16             ack_policy,
                                 uint16             rate)
{
  MTLK_ASSERT(peer != NULL);

  /* Check for TID and drop message if it isn't correct */
  if (tid_idx >= SIZEOF(peer->tid))
  {
    ELOG_YDDDD("RX %Y TID=%d TAG=%d: wrong priority (%d >= %ld)",
        &peer->addr, (int)tid_idx, (int)dlgt,
        (int)tid_idx, SIZEOF(peer->tid));

    return;
  }

  ILOG2_YDDDDDD("RX %Y TID=%d TAG=%d req recvd WSIZE=%d SSN=%d TMBA=%d ACP=%d",
       &peer->addr,
       (int)tid_idx,
       (int)dlgt,
       (int)win_size,
       (int)ssn,
       (int)tmout,
       (int)ack_policy);

  if (peer->is_active)
  {
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_ADDBA_REQ_RECEIVED]);

    mtlk_osal_lock_acquire(&peer->addba->lock);

    {
      /* configure cash for answer */
      UMI_ADDBA_RES_SEND* addba_res = &peer->tid[tid_idx].rx.addba_res;

      if (peer->tid[tid_idx].rx.state != MTLK_ADDBA_RX_NONE)
      {
        if (dlgt == addba_res->u8DialogToken) {
          ILOG2_YDD("RX %Y TID=%d TAG=%d: ADDBA Request received with the same dialog token as currently processed -> Dropped", &peer->addr, (int)tid_idx, (int)dlgt);
          goto out;
        } else if (peer->tid[tid_idx].rx.state == MTLK_ADDBA_RX_ADDBA_POSITIVE_RES_SENT ||
                   peer->tid[tid_idx].rx.state == MTLK_ADDBA_RX_ADDBA_NEGATIVE_RES_SENT) {
          WLOG_YDDD("RX %Y TID=%d TAG=%d: Invalid State %d : ADDBA Request received, but confirmation on previously sent ADDBA Response not yet received -> Dropped", &peer->addr, (int)tid_idx, (int)dlgt, peer->tid[tid_idx].rx.state);
          goto out;
        } else {
          WLOG_YDDD("RX %Y TID=%d TAG=%d: Invalid State %d : ADDBA Request received while reordering is opened -> Reopen reordering",  &peer->addr, (int)tid_idx, (int)dlgt, peer->tid[tid_idx].rx.state);
        }
      }

      _mtlk_addba_peer_reordering_stop(peer, tid_idx);

      addba_res->sDA               = peer->addr;
      addba_res->u8DialogToken     = dlgt;
      addba_res->u16AccessProtocol = HOST_TO_MAC16(tid_idx);
      addba_res->u16Status         = HOST_TO_MAC16(UMI_OK);

      _mtlk_addba_correct_res_win_size(&win_size);
      if (!_mtlk_addba_is_allowed_rate(rate))
      {
        ILOG2_YDDD("RX %Y TID=%d TAG=%d (DECLINED: RATE == %d)",
             &peer->addr, (int)tid_idx, (int)dlgt, (int)rate);
        addba_res->u16ResultCode = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_FAILURE);
      }
      else if (!peer->addba->cfg.tid[tid_idx].accept_aggr)
      {
        ILOG2_YDD("RX %Y TID=%d TAG=%d (DECLINED: CFG OFF)",
            &peer->addr, (int)tid_idx, (int)dlgt);
        addba_res->u16ResultCode = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_FAILURE);
      }
      else if (!ack_policy)
      {
        ILOG2_YDD("RX %Y TID=%d TAG=%d (DECLINED: ACP == 0)",
            &peer->addr, (int)tid_idx, (int)dlgt);
        addba_res->u16ResultCode = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_FAILURE);
      }
      else if (MTLK_ERR_OK != _mtlk_addba_peer_reordering_start(peer, tid_idx, ssn, win_size))
      {
        ILOG2_YDD("RX %Y TID=%d TAG=%d (DECLINED: No resources)",
                  &peer->addr, (int)tid_idx, (int)dlgt);
        addba_res->u16ResultCode = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_FAILURE);
      }
      else
      {
        _mtlk_addba_correct_res_timeout(&tmout);

        addba_res->u16ResultCode = HOST_TO_MAC16(MTLK_ADDBA_RES_CODE_SUCCESS);
        addba_res->u16BATimeout  = HOST_TO_MAC16(tmout);
        addba_res->u8WinSize     = win_size;

        ILOG2_YDDDD("RX %Y TID=%d TAG=%d (ACCEPTED: TMBA=%d WSIZE=%d)",
          &peer->addr, (int)tid_idx, (int)dlgt,
          (int)tmout, (int)win_size);
      }

      if (MTLK_ERR_OK != _mtlk_addba_peer_send_rx_addba_res(peer, tid_idx))
      {
        _mtlk_addba_peer_reordering_stop(peer, tid_idx);
      }
      else if (TRUE == _mtlk_addba_is_reord_on(peer, tid_idx))
      {
        peer->tid[tid_idx].rx.delba_timeout = (uint32)ADDBA_TU_TO_MS(tmout);
      }
    }
  out:
    mtlk_osal_lock_release(&peer->addba->lock);
  }
}

/*
  Handler of transmission status for DELBA request from recipient

  \param peer       - Handle of addba peer
  \param tid_idx    - Access protocol index
  \param status     - Frame transmission status

  \return
    none
 */
static void
_mtlk_addba_peer_on_rx_delba_req_tx_status (mtlk_addba_peer_t* peer, uint16 tid_idx, uint16 status)
{
  mtlk_addba_peer_rx_t *tid_rx = &peer->tid[tid_idx].rx;

  if (UMI_OK == status)
  {
    ILOG2_YD("RX %Y TID=%d: DELBA request delivered", &peer->addr, tid_idx);

    if (tid_rx->state == MTLK_ADDBA_RX_DELBA_REQ_SENT)
      _mtlk_addba_peer_reordering_stop(peer, tid_idx);
    else if (tid_rx->state == MTLK_ADDBA_RX_DELBA_REQ_SENT_WITHOUT_REORDERING)
      _mtlk_addba_peer_reset_rx_state(peer, tid_idx);

    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_DELBA_REQ_REACHED]);
    return;
  }

  if (MTLK_ADDBA_RX_DELBA_REQ_SENT_WITHOUT_REORDERING == tid_rx->state)
  {
    /* no any retransmissions if reordering was not enabled */
    ILOG3_YD("BA_TX RX %Y TID=%d: Skip DELBA_REQ retransmission", &peer->addr, tid_idx);

    /* restore NONE state */
    _mtlk_addba_peer_reset_rx_state(peer, tid_idx);

    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_DELBA_REQ_LOST]);
    return;
  }

  if (tid_rx->state != MTLK_ADDBA_RX_DELBA_REQ_SENT)
  {
    ILOG3_YDD("BA_TX RX %Y TID=%d: Wrong state [%d] for DELBA_REQ retransmission",
              &peer->addr, tid_idx, tid_rx->state);

    tid_rx->delba_req_attempt = 0; /* stop retransmission */

    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_DELBA_REQ_LOST]);
    return;
  }

  MTLK_ASSERT(tid_rx->delba_req_attempt != 0);

  if (tid_rx->delba_req_attempt >= MTLK_DELBA_REQ_MAX_RETRANSMISSIONS)
  {
    ILOG2_YDD("BA_TX RX %Y TID=%d: DELBA_REQ was not delivered after %d attempts",
              &peer->addr, tid_idx, tid_rx->delba_req_attempt);

    tid_rx->delba_req_attempt = 0; /* stop retransmission */
    _mtlk_addba_peer_reordering_stop(peer, tid_idx);
    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_DELBA_REQ_LOST]);
    return;
  }

  tid_rx->delba_req_attempt++;
  _mtlk_addba_peer_send_delba_req(peer, tid_idx, BA_DIR_RECIPIENT);

  _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_DELBA_REQ_RETRANSMISSIONS]);
}

/*
  Handler of transmission status for DELBA request from originator

  \param peer       - Handle of addba peer
  \param tid_idx    - Access protocol index
  \param status     - Frame transmission status

  \return
    none
*/
static void
_mtlk_addba_peer_on_tx_delba_req_tx_status (mtlk_addba_peer_t* peer, uint16 tid_idx, uint16 status)
{
  mtlk_addba_peer_tx_t *tid_tx = &peer->tid[tid_idx].tx;

  if (UMI_OK == status)
  {
    ILOG2_YD("TX %Y TID=%d: DELBA request delivered", &peer->addr, tid_idx);

    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_DELBA_REQ_REACHED]);
    return;
  }

  if ((tid_tx->state != MTLK_ADDBA_TX_NONE) &&
      (tid_tx->state != MTLK_ADDBA_TX_DEL_AGGR_REQ_SENT))
  {
    ILOG3_YDD("BA_TX TX %Y TID=%d: Wrong state [%d] for DELBA_REQ retransmission",
              &peer->addr, tid_idx, tid_tx->state);

    tid_tx->delba_req_attempt = 0; /* stop retransmission */

    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_DELBA_REQ_LOST]);
    return;
  }

  MTLK_ASSERT(tid_tx->delba_req_attempt != 0);

  if (tid_tx->delba_req_attempt >= MTLK_DELBA_REQ_MAX_RETRANSMISSIONS)
  {
    ILOG2_YDD("BA_TX TX %Y TID=%d: DELBA_REQ was not delivered after %d attempts",
              &peer->addr, tid_idx, tid_tx->delba_req_attempt);

    tid_tx->delba_req_attempt = 0; /* stop retransmission */

    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_DELBA_REQ_LOST]);

    /* Stop retransmissions */
    return;
  }

  tid_tx->delba_req_attempt++;
  _mtlk_addba_peer_send_delba_req(peer, tid_idx, BA_DIR_ORIGINATOR);

  _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_DELBA_REQ_RETRANSMISSIONS]);
}

/*
  Handler of transmission status for ADDBA response

  \param peer       - Handle of addba peer
  \param tid_idx    - Access protocol index
  \param status     - Frame transmission status

  \return
    none
 */
static void
_mtlk_addba_peer_on_addba_res_tx_req_tx_status (mtlk_addba_peer_t* peer, uint16 tid_idx, uint16 status)
{
  mtlk_addba_peer_rx_t *tid_rx = &peer->tid[tid_idx].rx;

  if (UMI_OK == status)
  {
    ILOG2_YD("RX %Y TID=%d: response delivered", &peer->addr, tid_idx);

    if (tid_rx->state == MTLK_ADDBA_RX_ADDBA_POSITIVE_RES_SENT)
    {
      tid_rx->state = MTLK_ADDBA_RX_REORD_IN_PROCESS;
    }
    else if (tid_rx->state == MTLK_ADDBA_RX_ADDBA_NEGATIVE_RES_SENT)
    {
      _mtlk_addba_peer_reset_rx_state(peer, tid_idx);
    }
    else
    {
      ILOG2_YDD("BA_TX %Y TID=%d: ADDBA_RES_STATUS in invalid state %d",
               &peer->addr, tid_idx, tid_rx->state);
    }

    tid_rx->addba_res_attempt = 0; /* stop retransmission */

    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_ADDBA_RES_REACHED]);
    return;
  }

  if ((tid_rx->state != MTLK_ADDBA_RX_ADDBA_POSITIVE_RES_SENT) &&
      (tid_rx->state != MTLK_ADDBA_RX_ADDBA_NEGATIVE_RES_SENT))
  {
    ILOG3_YD("BA_TX %Y TID=%d: ADDBA_RES retransmission skipped", &peer->addr, tid_idx);

    tid_rx->addba_res_attempt = 0; /* stop retransmission */
    return;
  }

  MTLK_ASSERT(tid_rx->addba_res_attempt != 0);

  if (tid_rx->addba_res_attempt >= MTLK_ADDBA_RSP_MAX_RETRANSMISSIONS)
  {
    ILOG2_YDD("BA_TX %Y TID=%d: ADDBA_RES was not delivered after %d attempts",
              &peer->addr, tid_idx, tid_rx->addba_res_attempt);

    tid_rx->addba_res_attempt = 0; /* stop retransmission */
    tid_rx->state = MTLK_ADDBA_RX_NONE;
    _mtlk_addba_peer_reordering_stop(peer, tid_idx);

    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_ADDBA_RES_LOST]);

    return;
  }

  tid_rx->addba_res_attempt++;
  (void) _mtlk_addba_peer_send_rx_addba_res(peer, tid_idx);

  _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_ADDBA_RES_RETRANSMISSIONS]);
}

/*
  Handler of transmission status for DELBA request and ADDBA response

  \param peer       - Handle of addba peer
  \param ba_tx      - Handle to the \ref UMI_BA_TX_STATUS message

  \return
    none
 */
void __MTLK_IFUNC
mtlk_addba_peer_on_ba_tx_status(mtlk_addba_peer_t *peer, UMI_BA_TX_STATUS *ba_tx)
{
  uint16 um_id, tid_idx;

  MTLK_ASSERT(NULL != peer);
  MTLK_ASSERT(NULL != ba_tx);

  um_id     = MAC_TO_HOST16(ba_tx->u16MessageId);
  tid_idx   = MAC_TO_HOST16(ba_tx->u16AccessProtocol);

  ILOG2_YDDDD("BA_TX %Y TID=%d BA status received UMID=0x%04x I=%d ST=%d",
             &peer->addr, tid_idx, um_id, ba_tx->u8Initiator, ba_tx->u8Status);

  if (!peer->is_active)
    return;

  if (tid_idx >= SIZEOF(peer->tid))
  {
    ELOG_YDD("BA_TX %Y: wrong priority (%d >= %ld)",
        &peer->addr, tid_idx, SIZEOF(peer->tid));
    return;
  }

  mtlk_osal_lock_acquire(&peer->addba->lock);

  switch(um_id)
  {
    case UM_MAN_DELBA_REQ:
      if (ba_tx->u8Initiator)
        _mtlk_addba_peer_on_tx_delba_req_tx_status(peer, tid_idx, ba_tx->u8Status);
      else
        _mtlk_addba_peer_on_rx_delba_req_tx_status(peer, tid_idx, ba_tx->u8Status);
      break;
    case UM_MAN_ADDBA_RES_TX_REQ:
      _mtlk_addba_peer_on_addba_res_tx_req_tx_status(peer, tid_idx, ba_tx->u8Status);
      break;
    default:
      ELOG_YDD("RX %Y TID=%d Unknown UMID [0x%04x]", &peer->addr, tid_idx, um_id);
      break;
  }

  mtlk_osal_lock_release(&peer->addba->lock);
}

/*
  Handler for received ACK on BAR frame

  \param peer       - Handle of addba peer
  \param tid_idx    - Access protocol index

  \return
    none

  \remarks
    The ACK on BAR frame indicate that on RX side do not started reordering.
    As result our (TX) side have to close the current aggregation and reopen
    it in the future if needed.
 */
void __MTLK_IFUNC
mtlk_addba_peer_on_ack_on_bar (mtlk_addba_peer_t *peer, uint16 tid_idx)
{
  MTLK_ASSERT(NULL != peer);

  ILOG2_YD("TX %Y TID=%d ACK on BAR received", &peer->addr, tid_idx);

  if (!peer->is_active)
    return;

  if (tid_idx >= SIZEOF(peer->tid))
  {
    ELOG_YDD("TX %Y: wrong priority (%d >= %ld)",
        &peer->addr, tid_idx, SIZEOF(peer->tid));
    return;
  }

  _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_TX_ACK_ON_BAR_DETECTED]);

  mtlk_addba_delba_req(peer, tid_idx);
}

/*
  Handler of BAR frames

  \param peer       - Handle of addba peer
  \param tid_idx    - Access protocol index

  \return
    none
 */
void __MTLK_IFUNC
mtlk_addba_peer_on_bar(mtlk_addba_peer_t *peer, uint16 tid_idx)
{
  if (!peer->is_active)
    return;

  if (tid_idx >= SIZEOF(peer->tid))
  {
    ELOG_YDD("RX %Y: wrong priority (%d >= %ld)",
        &peer->addr, tid_idx, SIZEOF(peer->tid));
    return;
  }

  mtlk_osal_lock_acquire(&peer->addba->lock);

  if ((MTLK_ADDBA_RX_NONE == peer->tid[tid_idx].rx.state) ||
      (MTLK_ADDBA_RX_ADDBA_NEGATIVE_RES_SENT == peer->tid[tid_idx].rx.state))
  {
    ILOG2_YD("RX %Y TID=%d BAR detected with disabled reordering",
              &peer->addr, (int)tid_idx);

    _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_BAR_WITHOUT_REORDERING]);
    _mtlk_addba_peer_send_delba_req(peer, tid_idx, BA_DIR_RECIPIENT);
  }

  mtlk_osal_lock_release(&peer->addba->lock);

}

void __MTLK_IFUNC
mtlk_addba_peer_iterate (mtlk_addba_peer_t *peer)
{
  MTLK_ASSERT(peer != NULL);

  if (peer->is_active)
  {
    _mtlk_addba_peer_check_delba_timeouts(peer);
    _mtlk_addba_peer_check_addba_res_rx_timeouts(peer);
  }
}

int __MTLK_IFUNC
mtlk_addba_peer_start (mtlk_addba_peer_t *peer,
                       const IEEE_ADDR   *addr,
                       BOOL               dot11n_mode)
{
  MTLK_ASSERT(peer != NULL);
  MTLK_ASSERT(addr != NULL);
  MTLK_ASSERT(peer->is_active == FALSE);

  peer->addr      = *addr;

  if (dot11n_mode) {
    _mtlk_addba_peer_stop(peer);
     peer->is_active = TRUE;
  }

  return MTLK_ERR_OK;
}

#if 0
void __MTLK_IFUNC
mtlk_addba_peer_start_negotiation (mtlk_addba_peer_t *peer,
                                   uint16             rate)
{
  MTLK_ASSERT(peer != NULL);
  MTLK_ASSERT(peer->is_active == TRUE);

  if (_mtlk_addba_is_allowed_rate(rate)) {
    mtlk_osal_lock_acquire(&peer->addba->lock);
    _mtlk_addba_peer_start_negotiation(peer);
    mtlk_osal_lock_release(&peer->addba->lock);
  }
  else {
     /* Aggregations is not allowed for this rate */
    ILOG2_D("Aggregations are not allowed (rate=%d)", (int)rate);
  }
}
#endif /* 0 */

/*
  Close TX aggregation and send DELBA request to the peer

  \param peer     - handle to the peer [I]
  \param tid_idx  - Access protocol index [I]

  \return
    none
*/
void __MTLK_IFUNC
mtlk_addba_delba_req(mtlk_addba_peer_t *peer, uint16 tid_idx)
{
  MTLK_ASSERT(peer != NULL);
  MTLK_ASSERT(tid_idx < ARRAY_SIZE(peer->addba->cfg.tid));

  WLOG_YD("TX %Y TID=%d TX aggregation termination requested", &peer->addr, tid_idx);

  if (FALSE == peer->is_active)
  {
    ILOG1_YD("TX %Y TID=%d Skip TX aggregation termination, peer is not active", &peer->addr, tid_idx);
    return;
  }

  MTLK_ASSERT(peer->addba != NULL);

  mtlk_osal_lock_acquire(&peer->addba->lock);

  if (peer->tid[tid_idx].tx.state != MTLK_ADDBA_TX_ADD_AGGR_REQ_SENT &&
      peer->tid[tid_idx].tx.state != MTLK_ADDBA_TX_AGGR_OPENED)
  {
    ILOG1_YD("TX %Y TID=%d Skip TX aggregation termination, aggregation is not active", &peer->addr, tid_idx);
  }
  else
  {
    _mtlk_addba_peer_close_aggr_req(peer, tid_idx);

    _mtlk_addba_peer_send_delba_req(peer, tid_idx, BA_DIR_ORIGINATOR);
  }

  mtlk_osal_lock_release(&peer->addba->lock);

}

void __MTLK_IFUNC
mtlk_addba_delba_req_all(mtlk_addba_peer_t *peer)
{
  uint8 tid;

  MTLK_ASSERT(peer != NULL);
  MTLK_ASSERT(peer->addba != NULL);

  if (FALSE == peer->is_active)
  {
    return;
  }

  for (tid = 0; tid < NTS_TIDS; tid++)
  {
    mtlk_osal_lock_acquire(&peer->addba->lock);

    if (peer->tid[tid].tx.state == MTLK_ADDBA_TX_ADD_AGGR_REQ_SENT ||
        peer->tid[tid].tx.state == MTLK_ADDBA_TX_AGGR_OPENED)
    {
      _mtlk_addba_peer_close_aggr_req(peer, tid);

      _mtlk_addba_peer_send_delba_req(peer, tid, BA_DIR_ORIGINATOR);
    }

    mtlk_osal_lock_release(&peer->addba->lock);
  }
}

void __MTLK_IFUNC
mtlk_addba_on_qos_without_reord(mtlk_addba_peer_t *peer, uint16 tid_idx)
{
  mtlk_osal_msec_t now_time;
  mtlk_osal_ms_diff_t diff_time;

  if (!peer->is_active)
  {
    ELOG_Y("RX %Y: ADDBA peer is not active", &peer->addr);
    return;
  }

  if (tid_idx >= SIZEOF(peer->tid))
  {
    ELOG_YDD("RX %Y: wrong priority (%d >= %ld)",
        &peer->addr, tid_idx, SIZEOF(peer->tid));
    return;
  }

  mtlk_osal_lock_acquire(&peer->addba->lock);

  /* Limit rate of DELBA triggered by aggregated packets without reordering */

  now_time  = mtlk_osal_timestamp_to_ms(mtlk_osal_timestamp());
  diff_time = mtlk_osal_ms_time_diff(now_time, peer->tid[tid_idx].rx.delba_on_rx_time);

  if (diff_time >= MTLK_DELBA_REQ_ON_RX_INTERVAL_MS)
  {
    peer->tid[tid_idx].rx.delba_on_rx_time = now_time;

    if ((MTLK_ADDBA_RX_NONE == peer->tid[tid_idx].rx.state) ||
        (MTLK_ADDBA_RX_ADDBA_NEGATIVE_RES_SENT == peer->tid[tid_idx].rx.state))
    {
      ILOG1_YD("RX %Y TID=%d QoS packet without reordering. Will send DELBA.",
                &peer->addr, (int)tid_idx);

      _mtlk_addba_wss_cntr_inc(peer->wss_hcntrs[tid_idx][MTLK_ADDBAPI_CNT_RX_AGGR_PKT_WITHOUT_REORDERING]);
      _mtlk_addba_peer_send_delba_req(peer, tid_idx, BA_DIR_RECIPIENT);
    }
  }

  mtlk_osal_lock_release(&peer->addba->lock);
}

void __MTLK_IFUNC
mtlk_addba_peer_stop (mtlk_addba_peer_t *peer,
                      BOOL               dot11n_mode)
{
  MTLK_ASSERT(peer != NULL);

  if(dot11n_mode)
  {
    MTLK_ASSERT(peer->is_active == TRUE);

    peer->is_active = FALSE;
    _mtlk_addba_peer_stop(peer);
  }
  memset(&peer->addr, 0, sizeof(peer->addr));
}

void __MTLK_IFUNC
mtlk_addba_peer_start_aggr_negotiation (mtlk_addba_peer_t *peer,
                                        uint16             tid, BOOL ps_mode)
{
  MTLK_ASSERT(peer != NULL);
  MTLK_ASSERT(peer->is_active == TRUE);
  MTLK_ASSERT(tid < ARRAY_SIZE(peer->addba->cfg.tid));

  mtlk_osal_lock_acquire(&peer->addba->lock);
  if (peer->addba->cfg.tid[tid].use_aggr)
  {
    _mtlk_addba_peer_tx_addba_req(peer, tid, ps_mode);
  }
  mtlk_osal_lock_release(&peer->addba->lock);
}

BOOL __MTLK_IFUNC
mtlk_addba_peer_reset_power_save (mtlk_addba_peer_t *peer,
                                  uint16             tid_idx)
{
  BOOL res = FALSE;

  MTLK_ASSERT(peer != NULL);
  MTLK_ASSERT(peer->is_active == TRUE);
  MTLK_ASSERT(tid_idx < ARRAY_SIZE(peer->addba->cfg.tid));

  mtlk_osal_lock_acquire(&peer->addba->lock);
  if (peer->tid[tid_idx].tx.state == MTLK_ADDBA_TX_STA_POWER_SAVE)
  {
    peer->tid[tid_idx].tx.state = MTLK_ADDBA_TX_NONE;
    res = TRUE;
  }
  mtlk_osal_lock_release(&peer->addba->lock);

  return res;
}


int __MTLK_IFUNC
mtlk_addba_peer_status(mtlk_addba_peer_t *peer, mtlk_clpb_t *clpb)
{
  int res;
  mtlk_addba_peer_state_t *peer_state;

  MTLK_ASSERT(NULL != peer);
  MTLK_ASSERT(NULL != peer->addba);
  MTLK_ASSERT(NULL != clpb);
  MTLK_ASSERT(sizeof(peer_state->tid) == sizeof(peer->tid));

  peer_state = mtlk_osal_mem_alloc(sizeof(*peer_state), MTLK_MEM_TAG_CLPB);
  if(peer_state == NULL) {
    ELOG_V("Can't allocate clipboard data");
    return MTLK_ERR_NO_MEM;
  }
  memset(peer_state, 0, sizeof(*peer_state));

  mtlk_osal_lock_acquire(&peer->addba->lock);

  peer_state->addr = peer->addr;
  peer_state->is_active = peer->is_active;

  memcpy(peer_state->tid, peer->tid, sizeof(peer->tid));

  mtlk_osal_lock_release(&peer->addba->lock);

  res = mtlk_clpb_push_nocopy(clpb, peer_state, sizeof(*peer_state));

  if (MTLK_ERR_OK != res) {
    mtlk_osal_mem_free(peer_state);
    mtlk_clpb_purge(clpb);
  }

  return res;
}

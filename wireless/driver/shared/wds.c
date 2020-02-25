/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
 * WDS peer manager
 *
 * TODO: Collect all calls of _wds_send_fw_cfg_peer_ap() and put them inside FSM
 *       This should increase readability and simplify further code maintenance
 *
 */
#include "mtlkinc.h"
#include "mtlk_vap_manager.h"
#include "mtlk_coreui.h"
#include "core.h"
#include "mhi_umi.h"
#include "wds.h"

#define LOG_LOCAL_GID   GID_WDS
#define LOG_LOCAL_FID   0

#define RECONNECT_TIMER_MAX_MS    3000
#define CONNECTING_TIMEOUT_MS     1000
#define DISCONNECTING_TIMEOUT_MS  1000

#define WDS_ADD_PEER 1
#define WDS_DEL_PEER 0

typedef enum {
  WDS_ACTION_CONNECT,         /* Connection attempt for ex. on beacon*/
  WDS_ACTION_CONNECTED,       /* event from FW (UMI_CONNECTED) */
  WDS_ACTION_DISCONNECT,
  WDS_ACTION_ERROR,
} _wds_peer_action_t;

static const mtlk_ability_id_t _wds_abilities[] = {
  MTLK_CORE_REQ_SET_WDS_CFG,
  MTLK_CORE_REQ_GET_WDS_CFG,
  MTLK_CORE_REQ_GET_WDS_PEERAP,
  MTLK_CORE_REQ_SET_WDS_DBG
};

static const char wds_state_str[][14] = {
  {"DISCONNECTED "},
  {"CONNECTING   "},
  {"CONNECTED    "},
  {"DISCONNECTING"},
  {"ERROR        "}
};

static const char wds_action_str[][14] = {
  {"CONNECT"      },
  {"CONNECTED"    },
  {"DISCONNECT"   },
  {"ERROR"        },
};


MTLK_INIT_STEPS_LIST_BEGIN(wds)
  MTLK_INIT_STEPS_LIST_ENTRY(wds, ON_BEACON_LIST)
  MTLK_INIT_STEPS_LIST_ENTRY(wds, ON_BEACON_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(wds, PEERS_LIST)
  MTLK_INIT_STEPS_LIST_ENTRY(wds, PEERS_LIST_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(wds, PEERS_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(wds, REG_ABILITIES)
  MTLK_INIT_STEPS_LIST_ENTRY(wds, EN_ABILITIES)
MTLK_INIT_INNER_STEPS_BEGIN(wds)
MTLK_INIT_STEPS_LIST_END(wds);

MTLK_INIT_STEPS_LIST_BEGIN(wds_peer)
  MTLK_INIT_STEPS_LIST_ENTRY(wds_peer, LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(wds_peer, RECONNECT_TMR)
MTLK_INIT_INNER_STEPS_BEGIN(wds_peer)
MTLK_INIT_STEPS_LIST_END(wds_peer);

static uint32 _wds_get_reconnect_delay (wds_t *wds)
{
  mtlk_core_t *core;
  uint32 ka_in, delay, max_delay;

  core = mtlk_vap_get_core(wds->vap_handle);

  ka_in = mtlk_stadb_get_keepalive_interval(&core->slow_ctx->stadb);

  /* WDS unable to connect if reconnection delay greater than
     keepalive timer of AP on opposite side.
     Assuming KA timer on both AP configured to the same value.
     Ignore Keepalive timer value if less than reasonable or
     greater than MAX. KA might be changed onfly.
     According to 802.11 Keepalive is an integer * 1024ms. */
  if (ka_in < 500)
  {
    delay = RECONNECT_TIMER_MAX_MS;
    goto end;
  }

  /* ~(keep_alive - 200ms)*0.75; */
  delay = (ka_in - 200) /4 *3;
  max_delay = RECONNECT_TIMER_MAX_MS;

  if (delay > max_delay) delay = max_delay;

end:
  ILOG3_D("WDS Reconnection delay %d", delay);
  return delay;
}

static wds_t* _wds_get_master (wds_t *wds)
{
  mtlk_vap_handle_t  master_vap;

  mtlk_vap_manager_get_master_vap(mtlk_vap_get_manager(wds->vap_handle), &master_vap);
  return &(mtlk_vap_get_core(master_vap)->slow_ctx->wds_mng);
}

static void _wds_on_beacon_init (wds_t *wds, wds_on_beacon_t *on_beacon, const IEEE_ADDR *addr)
{
  on_beacon->addr = *addr;
  on_beacon->vap_id = mtlk_vap_get_id(wds->vap_handle);
}

static int _wds_on_beacon_create_add (wds_t *wds, const IEEE_ADDR *addr)
{
  wds_on_beacon_t *on_beacon = NULL;
  wds_t           *master_wds;

  on_beacon = mtlk_osal_mem_alloc(sizeof(wds_on_beacon_t), LQLA_MEM_TAG_WDS);

  if (!on_beacon)
  {
    WLOG_V("Can't alloc on beacon entry");
    return MTLK_ERR_NO_MEM;
  }

  ILOG3_D("CID-%04x: WDS create on_beacon entry ", mtlk_vap_get_oid(wds->vap_handle));
  memset(on_beacon, 0, sizeof(wds_on_beacon_t));

  _wds_on_beacon_init(wds, on_beacon, addr);

  master_wds = _wds_get_master(wds);

#ifdef MTLK_DEBUG
  /* Assert on duplicated entry */
  ASSERT(NULL == wds_on_beacon_exclude(master_wds, addr));
#endif

  mtlk_osal_lock_acquire(&master_wds->on_beacon_lock);
  mtlk_dlist_push_front(&master_wds->on_beacon_list, &on_beacon->list_entry);
  mtlk_osal_lock_release(&master_wds->on_beacon_lock);

  return MTLK_ERR_OK;
}

static int _wds_send_fw_cfg_peer_ap (wds_peer_t *peer, int add_peer)
{
  int res;
  mtlk_txmm_msg_t    man_msg;
  mtlk_txmm_data_t  *man_entry = NULL;
  UMI_PEERAP_REQ    *umi_peer_ap;
  mtlk_vap_handle_t  vap_handle;

  MTLK_ASSERT(peer != NULL);

  vap_handle = peer->wds->vap_handle;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(vap_handle), NULL);
  if (!man_entry){
    ELOG_V("Can't add/delete WDS peer AP due to lack of MAN_MSG");
    return MTLK_ERR_NO_RESOURCES;
  }

  umi_peer_ap = (UMI_PEERAP_REQ*)man_entry->payload;

  man_entry->id = (add_peer == WDS_ADD_PEER) ? UM_MAN_ADD_PEERAP_REQ : UM_MAN_DELETE_PEERAP_REQ;
  man_entry->payload_size = sizeof(*umi_peer_ap);

  umi_peer_ap->sStationID = peer->addr;
  umi_peer_ap->u16Status = UMI_OK;

  if (add_peer == WDS_ADD_PEER)
  {
    ILOG1_DY("CID-%04x: WDS Sending UM_MAN_ADD_PEERAP_REQ %Y", mtlk_vap_get_oid(vap_handle), peer->addr.au8Addr);
  }
  else
  {
    ILOG1_DY("CID-%04x: WDS Sending UM_MAN_DEL_PEERAP_REQ %Y", mtlk_vap_get_oid(vap_handle), peer->addr.au8Addr);
  }
  res = 0;
  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);

  if (MTLK_ERR_OK != res) {
    uint16 status;
    status = le16_to_cpu(umi_peer_ap->u16Status);

    switch (status) {
    case UMI_OUT_OF_MEMORY:
      WLOG_D("CID-%04x: No more peer-AP allowed", mtlk_vap_get_oid(vap_handle));
      res = MTLK_ERR_NO_MEM;
      break;
    case UMI_BAD_VALUE:
      WLOG_D("CID-%04x: There is no such peer-AP configured", mtlk_vap_get_oid(vap_handle));
      res = MTLK_ERR_PARAMS;
      break;
    default:
      ELOG_DYD("CID-%04x: Can't ADD/DELETE peer AP %Y, status is %d", mtlk_vap_get_oid(vap_handle), peer->addr.au8Addr, status);
      res = MTLK_ERR_UNKNOWN;
    }
  }
  else
  {
    peer->fw_peer_sta_added = (add_peer == WDS_ADD_PEER);
  }

  mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}


static void _wds_peer_cleanup (wds_peer_t *peer)
{

  ILOG3_DY("CID-%04x: WDS peer cleanup %Y", mtlk_vap_get_oid(peer->wds->vap_handle), peer->addr.au8Addr);

  MTLK_CLEANUP_BEGIN(wds_peer, MTLK_OBJ_PTR(peer));
    MTLK_CLEANUP_STEP(wds_peer, RECONNECT_TMR, MTLK_OBJ_PTR(peer),
                      mtlk_osal_timer_cleanup, (&peer->fsm_timer));

    MTLK_CLEANUP_STEP(wds_peer, LOCK, MTLK_OBJ_PTR(peer),
                      mtlk_osal_mutex_cleanup, (&peer->lock));

  MTLK_CLEANUP_END(wds_peer, MTLK_OBJ_PTR(peer));

}

static int _wds_peer_init (wds_t *wds, wds_peer_t *peer, const IEEE_ADDR *addr)
{

  peer->addr = *addr;
  peer->wds = wds;
  peer->state = WDS_STATE_DISCONNECTED;

  ILOG3_DY("CID-%04x: WDS peer init %Y", mtlk_vap_get_oid(peer->wds->vap_handle), peer->addr.au8Addr);

  MTLK_INIT_TRY(wds_peer, MTLK_OBJ_PTR(peer))
    MTLK_INIT_STEP(wds_peer, LOCK, MTLK_OBJ_PTR(peer),
                   mtlk_osal_mutex_init, (&peer->lock));

    MTLK_INIT_STEP(wds_peer, RECONNECT_TMR, MTLK_OBJ_PTR(peer),
                   mtlk_osal_timer_init,
                   (&peer->fsm_timer, mtlk_core_wds_on_timer, (mtlk_handle_t)peer));

  MTLK_INIT_FINALLY(wds_peer, MTLK_OBJ_PTR(peer));
  MTLK_INIT_RETURN(wds_peer, MTLK_OBJ_PTR(peer), _wds_peer_cleanup, (peer));

}

static void _wds_peer_remove_delete (wds_peer_t *peer)
{
  wds_t *wds = peer->wds;
  wds_t *master_wds;
  wds_on_beacon_t *on_beacon;

  /* Remove reference to STA if exist */
  if (peer->sta){
    mtlk_sta_decref(peer->sta);
    peer->sta = NULL;
  }

  master_wds = _wds_get_master(wds);
  on_beacon = wds_on_beacon_exclude(master_wds, &peer->addr);
  if (on_beacon)
  {
    mtlk_osal_mem_free(on_beacon);
  }

  ILOG3_DY("CID-%04x: WDS peer remove from list %Y", mtlk_vap_get_oid(peer->wds->vap_handle), peer->addr.au8Addr);

  /* Remove peer from WDS DB */
  mtlk_osal_lock_acquire(&wds->peers_list_lock);
  mtlk_dlist_remove(&wds->peers_list, &peer->list_entry);
  mtlk_osal_lock_release(&wds->peers_list_lock);

  /* Delete peer context */
  _wds_peer_cleanup(peer);

  ILOG3_DY("CID-%04x: WDS peer delete context %Y", mtlk_vap_get_oid(peer->wds->vap_handle), peer->addr.au8Addr);
  mtlk_osal_mem_free(peer);
}

static int _wds_peer_create_add (wds_t *wds, const IEEE_ADDR *addr)
{
	
  int         res = MTLK_ERR_UNKNOWN;
  wds_peer_t  *peer = NULL;

  ILOG3_DD("CID-%04x: WDS peer create on wds 0x%08X", mtlk_vap_get_oid(wds->vap_handle), (uint32)wds);
  peer = mtlk_osal_mem_alloc(sizeof(wds_peer_t), LQLA_MEM_TAG_WDS);

  if (!peer)
  {
    WLOG_V("Can't alloc peer AP");
    return MTLK_ERR_NO_MEM;
  }

  memset(peer, 0, sizeof(wds_peer_t));

  res = _wds_peer_init(wds, peer, addr);
  if (res != MTLK_ERR_OK)
  {
    ELOG_D("Can't init peer AP (err#%d)", res);
    goto err_init;
  }

  mtlk_osal_lock_acquire(&wds->peers_list_lock);
  mtlk_dlist_push_front(&wds->peers_list, &peer->list_entry);
  mtlk_osal_lock_release(&wds->peers_list_lock);

  /* Wait for a beacon to connect if core is in connected state */
  if (mtlk_core_is_connected(mtlk_vap_get_core(wds->vap_handle)))
  {
    _wds_on_beacon_create_add(wds, addr);
  }

  return MTLK_ERR_OK;

err_init:
  mtlk_osal_mem_free(peer);

  return res;
}

static wds_peer_t *_wds_peer_find (wds_t *wds, const IEEE_ADDR *addr)
{
  mtlk_dlist_entry_t *entry;
  mtlk_dlist_entry_t *head;
  wds_peer_t *peer;

  mtlk_dlist_foreach(&wds->peers_list, entry, head)
  {
    peer = MTLK_LIST_GET_CONTAINING_RECORD(entry, wds_peer_t, list_entry);
    if (0 == mtlk_osal_compare_eth_addresses(addr->au8Addr, peer->addr.au8Addr))
    {
      ILOG3_DY("CID-%04x: WDS peer found %Y", mtlk_vap_get_oid(wds->vap_handle), peer->addr.au8Addr);
      return peer;
    }
  }

  ILOG3_DY("CID-%04x: WDS peer NOT found %Y", mtlk_vap_get_oid(wds->vap_handle), addr->au8Addr);
  return NULL;
}

static int _wds_peer_fsm (wds_peer_t *peer, _wds_peer_action_t action)
{
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(peer != NULL);

  ILOG3_DYSS("CID-%04x: WDS peer FSM started %Y. Action %s. State %s",
    mtlk_vap_get_oid(peer->wds->vap_handle),
    peer->addr.au8Addr,
    wds_action_str[action],
    wds_state_str[peer->state]);

  if ((peer->state == WDS_STATE_DISCONNECTED) ||
      (peer->state == WDS_STATE_ERROR))
  {

    switch(action)
    {
      case WDS_ACTION_CONNECT:
        peer->state = WDS_STATE_CONNECTING;
        mtlk_osal_timer_set(&peer->fsm_timer, CONNECTING_TIMEOUT_MS);
        break;

      case WDS_ACTION_ERROR:
          peer->state = WDS_STATE_ERROR;
          mtlk_osal_timer_set(&peer->fsm_timer, _wds_get_reconnect_delay(peer->wds));
        break;

      case WDS_ACTION_DISCONNECT:
        if (peer->kill_on_disconnect)
        {
          _wds_peer_remove_delete(peer);
          return res;
        }
        break;

      default:
        WLOG_DD("Unexpected action %d; state=DISC/ERR(%d)", action, peer->state);
    }

    goto wds_peer_fsm_end;
  }

  if (peer->state == WDS_STATE_CONNECTING)
  {
    switch(action)
    {
      case WDS_ACTION_DISCONNECT:
        peer->state = WDS_STATE_DISCONNECTED; /* Happens when trying to connect to incompatible peerAP */
        mtlk_osal_timer_set(&peer->fsm_timer, _wds_get_reconnect_delay(peer->wds));
        break;

      case WDS_ACTION_CONNECTED:
        peer->state = WDS_STATE_CONNECTED;
        mtlk_osal_timer_cancel(&peer->fsm_timer);
        break;

      case WDS_ACTION_ERROR:
        peer->state = WDS_STATE_ERROR;
        mtlk_osal_timer_set(&peer->fsm_timer, _wds_get_reconnect_delay(peer->wds));
        break;

      default:
        WLOG_DD("Unexpected action %d; state=CONNECTING(%d)", action, peer->state);
    }
    goto wds_peer_fsm_end;
  }

  if (peer->state == WDS_STATE_CONNECTED)
  {
    switch(action)
    {
      case WDS_ACTION_DISCONNECT:
        mtlk_osal_timer_set(&peer->fsm_timer, DISCONNECTING_TIMEOUT_MS);
        peer->state = WDS_STATE_DISCONNECTING;
        break;

      default:
        WLOG_DD("Unexpected action %d; state=CONNECTED(%d)", action, peer->state);
    }
    goto wds_peer_fsm_end;
  }

  if (peer->state == WDS_STATE_DISCONNECTING)
  {
    switch(action)
    {
      case WDS_ACTION_DISCONNECT:
        if (peer->kill_on_disconnect)
        {
          _wds_peer_remove_delete(peer);
          return res;
        }
        else
        {
          peer->state = WDS_STATE_DISCONNECTED;
          mtlk_osal_timer_set(&peer->fsm_timer, _wds_get_reconnect_delay(peer->wds));
        }

        break;
      default:
        WLOG_DD("Unexpected action %d; state=DISCONNECTING(%d)", action, peer->state);
    }
    goto wds_peer_fsm_end;
  }

  ASSERT(0);

wds_peer_fsm_end:

  ILOG3_DYS("CID-%04x: WDS peer FSM finished %Y. New state %s",
    mtlk_vap_get_oid(peer->wds->vap_handle),
    peer->addr.au8Addr,
    wds_state_str[peer->state]);

  return res;
}

wds_on_beacon_t * __MTLK_IFUNC
wds_on_beacon_exclude (wds_t *master_wds, const IEEE_ADDR *addr)
{
  mtlk_dlist_entry_t  *entry;
  mtlk_dlist_entry_t  *head;
  wds_on_beacon_t     *on_beacon_excl;

  /* Note: Executed within Tasklet and/or Thread contexts */

  /* The function finds addr in on_beacon list and exclude its
     entry if was found. */

  MTLK_ASSERT(mtlk_vap_is_master_ap(master_wds->vap_handle));

  on_beacon_excl = NULL;
  mtlk_osal_lock_acquire(&master_wds->on_beacon_lock);
  mtlk_dlist_foreach(&master_wds->on_beacon_list, entry, head)
  {
    wds_on_beacon_t *on_beacon;
    on_beacon = MTLK_LIST_GET_CONTAINING_RECORD(entry, wds_on_beacon_t, list_entry);
    if (0 == mtlk_osal_compare_eth_addresses(addr->au8Addr, on_beacon->addr.au8Addr))
    {
      ILOG3_DYD("CID-%04x: WDS on beacon excluded %Y, VAP %d",
        mtlk_vap_get_oid(master_wds->vap_handle),
        on_beacon->addr.au8Addr,
        on_beacon->vap_id);

      on_beacon_excl = on_beacon;
      mtlk_dlist_remove(&master_wds->on_beacon_list, entry);
      break;
    }
  }
  mtlk_osal_lock_release(&master_wds->on_beacon_lock);

  return on_beacon_excl;
}

int __MTLK_IFUNC
wds_on_beacon_proc (mtlk_handle_t master_wds_handle, const void *data,  uint32 data_size)
{
  wds_t               *wds;
  wds_on_beacon_t     *on_beacon;
  mtlk_vap_manager_t  *vap_mng;
  mtlk_vap_handle_t   vap_handle;
  mtlk_core_t         *core;
  wds_peer_t          *peer;
  int                 res;

  MTLK_ASSERT(data_size == sizeof(wds_on_beacon_t));

  on_beacon = (wds_on_beacon_t*)data;
  vap_mng = mtlk_vap_get_manager(((wds_t*)master_wds_handle)->vap_handle);

  if (MTLK_ERR_OK != mtlk_vap_manager_get_vap_handle(vap_mng, on_beacon->vap_id, &vap_handle))
    return MTLK_ERR_OK;   /* VAP not exist */

  core = mtlk_vap_get_core(vap_handle);

  if (!mtlk_core_is_connected(core)){
    ILOG2_V("WDS: VAP not active. Ignore ADD PEER on beacon");
    return MTLK_ERR_OK;
  }

  if (BR_MODE_WDS != MTLK_CORE_HOT_PATH_PDB_GET_INT(core, CORE_DB_CORE_BRIDGE_MODE))
    return MTLK_ERR_OK;   /* VAP not in WDS mode */

  /* Find beacon bssid in WDS peer list of particular VAP */
  wds = &core->slow_ctx->wds_mng;
  mtlk_osal_mutex_acquire(&wds->peers_lock);

  peer = _wds_peer_find(wds, &on_beacon->addr);

  if (peer) { /* peer might be already deleted due to delay in serializer */

    /* Send ADD peer command to FW */
    res = _wds_send_fw_cfg_peer_ap(peer, WDS_ADD_PEER);
    if (MTLK_ERR_OK == res)
    {
      _wds_peer_fsm(peer, WDS_ACTION_CONNECT);
    }
    else
    {
      _wds_peer_fsm(peer, WDS_ACTION_ERROR);
    }

  }

  mtlk_osal_mutex_release(&wds->peers_lock);

  return MTLK_ERR_OK;
}

void __MTLK_IFUNC
wds_cleanup (wds_t *wds)
{
  mtlk_dlist_entry_t *entry;
  wds_peer_t *peer;

  /* delete all peers */
  mtlk_osal_lock_acquire(&wds->peers_list_lock);
  while( (entry = mtlk_dlist_pop_front(&wds->peers_list)))
  {
    peer = MTLK_LIST_GET_CONTAINING_RECORD(entry, wds_peer_t, list_entry);

    _wds_peer_cleanup(peer);

    ILOG2_DY("CID-%04x: WDS peer delete context %Y", mtlk_vap_get_oid(peer->wds->vap_handle), peer->addr.au8Addr);
    mtlk_osal_mem_free(peer);
  }
  mtlk_osal_lock_release(&wds->peers_list_lock);

  ILOG2_DD("CID-%04x: WDS Cleanup (0x%08X)", mtlk_vap_get_oid(wds->vap_handle), (uint32)wds);

  MTLK_CLEANUP_BEGIN(wds, MTLK_OBJ_PTR(wds))

    MTLK_CLEANUP_STEP(wds, EN_ABILITIES, MTLK_OBJ_PTR(wds),
                      wds_disable_abilities, (wds));

    MTLK_CLEANUP_STEP(wds, REG_ABILITIES, MTLK_OBJ_PTR(wds),
                      mtlk_abmgr_unregister_ability_set,
                      (mtlk_vap_get_abmgr(wds->vap_handle),
                       _wds_abilities, ARRAY_SIZE(_wds_abilities)));

    MTLK_CLEANUP_STEP(wds, PEERS_LOCK, MTLK_OBJ_PTR(wds),
                      mtlk_osal_mutex_cleanup, (&wds->peers_lock));

    MTLK_CLEANUP_STEP(wds, PEERS_LIST_LOCK, MTLK_OBJ_PTR(wds),
                      mtlk_osal_lock_cleanup, (&wds->peers_list_lock));

    MTLK_CLEANUP_STEP(wds, PEERS_LIST, MTLK_OBJ_PTR(wds),
                      mtlk_dlist_cleanup, (&wds->peers_list));

    MTLK_CLEANUP_STEP(wds, ON_BEACON_LOCK, MTLK_OBJ_PTR(wds),
                      mtlk_osal_lock_cleanup, (&wds->on_beacon_lock));

    MTLK_CLEANUP_STEP(wds, ON_BEACON_LIST, MTLK_OBJ_PTR(wds),
                      mtlk_dlist_cleanup, (&wds->on_beacon_list));

  MTLK_CLEANUP_END(wds, MTLK_OBJ_PTR(wds));

}

int __MTLK_IFUNC
wds_init (wds_t *wds, mtlk_vap_handle_t vap_handle)
{

  wds->vap_handle = vap_handle;

  ILOG2_DD("CID-%04x: WDS Init (0x%08X)", mtlk_vap_get_oid(wds->vap_handle), (uint32)wds);

  MTLK_INIT_TRY(wds, MTLK_OBJ_PTR(wds));

    MTLK_INIT_STEP_VOID(wds, ON_BEACON_LIST, MTLK_OBJ_PTR(wds),
                        mtlk_dlist_init, (&wds->on_beacon_list));

    MTLK_INIT_STEP(wds, ON_BEACON_LOCK, MTLK_OBJ_PTR(wds),
                   mtlk_osal_lock_init, (&wds->on_beacon_lock));

    MTLK_INIT_STEP_VOID(wds, PEERS_LIST, MTLK_OBJ_PTR(wds),
                        mtlk_dlist_init, (&wds->peers_list));

    MTLK_INIT_STEP(wds, PEERS_LIST_LOCK, MTLK_OBJ_PTR(wds),
                   mtlk_osal_lock_init, (&wds->peers_list_lock));

    MTLK_INIT_STEP(wds, PEERS_LOCK, MTLK_OBJ_PTR(wds),
                   mtlk_osal_mutex_init, (&wds->peers_lock));

    MTLK_INIT_STEP(wds, REG_ABILITIES, MTLK_OBJ_PTR(wds),
                   mtlk_abmgr_register_ability_set,
                   (mtlk_vap_get_abmgr(wds->vap_handle),
                    _wds_abilities, ARRAY_SIZE(_wds_abilities)));

    /* Note: WDS abilities are enabled on Bridge mode change to WDS */
    MTLK_INIT_STEP_VOID(wds, EN_ABILITIES, MTLK_OBJ_PTR(wds),
                        MTLK_NOACTION, ());

  MTLK_INIT_FINALLY(wds, MTLK_OBJ_PTR(wds))
  MTLK_INIT_RETURN(wds, MTLK_OBJ_PTR(wds), wds_cleanup, (wds))

}

int __MTLK_IFUNC
wds_usr_add_peer_ap (wds_t *wds, const IEEE_ADDR *addr)
{
  mtlk_core_t *core;
  int res;
  uint32 max_sta;

  ILOG2_DY("CID-%04x: WDS adding new peer ap %Y", mtlk_vap_get_oid(wds->vap_handle), addr->au8Addr);
  mtlk_osal_mutex_acquire(&wds->peers_lock);

  /* Check is peer already in wds db */
  if (NULL != _wds_peer_find(wds, addr))
  {
    ELOG_V("Peer already exist");
    res = MTLK_ERR_ALREADY_EXISTS;
    goto end;
  }

  /* Check is maximum allowed number of peers reached */
  core = mtlk_vap_get_core(wds->vap_handle);
  max_sta = mtlk_core_get_max_stas_supported_by_fw(core);
  if (max_sta == mtlk_dlist_size(&wds->peers_list))
  {
    ELOG_D("MAX number of Peer APs reached (%d)", max_sta);
    res = MTLK_ERR_NO_MEM;
    goto end;
  }

  res = _wds_peer_create_add(wds, addr);

end:

  mtlk_osal_mutex_release(&wds->peers_lock);
  return res;
}

int __MTLK_IFUNC
wds_usr_del_peer_ap (wds_t *wds, const IEEE_ADDR *addr)
{
  int res = MTLK_ERR_OK;
  wds_peer_t  *peer;

  ILOG2_DY("CID-%04x: WDS delete peer ap %Y", mtlk_vap_get_oid(wds->vap_handle), addr->au8Addr);
  mtlk_osal_mutex_acquire(&wds->peers_lock);

  /* find peer in wds DB*/
  peer = _wds_peer_find(wds, addr);
  if (NULL == peer)
  {
    WLOG_Y("Can't remove peerAP %Y. Peer AP doesn't exist", addr);
    res = MTLK_ERR_NO_ENTRY;
    goto end;
  }

  peer->kill_on_disconnect = TRUE;

  if (peer->fw_peer_sta_added)
  {
    res = _wds_send_fw_cfg_peer_ap(peer, WDS_DEL_PEER);
  }

  if (MTLK_ERR_OK == res)
  {
    _wds_peer_fsm(peer, WDS_ACTION_DISCONNECT);
  }

end:

  mtlk_osal_mutex_release(&wds->peers_lock);
  return res;
}

void __MTLK_IFUNC
wds_peer_connect (wds_t *wds, sta_entry *sta, const IEEE_ADDR *addr)
{
  wds_peer_t  *peer;

  ILOG2_DY("CID-%04x: WDS peer connect %Y", mtlk_vap_get_oid(wds->vap_handle), addr->au8Addr);
  mtlk_osal_mutex_acquire(&wds->peers_lock);

  /* find peer in wds DB*/
  peer = _wds_peer_find(wds, addr);
  if (peer)
  {
    if (sta)  /* STA might be NULL due to FAIL on add STA */
    {
      mtlk_sta_incref(sta);
      peer->sta = sta;
      _wds_peer_fsm(peer, WDS_ACTION_CONNECTED);
    }
    else
    { /* connection failed. Remove peer AP */
      _wds_send_fw_cfg_peer_ap(peer, WDS_DEL_PEER);
      _wds_peer_fsm(peer, WDS_ACTION_ERROR);
    }
  }

  mtlk_osal_mutex_release(&wds->peers_lock);
  return;
}

int __MTLK_IFUNC
wds_peer_disconnect (wds_t *wds, const IEEE_ADDR *addr, int *terminate)
{
  wds_peer_t  *peer;
  int res = MTLK_ERR_OK;

  *terminate = FALSE;

  mtlk_osal_mutex_acquire(&wds->peers_lock);

  peer = _wds_peer_find(wds, addr);
  if (peer)
  {
    ILOG1_DY("CID-%04x: WDS peer disconnect %Y", mtlk_vap_get_oid(wds->vap_handle), addr);

    /* Remove reference to STA if exist */
    if (peer->sta){
      mtlk_sta_decref(peer->sta);
      peer->sta = NULL;
    }

    if (peer->fw_peer_sta_added)
    {
      res = _wds_send_fw_cfg_peer_ap(peer, WDS_DEL_PEER);
      *terminate = TRUE;  /* terminate further disconnection. Wait for UMI_DISCONNECTED */
    }

    if (MTLK_ERR_OK == res)
      _wds_peer_fsm(peer, WDS_ACTION_DISCONNECT);
  }

  mtlk_osal_mutex_release(&wds->peers_lock);
  return res;
}


void __MTLK_IFUNC
wds_enable_abilities (wds_t *wds)
{
  if (wds && mtlk_vap_is_ap(wds->vap_handle))
  {
    ILOG2_D("CID-%04x: Enable WDS abilities", mtlk_vap_get_oid(wds->vap_handle));
    mtlk_abmgr_enable_ability_set(mtlk_vap_get_abmgr(wds->vap_handle),
                                  _wds_abilities, ARRAY_SIZE(_wds_abilities));
  }

  return;
}

void __MTLK_IFUNC
wds_disable_abilities (wds_t *wds)
{

  if (wds && mtlk_vap_is_ap(wds->vap_handle))
  {
    ILOG2_D("CID-%04x: Disable WDS abilities", mtlk_vap_get_oid(wds->vap_handle));
    mtlk_abmgr_disable_ability_set(mtlk_vap_get_abmgr(wds->vap_handle),
                                    _wds_abilities, ARRAY_SIZE(_wds_abilities));
  }
  return;
}

void __MTLK_IFUNC
wds_on_if_down (wds_t *wds)
{

  mtlk_dlist_entry_t *entry;
  mtlk_dlist_entry_t *head;
  wds_peer_t *peer;
  wds_on_beacon_t *on_beacon;
  wds_t *master_wds;
  uint8 vap_id;

  ILOG2_D("CID-%04x: WDS on IF DOWN", mtlk_vap_get_oid(wds->vap_handle));
  vap_id = mtlk_vap_get_id(wds->vap_handle);

  /* Remove all STA with this->vap_handle from master's on_beacon_DB */
  master_wds = _wds_get_master(wds);
  mtlk_osal_lock_acquire(&master_wds->on_beacon_lock);
  mtlk_dlist_foreach(&master_wds->on_beacon_list, entry, head)
  {
    on_beacon = MTLK_LIST_GET_CONTAINING_RECORD(entry, wds_on_beacon_t, list_entry);
    if (vap_id == on_beacon->vap_id)
    {
      mtlk_dlist_entry_t *prev_entry;

      ILOG3_DY("CID-%04x: WDS On beacon record removed %Y", mtlk_vap_get_oid(wds->vap_handle), on_beacon->addr.au8Addr);
      prev_entry = mtlk_dlist_prev(entry);
      mtlk_dlist_remove(&master_wds->on_beacon_list, entry);
      entry = prev_entry;

      mtlk_osal_mem_free(on_beacon);
    }
  }
  mtlk_osal_lock_release(&master_wds->on_beacon_lock);

  /* Remove Peers from WDS DB */
  mtlk_osal_mutex_acquire(&wds->peers_lock);

  mtlk_dlist_foreach(&wds->peers_list, entry, head)
  {
    peer = MTLK_LIST_GET_CONTAINING_RECORD(entry, wds_peer_t, list_entry);
    ILOG3_DY("CID-%04x: WDS peer force disconnect %Y", mtlk_vap_get_oid(wds->vap_handle), peer->addr.au8Addr);

    peer->kill_on_disconnect = FALSE;
    peer->fw_peer_sta_added = FALSE;
    mtlk_osal_timer_cancel(&peer->fsm_timer);
    peer->state = WDS_STATE_DISCONNECTED;
    if (peer->sta){
      mtlk_sta_decref(peer->sta);
      peer->sta = NULL;
    }
  }

  mtlk_osal_mutex_release(&wds->peers_lock);
  return;
}

void __MTLK_IFUNC
wds_on_if_up (wds_t *wds)
{
  mtlk_dlist_entry_t *entry;
  mtlk_dlist_entry_t *head;
  wds_peer_t *peer;

  ILOG2_D("CID-%04x: WDS on IF UP", mtlk_vap_get_oid(wds->vap_handle));
  mtlk_osal_mutex_acquire(&wds->peers_lock);

  mtlk_dlist_foreach(&wds->peers_list, entry, head)
  {
    peer = MTLK_LIST_GET_CONTAINING_RECORD(entry, wds_peer_t, list_entry);
    _wds_on_beacon_create_add(wds, &peer->addr);
  }

  mtlk_osal_mutex_release(&wds->peers_lock);
  return;
}

int __MTLK_IFUNC
mtlk_wds_get_peer_vect (wds_t *wds, mtlk_clpb_t **ppclpb_peer_vect)
{
  int res = MTLK_ERR_OK;
  mtlk_dlist_entry_t *entry;
  mtlk_dlist_entry_t *head;
  wds_peer_t *peer;

  *ppclpb_peer_vect = mtlk_clpb_create();
  if (NULL == *ppclpb_peer_vect) {
    ELOG_V("Cannot allocate clipboard object");
    res = MTLK_ERR_NO_MEM;
    goto end;
  }

  /* Add peers to output data buff */
  mtlk_osal_mutex_acquire(&wds->peers_lock);
  mtlk_dlist_foreach(&wds->peers_list, entry, head)
  {
    peer = MTLK_LIST_GET_CONTAINING_RECORD(entry, wds_peer_t, list_entry);
    res = mtlk_clpb_push(*ppclpb_peer_vect, &peer->addr, sizeof(IEEE_ADDR));
    if(MTLK_ERR_OK != res) {
      ELOG_V("Cannot push data to the clipboard");
      goto err_push_data;
    }
  }

  goto no_err;

err_push_data:
  mtlk_clpb_delete(*ppclpb_peer_vect);
  *ppclpb_peer_vect = NULL;

no_err:
  mtlk_osal_mutex_release(&wds->peers_lock);

end:
  return res;
}


int __MTLK_IFUNC
wds_get_peers_status(wds_t *wds)
{
  int res = MTLK_ERR_OK;

  mtlk_dlist_entry_t *entry;
  mtlk_dlist_entry_t *head;
  wds_peer_t *peer;
  wds_t *master_wds;
  wds_on_beacon_t *on_beacon;

  WLOG_V("\nWDS Peers list");
  WLOG_V("ADDR              State          Kill on disc   STA");
/*WLOG_V("11:11:11:11:11:11 DISCONNECTING        1         "); */

  mtlk_osal_mutex_acquire(&wds->peers_lock);
  mtlk_dlist_foreach(&wds->peers_list, entry, head)
  {
    peer = MTLK_LIST_GET_CONTAINING_RECORD(entry, wds_peer_t, list_entry);
    WLOG_YSDD("%Y %s        %d        0x%08X",
      peer->addr.au8Addr,
      wds_state_str[peer->state],
      peer->kill_on_disconnect,
      peer->sta);
  }
  mtlk_osal_mutex_release(&wds->peers_lock);

  /* Print-out On_beacon list */
  WLOG_V("\nWDS on beacon peer list");
  WLOG_V("ADDR               Vap ID");
/*WLOG_V("11:11:11:11:11:11     1"); */

  master_wds = _wds_get_master(wds);
  mtlk_osal_lock_acquire(&master_wds->on_beacon_lock);
  mtlk_dlist_foreach(&master_wds->on_beacon_list, entry, head)
  {
    on_beacon = MTLK_LIST_GET_CONTAINING_RECORD(entry, wds_on_beacon_t, list_entry);
    WLOG_YD("%Y     %d",
      on_beacon->addr.au8Addr,
      on_beacon->vap_id);
  }
  mtlk_osal_lock_release(&master_wds->on_beacon_lock);

  /* Print out WDS Hosts DB */
  WLOG_V("\nWDS hosts list");
  mtlk_hstdb_dbg(&(mtlk_vap_get_core(wds->vap_handle)->slow_ctx->hstdb));

  return res;
}

int wds_peer_fsm_timer (mtlk_handle_t wds_handle, const void *peer_addr,  uint32 ieee_addr_size)
{
  wds_t       *wds;
  wds_peer_t  *peer;

  MTLK_ASSERT(ieee_addr_size == sizeof(IEEE_ADDR));

  wds = (wds_t *)wds_handle;

  peer = _wds_peer_find(wds, (IEEE_ADDR*)peer_addr);

  if (!peer){
    /* Peer is already removed */
    return MTLK_ERR_OK;
  }

  if (peer->state == WDS_STATE_CONNECTING){
    WLOG_DY("CID-%04x: WDS peer connecting timeout %Y", mtlk_vap_get_oid(peer->wds->vap_handle), peer->addr.au8Addr);
    _wds_send_fw_cfg_peer_ap(peer, WDS_DEL_PEER);
    _wds_peer_fsm(peer, WDS_ACTION_ERROR);
  } 
  else if (peer->state == WDS_STATE_DISCONNECTING){
    WLOG_DY("CID-%04x: WDS peer disconnecting timeout %Y", mtlk_vap_get_oid(peer->wds->vap_handle), peer->addr.au8Addr);
    _wds_peer_fsm(peer, WDS_ACTION_DISCONNECT);
  }
  else if ( peer->state == WDS_STATE_ERROR ||
            peer->state == WDS_STATE_DISCONNECTED){

    ILOG3_DY("CID-%04x: WDS peer reconnection timer %Y", mtlk_vap_get_oid(peer->wds->vap_handle), peer->addr.au8Addr);
    _wds_on_beacon_create_add(wds, &peer->addr);
  }
  else{
    ILOG3_DY("CID-%04x: Unexpected WDS timer trigger %Y", mtlk_vap_get_oid(peer->wds->vap_handle), peer->addr.au8Addr);
  }

  return MTLK_ERR_OK;
}

/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
  * WDS peer manager header file
 *
 */
#ifndef __WDS_H__
#define __WDS_H__

typedef enum {
  WDS_STATE_DISCONNECTED,
  WDS_STATE_CONNECTING,
  WDS_STATE_CONNECTED,
  WDS_STATE_DISCONNECTING,
  WDS_STATE_ERROR
} wds_peer_state_t;

typedef struct _wds_t {
  mtlk_osal_mutex_t     peers_lock;       /* Protect content of peers list entries */

  mtlk_osal_spinlock_t  peers_list_lock;  /* Protect peers list only */
  mtlk_dlist_t          peers_list;       /* Shared access Tasklet(RO)/Serializer(RW) */

  /* "On beacon" fields only for master VAP */
  mtlk_osal_spinlock_t  on_beacon_lock;   /* Protect on_beacon list only */
  mtlk_dlist_t          on_beacon_list;   /* Shared access Tasklet(RW)/Serializer(RW) */

  mtlk_vap_handle_t     vap_handle;

  MTLK_DECLARE_INIT_STATUS;
} wds_t;

typedef struct _wds_peer_t {
  mtlk_dlist_entry_t    list_entry;
  mtlk_osal_mutex_t     lock;

  IEEE_ADDR         addr;
  BOOL              kill_on_disconnect;
  BOOL              fw_peer_sta_added;
  sta_entry         *sta;                 /* Is used for broadcasting over WDS*/
  mtlk_osal_timer_t fsm_timer;
  wds_peer_state_t  state;
  wds_t             *wds;

  MTLK_DECLARE_INIT_STATUS;
} wds_peer_t;

typedef struct _wds_on_beacon_t {
  mtlk_dlist_entry_t    list_entry;
  IEEE_ADDR             addr;
  uint8                 vap_id;
} wds_on_beacon_t;


int __MTLK_IFUNC
wds_init(wds_t *wds, mtlk_vap_handle_t vap_handle);

void __MTLK_IFUNC
wds_cleanup(wds_t *wds);

void __MTLK_IFUNC
wds_on_if_down(wds_t *wds);

void __MTLK_IFUNC
wds_on_if_up(wds_t *wds);

int __MTLK_IFUNC
wds_usr_del_peer_ap(wds_t *wds, const IEEE_ADDR *addr);

int __MTLK_IFUNC
wds_usr_add_peer_ap(wds_t *wds, const IEEE_ADDR *addr);

int __MTLK_IFUNC
wds_peer_disconnect(wds_t *wds, const IEEE_ADDR *addr, int *terminate);

void __MTLK_IFUNC
wds_peer_connect(wds_t *wds, sta_entry *sta,  const IEEE_ADDR *addr);

void __MTLK_IFUNC
wds_enable_abilities(wds_t *wds);

void __MTLK_IFUNC
wds_disable_abilities(wds_t *wds);

int __MTLK_IFUNC
wds_get_peers_status(wds_t *wds);

int __MTLK_IFUNC
mtlk_wds_get_peer_vect(wds_t *wds, mtlk_clpb_t **ppclpb);

int __MTLK_IFUNC
wds_on_beacon_proc(mtlk_handle_t master_wds_handle, const void *data,  uint32 data_size);

int __MTLK_IFUNC
wds_peer_fsm_timer(mtlk_handle_t wds_handle, const void *peer_addr,  uint32 ieee_addr_size);

wds_on_beacon_t * __MTLK_IFUNC
wds_on_beacon_exclude(wds_t *master_wds, const IEEE_ADDR *addr);

#endif /* __WDS_H__ */


/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
 * FW RECOVERY
 *
 *
 */
#include "mtlkinc.h"
#include "mtlkirbd.h"
#include "dataex.h"
#include "mtlk_vap_manager.h"
#include "mtlk_df.h"
#include "mtlk_coreui.h"
#include "core.h"
#include "wds.h"
#include "ta.h"
#include "hw_mmb.h"
#include "mtlkhal.h"

#include "mhi_umi.h"
#include "fw_recovery.h"
#include "nlmsgs.h"

#define LOG_LOCAL_GID   GID_RECOVERY
#define LOG_LOCAL_FID   0

static const mtlk_guid_t IRBE_HANG                    = MTLK_IRB_GUID_HANG;
static const mtlk_guid_t IRBE_RECOVERY_START_DUMP     = MTLK_IRB_GUID_RECOVERY_START_DUMP;
static const mtlk_guid_t IRBE_RECOVERY_RCVRY_FINISH   = MTLK_IRB_GUID_RECOVERY_RCVRY_FINISH;
static const mtlk_guid_t IRBE_RECOVERY_START_DUMP_CFM = MTLK_IRB_GUID_RECOVERY_START_DUMP_CFM;
static const mtlk_guid_t IRBE_RECOVERY_STOP_DUMP_CFM  = MTLK_IRB_GUID_RECOVERY_STOP_DUMP_CFM;

#define RCVRY_SLAVE_SYNC_TIMEOUT  30000
#define RCVRY_START_DUMP_TIMEOUT  2000   /* 2 sec */
#define RCVRY_STOP_DUMP_TIMEOUT   120000 /* 2 min */

/* statistics */
typedef enum {
  RCVRY_NOF_FAST_RCVRY_PROCESSED,   /* number of FAST recovery processed successfully */
  RCVRY_NOF_FULL_RCVRY_PROCESSED,   /* number of FULL recovery processed successfully */
  RCVRY_NOF_FAST_RCVRY_FAILED,      /* number of FAST recovery failed */
  RCVRY_NOF_FULL_RCVRY_FAILED,      /* number of FULL recovery failed */

  RCVRY_CNT_LAST
} rcvry_cnt_id_e;

static const uint32 _rcvry_wss_id_map[] =
{
  MTLK_WWSS_WLAN_STAT_ID_NOF_FAST_RCVRY_PROCESSED,  /* RCVRY_NOF_FAST_RCVRY_PROCESSED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_FULL_RCVRY_PROCESSED,  /* RCVRY_NOF_FULL_RCVRY_PROCESSED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_FAST_RCVRY_FAILED,     /* RCVRY_NOF_FAST_RCVRY_FAILED */
  MTLK_WWSS_WLAN_STAT_ID_NOF_FULL_RCVRY_FAILED      /* RCVRY_NOF_FULL_RCVRY_FAILED */
};

#define RCVRY_SIGN                0x5259BEAD /* RY */
typedef struct __vap_info_t
{
  uint32            vap_info_idx;     /* Index of vap info structure within recovery's array */
  uint32            vap_num;          /* Global VAP number for that vap_info */
  uint32            was_connected;
  mtlk_vap_handle_t vap_handle;
} _vap_info_t;

typedef struct __rcvry_t
{
  uint32                  signature;
  mtlk_vap_manager_t      *vap_manager;
  mtlk_osal_spinlock_t    lock;               /* Recovery might be initiated from serializer context
                                                 and from timer content, thus recovery structure must be
                                                 protected from simultaneous access */
  rcvry_cfg_t             cfg;                /* configuration */

  uint8                   fast_rcvry_cnt;     /* number of consecutive fast recovery */
  uint8                   full_rcvry_cnt;     /* number of consecutive full recovery */

  rcvry_type_e            forced_rcvry_type;  /* forced recovery type */
  rcvry_type_e            current_rcvry_type; /* current recovery type */

  mtlk_osal_spinlock_t    isolation_lock;     /* Prohibits timers execution like SMP protection */
  _vap_info_t             **vap_info;         /* Null terminated array of pointers. Created at rcvry initiation */

  mtlk_osal_mutex_t       slave_sync_mutex;   /* Mutex is used to wait until all slave SZRs are blocked */
  mtlk_osal_mutex_t       slave_rlse_mutex;   /* Mutex is release slave SZR's after recovery finished */

  mtlk_osal_timer_t       fail_timer;         /*  */

  mtlk_osal_event_t       start_dump_event;   /*  */
  mtlk_osal_event_t       stop_dump_event;    /*  */

  BOOL                    error_on_rcvry;     /*  */
  BOOL                    dump_in_progress;   /* firmware dump is in progress */
  BOOL                    is_scheduled;       /* recovery task is scheduled */

  /* statistics */
  mtlk_wss_t              *wss;
  mtlk_wss_cntr_handle_t  *wss_hcntrs[RCVRY_CNT_LAST];

  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_START_STATUS;
} _rcvry_t;

MTLK_INIT_STEPS_LIST_BEGIN(rcvry)
  MTLK_INIT_STEPS_LIST_ENTRY(rcvry, LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(rcvry, ISOLATION_LOCK)
  MTLK_INIT_STEPS_LIST_ENTRY(rcvry, SYNC_MUTEX)
  MTLK_INIT_STEPS_LIST_ENTRY(rcvry, RLSE_MUTEX)
  MTLK_INIT_STEPS_LIST_ENTRY(rcvry, FAIL_TIMER)
  MTLK_INIT_STEPS_LIST_ENTRY(rcvry, START_DUMP_EVENT)
  MTLK_INIT_STEPS_LIST_ENTRY(rcvry, STOP_DUMP_EVENT)
  MTLK_INIT_STEPS_LIST_ENTRY(rcvry, WSS_CREATE)
  MTLK_INIT_STEPS_LIST_ENTRY(rcvry, WSS_HCNTRs)
MTLK_INIT_INNER_STEPS_BEGIN(rcvry)
MTLK_INIT_STEPS_LIST_END(rcvry);

MTLK_START_STEPS_LIST_BEGIN(rcvry)
  MTLK_START_STEPS_LIST_ENTRY(rcvry, ISOLATION)
  MTLK_START_STEPS_LIST_ENTRY(rcvry, FW_DUMP)
  MTLK_START_STEPS_LIST_ENTRY(rcvry, RESTORATION)
  MTLK_START_STEPS_LIST_ENTRY(rcvry, CONFIGURATION)
  MTLK_START_STEPS_LIST_ENTRY(rcvry, FINALIZATION)
MTLK_START_INNER_STEPS_BEGIN(rcvry)
MTLK_START_STEPS_LIST_END(rcvry);

static void
_rcvry_inc_cnt (_rcvry_t *rcvry, rcvry_cnt_id_e cnt_id)
{
  MTLK_ASSERT(NULL != rcvry);
  MTLK_ASSERT(cnt_id >= 0 && cnt_id < RCVRY_CNT_LAST);

  mtlk_wss_cntr_inc(rcvry->wss_hcntrs[cnt_id]);
}

static void
_rcvry_start_dump_irb_handler (mtlk_irbd_t       *irbd,
                               mtlk_handle_t      context,
                               const mtlk_guid_t *evt,
                               void              *buffer,
                               uint32            *size)
{
  _rcvry_t *rcvry = (_rcvry_t *)context;
  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RECOVERY_START_DUMP_CFM) == 0);
  MTLK_ASSERT(rcvry != NULL);

  ILOG1_V("RECOVERY start dump confirmation received");
  mtlk_osal_event_set(&rcvry->start_dump_event);
}

static void
_rcvry_stop_dump_irb_handler (mtlk_irbd_t       *irbd,
                              mtlk_handle_t      context,
                              const mtlk_guid_t *evt,
                              void              *buffer,
                              uint32            *size)
{
  _rcvry_t *rcvry = (_rcvry_t *)context;
  MTLK_ASSERT(mtlk_guid_compare(evt, &IRBE_RECOVERY_STOP_DUMP_CFM) == 0);
  MTLK_ASSERT(rcvry != NULL);

  ILOG1_V("RECOVERY stop dump confirmation received");
  mtlk_osal_event_set(&rcvry->stop_dump_event);
}

static int
_rcvry_fw_dump (_rcvry_t *rcvry, mtlk_vap_handle_t vap_handle)
{
  int res = MTLK_ERR_OK;
  mtlk_irbd_handle_t *start_dump_handle = NULL;
  mtlk_irbd_handle_t *stop_dump_handle = NULL;

  ILOG0_D("CID-%04x: FW dump started", mtlk_vap_get_oid(vap_handle));

  mtlk_osal_lock_acquire(&rcvry->lock);
  rcvry->dump_in_progress = TRUE;
  mtlk_osal_lock_release(&rcvry->lock);

  mtlk_osal_event_reset(&rcvry->start_dump_event);
  mtlk_osal_event_reset(&rcvry->stop_dump_event);

  start_dump_handle = mtlk_irbd_register(mtlk_vap_get_irbd(vap_handle),
                                         &IRBE_RECOVERY_START_DUMP_CFM,
                                         1,
                                         _rcvry_start_dump_irb_handler,
                                         HANDLE_T(rcvry));
  if (NULL == start_dump_handle) {
    res = MTLK_ERR_NO_RESOURCES;
    goto FINISH;
  }

  stop_dump_handle = mtlk_irbd_register(mtlk_vap_get_irbd(vap_handle),
                                        &IRBE_RECOVERY_STOP_DUMP_CFM,
                                        1,
                                        _rcvry_stop_dump_irb_handler,
                                        HANDLE_T(rcvry));
  if (NULL == stop_dump_handle) {
    res = MTLK_ERR_NO_RESOURCES;
    goto FINISH;
  }

  res = mtlk_irbd_notify_app(mtlk_irbd_get_root(),
                             &IRBE_RECOVERY_START_DUMP,
                             (char *)mtlk_df_get_name(mtlk_vap_get_df(vap_handle)),
                             IFNAMSIZ);

  if (res == MTLK_ERR_OK) {
    ILOG1_V("Wait for RECOVERY start dump confirmation from application");
    res = mtlk_osal_event_wait(&rcvry->start_dump_event, RCVRY_START_DUMP_TIMEOUT);
    if (res == MTLK_ERR_OK) {
      ILOG1_V("Wait for RECOVERY stop dump confirmation from application");
      res = mtlk_osal_event_wait(&rcvry->stop_dump_event, RCVRY_STOP_DUMP_TIMEOUT);
      if (res != MTLK_ERR_OK) {
        ELOG_V("ERROR during wait RECOVERY stop dump confirmation");
      }
    }
    else {
      ELOG_V("ERROR during wait RECOVERY start dump confirmation");
    }
  }
  else {
    WLOG_V("Cannot notify application about RECOVERY process initiation");
  }

  res = MTLK_ERR_OK;

FINISH:
  if (NULL != start_dump_handle) {
    mtlk_irbd_unregister(mtlk_vap_get_irbd(vap_handle), start_dump_handle);
  }
  if (NULL != stop_dump_handle) {
    mtlk_irbd_unregister(mtlk_vap_get_irbd(vap_handle), stop_dump_handle);
  }

  mtlk_osal_lock_acquire(&rcvry->lock);
  rcvry->dump_in_progress = FALSE;
  mtlk_osal_lock_release(&rcvry->lock);

  return res;
}

int __MTLK_IFUNC
finish_recovery (mtlk_handle_t rcvry_handle, const void *payload, uint32 size)
{
  int res;
  _rcvry_t *rcvry = (_rcvry_t *)rcvry_handle;
  mtlk_vap_handle_t vap_handle;

  MTLK_ASSERT(rcvry_handle != MTLK_INVALID_HANDLE);
  MTLK_ASSERT(rcvry->signature == RCVRY_SIGN);

  res = mtlk_vap_manager_get_master_vap(rcvry->vap_manager, &vap_handle);
  MTLK_ASSERT(res == MTLK_ERR_OK);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  res = mtlk_irbd_notify_app(mtlk_irbd_get_root(),
                             &IRBE_RECOVERY_RCVRY_FINISH,
                             (char *)mtlk_df_get_name(mtlk_vap_get_df(vap_handle)),
                             IFNAMSIZ);
  if (res == MTLK_ERR_OK) {
    ILOG1_V("RECOVERY finish notification has been sent");
  }
  else {
    ELOG_V("ERROR during RECOVERY finish notification");
  }

  return res;
}

static int
_rcvry_finalize (_rcvry_t *rcvry, mtlk_vap_handle_t vap_handle)
{
  uint32 fail_time_interval;
  rcvry_type_e rcvry_type;

  ILOG0_D("CID-%04x: Finalization started", mtlk_vap_get_oid(vap_handle));

  mtlk_osal_lock_acquire(&rcvry->lock);

  fail_time_interval = rcvry->cfg.fail_time_interval;
  rcvry_type = rcvry->current_rcvry_type;

  if (fail_time_interval) {
    ILOG0_DD("CID-%04x: Recovery timer started (%d ms)", mtlk_vap_get_oid(vap_handle), fail_time_interval);
    mtlk_osal_timer_set(&rcvry->fail_timer, fail_time_interval);
  }

  mtlk_osal_lock_release(&rcvry->lock);

  core_schedule_recovery_task(vap_handle, (void *)finish_recovery, (mtlk_handle_t)rcvry, 0);

  /* calculate statistics */
  switch (rcvry_type) {
    case RCVRY_TYPE_FAST:
      _rcvry_inc_cnt(rcvry, RCVRY_NOF_FAST_RCVRY_PROCESSED);
      break;
    case RCVRY_TYPE_FULL:
      _rcvry_inc_cnt(rcvry, RCVRY_NOF_FULL_RCVRY_PROCESSED);
      break;
    default:
      break;
  }

  return MTLK_ERR_OK;
}

static int
_rcvry_configure (_rcvry_t *rcvry, rcvry_type_e rcvry_type, mtlk_vap_handle_t vap_handle)
{
  int res;
  BOOL preactivate = FALSE;
  _vap_info_t *vap_info;
  _vap_info_t **vap_info_ptr;

  ILOG0_D("CID-%04x: Configuration started", mtlk_vap_get_oid(vap_handle));

  /* Check if preactivation is required */
  vap_info_ptr = rcvry->vap_info;
  while (NULL != (vap_info = *vap_info_ptr++)) {
    if (vap_info->was_connected) {
      preactivate = TRUE;
      break;
    }
  }

  /* Configure VAPs */
  vap_info_ptr = rcvry->vap_info;
  while (NULL != (vap_info = *vap_info_ptr++)) {
    res = core_on_rcvry_configure(mtlk_vap_get_core(vap_info->vap_handle),
                                  vap_info->was_connected, preactivate, (uint32)rcvry_type);
    if (res != MTLK_ERR_OK) {
      return res;
    }
    preactivate = FALSE;
  }

  return MTLK_ERR_OK;
}

static int
_rcvry_restore (_rcvry_t *rcvry, mtlk_vap_handle_t vap_handle)
{
  int res;
  mtlk_hw_t *hw;
  _vap_info_t *vap_info;
  _vap_info_t **vap_info_ptr;

  ILOG0_D("CID-%04x: Restoration started", mtlk_vap_get_oid(vap_handle));

  hw = mtlk_vap_manager_get_hw(rcvry->vap_manager);

  res = mtlk_hw_mmb_restore(hw);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  mtlk_ta_on_rcvry_restore(mtlk_vap_get_ta((*rcvry->vap_info)->vap_handle));

  /* Restore of all VAPs and theirs sub modules */
  vap_info_ptr = rcvry->vap_info;
  while (NULL != (vap_info = *vap_info_ptr++)) {
    if (vap_info->vap_handle) {
      mtlk_df_on_rcvry_restore(mtlk_vap_get_df(vap_info->vap_handle));
      res = core_on_rcvry_restore(mtlk_vap_get_core(vap_info->vap_handle));
      if (res != MTLK_ERR_OK) {
        return res;
      }
    }
  }

  return MTLK_ERR_OK;
}

static int
_rcvry_isolate (_rcvry_t *rcvry, rcvry_type_e rcvry_type, mtlk_vap_handle_t vap_handle)
{
  mtlk_hw_t *hw;
  _vap_info_t *vap_info;
  _vap_info_t **vap_info_ptr;

  ILOG0_D("CID-%04x: Isolation started", mtlk_vap_get_oid(vap_handle));

  /* do irbd unregister outside of the lock */
  vap_info_ptr = rcvry->vap_info;
  while (NULL != (vap_info = *vap_info_ptr++)) {
    if (vap_info->vap_handle) {
      core_on_rcvry_isol_irbd_unregister(mtlk_vap_get_core(vap_info->vap_handle));
    }
  }

  mtlk_osal_lock_acquire(&rcvry->isolation_lock);

  hw = mtlk_vap_manager_get_hw(rcvry->vap_manager);

  mtlk_hw_mmb_isolate(hw);

  mtlk_ta_on_rcvry_isol(mtlk_vap_get_ta((*rcvry->vap_info)->vap_handle));

  /* VAP and it's submodules isolation */
  vap_info_ptr = rcvry->vap_info;
  while (NULL != (vap_info = *vap_info_ptr++)) {
    if (vap_info->vap_handle) {
      core_on_rcvry_isol(mtlk_vap_get_core(vap_info->vap_handle), (uint32)rcvry_type);
      mtlk_df_on_rcvry_isol(mtlk_vap_get_df(vap_info->vap_handle));
    }
  }

  mtlk_osal_lock_release(&rcvry->isolation_lock);

  return MTLK_ERR_OK;
}

static uint32
_rcvry_fail_timer_handler (mtlk_osal_timer_t *timer, mtlk_handle_t data)
{
  int res;
  mtlk_vap_handle_t vap_handle;
  _rcvry_t *rcvry = (_rcvry_t *)data;

  MTLK_ASSERT(rcvry != NULL);
  MTLK_ASSERT(rcvry->signature == RCVRY_SIGN);

  res = mtlk_vap_manager_get_master_vap(rcvry->vap_manager, &vap_handle);
  MTLK_ASSERT(res == MTLK_ERR_OK);
  if (MTLK_ERR_OK != res) {
    return 0;
  }

  ILOG0_D("CID-%04x: Recovery timer finished", mtlk_vap_get_oid(vap_handle));
  mtlk_osal_lock_acquire(&rcvry->lock);

  rcvry->fast_rcvry_cnt = 0;
  rcvry->full_rcvry_cnt = 0;

  mtlk_osal_lock_release(&rcvry->lock);

  return 0;
}

static void
_rcvry_cleanup (_rcvry_t *rcvry)
{
  MTLK_ASSERT(rcvry != NULL);

  MTLK_CLEANUP_BEGIN(rcvry, MTLK_OBJ_PTR(rcvry))

    MTLK_CLEANUP_STEP(rcvry, WSS_HCNTRs, MTLK_OBJ_PTR(rcvry),
                      mtlk_wss_cntrs_close,
                      (rcvry->wss, rcvry->wss_hcntrs, ARRAY_SIZE(rcvry->wss_hcntrs)));

    MTLK_CLEANUP_STEP(rcvry, WSS_CREATE, MTLK_OBJ_PTR(rcvry),
                      mtlk_wss_delete, (rcvry->wss));

    MTLK_CLEANUP_STEP(rcvry, STOP_DUMP_EVENT, MTLK_OBJ_PTR(rcvry),
                      mtlk_osal_event_cleanup, (&rcvry->stop_dump_event));

    MTLK_CLEANUP_STEP(rcvry, START_DUMP_EVENT, MTLK_OBJ_PTR(rcvry),
                      mtlk_osal_event_cleanup, (&rcvry->start_dump_event));

    MTLK_CLEANUP_STEP(rcvry, FAIL_TIMER, MTLK_OBJ_PTR(rcvry),
                      mtlk_osal_timer_cleanup, (&rcvry->fail_timer));

    MTLK_CLEANUP_STEP(rcvry, RLSE_MUTEX, MTLK_OBJ_PTR(rcvry),
                      mtlk_osal_mutex_cleanup, (&rcvry->slave_rlse_mutex));

    MTLK_CLEANUP_STEP(rcvry, SYNC_MUTEX, MTLK_OBJ_PTR(rcvry),
                      mtlk_osal_mutex_cleanup, (&rcvry->slave_sync_mutex));

    MTLK_CLEANUP_STEP(rcvry, ISOLATION_LOCK, MTLK_OBJ_PTR(rcvry),
                      mtlk_osal_lock_cleanup, (&rcvry->isolation_lock));

    MTLK_CLEANUP_STEP(rcvry, LOCK, MTLK_OBJ_PTR(rcvry),
                      mtlk_osal_lock_cleanup, (&rcvry->lock));

  MTLK_CLEANUP_END(rcvry, MTLK_OBJ_PTR(rcvry));
}

static int
_rcvry_init (_rcvry_t *rcvry, mtlk_vap_manager_t *vap_manager, mtlk_wss_t *parent_wss)
{
  MTLK_ASSERT(rcvry != NULL);

  /* Set default configuration */
  rcvry->cfg.fast_rcvry_num = 3;
  rcvry->cfg.full_rcvry_num = 1;
  rcvry->cfg.complete_rcvry = 0;
  rcvry->cfg.fail_time_interval = 60 * MTLK_OSAL_MSEC_IN_SEC;
  rcvry->cfg.fw_dump = 1;

  /* Init internals variables */
  rcvry->signature = RCVRY_SIGN;
  rcvry->vap_manager = vap_manager;

  rcvry->current_rcvry_type = RCVRY_TYPE_UNDEF;
  rcvry->forced_rcvry_type = RCVRY_TYPE_UNDEF;

  rcvry->error_on_rcvry = FALSE;
  rcvry->dump_in_progress = FALSE;

  MTLK_INIT_TRY(rcvry, MTLK_OBJ_PTR(rcvry))

    MTLK_INIT_STEP_VOID(rcvry, LOCK, MTLK_OBJ_PTR(rcvry),
                        mtlk_osal_lock_init, (&rcvry->lock));

    MTLK_INIT_STEP_VOID(rcvry, ISOLATION_LOCK, MTLK_OBJ_PTR(rcvry),
                        mtlk_osal_lock_init, (&rcvry->isolation_lock));

    MTLK_INIT_STEP(rcvry, SYNC_MUTEX, MTLK_OBJ_PTR(rcvry),
                   mtlk_osal_mutex_init, (&rcvry->slave_sync_mutex));

    MTLK_INIT_STEP(rcvry, RLSE_MUTEX, MTLK_OBJ_PTR(rcvry),
                   mtlk_osal_mutex_init, (&rcvry->slave_rlse_mutex));

    MTLK_INIT_STEP(rcvry, FAIL_TIMER, MTLK_OBJ_PTR(rcvry),
                   mtlk_osal_timer_init,
                   (&rcvry->fail_timer, _rcvry_fail_timer_handler, HANDLE_T(rcvry)));

    MTLK_INIT_STEP(rcvry, START_DUMP_EVENT, MTLK_OBJ_PTR(rcvry),
                   mtlk_osal_event_init, (&rcvry->start_dump_event));

    MTLK_INIT_STEP(rcvry, STOP_DUMP_EVENT, MTLK_OBJ_PTR(rcvry),
                   mtlk_osal_event_init, (&rcvry->stop_dump_event));

    MTLK_INIT_STEP_EX(rcvry, WSS_CREATE, MTLK_OBJ_PTR(rcvry),
                      mtlk_wss_create,
                      (parent_wss, _rcvry_wss_id_map, ARRAY_SIZE(_rcvry_wss_id_map)),
                      rcvry->wss, rcvry->wss != NULL, MTLK_ERR_NO_MEM);

    MTLK_INIT_STEP(rcvry, WSS_HCNTRs, MTLK_OBJ_PTR(rcvry),
                   mtlk_wss_cntrs_open,
                   (rcvry->wss, _rcvry_wss_id_map, rcvry->wss_hcntrs, RCVRY_CNT_LAST));

  MTLK_INIT_FINALLY(rcvry, MTLK_OBJ_PTR(rcvry))
  MTLK_INIT_RETURN(rcvry, MTLK_OBJ_PTR(rcvry), _rcvry_cleanup, (rcvry));
}

mtlk_handle_t __MTLK_IFUNC
mtlk_rcvry_create (mtlk_vap_manager_t *vap_manager, mtlk_wss_t *parent_wss)
{
  _rcvry_t *rcvry;

  rcvry = (_rcvry_t*)mtlk_osal_mem_alloc(sizeof(_rcvry_t), LQLA_MEM_TAG_FW_RECOVERY);
  if (NULL != rcvry) {
    memset(rcvry, 0, sizeof(_rcvry_t));
    if (MTLK_ERR_OK != _rcvry_init(rcvry, vap_manager, parent_wss)) {
      mtlk_osal_mem_free(rcvry);
      rcvry = NULL;
    }
  }

  return (mtlk_handle_t)rcvry;
}

void __MTLK_IFUNC
mtlk_rcvry_delete (mtlk_handle_t rcvry_handle)
{
  _rcvry_t *rcvry = (_rcvry_t*)rcvry_handle;

  MTLK_ASSERT(rcvry_handle != MTLK_INVALID_HANDLE);
  MTLK_ASSERT(rcvry->signature == RCVRY_SIGN);

  _rcvry_cleanup(rcvry);

  mtlk_osal_mem_free(rcvry);
}

/* count of maximal available number: parameter count x MAX_UINT32 */
#define _RCVRY_GET_CFG_MAX_NUM_COUNT 35

static void
_rcvry_change_config_notify (rcvry_cfg_t *rcvry_cfg, mtlk_vap_handle_t vap_handle)
{
  int res;
  char *buff_to_send = NULL;
  /* note: allocate memory for end-of-line + space between words */
  int buff_size = IFNAMSIZ + _RCVRY_GET_CFG_MAX_NUM_COUNT + 1 + 5;

  buff_to_send = mtlk_osal_mem_alloc(buff_size, LQLA_MEM_TAG_FW_RECOVERY);
  if (NULL == buff_to_send) {
    goto FINISH;
  }

  res = snprintf(buff_to_send, buff_size, "%s %d %d %d %d %d",
                 (char *)mtlk_df_get_name(mtlk_vap_get_df(vap_handle)),
                 rcvry_cfg->fast_rcvry_num,
                 rcvry_cfg->full_rcvry_num,
                 rcvry_cfg->complete_rcvry,
                 rcvry_cfg->fail_time_interval,
                 rcvry_cfg->fw_dump);

  if (res < 0) {
    ELOG_D("Failed to prepare netlink message, return value is %d", res);
    goto FINISH;
  }

  res = mtlk_nl_send_brd_msg(buff_to_send,
                             buff_size,
                             GFP_ATOMIC,
                             NETLINK_SIMPLE_CONFIG_GROUP,
                             NL_DRV_CMD_RCVRY_CONFIG);
  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: Unable to notify application: message = %d",
            mtlk_vap_get_oid(vap_handle),
            NL_DRV_CMD_RCVRY_CONFIG);
  }

FINISH:
  if (buff_to_send) {
    mtlk_osal_mem_free(buff_to_send);
  }
}

int __MTLK_IFUNC
mtlk_rcvry_set_cfg (mtlk_handle_t rcvry_handle, rcvry_cfg_t *rcvry_cfg)
{
  int res;
  _rcvry_t *rcvry = (_rcvry_t*)rcvry_handle;
  mtlk_vap_handle_t vap_handle;

  MTLK_ASSERT(rcvry_handle != MTLK_INVALID_HANDLE);
  MTLK_ASSERT(rcvry->signature == RCVRY_SIGN);

  res = mtlk_vap_manager_get_master_vap(rcvry->vap_manager, &vap_handle);
  MTLK_ASSERT(res == MTLK_ERR_OK);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  mtlk_osal_lock_acquire(&rcvry->lock);

  rcvry->cfg.fast_rcvry_num = rcvry_cfg->fast_rcvry_num;
  rcvry->cfg.full_rcvry_num = rcvry_cfg->full_rcvry_num;
  rcvry->cfg.complete_rcvry = rcvry_cfg->complete_rcvry;
  rcvry->cfg.fail_time_interval = rcvry_cfg->fail_time_interval * MTLK_OSAL_MSEC_IN_SEC;
  rcvry->cfg.fw_dump = rcvry_cfg->fw_dump;

  /* Reset number of consecutive recovery */
  rcvry->fast_rcvry_cnt = 0;
  rcvry->full_rcvry_cnt = 0;

  mtlk_osal_lock_release(&rcvry->lock);

  _rcvry_change_config_notify(rcvry_cfg, vap_handle);

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_rcvry_get_cfg (mtlk_handle_t rcvry_handle, rcvry_cfg_t *rcvry_cfg)
{
  _rcvry_t *rcvry = (_rcvry_t*)rcvry_handle;

  MTLK_ASSERT(rcvry_handle != MTLK_INVALID_HANDLE);
  MTLK_ASSERT(rcvry->signature == RCVRY_SIGN);

  mtlk_osal_lock_acquire(&rcvry->lock);

  rcvry_cfg->fast_rcvry_num = rcvry->cfg.fast_rcvry_num;
  rcvry_cfg->full_rcvry_num = rcvry->cfg.full_rcvry_num;
  rcvry_cfg->complete_rcvry = rcvry->cfg.complete_rcvry;
  rcvry_cfg->fail_time_interval = rcvry->cfg.fail_time_interval / MTLK_OSAL_MSEC_IN_SEC;
  rcvry_cfg->fw_dump = rcvry->cfg.fw_dump;

  mtlk_osal_lock_release(&rcvry->lock);

  return MTLK_ERR_OK;
}

static void
_rcvry_determine_type (_rcvry_t *rcvry, mtlk_vap_handle_t vap_handle)
{
  /* Recovery is supported for AP mode only,
     (DUT and STA modes are not supported) */
  if (!mtlk_vap_is_ap(vap_handle)) {
    rcvry->current_rcvry_type = (rcvry->cfg.complete_rcvry) ? RCVRY_TYPE_COMPLETE : RCVRY_TYPE_NONE;
    return;
  }

  if (rcvry->forced_rcvry_type != RCVRY_TYPE_UNDEF) {
    rcvry->current_rcvry_type = rcvry->forced_rcvry_type;
    /* Forced recovery used only once */
    rcvry->forced_rcvry_type = RCVRY_TYPE_UNDEF;
    return;
  }

  /* Choose recovery counters according to counters & configuration */
  if (rcvry->fast_rcvry_cnt < rcvry->cfg.fast_rcvry_num) {
    rcvry->fast_rcvry_cnt++;
    rcvry->current_rcvry_type = RCVRY_TYPE_FAST;
  }
  else if (rcvry->full_rcvry_cnt < rcvry->cfg.full_rcvry_num)
  {
    rcvry->full_rcvry_cnt++;
    rcvry->current_rcvry_type = RCVRY_TYPE_FULL;
  }
  else if (rcvry->cfg.complete_rcvry) {
    rcvry->current_rcvry_type = RCVRY_TYPE_COMPLETE;
  }
  else {
    /* Recovery not selected */
    rcvry->current_rcvry_type = RCVRY_TYPE_NONE;
  }
}

void __MTLK_IFUNC
rcvry_set_forced_type (mtlk_handle_t rcvry_handle, rcvry_type_e recovery_type)
{
  _rcvry_t *rcvry = (_rcvry_t *)rcvry_handle;

  MTLK_ASSERT(rcvry_handle != MTLK_INVALID_HANDLE);
  MTLK_ASSERT(rcvry->signature == RCVRY_SIGN);

  mtlk_osal_lock_acquire(&rcvry->lock);

  rcvry->forced_rcvry_type = recovery_type;

  mtlk_osal_lock_release(&rcvry->lock);
}

static void
_rcvry_vap_info_cleanup (_rcvry_t *rcvry)
{
  _vap_info_t *vap_info;
  _vap_info_t **vap_info_ptr;

  vap_info_ptr = rcvry->vap_info;
  while (NULL != (vap_info = *vap_info_ptr)) {
    memset(vap_info, 0, sizeof(_vap_info_t));
    mtlk_osal_mem_free(vap_info);
    *vap_info_ptr++ = NULL;
  }

  mtlk_osal_mem_free(rcvry->vap_info);
}

static void
_rcvry_vap_info_init (_vap_info_t *vap_info, mtlk_vap_handle_t vap_handle, int self_idx)
{
  mtlk_core_t *core;
  core = mtlk_vap_get_core(vap_handle);

  vap_info->vap_info_idx = self_idx;
  vap_info->vap_handle = vap_handle;
  vap_info->vap_num = mtlk_vap_get_id(vap_handle);
  vap_info->was_connected = mtlk_core_is_connected(core);

  ILOG2_DDDDD("CID-%04x: VAP info allocated 0x%08X. idx %d, num %d, vap_handle 0x%08X",
              mtlk_vap_get_oid(vap_info->vap_handle),
              (uint32)vap_info,
              vap_info->vap_info_idx,
              vap_info->vap_num,
              (uint32)vap_info->vap_handle);

  MTLK_ASSERT(!mtlk_core_is_halted(core));
}

static int
_rcvry_vap_info_create (_rcvry_t *rcvry)
{
  int vap_num, act_vap_num = 0, max_vap_num, vap_info_array_size;
  mtlk_vap_handle_t vap_handle;

  max_vap_num = mtlk_vap_manager_get_max_vaps_count(rcvry->vap_manager);

  /* Init recovery structure */
  /* Allocate NULL terminated array of pointers to VAP info */
  vap_info_array_size = sizeof(_vap_info_t*) * (max_vap_num+1);
  rcvry->vap_info = mtlk_osal_mem_alloc(vap_info_array_size, LQLA_MEM_TAG_FW_RECOVERY);
  if (rcvry->vap_info == NULL) {
    ELOG_V("Failed to allocate RECOVERY info array");
    goto FINISH_ERR;
  }
  memset(rcvry->vap_info, 0, vap_info_array_size);

  for (vap_num = 0; vap_num < max_vap_num; vap_num++) {
    _vap_info_t *vap_info;

    if (MTLK_ERR_OK != mtlk_vap_manager_get_vap_handle(rcvry->vap_manager, vap_num, &vap_handle)) {
      continue;
    }

    vap_info = mtlk_osal_mem_alloc(sizeof(_vap_info_t), LQLA_MEM_TAG_FW_RECOVERY);
    if (vap_info == NULL) {
      ELOG_V("Failed to allocate RECOVERY info");
      goto FINISH_ERR;
    }
    memset(vap_info, 0, sizeof(_vap_info_t));

    _rcvry_vap_info_init(vap_info, vap_handle, act_vap_num);
    rcvry->vap_info[act_vap_num] = vap_info;

    act_vap_num++;
  }

  return MTLK_ERR_OK;

FINISH_ERR:
  if (rcvry->vap_info) {
    for (vap_num = 0; vap_num < act_vap_num; vap_num++) {
      mtlk_osal_mem_free(rcvry->vap_info[vap_num]);
    }
    mtlk_osal_mem_free(rcvry->vap_info);
  }

  return MTLK_ERR_NO_MEM;
}

static void
_rcvry_on_error (_rcvry_t *rcvry, rcvry_type_e rcvry_type)
{
  int vap_index = 0;
  int max_vaps_count = 0;
  mtlk_vap_handle_t vap_handle;

  max_vaps_count = mtlk_vap_manager_get_max_vaps_count(rcvry->vap_manager);
  for (vap_index = max_vaps_count - 1; vap_index >= 0; vap_index--) {
    int res = mtlk_vap_manager_get_vap_handle(rcvry->vap_manager, vap_index, &vap_handle);
    if (MTLK_ERR_OK == res) {
      core_on_rcvry_error(mtlk_vap_get_core(vap_handle));
    }
  }
  /* Initiate recovery if current one is failed */
  rcvry_initiate(HANDLE_T(rcvry));

  /* calculate statistics */
  switch (rcvry_type) {
    case RCVRY_TYPE_FAST:
      _rcvry_inc_cnt(rcvry, RCVRY_NOF_FAST_RCVRY_FAILED);
      break;
    case RCVRY_TYPE_FULL:
      _rcvry_inc_cnt(rcvry, RCVRY_NOF_FULL_RCVRY_FAILED);
      break;
    default:
      break;
  }

  MTLK_STOP_BEGIN(rcvry, MTLK_OBJ_PTR(rcvry))
    MTLK_STOP_STEP(rcvry, FINALIZATION, MTLK_OBJ_PTR(rcvry), MTLK_NOACTION, ());
    MTLK_STOP_STEP(rcvry, CONFIGURATION, MTLK_OBJ_PTR(rcvry), MTLK_NOACTION, ());
    MTLK_STOP_STEP(rcvry, RESTORATION, MTLK_OBJ_PTR(rcvry), MTLK_NOACTION, ());
    MTLK_STOP_STEP(rcvry, FW_DUMP, MTLK_OBJ_PTR(rcvry), MTLK_NOACTION, ());
    MTLK_STOP_STEP(rcvry, ISOLATION, MTLK_OBJ_PTR(rcvry), MTLK_NOACTION, ());
  MTLK_STOP_END(rcvry, MTLK_OBJ_PTR(rcvry))
}

static const char _DRV_CMD_START[] = "fw_crash";

static void
_rcvry_start_notify (mtlk_vap_handle_t vap_handle)
{
  int res;
  char *buff_to_send = NULL;
  /* note: allocate memory for end-of-line + space between words */
  int buff_size = IFNAMSIZ + sizeof(_DRV_CMD_START) + 1 + 1;

  buff_to_send = mtlk_osal_mem_alloc(buff_size, LQLA_MEM_TAG_FW_RECOVERY);
  if (NULL == buff_to_send) {
    goto FINISH;
  }

  res = snprintf(buff_to_send, buff_size, "%s %s",
                 (char *)mtlk_df_get_name(mtlk_vap_get_df(vap_handle)),
                 _DRV_CMD_START);

  if (res < 0) {
    ELOG_D("Failed to prepare netlink message, return value is %d", res);
    goto FINISH;
  }

  res = mtlk_nl_send_brd_msg(buff_to_send,
                             buff_size,
                             GFP_ATOMIC,
                             NETLINK_SIMPLE_CONFIG_GROUP,
                             NL_DRV_CMD_RCVRY_STARTED);
  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: Unable to notify application: message = %d",
            mtlk_vap_get_oid(vap_handle),
            NL_DRV_CMD_RCVRY_STARTED);
  }

FINISH:
  if (buff_to_send) {
    mtlk_osal_mem_free(buff_to_send);
  }
}

int __MTLK_IFUNC
complete_recovery (mtlk_handle_t rcvry_handle, const void *payload, uint32 size)
{
  int res;
  _rcvry_t *rcvry = (_rcvry_t *)rcvry_handle;
  mtlk_hw_state_e hw_state;
  mtlk_vap_handle_t vap_handle;

  MTLK_ASSERT(rcvry_handle != MTLK_INVALID_HANDLE);
  MTLK_ASSERT(rcvry->signature == RCVRY_SIGN);

  res = mtlk_vap_manager_get_master_vap(rcvry->vap_manager, &vap_handle);
  MTLK_ASSERT(res == MTLK_ERR_OK);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  mtlk_vap_get_hw_vft(vap_handle)->get_prop(vap_handle, MTLK_HW_PROP_STATE, &hw_state, sizeof(hw_state));

  res = mtlk_irbd_notify_app(mtlk_irbd_get_root(),
                             &IRBE_HANG,
                             (char *)mtlk_df_get_name(mtlk_vap_get_df(vap_handle)),
                             IFNAMSIZ);

  return res;
}

int __MTLK_IFUNC
fw_dump_recovery (mtlk_handle_t rcvry_handle, const void *payload, uint32 size)
{
  int res;
  mtlk_vap_handle_t vap_handle;
  _rcvry_t *rcvry = (_rcvry_t *)rcvry_handle;

  MTLK_ASSERT(rcvry_handle != MTLK_INVALID_HANDLE);
  MTLK_ASSERT(rcvry->signature == RCVRY_SIGN);

  res = mtlk_vap_manager_get_master_vap(rcvry->vap_manager, &vap_handle);
  MTLK_ASSERT(res == MTLK_ERR_OK);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  res = _rcvry_fw_dump(rcvry, vap_handle);
  return res;
}

int __MTLK_IFUNC
master_recovery (mtlk_handle_t rcvry_handle, const void *payload, uint32 size)
{
  int res;
  _rcvry_t *rcvry = (_rcvry_t *)rcvry_handle;
  _vap_info_t *vap_info;
  _vap_info_t **vap_info_ptr;
  mtlk_vap_handle_t vap_handle;
  rcvry_type_e rcvry_type;

  MTLK_ASSERT(rcvry_handle != MTLK_INVALID_HANDLE);
  MTLK_ASSERT(rcvry->signature == RCVRY_SIGN);

  res = mtlk_vap_manager_get_master_vap(rcvry->vap_manager, &vap_handle);
  MTLK_ASSERT(res == MTLK_ERR_OK);
  if (MTLK_ERR_OK != res) {
    return res;
  }

  res = MTLK_ERR_UNKNOWN;

  ILOG0_D("CID-%04x: Master Recovery Started", mtlk_vap_get_oid(vap_handle));

  /* Wait until all slave serializers blocked by recovery task */
  vap_info_ptr = rcvry->vap_info;
  while (NULL != (vap_info = *vap_info_ptr++)) {
    mtlk_osal_mutex_acquire(&rcvry->slave_sync_mutex);
  }
  mtlk_osal_mutex_release(&rcvry->slave_sync_mutex);

  _rcvry_start_notify(vap_handle);

  mtlk_osal_lock_acquire(&rcvry->lock);

  rcvry->is_scheduled = FALSE;
  rcvry_type = rcvry->current_rcvry_type;

  mtlk_osal_lock_release(&rcvry->lock);

  MTLK_START_TRY(rcvry, MTLK_OBJ_PTR(rcvry))
    MTLK_START_STEP(rcvry, ISOLATION, MTLK_OBJ_PTR(rcvry),
                    _rcvry_isolate, (rcvry, rcvry_type, vap_handle));
    MTLK_START_STEP_IF(rcvry->cfg.fw_dump,
                       rcvry, FW_DUMP, MTLK_OBJ_PTR(rcvry),
                       _rcvry_fw_dump, (rcvry, vap_handle));
    MTLK_START_STEP(rcvry, RESTORATION, MTLK_OBJ_PTR(rcvry),
                    _rcvry_restore, (rcvry, vap_handle));
    MTLK_START_STEP(rcvry, CONFIGURATION, MTLK_OBJ_PTR(rcvry),
                    _rcvry_configure, (rcvry, rcvry_type, vap_handle));
    MTLK_START_STEP_EX(rcvry, FINALIZATION, MTLK_OBJ_PTR(rcvry),
                       _rcvry_finalize, (rcvry, vap_handle),
                       res, res == MTLK_ERR_OK, MTLK_ERR_UNKNOWN);
  MTLK_START_FINALLY(rcvry, MTLK_OBJ_PTR(rcvry));

    vap_info_ptr = rcvry->vap_info;
    while (NULL != (vap_info = *vap_info_ptr++)) {
      mtlk_osal_mutex_release(&rcvry->slave_rlse_mutex);
    }
    ILOG0_D("CID-%04x: Slaves released", mtlk_vap_get_oid(vap_handle));

    mtlk_osal_lock_acquire(&rcvry->lock);

    if (res == MTLK_ERR_OK) {
      ILOG0_D("CID-%04x: Recovery finished successfully", mtlk_vap_get_oid(vap_handle));
      _rcvry_vap_info_cleanup(rcvry);
    }
    else {
      ILOG0_D("CID-%04x: Recovery finished with error", mtlk_vap_get_oid(vap_handle));
      rcvry->error_on_rcvry = TRUE;
    }

    rcvry->current_rcvry_type = RCVRY_TYPE_UNDEF;

    mtlk_osal_lock_release(&rcvry->lock);

  MTLK_START_RETURN(rcvry, MTLK_OBJ_PTR(rcvry), _rcvry_on_error, (rcvry, rcvry_type));
}

void __MTLK_IFUNC
slave_recovery (mtlk_handle_t rcvry_handle, const void *payload, uint32 size)
{
  int vap_num, vap_oid;
  _rcvry_t *rcvry = (_rcvry_t*)rcvry_handle;
  _vap_info_t *vap_info;
  _vap_info_t **vap_info_ptr;

  MTLK_ASSERT(rcvry_handle != MTLK_INVALID_HANDLE);
  MTLK_ASSERT(rcvry->signature == RCVRY_SIGN);

  /* Get vap_info for this VAP. VAP number passed as an argument */
  vap_num = *(int*)payload;
  MTLK_ASSERT(size == sizeof(vap_num));

  vap_info_ptr = rcvry->vap_info;
  while (NULL != (vap_info = *vap_info_ptr++)) {
    if (vap_info->vap_num == vap_num) {
      break;
    }
  }

  MTLK_ASSERT(vap_info);
  vap_oid = mtlk_vap_get_oid(vap_info->vap_handle);
  ILOG1_DD("CID-%04x: Slave Recovery Task (0x%08X)", vap_oid, (uint32)vap_info);

  /* Some slave specific actions might be here */

  /* Att: Don't use vap_info below this point due to racing
          with master recovery task
  */

  /* Unblock Master Recovery Task*/
  mtlk_osal_mutex_release(&rcvry->slave_sync_mutex);

  /* Wait for release from Master recovery task */
  mtlk_osal_mutex_acquire(&rcvry->slave_rlse_mutex);
  ILOG1_D("CID-%04x: Slave task released", vap_oid);
}

static char *
__rcvry_type_to_string (rcvry_type_e rcvry_type)
{
  switch (rcvry_type) {
    case RCVRY_TYPE_NONE:
      return "NONE";
    case RCVRY_TYPE_FAST:
      return "FAST";
    case RCVRY_TYPE_FULL:
      return "FULL";
    case RCVRY_TYPE_COMPLETE:
      return "COMPLETE";
    case RCVRY_TYPE_UNDEF:
      return "UNDEF";
    default:
      ILOG0_D("Unknown recovery type 0x%04X", (uint32)rcvry_type);
  }
  return "Unknown recovery type";
}

void __MTLK_IFUNC
rcvry_initiate (mtlk_handle_t rcvry_handle)
{
  int res;
  _rcvry_t *rcvry = (_rcvry_t *)rcvry_handle;
  _vap_info_t *vap_info;
  _vap_info_t **vap_info_ptr;
  mtlk_vap_handle_t vap_handle;

  MTLK_ASSERT(rcvry_handle != MTLK_INVALID_HANDLE);
  MTLK_ASSERT(rcvry->signature == RCVRY_SIGN);

  res = mtlk_vap_manager_get_master_vap(rcvry->vap_manager, &vap_handle);
  MTLK_ASSERT(res == MTLK_ERR_OK);
  if (res != MTLK_ERR_OK) {
    return;
  }

  if (!mtlk_osal_timer_is_stopped(&rcvry->fail_timer)) {
    ILOG0_D("CID-%04x: Recovery timer canceled", mtlk_vap_get_oid(vap_handle));
    mtlk_osal_timer_cancel_sync(&rcvry->fail_timer);
  }

  /* Protect recovery content from simultaneous access */
  mtlk_osal_lock_acquire(&rcvry->lock);

  /* Do nothing if already in progress but isolation not finished */
  if (rcvry->current_rcvry_type != RCVRY_TYPE_UNDEF) {
    goto END;
  }

  _rcvry_determine_type(rcvry, vap_handle);
  ILOG0_DS("CID-%04x: %s recovery initiated",
           mtlk_vap_get_oid(vap_handle),
           __rcvry_type_to_string(rcvry->current_rcvry_type));
  ILOG0_DDDDD("CID-%04x: Fast: %d (%d), Full: %d (%d)",
              mtlk_vap_get_oid(vap_handle),
              rcvry->fast_rcvry_cnt, rcvry->cfg.fast_rcvry_num,
              rcvry->full_rcvry_cnt, rcvry->cfg.full_rcvry_num);

  /* Do not start recovery */
  if (rcvry->current_rcvry_type == RCVRY_TYPE_NONE ||
      rcvry->current_rcvry_type == RCVRY_TYPE_COMPLETE) {
    goto FW_DUMP;
  }

  /* Disable WLAN interrupts */
  mtlk_hw_mmb_dis_interrupts(mtlk_vap_manager_get_hw(rcvry->vap_manager));

  if (rcvry->error_on_rcvry == FALSE) {
    res = _rcvry_vap_info_create(rcvry);
    if (res != MTLK_ERR_OK) {
      goto FINISH;
    }
  }

  /* Prohibit slave serializer from release */
  mtlk_osal_mutex_acquire(&rcvry->slave_rlse_mutex);

  /* Schedule recovery task in core's serializer */
  vap_info_ptr = rcvry->vap_info;
  while (NULL != (vap_info = *vap_info_ptr++)) {
    void *task_to_schedule;
    task_to_schedule = mtlk_vap_is_master(vap_info->vap_handle) ? (void*)master_recovery : (void*)slave_recovery;
    core_schedule_recovery_task(vap_info->vap_handle, task_to_schedule, (mtlk_handle_t)rcvry, vap_info->vap_num);
  }
  rcvry->is_scheduled = TRUE;
  goto FINISH;

FW_DUMP:
  if (rcvry->cfg.fw_dump) {
    core_schedule_recovery_task(vap_handle, (void *)fw_dump_recovery, (mtlk_handle_t)rcvry, 0);
  }

  /* Notify application about hang or finish */
  if (rcvry->current_rcvry_type == RCVRY_TYPE_COMPLETE) {
    core_schedule_recovery_task(vap_handle, (void *)complete_recovery, (mtlk_handle_t)rcvry, 0);
  }   else {
    core_schedule_recovery_task(vap_handle, (void *)finish_recovery, (mtlk_handle_t)rcvry, 0);
  }

  if (rcvry->error_on_rcvry) {
    _rcvry_vap_info_cleanup(rcvry);
  }

FINISH:
  rcvry->error_on_rcvry = FALSE;

END:
  mtlk_osal_lock_release(&rcvry->lock);
}

BOOL __MTLK_IFUNC
rcvry_is_dump_in_progress (mtlk_handle_t rcvry_handle)
{
  _rcvry_t *rcvry = (_rcvry_t *)rcvry_handle;
  BOOL is_dump_in_progress = FALSE;

  MTLK_ASSERT(rcvry_handle != MTLK_INVALID_HANDLE);
  MTLK_ASSERT(rcvry->signature == RCVRY_SIGN);

  mtlk_osal_lock_acquire(&rcvry->lock);

  is_dump_in_progress = rcvry->dump_in_progress;

  mtlk_osal_lock_release(&rcvry->lock);

  return is_dump_in_progress;
}

BOOL __MTLK_IFUNC
rcvry_is_in_progress (mtlk_handle_t rcvry_handle)
{
  _rcvry_t *rcvry = (_rcvry_t *)rcvry_handle;
  BOOL is_recovery_in_progress = FALSE;

  MTLK_ASSERT(rcvry_handle != MTLK_INVALID_HANDLE);
  MTLK_ASSERT(rcvry->signature == RCVRY_SIGN);

  mtlk_osal_lock_acquire(&rcvry->lock);

  if (rcvry->current_rcvry_type != RCVRY_TYPE_UNDEF) {
    is_recovery_in_progress = TRUE;
  }

  mtlk_osal_lock_release(&rcvry->lock);

  return is_recovery_in_progress;
}

BOOL __MTLK_IFUNC
rcvry_is_scheduled (mtlk_handle_t rcvry_handle)
{
  _rcvry_t *rcvry = (_rcvry_t *)rcvry_handle;
  BOOL is_scheduled = FALSE;

  MTLK_ASSERT(rcvry_handle != MTLK_INVALID_HANDLE);
  MTLK_ASSERT(rcvry->signature == RCVRY_SIGN);

  mtlk_osal_lock_acquire(&rcvry->lock);

  is_scheduled = rcvry->is_scheduled;

  mtlk_osal_lock_release(&rcvry->lock);

  return is_scheduled;
}

BOOL __MTLK_IFUNC
rcvry_is_initialized (mtlk_handle_t rcvry_handle)
{
  _rcvry_t *rcvry = (_rcvry_t *)rcvry_handle;

  if (rcvry_handle == MTLK_INVALID_HANDLE) {
    return FALSE;
  }

  if (rcvry->signature != RCVRY_SIGN) {
    return FALSE;
  }

  return TRUE;
}

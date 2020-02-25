/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * Power management functionality implementation in compliance with
 * Code of Conduct on Energy Consumption of Broadband Equipment (a.k.a CoC)
 */

#include "mtlkinc.h"
#include "mhi_umi.h"
#include "txmm.h"
#include "core.h"
#include "mtlk_coreui.h"
#include "mtlk_coc.h"
#include "dfs.h"

#define LOG_LOCAL_GID   GID_COC
#define LOG_LOCAL_FID   1

/**********************************************************************
 * Local definitions
***********************************************************************/
#define COC_TXNUM   MTLK_BFIELD_INFO(2, 2)
#define COC_RXNUM   MTLK_BFIELD_INFO(0, 2)

#define COC_MAKE_POWERMODE(txnum, rxnum) \
  (MTLK_BFIELD_VALUE(COC_TXNUM, txnum, uint8) | \
   MTLK_BFIELD_VALUE(COC_RXNUM, rxnum, uint8))

#define MTLK_COC_MSEC_IN_INTERVAL     (100)

typedef enum {
  COC_POWER_0x0_STATE=0,
  COC_POWER_1x1_STATE,
  COC_POWER_2x2_STATE,
  COC_POWER_3x3_STATE
} _mtlk_coc_power_state_t;

struct _mtlk_coc_t
{
  mtlk_vap_handle_t vap_handle;
  mtlk_handle_t ta_crit_handle;
  mtlk_txmm_t *txmm;
  mtlk_coc_antenna_cfg_t hw_antenna_cfg;
  mtlk_coc_antenna_cfg_t current_antenna_cfg;
  mtlk_coc_antenna_cfg_t manual_antenna_cfg;
  mtlk_coc_auto_cfg_t auto_antenna_cfg;
  BOOL is_auto_mode;
  BOOL auto_mode_cfg;
  uint8 current_state;
  mtlk_osal_spinlock_t lock;

  MTLK_DECLARE_INIT_STATUS;
};

static const mtlk_ability_id_t _coc_abilities[] = {
  MTLK_CORE_REQ_GET_COC_CFG,
  MTLK_CORE_REQ_SET_COC_CFG
};

static int
_mtlk_coc_switch_mode (mtlk_coc_t *coc_obj, uint8 num_tx_antennas, uint8 num_rx_antennas)
{
  mtlk_txmm_msg_t         man_msg;
  mtlk_txmm_data_t*       man_entry = NULL;
  UMI_CHANGE_POWER_STATE  *mac_msg;
  int                     res = MTLK_ERR_OK;
  mtlk_core_t             *core = mtlk_vap_get_core(coc_obj->vap_handle);
  uint8                   cur_channel = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_CHANNEL_CUR);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, coc_obj->txmm, NULL);

  if (NULL == man_entry)
  {
    res = MTLK_ERR_NO_RESOURCES;
  }
  else
  {
    man_entry->id           = MC_MAN_CHANGE_POWER_STATE_REQ;
    man_entry->payload_size = sizeof(UMI_CHANGE_POWER_STATE);
    mac_msg = (UMI_CHANGE_POWER_STATE *)man_entry->payload;

    memset(mac_msg, 0, sizeof(*mac_msg));

    mac_msg->TxNum = num_tx_antennas;
    mac_msg->RxNum = num_rx_antennas;
    mac_msg->i16nCbTransmitPowerLimit = HOST_TO_MAC16(mtlk_calc_tx_power_lim_wrapper(HANDLE_T(core), 0, cur_channel));
    mac_msg->i16CbTransmitPowerLimit = HOST_TO_MAC16(mtlk_calc_tx_power_lim_wrapper(HANDLE_T(core), 1, cur_channel));

    res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);

    if ((MTLK_ERR_OK == res) && (UMI_OK != mac_msg->status))
    {
      res = MTLK_ERR_UNKNOWN;
    }

    mtlk_txmm_msg_cleanup(&man_msg);
  }

  if (MTLK_ERR_OK != res)
  {
    ELOG_DDD("UMI_CHANGE_POWER_STATE (TX%dxRX%d failed (res = %d)",
             num_tx_antennas, num_rx_antennas, res);
  }

  return res;
}

static _mtlk_coc_power_state_t
_mtlk_coc_get_current_state (mtlk_coc_t *coc_obj)
{
  MTLK_ASSERT(NULL != coc_obj);

  return coc_obj->current_state;
}

static uint32
_mtlk_coc_get_interval_by_state (mtlk_coc_t *coc_obj, const _mtlk_coc_power_state_t power_state)
{
  uint32 coc_interval = 0;

  switch (power_state) {
    case COC_POWER_1x1_STATE: coc_interval = coc_obj->auto_antenna_cfg.interval_1x1; break;
    case COC_POWER_2x2_STATE: coc_interval = coc_obj->auto_antenna_cfg.interval_2x2; break;
    case COC_POWER_3x3_STATE: coc_interval = coc_obj->auto_antenna_cfg.interval_3x3; break;
    default: MTLK_ASSERT(FALSE);
  }

  return coc_interval;
}

static void
_mtlk_coc_get_antenna_by_state (mtlk_coc_t *coc_obj, const _mtlk_coc_power_state_t power_state, mtlk_coc_antenna_cfg_t *antenna_cfg)
{
  switch (power_state) {
    case COC_POWER_1x1_STATE:
      antenna_cfg->num_rx_antennas = 1;
      antenna_cfg->num_tx_antennas = 1;
      break;
    case COC_POWER_2x2_STATE:
      antenna_cfg->num_rx_antennas = 2;
      antenna_cfg->num_tx_antennas = 2;
      if ((antenna_cfg->num_rx_antennas > coc_obj->hw_antenna_cfg.num_rx_antennas) ||
          (antenna_cfg->num_tx_antennas > coc_obj->hw_antenna_cfg.num_tx_antennas)) {
        antenna_cfg->num_rx_antennas = 1;
        antenna_cfg->num_tx_antennas = 1;
      }
      break;
    case COC_POWER_3x3_STATE:
      *antenna_cfg = coc_obj->hw_antenna_cfg;
      break;
    default: MTLK_ASSERT(FALSE);
  }
}

static int
_mtlk_coc_set_antennas (mtlk_coc_t *coc_obj, const mtlk_coc_antenna_cfg_t *antenna_cfg)
{
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(NULL != coc_obj);
  MTLK_ASSERT(0 != mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(coc_obj->vap_handle)));

  if ((antenna_cfg->num_rx_antennas == coc_obj->current_antenna_cfg.num_rx_antennas) &&
      (antenna_cfg->num_tx_antennas == coc_obj->current_antenna_cfg.num_tx_antennas)) {
    return MTLK_ERR_OK;
  }

  if (mtlk_core_scan_is_running(mtlk_vap_get_core(coc_obj->vap_handle))) {
    return MTLK_ERR_PROHIB;
  }

  if (!(mtlk_dot11h_can_switch_now(mtlk_core_get_dfs(mtlk_vap_get_core(coc_obj->vap_handle))))) {
    return MTLK_ERR_BUSY;
  }

  if ((antenna_cfg->num_tx_antennas > coc_obj->hw_antenna_cfg.num_tx_antennas) ||
      (antenna_cfg->num_rx_antennas > coc_obj->hw_antenna_cfg.num_rx_antennas)) {
      ELOG_DDD("CID-%04x: Wrong CoC power mode TX%dxRX%d",
               mtlk_vap_get_oid(coc_obj->vap_handle),
               antenna_cfg->num_tx_antennas, antenna_cfg->num_rx_antennas);
      return MTLK_ERR_PARAMS;
  }

  /* change number of TX antennas before switch power mode */
  res = mtlk_core_set_tx_antennas(mtlk_vap_get_core(coc_obj->vap_handle),
                                  antenna_cfg->num_tx_antennas);
  if (MTLK_ERR_OK != res) {
    return res;
  }

  /* lock should be set outside this function */
  mtlk_osal_lock_release(&coc_obj->lock);
  res = _mtlk_coc_switch_mode(coc_obj,
                              antenna_cfg->num_tx_antennas,
                              antenna_cfg->num_rx_antennas);
  /* lock should be unset outside this function */
  mtlk_osal_lock_acquire(&coc_obj->lock);

  if (MTLK_ERR_OK == res) {
    ILOG0_DDDDD("CID-%04x: CoC power mode changed from TX%dxRX%d to TX%dxRX%d",
                mtlk_vap_get_oid(coc_obj->vap_handle),
                coc_obj->current_antenna_cfg.num_tx_antennas,
                coc_obj->current_antenna_cfg.num_rx_antennas,
                antenna_cfg->num_tx_antennas,
                antenna_cfg->num_rx_antennas);

    /* update current antenna set */
    coc_obj->current_antenna_cfg = *antenna_cfg;
  }
  else {
    /* restore previous number of TX antennas */
    res = mtlk_core_set_tx_antennas(mtlk_vap_get_core(coc_obj->vap_handle),
                                    coc_obj->current_antenna_cfg.num_tx_antennas);
  }

  return res;
}

static int
_mtlk_coc_set_interval (mtlk_coc_t *coc_obj, const _mtlk_coc_power_state_t power_state)
{
  int res = MTLK_ERR_OK;
  ta_crit_coc_cfg_t ta_crit_cfg;

  MTLK_ASSERT(NULL != coc_obj);

  /* select interval according to state */
  ta_crit_cfg.interval = _mtlk_coc_get_interval_by_state(coc_obj, power_state);

  res = mtlk_ta_coc_cfg(mtlk_vap_get_ta(coc_obj->vap_handle), &ta_crit_cfg);

  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: Wrong CoC auto configuration: interval %d doesn't fit for TA",
            mtlk_vap_get_oid(coc_obj->vap_handle), ta_crit_cfg.interval);
  }

  return res;
}

static void
_mtlk_coc_set_state (mtlk_coc_t *coc_obj, const _mtlk_coc_power_state_t power_state)
{
  MTLK_ASSERT(NULL != coc_obj);

  coc_obj->current_state = power_state;
}

static int
_mtlk_coc_auto_mode_process (mtlk_coc_t *coc_obj, uint32 max_tp)
{
  int res = MTLK_ERR_OK;
  _mtlk_coc_power_state_t current_state, new_state;
  mtlk_coc_antenna_cfg_t antenna_cfg;

  MTLK_ASSERT(NULL != coc_obj);

  mtlk_osal_lock_acquire(&coc_obj->lock);

  current_state = _mtlk_coc_get_current_state(coc_obj);
  new_state = current_state;
  switch (current_state) {
    case COC_POWER_1x1_STATE:
      if (max_tp >= coc_obj->auto_antenna_cfg.low_limit_3x3) {
        new_state = COC_POWER_3x3_STATE;
      }
      else if (max_tp >= coc_obj->auto_antenna_cfg.low_limit_2x2) {
        new_state = COC_POWER_2x2_STATE;
      }
      break;
    case COC_POWER_2x2_STATE:
      if (max_tp < coc_obj->auto_antenna_cfg.high_limit_1x1) {
        new_state = COC_POWER_1x1_STATE;
      }
      else if (max_tp >= coc_obj->auto_antenna_cfg.low_limit_3x3) {
        new_state = COC_POWER_3x3_STATE;
      }
      break;
    case COC_POWER_3x3_STATE:
      if (max_tp < coc_obj->auto_antenna_cfg.high_limit_1x1) {
        new_state = COC_POWER_1x1_STATE;
      }
      else if (max_tp < coc_obj->auto_antenna_cfg.high_limit_2x2) {
        new_state = COC_POWER_2x2_STATE;
      }
      break;
    default: MTLK_ASSERT(FALSE);
  }

  if (current_state != new_state) {
    _mtlk_coc_get_antenna_by_state(coc_obj, new_state, &antenna_cfg);
    res = _mtlk_coc_set_antennas(coc_obj, &antenna_cfg);
    if (MTLK_ERR_OK != res) {
      goto FINISH;
    }

    res = _mtlk_coc_set_interval(coc_obj, new_state);
    if (MTLK_ERR_OK != res) {
      goto FINISH;
    }

    _mtlk_coc_set_state(coc_obj, new_state);
  }

FINISH:
  mtlk_osal_lock_release(&coc_obj->lock);
  return MTLK_ERR_OK;
}

static void
_mtlk_coc_auto_power_callback (mtlk_handle_t clb_ctx, mtlk_handle_t clb_crit)
{
  mtlk_coc_t *coc_obj;
  uint32 max_tp;

  coc_obj = HANDLE_T_PTR(mtlk_coc_t, clb_ctx);

  MTLK_ASSERT(NULL != coc_obj);

  max_tp = *((uint32 *)clb_crit);

  (void)_mtlk_coc_auto_mode_process(coc_obj, max_tp);
}

MTLK_INIT_STEPS_LIST_BEGIN(coc)
  MTLK_INIT_STEPS_LIST_ENTRY(coc, LOCK_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(coc, CONFIG_TA)
  MTLK_INIT_STEPS_LIST_ENTRY(coc, REG_CALLBACK)
  MTLK_INIT_STEPS_LIST_ENTRY(coc, REG_ABILITIES)
  MTLK_INIT_STEPS_LIST_ENTRY(coc, EN_ABILITIES)
MTLK_INIT_INNER_STEPS_BEGIN(coc)
MTLK_INIT_STEPS_LIST_END(coc);

static void
_mtlk_coc_cleanup (mtlk_coc_t *coc_obj)
{
  MTLK_ASSERT(NULL != coc_obj);

  MTLK_CLEANUP_BEGIN(coc, MTLK_OBJ_PTR(coc_obj))
    MTLK_CLEANUP_STEP(coc, EN_ABILITIES, MTLK_OBJ_PTR(coc_obj),
                      mtlk_abmgr_disable_ability_set,
                      (mtlk_vap_get_abmgr(coc_obj->vap_handle),
                      _coc_abilities, ARRAY_SIZE(_coc_abilities)));
    MTLK_CLEANUP_STEP(coc, REG_ABILITIES, MTLK_OBJ_PTR(coc_obj),
                      mtlk_abmgr_unregister_ability_set,
                      (mtlk_vap_get_abmgr(coc_obj->vap_handle),
                      _coc_abilities, ARRAY_SIZE(_coc_abilities)));
    MTLK_CLEANUP_STEP(coc, REG_CALLBACK, MTLK_OBJ_PTR(coc_obj),
                      mtlk_ta_crit_unregister, (coc_obj->ta_crit_handle));
    MTLK_CLEANUP_STEP(coc, CONFIG_TA, MTLK_OBJ_PTR(coc_obj),
                      MTLK_NOACTION, ());
    MTLK_CLEANUP_STEP(coc, LOCK_INIT, MTLK_OBJ_PTR(coc_obj),
                      mtlk_osal_lock_cleanup, (&coc_obj->lock));
  MTLK_CLEANUP_END(coc, MTLK_OBJ_PTR(coc_obj));
}

static int
_mtlk_coc_init (mtlk_coc_t *coc_obj, const mtlk_coc_cfg_t *cfg)
{
  ta_crit_coc_cfg_t ta_crit_cfg;
  uint32 res;

  MTLK_ASSERT(NULL != coc_obj);
  MTLK_ASSERT(NULL != cfg->txmm);

  /* Initial state is maximal power mode allowed by hardware */
  coc_obj->vap_handle = cfg->vap_handle;
  coc_obj->txmm = cfg->txmm;

  coc_obj->hw_antenna_cfg = cfg->hw_antenna_cfg;
  coc_obj->manual_antenna_cfg = cfg->hw_antenna_cfg;
  coc_obj->auto_antenna_cfg = cfg->default_auto_cfg;

  coc_obj->current_antenna_cfg = cfg->hw_antenna_cfg;
  coc_obj->current_state = COC_POWER_3x3_STATE;
  ta_crit_cfg.interval = cfg->default_auto_cfg.interval_3x3;

  MTLK_INIT_TRY(coc, MTLK_OBJ_PTR(coc_obj))
    MTLK_INIT_STEP(coc, LOCK_INIT, MTLK_OBJ_PTR(coc_obj),
                   mtlk_osal_lock_init, (&coc_obj->lock));
    MTLK_INIT_STEP_EX(coc, CONFIG_TA, MTLK_OBJ_PTR(coc_obj),
                      mtlk_ta_coc_cfg,
                      (mtlk_vap_get_ta(coc_obj->vap_handle), &ta_crit_cfg),
                      res,
                      res == MTLK_ERR_OK, MTLK_ERR_PARAMS);
    MTLK_INIT_STEP_EX(coc, REG_CALLBACK, MTLK_OBJ_PTR(coc_obj),
                      mtlk_ta_crit_register,
                      (mtlk_vap_get_ta(coc_obj->vap_handle),
                       TA_CRIT_ID_COC,
                       _mtlk_coc_auto_power_callback,
                       HANDLE_T(coc_obj)),
                      coc_obj->ta_crit_handle,
                      coc_obj->ta_crit_handle != MTLK_INVALID_HANDLE, MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP(coc, REG_ABILITIES, MTLK_OBJ_PTR(coc_obj),
                   mtlk_abmgr_register_ability_set,
                   (mtlk_vap_get_abmgr(coc_obj->vap_handle),
                    _coc_abilities, ARRAY_SIZE(_coc_abilities)));
    MTLK_INIT_STEP_VOID(coc, EN_ABILITIES, MTLK_OBJ_PTR(coc_obj),
                        mtlk_abmgr_enable_ability_set,
                        (mtlk_vap_get_abmgr(coc_obj->vap_handle),
                         _coc_abilities, ARRAY_SIZE(_coc_abilities)));
  MTLK_INIT_FINALLY(coc, MTLK_OBJ_PTR(coc_obj))
  MTLK_INIT_RETURN(coc, MTLK_OBJ_PTR(coc_obj), _mtlk_coc_cleanup, (coc_obj));
}

/*****************************************************************************
* Function implementation
******************************************************************************/
mtlk_coc_t* __MTLK_IFUNC
mtlk_coc_create (const mtlk_coc_cfg_t *cfg)
{
  mtlk_coc_t *coc_obj = mtlk_osal_mem_alloc(sizeof(mtlk_coc_t), MTLK_MEM_TAG_DFS);

  if (NULL != coc_obj)
  {
    memset(coc_obj, 0, sizeof(mtlk_coc_t));
    if (MTLK_ERR_OK != _mtlk_coc_init(coc_obj, cfg)) {
      mtlk_osal_mem_free(coc_obj);
      coc_obj = NULL;
    }
  }

  return coc_obj;
}

void __MTLK_IFUNC
mtlk_coc_delete (mtlk_coc_t *coc_obj)
{
  MTLK_ASSERT(NULL != coc_obj);

  _mtlk_coc_cleanup(coc_obj);

  mtlk_osal_mem_free(coc_obj);
}

BOOL __MTLK_IFUNC
mtlk_coc_is_auto_mode (mtlk_coc_t *coc_obj)
{
  MTLK_ASSERT(NULL != coc_obj);

  return coc_obj->is_auto_mode;
}

BOOL __MTLK_IFUNC
mtlk_coc_get_auto_mode_cfg (mtlk_coc_t *coc_obj)
{
  MTLK_ASSERT(NULL != coc_obj);

  return coc_obj->auto_mode_cfg;
}

int __MTLK_IFUNC
mtlk_coc_set_power_mode (mtlk_coc_t *coc_obj, const BOOL is_auto_mode)
{
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(NULL != coc_obj);

  coc_obj->auto_mode_cfg = is_auto_mode;

  if (0 == mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(coc_obj->vap_handle))) {
    return MTLK_ERR_OK;
  }

  mtlk_osal_lock_acquire(&coc_obj->lock);

  if (is_auto_mode) {
    if (!coc_obj->is_auto_mode) {
      /* set mode to high power mode */
      res = _mtlk_coc_set_antennas(coc_obj, &coc_obj->hw_antenna_cfg);
      if (MTLK_ERR_BUSY == res) {
        ILOG1_DDD("CID-%04x: Will select CoC power mode later: TX%dxRX%d",
                  mtlk_vap_get_oid(coc_obj->vap_handle),
                  coc_obj->hw_antenna_cfg.num_tx_antennas,
                  coc_obj->hw_antenna_cfg.num_rx_antennas);
        goto FINISH;
      }
      else if (MTLK_ERR_OK != res) {
        WLOG_DDD("CID-%04x: Can not select CoC power mode: TX%dxRX%d",
                 mtlk_vap_get_oid(coc_obj->vap_handle),
                 coc_obj->hw_antenna_cfg.num_tx_antennas,
                 coc_obj->hw_antenna_cfg.num_rx_antennas);
        goto FINISH;
      }

      res = _mtlk_coc_set_interval(coc_obj, COC_POWER_3x3_STATE);
      if (MTLK_ERR_OK != res)
        goto FINISH;

      _mtlk_coc_set_state(coc_obj, COC_POWER_3x3_STATE);

      mtlk_ta_crit_start(coc_obj->ta_crit_handle);
    }
  }
  else {
    /* restore previous configuration */
    res = _mtlk_coc_set_antennas(coc_obj, &coc_obj->manual_antenna_cfg);
    if (MTLK_ERR_BUSY == res) {
      ILOG1_DDD("CID-%04x: Will select CoC power mode later: TX%dxRX%d",
                mtlk_vap_get_oid(coc_obj->vap_handle),
                coc_obj->manual_antenna_cfg.num_tx_antennas,
                coc_obj->manual_antenna_cfg.num_rx_antennas);
      goto FINISH;
    }
    else if (MTLK_ERR_OK != res) {
      WLOG_DDD("CID-%04x: Can not select CoC power mode: TX%dxRX%d",
               mtlk_vap_get_oid(coc_obj->vap_handle),
               coc_obj->manual_antenna_cfg.num_tx_antennas,
               coc_obj->manual_antenna_cfg.num_rx_antennas);
      goto FINISH;
    }

    if (coc_obj->is_auto_mode) {
      mtlk_ta_crit_stop(coc_obj->ta_crit_handle);
    }
  }

  coc_obj->is_auto_mode = is_auto_mode;

FINISH:
  mtlk_osal_lock_release(&coc_obj->lock);
  return res;
}

mtlk_coc_antenna_cfg_t * __MTLK_IFUNC
mtlk_coc_get_current_params (mtlk_coc_t *coc_obj)
{
  mtlk_coc_antenna_cfg_t *current_antenna_cfg;
  MTLK_ASSERT(NULL != coc_obj);

  mtlk_osal_lock_acquire(&coc_obj->lock);
  current_antenna_cfg = &coc_obj->current_antenna_cfg;
  mtlk_osal_lock_release(&coc_obj->lock);

  return current_antenna_cfg;
}

mtlk_coc_auto_cfg_t * __MTLK_IFUNC
mtlk_coc_get_auto_params (mtlk_coc_t *coc_obj)
{
  MTLK_ASSERT(NULL != coc_obj);

  return &coc_obj->auto_antenna_cfg;
}

void __MTLK_IFUNC
mtlk_coc_reset_antenna_params (mtlk_coc_t *coc_obj)
{
  MTLK_ASSERT(NULL != coc_obj);

  mtlk_osal_lock_acquire(&coc_obj->lock);
  coc_obj->current_antenna_cfg = coc_obj->hw_antenna_cfg;
  mtlk_osal_lock_release(&coc_obj->lock);
}

int __MTLK_IFUNC
mtlk_coc_set_antenna_params (mtlk_coc_t *coc_obj, const mtlk_coc_antenna_cfg_t *antenna_params)
{
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(NULL != coc_obj);

  if ((antenna_params->num_rx_antennas > coc_obj->hw_antenna_cfg.num_rx_antennas) ||
      (antenna_params->num_tx_antennas > coc_obj->hw_antenna_cfg.num_tx_antennas) ||
      (antenna_params->num_rx_antennas != antenna_params->num_tx_antennas)) {
    ELOG_DDD("CID-%04x: Wrong CoC power mode TX%dxRX%d",
             mtlk_vap_get_oid(coc_obj->vap_handle),
             antenna_params->num_tx_antennas, antenna_params->num_rx_antennas);
    res = MTLK_ERR_PARAMS;
  }
  else {
    mtlk_osal_lock_acquire(&coc_obj->lock);
    memcpy(&coc_obj->manual_antenna_cfg, antenna_params, sizeof(mtlk_coc_antenna_cfg_t));
    mtlk_osal_lock_release(&coc_obj->lock);
  }

  return res;
}

int __MTLK_IFUNC
mtlk_coc_set_auto_params (mtlk_coc_t *coc_obj, const mtlk_coc_auto_cfg_t *auto_params)
{
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(NULL != coc_obj);

  if ((auto_params->interval_1x1 == 0) ||
      (auto_params->interval_2x2 == 0) ||
      (auto_params->interval_3x3 == 0)) {
    ELOG_D("CID-%04x: Wrong CoC auto configuration: interval = 0",
           mtlk_vap_get_oid(coc_obj->vap_handle));
    return MTLK_ERR_PARAMS;
  }
  else if ((auto_params->high_limit_1x1 == 0) ||
           (auto_params->low_limit_2x2 == 0) ||
           (auto_params->high_limit_2x2 == 0) ||
           (auto_params->low_limit_3x3 == 0)) {
    ELOG_D("CID-%04x: Wrong CoC auto configuration: threshold = 0",
           mtlk_vap_get_oid(coc_obj->vap_handle));
    return MTLK_ERR_PARAMS;
  }
  else if ((auto_params->high_limit_1x1 <= auto_params->low_limit_2x2) &&
           (auto_params->low_limit_2x2 <= auto_params->high_limit_2x2) &&
           (auto_params->high_limit_2x2 <= auto_params->low_limit_3x3)) {
    mtlk_osal_lock_acquire(&coc_obj->lock);
    memcpy(&coc_obj->auto_antenna_cfg, auto_params, sizeof(mtlk_coc_auto_cfg_t));
    if (coc_obj->is_auto_mode) {
      /* restart timer on-the-fly */
      res = _mtlk_coc_set_interval(coc_obj, _mtlk_coc_get_current_state(coc_obj));
      if (MTLK_ERR_OK != res) {
        mtlk_osal_lock_release(&coc_obj->lock);
        return MTLK_ERR_PARAMS;
      }
    }
    mtlk_osal_lock_release(&coc_obj->lock);
  }
  else {
    ELOG_DDDDD("CID-%04x: Wrong CoC auto configuration: thresholds %d<=%d<=%d<=%d",
               mtlk_vap_get_oid(coc_obj->vap_handle),
               auto_params->high_limit_1x1,
               auto_params->low_limit_2x2,
               auto_params->high_limit_2x2,
               auto_params->low_limit_3x3);
    return MTLK_ERR_PARAMS;
  }

  return MTLK_ERR_OK;
}

void __MTLK_IFUNC
mtlk_coc_auto_mode_disable (mtlk_coc_t *coc_obj)
{
  MTLK_ASSERT(NULL != coc_obj);

  mtlk_osal_lock_acquire(&coc_obj->lock);

  if (coc_obj->is_auto_mode) {
    mtlk_ta_crit_stop(coc_obj->ta_crit_handle);
    coc_obj->is_auto_mode = FALSE;
  }

  mtlk_osal_lock_release(&coc_obj->lock);
}

int __MTLK_IFUNC
mtlk_coc_on_rcvry_configure (mtlk_coc_t *coc_obj)
{
  int res;

  mtlk_coc_reset_antenna_params(coc_obj);
  res = mtlk_coc_set_power_mode(coc_obj, mtlk_coc_get_auto_mode_cfg(coc_obj));

  return res;
}

void __MTLK_IFUNC
mtlk_coc_on_rcvry_isol (mtlk_coc_t *coc_obj)
{
  mtlk_coc_auto_mode_disable(coc_obj);
}

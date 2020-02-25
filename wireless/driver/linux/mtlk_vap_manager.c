/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "mtlk_vap_manager.h"
#include "mtlk_df.h"
#include "mtlkhal.h"
#include "mtlk_param_db.h"
#include "mtlk_ab_manager.h"
#include "mtlk_fast_mem.h"
#include "mtlkwlanirbdefs.h"
#include "mtlk_snprintf.h"
#include "txmm.h"
#include "hw_mmb.h"
#include "ta.h"
#include "fw_recovery.h"

#define LOG_LOCAL_GID   GID_VAPM
#define LOG_LOCAL_FID   0

static int _mtlk_vap_init (mtlk_vap_handle_t vap_handle, mtlk_vap_manager_t *obj, mtlk_card_type_t hw_type, uint8 vap_index);

#define MAX_BSS_COUNT        5
#define MTLK_VAP_INVALID_IDX ((uint8)-1)

typedef struct _mtlk_vap_manager_vap_node_t
{
  struct _mtlk_vap_handle_t vap_info;
  mtlk_txmm_t               txmm;
  mtlk_txmm_t               txdm;
} mtlk_vap_manager_vap_node_t;

struct _mtlk_vap_manager_t
{
  mtlk_vap_manager_vap_node_t   *master_vap;
  mtlk_vap_manager_vap_node_t  **guest_vap_array;

  mtlk_osal_spinlock_t           guest_lock;

  mtlk_hw_api_t                 *hw_api;
  mtlk_irbd_t                   *hw_irbd;
  mtlk_wss_t                    *hw_wss;

  mtlk_txmm_base_t              *txmm;
  mtlk_txmm_base_t              *txdm;

  mtlk_atomic_t                  activated_vaps_num;
  mtlk_work_mode_e               work_mode;

  uint32                         max_vaps_count;
  mtlk_handle_t                  ta;
  mtlk_handle_t                  rcvry;

  /* Always use mtlk_vap_get_core_array() to access this array */
  mtlk_core_t                  **core_array;

  MTLK_DECLARE_INIT_STATUS;
};


MTLK_INIT_STEPS_LIST_BEGIN(vap_manager)
  MTLK_INIT_STEPS_LIST_ENTRY(vap_manager, VAP_MANAGER_PREPARE_SYNCHRONIZATION)
  MTLK_INIT_STEPS_LIST_ENTRY(vap_manager, PROCESS_TA)
MTLK_INIT_INNER_STEPS_BEGIN(vap_manager)
MTLK_INIT_STEPS_LIST_END(vap_manager);

MTLK_INIT_STEPS_LIST_BEGIN(vap_handler)
  MTLK_INIT_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_PDB_CREATE)
  MTLK_INIT_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_DF_CREATE)
  MTLK_INIT_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_ABMGR_CREATE)
  MTLK_INIT_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_CORE_API_ALLOC)
  MTLK_INIT_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_CORE_API_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_IRBD_ALLOC)
MTLK_INIT_INNER_STEPS_BEGIN(vap_handler)
MTLK_INIT_STEPS_LIST_END(vap_handler);

MTLK_START_STEPS_LIST_BEGIN(vap_handler)
  MTLK_START_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_IRBD_INIT)
  MTLK_START_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_CORE_START)
  MTLK_START_STEPS_LIST_ENTRY(vap_handler, VAP_HANDLER_DF_START)
MTLK_START_INNER_STEPS_BEGIN(vap_handler)
MTLK_START_STEPS_LIST_END(vap_handler);

static __INLINE mtlk_vap_handle_t
__mtlk_vap_manager_vap_handle_by_id (mtlk_vap_manager_t* obj, uint8 vap_id)
{
  MTLK_ASSERT(obj != NULL);
  MTLK_ASSERT(vap_id < obj->max_vaps_count);

  if (obj->guest_vap_array) {
    if (obj->guest_vap_array[vap_id])
      return &(obj->guest_vap_array[vap_id]->vap_info);
  }

  if (MTLK_MASTER_VAP_ID == vap_id) {
    if (obj->master_vap)
      return &obj->master_vap->vap_info;
  }

  return NULL;
}

static __INLINE void
__mtlk_vap_manager_vap_set_txmm (mtlk_vap_manager_t *obj, mtlk_vap_handle_t vap_handle, uint8 vap_index)
{
  obj->guest_vap_array[vap_index]->txmm.base = obj->txmm;
  obj->guest_vap_array[vap_index]->txmm.vap_handle = vap_handle;
}

static __INLINE mtlk_txmm_t*
__mtlk_vap_manager_vap_get_txmm (mtlk_vap_manager_t *obj, uint8 vap_index)
{
  return &(obj->guest_vap_array[vap_index]->txmm);
}

static __INLINE void
__mtlk_vap_manager_vap_set_txdm (mtlk_vap_manager_t *obj, mtlk_vap_handle_t vap_handle, uint8 vap_index)
{
  obj->guest_vap_array[vap_index]->txdm.base = obj->txdm;
  obj->guest_vap_array[vap_index]->txdm.vap_handle = vap_handle;
}

static __INLINE mtlk_txmm_t*
__mtlk_vap_manager_vap_get_txdm (mtlk_vap_manager_t *obj, uint8 vap_index)
{
  return &((obj->guest_vap_array[vap_index])->txdm);
}

static __INLINE mtlk_irbd_t*
__mtlk_vap_manager_get_hw_irbd (mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(obj);
  MTLK_ASSERT(obj->hw_irbd);

  return obj->hw_irbd;
}

static void __MTLK_IFUNC 
_mtlk_vap_manager_cleanup (mtlk_vap_manager_t* obj)
{
    MTLK_ASSERT(NULL != obj);

    MTLK_CLEANUP_BEGIN(vap_manager, MTLK_OBJ_PTR(obj))
      MTLK_CLEANUP_STEP(vap_manager, PROCESS_TA, MTLK_OBJ_PTR(obj),
                        mtlk_ta_delete, (obj->ta));
      
      MTLK_CLEANUP_STEP(vap_manager, VAP_MANAGER_PREPARE_SYNCHRONIZATION, MTLK_OBJ_PTR(obj),
                        mtlk_osal_lock_cleanup, (&obj->guest_lock));
    MTLK_CLEANUP_END(vap_manager, MTLK_OBJ_PTR(obj));
}

static int __MTLK_IFUNC
_mtlk_vap_manager_init (mtlk_vap_manager_t* obj, mtlk_hw_api_t *hw_api, mtlk_work_mode_e work_mode)
{
    MTLK_ASSERT(NULL != obj);

    MTLK_INIT_TRY(vap_manager, MTLK_OBJ_PTR(obj))
      obj->hw_api = hw_api;
      obj->work_mode = work_mode;
      obj->max_vaps_count = 1;
      mtlk_osal_atomic_set(&obj->activated_vaps_num, 0);

      MTLK_INIT_STEP(vap_manager, VAP_MANAGER_PREPARE_SYNCHRONIZATION, MTLK_OBJ_PTR(obj),
                     mtlk_osal_lock_init, (&obj->guest_lock));

      MTLK_INIT_STEP_EX(vap_manager, PROCESS_TA, MTLK_OBJ_PTR(obj),
                        mtlk_ta_create, (obj),
                        obj->ta,
                        obj->ta != MTLK_INVALID_HANDLE, MTLK_ERR_NO_MEM);

    MTLK_INIT_FINALLY(vap_manager, MTLK_OBJ_PTR(obj))
    MTLK_INIT_RETURN(vap_manager, MTLK_OBJ_PTR(obj), _mtlk_vap_manager_cleanup, (obj))
}

mtlk_vap_manager_t * __MTLK_IFUNC 
mtlk_vap_manager_create (mtlk_hw_api_t *hw_api, mtlk_work_mode_e work_mode)
{
    mtlk_vap_manager_t *vap_manager;

    MTLK_ASSERT(hw_api != NULL);

    vap_manager = (mtlk_vap_manager_t *)mtlk_osal_mem_alloc(sizeof(mtlk_vap_manager_t), MTLK_MEM_TAG_VAP_MANAGER);
    if(NULL == vap_manager) {
      return NULL;
    }

    memset(vap_manager, 0, sizeof(mtlk_vap_manager_t));

    if (MTLK_ERR_OK != _mtlk_vap_manager_init(vap_manager, hw_api, work_mode)) {
      mtlk_osal_mem_free(vap_manager);
      return NULL;
    }

    return vap_manager;
}

void __MTLK_IFUNC
mtlk_vap_manager_delete (mtlk_vap_manager_t *obj)
{
  _mtlk_vap_manager_cleanup(obj);
  mtlk_osal_mem_free(obj);
}

int __MTLK_IFUNC
mtlk_vap_manager_create_vap (mtlk_vap_manager_t *obj, 
                             mtlk_vap_handle_t  *_vap_handle,
                             mtlk_card_type_t hw_type,
                             uint8 vap_index)
{
  mtlk_vap_handle_t            vap_handle = MTLK_INVALID_VAP_HANDLE;
  mtlk_vap_manager_vap_node_t *vap_node;
  int                          res_value;

  MTLK_ASSERT(NULL != obj);
  MTLK_ASSERT(NULL != _vap_handle);

  if (vap_index >= obj->max_vaps_count) {
    ELOG_D("Invalid VAP ID %d", vap_index);
    return MTLK_VAP_INVALID_IDX;
  }

  vap_handle = __mtlk_vap_manager_vap_handle_by_id(obj, vap_index);
  if (NULL != vap_handle) {
    ELOG_D("VAP %d already exists", vap_index);
    return MTLK_VAP_INVALID_IDX;
  }

  vap_node = (mtlk_vap_manager_vap_node_t *)mtlk_osal_mem_alloc(sizeof(mtlk_vap_manager_vap_node_t), MTLK_MEM_TAG_VAP_NODE);
  if (NULL == vap_node) {
    ELOG_D("VAP %d can not to be created", vap_index);
    return MTLK_ERR_NO_MEM;
  }
  memset(vap_node, 0, sizeof(mtlk_vap_manager_vap_node_t));

  vap_handle = &vap_node->vap_info;

  res_value = _mtlk_vap_init(vap_handle, obj, hw_type, vap_index);
  if (MTLK_ERR_OK != res_value) {
    mtlk_osal_mem_free(vap_node);
    return res_value;
  }

  if (MTLK_MASTER_VAP_ID == vap_index) {
    obj->master_vap = vap_node;
  }
  else {
    MTLK_ASSERT(NULL != obj->guest_vap_array);
    obj->guest_vap_array[vap_index] = vap_node;
  }

  *_vap_handle = vap_handle;
  return MTLK_ERR_OK;
}

void __MTLK_IFUNC 
mtlk_vap_manager_delete_master_vap (mtlk_vap_manager_t *obj)
{
  mtlk_vap_handle_t vap_handle;
  MTLK_ASSERT(NULL != obj);

  vap_handle = __mtlk_vap_manager_vap_handle_by_id(obj, MTLK_MASTER_VAP_ID);
  if (NULL != vap_handle) {
    mtlk_vap_delete(vap_handle);
  }
}

int __MTLK_IFUNC
mtlk_vap_manager_prepare_vaps (mtlk_vap_manager_t *obj)
{
  mtlk_vap_handle_t             vap_handle;
  mtlk_vap_manager_vap_node_t **guest_vap_array;
  mtlk_core_t                 **core_array;
  uint32                        max_vaps_count;
  int                           res;

  MTLK_ASSERT(NULL != obj);
  MTLK_ASSERT(NULL != obj->master_vap);

  vap_handle = __mtlk_vap_manager_vap_handle_by_id(obj, MTLK_MASTER_VAP_ID);
  res = mtlk_vap_get_hw_vft(vap_handle)->get_prop(vap_handle, MTLK_HW_FW_CAPS_MAX_VAPs, &max_vaps_count, sizeof(max_vaps_count));
  if (MTLK_ERR_OK != res)
      return res;

  guest_vap_array = (mtlk_vap_manager_vap_node_t **)mtlk_osal_mem_alloc((sizeof(mtlk_vap_manager_vap_node_t *) * max_vaps_count), MTLK_MEM_TAG_VAP_NODE);
  if (NULL == guest_vap_array) {
    return MTLK_ERR_NO_MEM;
  }
  memset(guest_vap_array, 0, (sizeof(mtlk_vap_manager_vap_node_t *) * max_vaps_count));

  core_array = (mtlk_core_t **)mtlk_osal_mem_alloc((sizeof(mtlk_core_t *) * max_vaps_count), MTLK_MEM_TAG_VAP_NODE);
  if (NULL == core_array) {
    mtlk_osal_mem_free(guest_vap_array);
    return MTLK_ERR_NO_MEM;
  }
  memset(core_array, 0, (sizeof(mtlk_core_t *) * max_vaps_count));

  /* set master VAP to corresponding index in the array */
  guest_vap_array[MTLK_MASTER_VAP_ID] = obj->master_vap;

  obj->max_vaps_count = max_vaps_count;
  obj->guest_vap_array = guest_vap_array;
  obj->core_array = core_array;
  return MTLK_ERR_OK;
}

void __MTLK_IFUNC
mtlk_vap_manager_deallocate_vaps (mtlk_vap_manager_t *obj)
{
  int vap_index = 0;
  int max_vaps_count = 0;
  MTLK_ASSERT(NULL != obj);

  max_vaps_count = mtlk_vap_manager_get_max_vaps_count(obj);
  /* deallocate memory for all vaps except master vap */
  for (vap_index = max_vaps_count - 1; vap_index > 0; vap_index--) {
    mtlk_vap_handle_t vap_handle = __mtlk_vap_manager_vap_handle_by_id(obj, vap_index);
    if (NULL != vap_handle) {
      mtlk_vap_delete(vap_handle);
    }
  }

  if (obj->guest_vap_array) {
    mtlk_osal_mem_free(obj->guest_vap_array);
    obj->guest_vap_array = NULL;
  }

  if (obj->core_array) {
    mtlk_osal_mem_free(obj->core_array);
    obj->core_array = NULL;
  }
}

void __MTLK_IFUNC
mtlk_vap_manager_unprepare (mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(NULL != obj);

  mtlk_rcvry_delete(obj->rcvry);
}

int __MTLK_IFUNC
mtlk_vap_manager_prepare_start (mtlk_vap_manager_t *obj, mtlk_handle_t txmm_handle, mtlk_handle_t txdm_handle)
{
  mtlk_vap_handle_t master_vap_handle = NULL;

  MTLK_ASSERT(NULL != obj);

  obj->hw_irbd = NULL;
  obj->hw_wss  = NULL;
  if (mtlk_vap_manager_get_master_vap(obj, &master_vap_handle) == MTLK_ERR_OK) {
    if (mtlk_vap_get_hw_vft(master_vap_handle)->get_prop(master_vap_handle,
                                                         MTLK_HW_IRBD,
                                                         &obj->hw_irbd,
                                                         sizeof(&obj->hw_irbd)) != MTLK_ERR_OK) {
      obj->hw_irbd = NULL;
    }
    if (mtlk_vap_get_hw_vft(master_vap_handle)->get_prop(master_vap_handle,
                                                         MTLK_HW_WSS,
                                                         &obj->hw_wss,
                                                         sizeof(&obj->hw_wss)) != MTLK_ERR_OK) {
      obj->hw_wss = NULL;
    }
  }

  MTLK_ASSERT(NULL != obj->hw_irbd);
  MTLK_ASSERT(NULL != obj->hw_wss);

  obj->txmm = HANDLE_T_PTR(mtlk_txmm_base_t, txmm_handle);
  obj->txdm = HANDLE_T_PTR(mtlk_txmm_base_t, txdm_handle);

  MTLK_ASSERT(NULL != obj->txmm);
  MTLK_ASSERT(NULL != obj->txdm);

  obj->rcvry = mtlk_rcvry_create(obj, obj->hw_wss);
  if (MTLK_INVALID_HANDLE == obj->rcvry) {
    return MTLK_ERR_NO_MEM;
  }
  return MTLK_ERR_OK;
}

void __MTLK_IFUNC 
mtlk_vap_manager_prepare_stop(mtlk_vap_manager_t *obj) 
{
  int vap_index = 0;
  int max_vaps_count = 0;
  MTLK_ASSERT(NULL != obj);

  max_vaps_count = mtlk_vap_manager_get_max_vaps_count(obj);
  for (vap_index = max_vaps_count - 1; vap_index >= 0; vap_index--) {
    mtlk_vap_handle_t vap_handle = __mtlk_vap_manager_vap_handle_by_id(obj, vap_index);
    if (NULL != vap_handle) {
      mtlk_vap_get_core_vft(vap_handle)->prepare_stop(vap_handle);
    }
  }

  obj->hw_irbd = NULL;
  obj->hw_wss  = NULL;
}

void __MTLK_IFUNC 
mtlk_vap_manager_stop_all_vaps(mtlk_vap_manager_t *obj, mtlk_vap_manager_interface_e intf)
{
  int vap_index = 0;
  int max_vaps_count = 0;
  MTLK_ASSERT(NULL != obj);

  max_vaps_count = mtlk_vap_manager_get_max_vaps_count(obj);
  for (vap_index = max_vaps_count - 1; vap_index >= 0; vap_index--) {
    mtlk_vap_handle_t vap_handle = __mtlk_vap_manager_vap_handle_by_id(obj, vap_index);
    if (NULL != vap_handle) {
      mtlk_vap_stop(vap_handle, intf);
    }
  }
}

int __MTLK_IFUNC
mtlk_vap_manager_get_master_vap (mtlk_vap_manager_t *obj,
                                 mtlk_vap_handle_t  *vap_handle)
{
  mtlk_vap_handle_t _vap_handle;

  MTLK_ASSERT(obj != NULL);
  MTLK_ASSERT(vap_handle != NULL);

  _vap_handle = __mtlk_vap_manager_vap_handle_by_id(obj, MTLK_MASTER_VAP_ID);
  *vap_handle = _vap_handle;

  if (NULL == _vap_handle) {
    return MTLK_ERR_NOT_IN_USE;
  }

  return MTLK_ERR_OK;
}

int __MTLK_IFUNC
mtlk_vap_manager_get_vap_handle (mtlk_vap_manager_t *obj,
                                 uint8               vap_id,
                                 mtlk_vap_handle_t  *vap_handle)
{
  mtlk_vap_handle_t _vap_handle;

  MTLK_ASSERT(obj != NULL);
  MTLK_ASSERT(vap_handle != NULL);

  if (vap_id >= obj->max_vaps_count) {
    return MTLK_VAP_INVALID_IDX;
  }

  _vap_handle = __mtlk_vap_manager_vap_handle_by_id(obj, vap_id);
  if (NULL == _vap_handle) {
    return MTLK_ERR_NOT_IN_USE;
  }

  *vap_handle = _vap_handle;

  return MTLK_ERR_OK;
}

void __MTLK_IFUNC
mtlk_vap_manager_notify_vap_activated(mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(NULL != obj);

  mtlk_osal_atomic_inc(&obj->activated_vaps_num);
}

void __MTLK_IFUNC
mtlk_vap_manager_notify_vap_deactivated(mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(NULL != obj);

  mtlk_osal_atomic_dec(&obj->activated_vaps_num);
}

uint32 __MTLK_IFUNC
mtlk_vap_manager_get_active_vaps_number(mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(NULL != obj);

  return mtlk_osal_atomic_get(&obj->activated_vaps_num);
}

uint32 __MTLK_IFUNC
mtlk_vap_manager_get_max_vaps_count (mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(obj != NULL);

  return obj->max_vaps_count;
}

mtlk_handle_t __MTLK_IFUNC
mtlk_vap_manager_get_rcvry (mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(obj != NULL);

  return obj->rcvry;
}

/* Always call this function right before using core array.
 * It becomes invalid each time when another VAP is created or deleted.
 */
mtlk_core_t ** __MTLK_IFUNC
mtlk_vap_get_core_array (mtlk_vap_manager_t *obj, uint32 *core_cnt)
{
  int vap_index = 0;
  int max_vaps_count = 0;
  mtlk_vap_handle_t vap_handle = NULL;

  MTLK_ASSERT(NULL != obj);
  MTLK_ASSERT(NULL != obj->core_array);
  MTLK_ASSERT(NULL != core_cnt);

  *core_cnt = 0;
  max_vaps_count = mtlk_vap_manager_get_max_vaps_count(obj);

  /* Prevent accumulation of stale pointers between calls */
  memset(obj->core_array, 0, (sizeof(mtlk_core_t *) * max_vaps_count));

  for (vap_index = 0; vap_index < max_vaps_count; vap_index++) {
    vap_handle = __mtlk_vap_manager_vap_handle_by_id(obj, vap_index);
    if (NULL != vap_handle) {
      obj->core_array[*core_cnt] = mtlk_vap_get_core(vap_handle);
      (*core_cnt)++;
    }
  }

  return obj->core_array;
}

mtlk_core_t * __MTLK_IFUNC
mtlk_vap_get_core (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);
  MTLK_ASSERT(NULL != _info->core_api);

  return HANDLE_T_PTR(mtlk_core_t, _info->core_api->core_handle);
}

mtlk_core_vft_t const * __MTLK_IFUNC
mtlk_vap_get_core_vft (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);
  MTLK_ASSERT(NULL != _info->core_api);
  MTLK_ASSERT(NULL != _info->core_api->vft);

  return _info->core_api->vft;
}

mtlk_work_mode_e __MTLK_IFUNC
mtlk_vap_manager_get_mode(mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(NULL != obj);

  return obj->work_mode;
}

mtlk_hw_t* __MTLK_IFUNC
mtlk_vap_get_hw(mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);
  MTLK_ASSERT(NULL != _info->manager->hw_api);
  MTLK_ASSERT(NULL != _info->manager->hw_api->hw);

  return _info->manager->hw_api->hw;
}

mtlk_hw_t* __MTLK_IFUNC
mtlk_vap_manager_get_hw (mtlk_vap_manager_t *obj)
{
  MTLK_ASSERT(NULL != obj);
  MTLK_ASSERT(NULL != obj->hw_api);
  MTLK_ASSERT(NULL != obj->hw_api->hw);

  return obj->hw_api->hw;
}

mtlk_hw_vft_t const * __MTLK_IFUNC
mtlk_vap_get_hw_vft (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);
  MTLK_ASSERT(NULL != _info->manager->hw_api);
  MTLK_ASSERT(NULL != _info->manager->hw_api->vft);

  return _info->manager->hw_api->vft;
}

mtlk_wss_t * __MTLK_IFUNC
mtlk_vap_get_hw_wss (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(NULL != _info);
  MTLK_ASSERT(NULL != _info->manager);
  MTLK_ASSERT(NULL != _info->manager->hw_wss);

  return _info->manager->hw_wss;
}

static void __MTLK_IFUNC
_mtlk_vap_cleanup (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *vap_info;

  MTLK_ASSERT(NULL != vap_handle);
  vap_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_CLEANUP_BEGIN(vap_handler, MTLK_OBJ_PTR(vap_info))
    MTLK_CLEANUP_STEP(vap_handler, VAP_HANDLER_IRBD_ALLOC, MTLK_OBJ_PTR(vap_info),
                      mtlk_irbd_free, (vap_info->irbd));
    MTLK_CLEANUP_STEP(vap_handler, VAP_HANDLER_CORE_API_INIT, MTLK_OBJ_PTR(vap_info),
                      mtlk_core_api_cleanup, (vap_info->core_api));
    MTLK_CLEANUP_STEP(vap_handler, VAP_HANDLER_CORE_API_ALLOC, MTLK_OBJ_PTR(vap_info),
                      mtlk_fast_mem_free, (vap_info->core_api));
    MTLK_CLEANUP_STEP(vap_handler, VAP_HANDLER_ABMGR_CREATE, MTLK_OBJ_PTR(vap_info),
                      mtlk_abmgr_delete, (vap_info->abmgr));
    MTLK_CLEANUP_STEP(vap_handler, VAP_HANDLER_DF_CREATE, MTLK_OBJ_PTR(vap_info),
                      mtlk_df_delete, (vap_info->df));
    MTLK_CLEANUP_STEP(vap_handler, VAP_HANDLER_PDB_CREATE, MTLK_OBJ_PTR(vap_info),
                      mtlk_pdb_delete, (vap_info->param_db));
  MTLK_CLEANUP_END(vap_handler, MTLK_OBJ_PTR(vap_info))
}

extern const char *mtlk_drv_info[];
static void
_mtlk_print_drv_info (void) {
#if (RTLOG_MAX_DLEVEL >= 1)
  int i = 0;
  ILOG1_V("*********************************************************");
  ILOG1_V("* Driver Compilation Details:");
  ILOG1_V("*********************************************************");
  while (mtlk_drv_info[i]) {
    ILOG1_S("* %s", mtlk_drv_info[i]);
    i++;
  }
  ILOG1_V("*********************************************************");
#endif
}

static int
_mtlk_vap_init (mtlk_vap_handle_t vap_handle, mtlk_vap_manager_t *obj, mtlk_card_type_t hw_type, uint8 vap_index)
{
  mtlk_vap_info_internal_t *vap_info;
  BOOL is_dut = (MTLK_MODE_DUT == mtlk_vap_manager_get_mode(obj));

  MTLK_ASSERT(vap_handle != NULL);
  vap_info = (mtlk_vap_info_internal_t *)vap_handle;

  _mtlk_print_drv_info();
  ILOG1_S("Working in %s mode", is_dut ? "DUT" : "normal");

  MTLK_INIT_TRY(vap_handler, MTLK_OBJ_PTR(vap_info))
    vap_info->manager = obj;
    vap_info->id = vap_index;
    vap_info->hw_idx = mtlk_hw_mmb_get_card_idx(obj->hw_api->hw);

    MTLK_INIT_STEP_EX(vap_handler, VAP_HANDLER_PDB_CREATE, MTLK_OBJ_PTR(vap_info),
                      mtlk_pdb_create, (hw_type),
                      vap_info->param_db,
                      vap_info->param_db != NULL,
                      MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP_EX(vap_handler, VAP_HANDLER_DF_CREATE, MTLK_OBJ_PTR(vap_info),
                      mtlk_df_create, (vap_handle),
                      vap_info->df,
                      vap_info->df != NULL,
                      MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP_EX(vap_handler, VAP_HANDLER_ABMGR_CREATE, MTLK_OBJ_PTR(vap_info),
                      mtlk_abmgr_create, (),
                      vap_info->abmgr,
                      vap_info->abmgr != NULL,
                      MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP_EX(vap_handler, VAP_HANDLER_CORE_API_ALLOC, MTLK_OBJ_PTR(vap_info),
                      mtlk_fast_mem_alloc, (MTLK_FM_USER_VAPMGR, sizeof(mtlk_core_api_t)),
                      vap_info->core_api,
                      vap_info->core_api != NULL,
                      MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP(vap_handler, VAP_HANDLER_CORE_API_INIT, MTLK_OBJ_PTR(vap_info),
                   mtlk_core_api_init, (vap_handle, vap_info->core_api, vap_info->df));

    /* validate core VFT
      NOTE: all functions should be initialized by core, no any NULL values
        accepted. In case of unsupported functionality the function with
        empty body required!
        Validation must be in sync with mtlk_core_vft_t declaration. */
    MTLK_ASSERT(NULL != vap_info->core_api->vft->start);
    MTLK_ASSERT(NULL != vap_info->core_api->vft->release_tx_data);
    MTLK_ASSERT(NULL != vap_info->core_api->vft->handle_rx_data);
    MTLK_ASSERT(NULL != vap_info->core_api->vft->handle_rx_ctrl);
    MTLK_ASSERT(NULL != vap_info->core_api->vft->get_prop);
    MTLK_ASSERT(NULL != vap_info->core_api->vft->set_prop);
    MTLK_ASSERT(NULL != vap_info->core_api->vft->stop);
    MTLK_ASSERT(NULL != vap_info->core_api->vft->prepare_stop);

    MTLK_INIT_STEP_EX(vap_handler, VAP_HANDLER_IRBD_ALLOC, MTLK_OBJ_PTR(vap_info),
                      mtlk_irbd_alloc, (),
                      vap_info->irbd,
                      vap_info->irbd != NULL,
                      MTLK_ERR_NO_MEM);
  MTLK_INIT_FINALLY(vap_handler, MTLK_OBJ_PTR(vap_info))
  MTLK_INIT_RETURN(vap_handler, MTLK_OBJ_PTR(vap_info), _mtlk_vap_cleanup, (vap_handle))
}

void __MTLK_IFUNC
mtlk_vap_delete (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_manager_t *obj = NULL;
  uint8              vap_index;

  MTLK_ASSERT(NULL != vap_handle);

  obj = mtlk_vap_get_manager(vap_handle);
  MTLK_ASSERT(NULL != obj);

  vap_index = mtlk_vap_get_id(vap_handle);
  MTLK_ASSERT(vap_index < obj->max_vaps_count);

  _mtlk_vap_cleanup(vap_handle);

  if (obj->guest_vap_array) {
    mtlk_osal_mem_free((mtlk_vap_manager_vap_node_t *)obj->guest_vap_array[vap_index]);
    obj->guest_vap_array[vap_index] = NULL;
  }

  if (MTLK_MASTER_VAP_ID == vap_index) {
    mtlk_osal_mem_free((mtlk_vap_manager_vap_node_t *)obj->master_vap);
    obj->master_vap = NULL;
  }  
}

int __MTLK_IFUNC
mtlk_vap_start (mtlk_vap_handle_t vap_handle, mtlk_vap_manager_interface_e intf)
{
  mtlk_vap_info_internal_t *vap_info;
  mtlk_vap_manager_t       *obj       = NULL;
  uint8                     vap_index = MTLK_VAP_INVALID_IDX;

  MTLK_ASSERT(NULL != vap_handle);
  vap_info = (mtlk_vap_info_internal_t *)vap_handle;

  obj = mtlk_vap_get_manager(vap_handle);
  MTLK_ASSERT(NULL != obj);

  vap_index = mtlk_vap_get_id(vap_handle);
  MTLK_ASSERT(vap_index < obj->max_vaps_count);

  __mtlk_vap_manager_vap_set_txmm(obj, vap_handle, vap_index);
  __mtlk_vap_manager_vap_set_txdm(obj, vap_handle, vap_index);

  vap_info->txmm = __mtlk_vap_manager_vap_get_txmm(obj, vap_index);
  vap_info->txdm = __mtlk_vap_manager_vap_get_txdm(obj, vap_index);

  MTLK_START_TRY(vap_handler, MTLK_OBJ_PTR(vap_info))
    MTLK_START_STEP(vap_handler, VAP_HANDLER_IRBD_INIT, MTLK_OBJ_PTR(vap_info),
                    mtlk_irbd_init, (vap_info->irbd,
                                     __mtlk_vap_manager_get_hw_irbd(vap_info->manager),
                                     mtlk_df_get_name(vap_info->df)));
    MTLK_START_STEP(vap_handler, VAP_HANDLER_CORE_START, MTLK_OBJ_PTR(vap_info),
                    mtlk_vap_get_core_vft(vap_handle)->start, (vap_handle));
    MTLK_START_STEP(vap_handler, VAP_HANDLER_DF_START, MTLK_OBJ_PTR(vap_info),
                    mtlk_df_start, (mtlk_vap_get_df(vap_handle), intf));

  MTLK_START_FINALLY(vap_handler, MTLK_OBJ_PTR(vap_info))
  MTLK_START_RETURN(vap_handler, MTLK_OBJ_PTR(vap_info), mtlk_vap_stop, (vap_handle, intf))
}

void __MTLK_IFUNC
mtlk_vap_stop (mtlk_vap_handle_t vap_handle, mtlk_vap_manager_interface_e intf)
{
  mtlk_vap_info_internal_t *vap_info;

  MTLK_ASSERT(NULL != vap_handle);
  vap_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_STOP_BEGIN(vap_handler, MTLK_OBJ_PTR(vap_info))
    MTLK_STOP_STEP(vap_handler, VAP_HANDLER_DF_START, MTLK_OBJ_PTR(vap_info),
                   mtlk_df_stop, (mtlk_vap_get_df(vap_handle), intf));
    MTLK_STOP_STEP(vap_handler, VAP_HANDLER_CORE_START, MTLK_OBJ_PTR(vap_info),
                   mtlk_vap_get_core_vft (vap_handle)->stop, (vap_handle));
    MTLK_STOP_STEP(vap_handler, VAP_HANDLER_IRBD_INIT, MTLK_OBJ_PTR(vap_info),
                   mtlk_irbd_cleanup, (vap_info->irbd));
  MTLK_STOP_END(vap_handler, MTLK_OBJ_PTR(vap_info))
}

BOOL __MTLK_IFUNC
mtlk_vap_is_master (mtlk_vap_handle_t vap_handle)
{
  mtlk_vap_info_internal_t *_info = (mtlk_vap_info_internal_t *)vap_handle;

  MTLK_ASSERT(_info != NULL);

  /* NOTE: don't use the mtlk_vap_get_id API here since this function
   *       can be called in context of DF/Core creation, i.e.
   *       prior to manager data member assignment.
   */
  return (_info->id == MTLK_MASTER_VAP_ID);
}

BOOL __MTLK_IFUNC
mtlk_vap_is_ap (mtlk_vap_handle_t vap_handle)
{
  mtlk_work_mode_e work_mode = mtlk_vap_manager_get_mode(mtlk_vap_get_manager(vap_handle));

  return (MTLK_MODE_AP == work_mode);
}

BOOL __MTLK_IFUNC
mtlk_vap_is_dut (mtlk_vap_handle_t vap_handle)
{
  BOOL is_dut = FALSE;
  mtlk_vap_handle_t master_vap_handle = NULL;
  mtlk_vap_manager_t *obj = mtlk_vap_get_manager(vap_handle);
  mtlk_work_mode_e work_mode = mtlk_vap_manager_get_mode(obj);

  if (MTLK_MODE_DUT == work_mode) {
    return TRUE;
  }

  if (mtlk_vap_manager_get_master_vap(obj, &master_vap_handle) == MTLK_ERR_OK) {
    if (mtlk_vap_get_core_vft(master_vap_handle)->get_prop(master_vap_handle,
                              MTLK_CORE_PROP_IS_DUT, &is_dut, sizeof(is_dut)) != MTLK_ERR_OK) {
      WLOG_V("Cannot get DUT mode status");
    }
  }

  return is_dut;
}

BOOL __MTLK_IFUNC
mtlk_vap_is_master_ap (mtlk_vap_handle_t vap_handle)
{
  return (BOOL)(mtlk_vap_is_ap(vap_handle) && mtlk_vap_is_master(vap_handle));
}

BOOL __MTLK_IFUNC
mtlk_vap_is_slave_ap (mtlk_vap_handle_t vap_handle)
{
  return (BOOL)(mtlk_vap_is_ap(vap_handle) && !mtlk_vap_is_master(vap_handle));
}

mtlk_handle_t __MTLK_IFUNC
mtlk_vap_get_ta (mtlk_vap_handle_t vap_handle)
{
  MTLK_ASSERT(vap_handle != NULL);
  return mtlk_vap_get_manager(vap_handle)->ta;
}

mtlk_handle_t __MTLK_IFUNC
mtlk_vap_get_rcvry (mtlk_vap_handle_t vap_handle)
{
  MTLK_ASSERT(vap_handle != NULL);
  return mtlk_vap_get_manager(vap_handle)->rcvry;
}

/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "mhi_dut.h"
#include "dataex.h"
#include "txmm.h"
#include "mtlk_snprintf.h"
#include "mtlkhal.h"
#include "mtlk_df.h"
#include "mtlk_df_fw.h"
#include "core.h"
#include "mtlk_core_iface.h"
#include "mtlk_coreui.h"
#include "core_common.h"
#include "eeprom_irbd.h"
#include "fw_recovery.h"

#define LOG_LOCAL_GID   GID_DUT_CORE
#define LOG_LOCAL_FID   4

#define MTLK_DUT_FW_MSG_SEND_TIMEOUT  20000 /* ms */

struct _mtlk_dut_handlers_t
{
  mtlk_irbd_handle_t *fw_msg_handle;
  mtlk_irbd_handle_t *progmodel_msg_handle;
  mtlk_eeprom_irbd_t *eeprom_irbd;
};

static const mtlk_guid_t _MTLK_IRBE_DUT_FW_CMD        = MTLK_IRB_GUID_DUT_FW_CMD;
static const mtlk_guid_t _MTLK_IRBE_DUT_PROGMODEL_CMD = MTLK_IRB_GUID_DUT_PROGMODEL_CMD;
static const mtlk_guid_t _MTLK_IRBE_DUT_START_CMD     = MTLK_IRB_GUID_DUT_START_CMD;
static const mtlk_guid_t _MTLK_IRBE_DUT_STOP_CMD      = MTLK_IRB_GUID_DUT_STOP_CMD;

static void __MTLK_IFUNC
_mtlk_dut_core_handle_progmodel_msg (mtlk_irbd_t       *irbd,
                                     mtlk_handle_t      context,
                                     const mtlk_guid_t *evt,
                                     void              *buffer,
                                     uint32            *size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_firmware_file_t pm_file;
  mtlk_core_t *core = (mtlk_core_t *)context;
  dut_progmodel_t *param = (dut_progmodel_t *)buffer;

  MTLK_ASSERT(mtlk_guid_compare(evt, &_MTLK_IRBE_DUT_PROGMODEL_CMD) == 0);
  MTLK_ASSERT(core != NULL);

  strncpy(pm_file.fname, (char *)param->name, ARRAY_SIZE(pm_file.fname) - 1);
  pm_file.fname[ARRAY_SIZE(pm_file.fname) - 1] = '\0';

  res = mtlk_df_fw_load_file(mtlk_vap_get_manager(core->vap_handle), (char *)param->name, &pm_file.fcontents);

  if (res != MTLK_ERR_OK)
  {
    ELOG_D ("Failed to read progmodel file contents, error: %d", res);
  }
  else
  {
    res = mtlk_vap_get_hw_vft(core->vap_handle)->set_prop(core->vap_handle, MTLK_HW_PROGMODEL, &pm_file, sizeof(pm_file));
    if (res != MTLK_ERR_OK) {
      ELOG_D ("Failed to send progmodel contents to FW, error: %d", res);
    }

    mtlk_df_fw_unload_file(mtlk_vap_get_manager(core->vap_handle), &pm_file.fcontents);
  }

  param->status = MTLK_ERR_OK;
  if (MTLK_ERR_OK != res) {
    param->status = MTLK_ERR_UNKNOWN;
  }

  ILOG0_SDS("DUT progmodel %s load status %d (%s)", pm_file.fname, res, mtlk_get_error_text(res));
}

static void __MTLK_IFUNC
_mtlk_dut_core_handle_fw_msg (mtlk_irbd_t       *irbd,
                              mtlk_handle_t      context,
                              const mtlk_guid_t *evt,
                              void              *buffer,
                              uint32            *size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t *)context;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry;

  MTLK_ASSERT(mtlk_guid_compare(evt, &_MTLK_IRBE_DUT_FW_CMD) == 0);
  MTLK_ASSERT(core != NULL);

  if (sizeof(dutMessage_t) < *size) {
    ELOG_DDD("CID-%04x: DUT FW request of unexpected size %d bytes arrived (expected %d bytes or less)",
             mtlk_vap_get_oid(core->vap_handle), *size, sizeof(dutMessage_t));
    *size = 0;
    return;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txdm(core->vap_handle), NULL);
  if (man_entry)
  {
    man_entry->id           = UM_DBG_C100_IN_REQ;
    man_entry->payload_size = *size;
    memcpy(man_entry->payload, buffer, *size);

    res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_DUT_FW_MSG_SEND_TIMEOUT);
    ELOG_DDS("CID-%04x: DUT FW request status %d (%s)", mtlk_vap_get_oid(core->vap_handle), res, mtlk_get_error_text(res));

    if (MTLK_ERR_OK == res)
    {
      memcpy(buffer, man_entry->payload, *size);
    }

    mtlk_txmm_msg_cleanup(&man_msg);
  }
  else
  {
    ELOG_D("CID-%04x: Failed to send DUT FW request due to lack of management messages", mtlk_vap_get_oid(core->vap_handle));
  }
}

MTLK_START_STEPS_LIST_BEGIN(dut_core)
  MTLK_START_STEPS_LIST_ENTRY(dut_core, ALLOC_HANDLERS)
  MTLK_START_STEPS_LIST_ENTRY(dut_core, FW_CMD_HANDLER)
  MTLK_START_STEPS_LIST_ENTRY(dut_core, PROGMODEL_CMD_HANDLER)
  MTLK_START_STEPS_LIST_ENTRY(dut_core, EEPROM_IRBD)
MTLK_START_INNER_STEPS_BEGIN(dut_core)
MTLK_START_STEPS_LIST_END(dut_core);

static void
_mtlk_dut_core_stop (mtlk_core_t *core)
{
  mtlk_vap_handle_t vap_handle;
  mtlk_dut_core_t *dut_core;

  MTLK_ASSERT(core != NULL);

  vap_handle = core->vap_handle;
  dut_core = &core->dut_core;

  ILOG0_D("CID-%04x: Stopping DUT core...", mtlk_vap_get_oid(vap_handle));

  MTLK_STOP_BEGIN(dut_core, MTLK_OBJ_PTR(dut_core))
    MTLK_STOP_STEP(dut_core, EEPROM_IRBD, MTLK_OBJ_PTR(dut_core),
                   eeprom_irbd_unregister, (dut_core->dut_handlers->eeprom_irbd));
    MTLK_STOP_STEP(dut_core, PROGMODEL_CMD_HANDLER, MTLK_OBJ_PTR(dut_core),
                   mtlk_irbd_unregister, (mtlk_vap_get_irbd(vap_handle), dut_core->dut_handlers->fw_msg_handle));
    MTLK_STOP_STEP(dut_core, FW_CMD_HANDLER, MTLK_OBJ_PTR(dut_core),
                   mtlk_irbd_unregister, (mtlk_vap_get_irbd(vap_handle), dut_core->dut_handlers->progmodel_msg_handle));
    MTLK_STOP_STEP(dut_core, ALLOC_HANDLERS, MTLK_OBJ_PTR(dut_core),
                   mtlk_osal_mem_free, (dut_core->dut_handlers));
    dut_core->dut_handlers = NULL;
  MTLK_STOP_END(dut_core, MTLK_OBJ_PTR(dut_core))
}

static int
_mtlk_dut_core_start (mtlk_core_t *core)
{
  mtlk_vap_handle_t vap_handle;
  mtlk_dut_core_t *dut_core;

  MTLK_ASSERT(core != NULL);

  vap_handle = core->vap_handle;
  dut_core = &core->dut_core;

  ILOG0_D("CID-%04x: Starting DUT core...", mtlk_vap_get_oid(vap_handle));

  MTLK_START_TRY(dut_core, MTLK_OBJ_PTR(dut_core))
    MTLK_START_STEP_EX(dut_core, ALLOC_HANDLERS, MTLK_OBJ_PTR(dut_core),
                       mtlk_osal_mem_alloc,
                       (sizeof(mtlk_dut_handlers_t), MTLK_MEM_TAG_DUT_CORE),
                       dut_core->dut_handlers, dut_core->dut_handlers != NULL,
                       MTLK_ERR_NO_MEM);
    memset(dut_core->dut_handlers, 0, sizeof(mtlk_dut_handlers_t));
    MTLK_START_STEP_EX(dut_core, FW_CMD_HANDLER, MTLK_OBJ_PTR(dut_core),
                       mtlk_irbd_register, (mtlk_vap_get_irbd(vap_handle),
                       &_MTLK_IRBE_DUT_FW_CMD, 1, _mtlk_dut_core_handle_fw_msg, HANDLE_T(core)),
                       dut_core->dut_handlers->fw_msg_handle, dut_core->dut_handlers->fw_msg_handle != NULL,
                       MTLK_ERR_NO_RESOURCES);
    MTLK_START_STEP_EX(dut_core, PROGMODEL_CMD_HANDLER, MTLK_OBJ_PTR(dut_core),
                       mtlk_irbd_register, (mtlk_vap_get_irbd(vap_handle),
                       &_MTLK_IRBE_DUT_PROGMODEL_CMD, 1, _mtlk_dut_core_handle_progmodel_msg, HANDLE_T(core)),
                       dut_core->dut_handlers->progmodel_msg_handle, dut_core->dut_handlers->progmodel_msg_handle != NULL,
                       MTLK_ERR_NO_RESOURCES);
    MTLK_START_STEP_EX(dut_core, EEPROM_IRBD, MTLK_OBJ_PTR(dut_core),
                       eeprom_irbd_register, (vap_handle),
                       dut_core->dut_handlers->eeprom_irbd, dut_core->dut_handlers->eeprom_irbd != NULL,
                       MTLK_ERR_NO_RESOURCES);
  MTLK_START_FINALLY(dut_core, MTLK_OBJ_PTR(dut_core))
  MTLK_START_RETURN(dut_core, MTLK_OBJ_PTR(dut_core), _mtlk_dut_core_stop, (core))
}

static int __MTLK_IFUNC
_mtlk_dut_core_prepare_callback (mtlk_handle_t context, const void *payload, uint32 size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t *)context;

  MTLK_ASSERT(core != NULL);

  res = _mtlk_dut_core_start(core);

  return res;
}

static int __MTLK_IFUNC
_mtlk_dut_core_unprepare_callback (mtlk_handle_t context, const void *payload, uint32 size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t *)context;

  MTLK_ASSERT(core != NULL);

  _mtlk_dut_core_stop(core);

  res = mtlk_core_stop_lm(core);
  if (MTLK_ERR_OK != res) {
    return res;
  }

  /* Restore operation mode with HW reset by recovery procedure */
  rcvry_set_forced_type(mtlk_vap_get_rcvry(core->vap_handle), MTLK_CORE_UI_RCVRY_TYPE_FAST);
  rcvry_initiate(mtlk_vap_get_rcvry(core->vap_handle));

  return res;
}

static void __MTLK_IFUNC
_mtlk_dut_core_prepare (mtlk_irbd_t       *irbd,
                        mtlk_handle_t      context,
                        const mtlk_guid_t *evt,
                        void              *buffer,
                        uint32            *size)
{
  int res = MTLK_ERR_OK;
  BOOL run_dut = TRUE;
  int max_vaps_count, vap_index;
  mtlk_core_t *core = (mtlk_core_t *)context;
  mtlk_vap_handle_t vap_handle;

  MTLK_ASSERT(mtlk_guid_compare(evt, &_MTLK_IRBE_DUT_START_CMD) == 0);
  MTLK_ASSERT(core != NULL);

  if (NULL == core->dut_core.dut_handlers) {
    if (mtlk_core_is_stopping(core)) {
      ELOG_D("CID-%04x: Cannot activate DUT mode during interface DOWNING", mtlk_vap_get_oid(core->vap_handle));
      return;
    }

    max_vaps_count = mtlk_vap_manager_get_max_vaps_count(mtlk_vap_get_manager(core->vap_handle));
    for (vap_index = max_vaps_count - 1; vap_index >= 0; vap_index--) {
      if (MTLK_ERR_OK == mtlk_vap_manager_get_vap_handle(mtlk_vap_get_manager(core->vap_handle),
                                                         vap_index,
                                                         &vap_handle)) {
        if (!mtlk_vap_get_core(vap_handle)->is_stopped) {
          res = mtlk_df_down(mtlk_vap_get_df(vap_handle));
          if (res != MTLK_ERR_OK) {
            ELOG_D("CID-%04x: Cannot interface DOWN for DUT mode", mtlk_vap_get_oid(vap_handle));
            run_dut = FALSE;
          }
        }
      }
    }
    if (run_dut) {
      mtlk_core_schedule_internal_task(core, context, _mtlk_dut_core_prepare_callback, core, sizeof(*core));
    }
  }
}

static void __MTLK_IFUNC
_mtlk_dut_core_unprepare (mtlk_irbd_t       *irbd,
                          mtlk_handle_t      context,
                          const mtlk_guid_t *evt,
                          void              *buffer,
                          uint32            *size)
{
  mtlk_core_t *core = (mtlk_core_t *)context;

  MTLK_ASSERT(mtlk_guid_compare(evt, &_MTLK_IRBE_DUT_STOP_CMD) == 0);
  MTLK_ASSERT(core != NULL);

  if (NULL != core->dut_core.dut_handlers) {
    mtlk_core_schedule_internal_task(core, context, _mtlk_dut_core_unprepare_callback, core, sizeof(*core));
    /* Interface UP is excluded, even if DUT mode has been activated from UP mode */
  }
}

int __MTLK_IFUNC
mtlk_dut_core_register (mtlk_core_t *core)
{
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(core != NULL);

  if (!mtlk_vap_is_master(core->vap_handle)) {
    return MTLK_ERR_OK;
  }
  core->dut_core.dut_start_handle = mtlk_irbd_register(mtlk_vap_get_irbd(core->vap_handle),
                                                       &_MTLK_IRBE_DUT_START_CMD,
                                                       1,
                                                       _mtlk_dut_core_prepare,
                                                       HANDLE_T(core));
  if (NULL == core->dut_core.dut_start_handle) {
    return MTLK_ERR_NO_RESOURCES;
  }

  core->dut_core.dut_stop_handle = mtlk_irbd_register(mtlk_vap_get_irbd(core->vap_handle),
                                                      &_MTLK_IRBE_DUT_STOP_CMD,
                                                      1,
                                                      _mtlk_dut_core_unprepare,
                                                      HANDLE_T(core));
  if (NULL == core->dut_core.dut_stop_handle) {
    return MTLK_ERR_NO_RESOURCES;
  }

  if (mtlk_vap_is_dut(core->vap_handle)) {
    res = _mtlk_dut_core_start(core);
  }

  return res;
}

void __MTLK_IFUNC
mtlk_dut_core_unregister (mtlk_core_t *core)
{
  MTLK_ASSERT(core != NULL);

  if (!mtlk_vap_is_master(core->vap_handle)) {
    return;
  }

  if (NULL != core->dut_core.dut_start_handle) {
    mtlk_irbd_unregister(mtlk_vap_get_irbd(core->vap_handle),
                         core->dut_core.dut_start_handle);
  }
  if (NULL != core->dut_core.dut_stop_handle) {
    mtlk_irbd_unregister(mtlk_vap_get_irbd(core->vap_handle),
                         core->dut_core.dut_stop_handle);
  }
  if (NULL != core->dut_core.dut_handlers) {
    _mtlk_dut_core_stop(core);
  }
}

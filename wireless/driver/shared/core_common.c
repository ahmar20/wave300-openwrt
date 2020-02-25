/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "mtlk_vap_manager.h"
#include "mhi_mac_event.h"
#include "core_common.h"
#include "mtlkhal.h"
#include "txmm.h"
#include "hw_mmb.h"

#define LOG_LOCAL_GID   GID_COMMON_CORE
#define LOG_LOCAL_FID   0

/* TODO: Looks like CLI won't work in case of silent build */
#ifndef MTCFG_SILENT
static const char
mtlk_mac_event_prefix[] = "MAC event"; // don't change this - used from CLI
#endif

static void
_mtlk_cc_process_fw_log_buffers_on_exception(mtlk_vap_handle_t vap_handle)
{
#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
  uint8 is_supported_ex;

  mtlk_vap_get_hw_vft(vap_handle)->get_prop(vap_handle,
                      MTLK_HW_FW_LOGGER_IS_EX, &is_supported_ex, sizeof(is_supported_ex));

  if (is_supported_ex) {
    mtlk_vap_get_hw_vft(vap_handle)->set_prop(vap_handle, MTLK_HW_FW_LOG_BUFFER, &is_supported_ex, sizeof(is_supported_ex));
  }
#else
  MTLK_UNREFERENCED_PARAM(vap_handle);
  MTLK_UNREFERENCED_PARAM(buffers);
#endif
}

void __MTLK_IFUNC
mtlk_cc_handle_mac_exception(mtlk_vap_handle_t vap_handle, const APP_FATAL *app_fatal)
{
  WLOG_DSSDDDD("CID-%04x: %s: From %s : MAC exception: Cause 0x%x, EPC 0x%x, Status 0x%x, TS 0x%x", mtlk_vap_get_oid(vap_handle),
                mtlk_mac_event_prefix,
                app_fatal->uLmOrUm == 0 ? "lower" : "upper",
                MAC_TO_HOST32(app_fatal->uCauseRegOrLineNum),
                MAC_TO_HOST32(app_fatal->uEpcReg),
                MAC_TO_HOST32(app_fatal->uStatusReg),
                MAC_TO_HOST32(app_fatal->uTimeStamp));

  mtlk_txmm_terminate_waiting_msg(mtlk_hw_mmb_get_txmm(mtlk_vap_get_hw(vap_handle)));
  mtlk_txmm_terminate_waiting_msg(mtlk_hw_mmb_get_txdm(mtlk_vap_get_hw(vap_handle)));

  _mtlk_cc_process_fw_log_buffers_on_exception(vap_handle);
}

void __MTLK_IFUNC
mtlk_cc_handle_mac_fatal(mtlk_vap_handle_t vap_handle, const APP_FATAL *app_fatal)
{
  WLOG_DSSDDDD("CID-%04x: %s: From %s : MAC fatal error: [GroupID: %u, FileID: %u, Line: %u], TS 0x%x", mtlk_vap_get_oid(vap_handle),
               mtlk_mac_event_prefix,
               app_fatal->uLmOrUm == 0 ? "lower" : "upper",
               MAC_TO_HOST32(app_fatal->GroupId),
               MAC_TO_HOST32(app_fatal->FileId),
               MAC_TO_HOST32(app_fatal->uCauseRegOrLineNum),
               MAC_TO_HOST32(app_fatal->uTimeStamp));

  mtlk_txmm_terminate_waiting_msg(mtlk_hw_mmb_get_txmm(mtlk_vap_get_hw(vap_handle)));
  mtlk_txmm_terminate_waiting_msg(mtlk_hw_mmb_get_txdm(mtlk_vap_get_hw(vap_handle)));

  _mtlk_cc_process_fw_log_buffers_on_exception(vap_handle);
}

void __MTLK_IFUNC
mtlk_cc_handle_eeprom_failure(mtlk_vap_handle_t vap_handle, const EEPROM_FAILURE_EVENT *data)
{
  WLOG_DSD("CID-%04x: %s: EEPROM failure : Code %d", mtlk_vap_get_oid(vap_handle),
           mtlk_mac_event_prefix, data->u8ErrCode);
}

void __MTLK_IFUNC
mtlk_cc_handle_generic_event(mtlk_vap_handle_t vap_handle, const GENERIC_EVENT *data)
{
  ILOG0_DSD("CID-%04x: %s: Generic data: size %u", mtlk_vap_get_oid(vap_handle),
            mtlk_mac_event_prefix, MAC_TO_HOST32(data->u32dataLength));
  mtlk_dump(0, &data->u8data[0], GENERIC_DATA_SIZE, "Generic MAC data");
}

void __MTLK_IFUNC
mtlk_cc_handle_algo_calibration_failure(mtlk_vap_handle_t vap_handle, const CALIBR_ALGO_EVENT* calibration_event)
{
  ILOG1_DSDD("CID-%04x: %s: "
             "Offline calibration failed on channel %u, failure mask is 0x%08X",
             mtlk_vap_get_oid(vap_handle),
             mtlk_mac_event_prefix,
             MAC_TO_HOST32(calibration_event->u32calibrAlgoType),
             MAC_TO_HOST32(calibration_event->u32ErrCode));
}

void __MTLK_IFUNC
mtlk_cc_handle_dummy_event(mtlk_vap_handle_t vap_handle, const DUMMY_EVENT* data)
{
  ILOG0_DSDDDDDDDD("CID-%04x: %s: Dummy event : %u %u %u %u %u %u %u %u", mtlk_vap_get_oid(vap_handle),
                   mtlk_mac_event_prefix,
                   MAC_TO_HOST32(data->u32Dummy1),
                   MAC_TO_HOST32(data->u32Dummy2),
                   MAC_TO_HOST32(data->u32Dummy3),
                   MAC_TO_HOST32(data->u32Dummy4),
                   MAC_TO_HOST32(data->u32Dummy5),
                   MAC_TO_HOST32(data->u32Dummy6),
                   MAC_TO_HOST32(data->u32Dummy7),
                   MAC_TO_HOST32(data->u32Dummy8));
}

void __MTLK_IFUNC
mtlk_cc_handle_unknown_event(mtlk_vap_handle_t vap_handle, uint32 event_id)
{
  ILOG0_DSD("CID-%04x: %s: unknown MAC event id %u",
            mtlk_vap_get_oid(vap_handle),
            mtlk_mac_event_prefix, MAC_TO_HOST32(event_id));
}

/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"
#include "dutserver.h"
#include "dataex.h"
#include "mhi_dut.h"

#define LOG_LOCAL_GID   GID_DUT_MSG_CLB
#define LOG_LOCAL_FID   1

static dut_hostif_funcs _dut_hostif_funcs_array[DUT_SERVER_MSG_CNT];

#define   MTLK_PACK_ON
#include "mtlkpack.h"

/* backward compatibility, doReset parameter is absent in V0 */
typedef struct _dutResetParamsCompat
{
  uint32 nvMemoryType;
  uint32 eepromSize;
} __MTLK_PACKED dutResetParamsV0_t;

#define   MTLK_PACK_OFF
#include "mtlkpack.h"

static void
_dut_msg_clbk_fill_response_context (int in_msg_id, int out_msg_id, char *data, int length)
{
  _dut_hostif_funcs_array[in_msg_id].response_data = data;
  _dut_hostif_funcs_array[in_msg_id].response_length = length;
  _dut_hostif_funcs_array[in_msg_id].returned_message_id = out_msg_id;
}

#define _dut_progmodel_free(msg) mtlk_osal_mem_free(msg)

static dut_progmodel_t *
_dut_progmodel_alloc (const char *name)
{
  dut_progmodel_t *msg;
  uint32 len = strlen(name);

  msg = (dut_progmodel_t *)mtlk_osal_mem_alloc(sizeof(*msg) + len + 1, MTLK_MEM_TAG_DUT_CORE);
  if (!msg) {
    return NULL;
  }
  memset(msg, 0, sizeof(*msg) + len + 1);

  msg->status = MTLK_ERR_OK;
  msg->len = len + 1;
  snprintf(msg->name, msg->len, "%s", name);

  return msg;
}

static void
_dut_msg_clbk_upload_progmodel(const char* data, int length, int dutIndex)
{
  dut_progmodel_t *msg0 = NULL;
  dut_progmodel_t *msg1 = NULL;

  char progNames[2][256];
  unsigned int namesLen[2];
  char* newLinePtr = strchr(data, '\n');

  if (!dut_api_is_connected_to_hw(dutIndex))
  {
    ELOG_D("No connection with HW #%d", dutIndex);
    return;
  }

  if (newLinePtr == NULL) 
  {
    ELOG_V("Invalid data in upload progmodel message.");
    return;
  }

  namesLen[0] = newLinePtr - data;
  namesLen[1] = length - namesLen[0] - 1;
  if (namesLen[0] >= 255 || namesLen[1] >= 255)
  {
    ELOG_DD("Filenames of progmodels are too long (%d, %d)", namesLen[0], namesLen[1]);
    return;
  }

  memcpy(progNames[0], data, namesLen[0]);
  progNames[0][namesLen[0]] = '\0';
  memcpy(progNames[1], newLinePtr + 1, namesLen[1]);
  progNames[1][namesLen[1]] = '\0';

  msg0 = _dut_progmodel_alloc(progNames[0]);
  msg1 = _dut_progmodel_alloc(progNames[1]);
  if ((!msg0) || (!msg1)) {
    goto FINISH;
  }

  if ((MTLK_ERR_OK != dut_api_upload_progmodel((char *)msg0, (sizeof(*msg0) + msg0->len), dutIndex)) ||
      (MTLK_ERR_OK != dut_api_upload_progmodel((char *)msg1, (sizeof(*msg1) + msg1->len), dutIndex))) {
    goto FINISH;
  }

  if ((MTLK_ERR_OK != msg0->status) ||
      (MTLK_ERR_OK != msg1->status)) {
    goto FINISH;
  }

  _dut_msg_clbk_fill_response_context (DUT_SERVER_MSG_UPLOAD_PROGMODEL, DUT_SERVER_MSG_UPLOAD_PROGMODEL, (char*)data, 0);

FINISH:
  /* in case the upload failed - do not send response */
  if (msg0) {
    _dut_progmodel_free(msg0);
  }
  if (msg1) {
    _dut_progmodel_free(msg1);
  }
}

static void
_dut_msg_clbk_c100 (const char* data, int length, int dutIndex)
{
  int res;

  if (!dut_api_is_connected_to_hw(dutIndex))
  {
    ELOG_D("No connection with HW #%d", dutIndex);
    return;
  }

  res = dut_api_send_fw_command(data, length, dutIndex);
  if (MTLK_ERR_OK != res)
  {
    ELOG_D("Failed to send C100 message, error %d", res);
    memset(&data, 0, length);
  }

  _dut_msg_clbk_fill_response_context (DUT_SERVER_MSG_MAC_C100, DUT_SERVER_MSG_MAC_C100, (char*)data, length);
}

static void
_dut_msg_clbk_drv (const char* data, int length, int dutIndex)
{
  int res;

  if (!dut_api_is_connected_to_hw(dutIndex))
  {
    ELOG_D("No connection with HW #%d", dutIndex);
    return;
  }

  res = dut_api_send_drv_command(data, length, dutIndex);
  if (MTLK_ERR_OK != res)
  {
    ELOG_D("Failed to send DRV message, error %d", res);
    memset(&data, 0, length);
  }

  _dut_msg_clbk_fill_response_context (DUT_SERVER_MSG_DRIVER_GENERAL, DUT_SERVER_MSG_DRIVER_GENERAL, (char*)data, length);
}

static void
_dut_msg_clbk_reset_fw(const char* data, int length, int dutIndex)
{
  dutResetParams_t *params = (dutResetParams_t *)data;
  int res;
  BOOL doReset = FALSE;

  MTLK_UNREFERENCED_PARAM(dutIndex);

  /* backward compatibility, doReset parameter is absent in V0 */
  if ((NULL != params) && (length == sizeof(dutResetParamsV0_t)))
  {
    doReset = TRUE;
  }

  if ((NULL != params) && (length == sizeof(dutResetParams_t)))
  {
    doReset = (BOOL)params->doReset;
  }

  /* call stop procedure anyway */
  (void)dut_api_driver_stop(doReset);

  sleep(2);

  (void)dut_api_driver_start(doReset);

  /* backward compatibility, no parameters */
  if ((NULL != params) && (length >= sizeof(dutResetParamsV0_t)))
  {
    if (!dut_api_is_connected_to_hw(dutIndex))
    {
      ELOG_D("No connection with HW #%d", dutIndex);
      return;
    }

    if (DUT_NV_MEMORY_FLASH == DUT_TO_HOST32(params->nvMemoryType))
    {
      res = dut_api_eeprom_data_on_flash_prepare(DUT_TO_HOST32(params->eepromSize), dutIndex);
      if (MTLK_ERR_OK != res)
      {
        ELOG_D("Failed to create EEPROM data on FLASH, error %d", res);
        memset(&data, 0, length);
      }
    }
  }

  _dut_msg_clbk_fill_response_context(DUT_SERVER_MSG_RESET_MAC, DUT_SERVER_MSG_RESET_MAC, (char*)data, 0);
}

void
dut_msg_clbk_init()
{
  memset (_dut_hostif_funcs_array, 0, sizeof(dut_hostif_funcs));

  _dut_hostif_funcs_array[DUT_SERVER_MSG_RESET_MAC].pOnDutRequest = _dut_msg_clbk_reset_fw;
  _dut_hostif_funcs_array[DUT_SERVER_MSG_UPLOAD_PROGMODEL].pOnDutRequest = _dut_msg_clbk_upload_progmodel;
  _dut_hostif_funcs_array[DUT_SERVER_MSG_MAC_C100].pOnDutRequest = _dut_msg_clbk_c100;
  _dut_hostif_funcs_array[DUT_SERVER_MSG_DRIVER_GENERAL].pOnDutRequest = _dut_msg_clbk_drv;
}

dut_hostif_funcs*
dut_msg_clbk_get_handler(int msgID)
{
  if(msgID >= DUT_SERVER_MSG_CNT || msgID <= 0)
  {
    return NULL;
  }

  return &_dut_hostif_funcs_array[msgID];
}

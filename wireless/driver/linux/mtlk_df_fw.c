/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#include "mtlkinc.h"

#include <linux/device.h>

#include "mtlk_df_fw.h"
#include "hw_mmb.h"
#include "mtlkhal.h"

#define LOG_LOCAL_GID   GID_DFFW
#define LOG_LOCAL_FID   1

void __MTLK_IFUNC
mtlk_df_fw_unload_file(mtlk_vap_manager_t *vap_mgr, mtlk_df_fw_file_buf_t *fb)
{
  MTLK_ASSERT(NULL != vap_mgr);
  MTLK_UNREFERENCED_PARAM(vap_mgr);

  release_firmware((const struct firmware *)fb->context);
}

static const struct firmware *
_mtlk_df_fw_request_firmware(mtlk_vap_manager_t *vap_mgr, const char *fname)
{
  const struct firmware *fw_entry = NULL;
  mtlk_vap_handle_t vap_handle;
  int result = 0;
  int try = 0;

  result = mtlk_vap_manager_get_master_vap(vap_mgr, &vap_handle);
  if (MTLK_ERR_OK != result)
  {
    ELOG_V("Skip Firmware loading, missed master VAP");
    return NULL;
  }

  /* on kernels 2.6 it could be that request_firmware returns -EEXIST
     it means that we tried to load firmware file before this time
     and kernel still didn't close sysfs entries it uses for download
     (see hotplug for details). In order to avoid such problems we
     will try number of times to load FW */
try_load_again:
  result = request_firmware(&fw_entry, fname,
    mtlk_vap_get_hw_vft(vap_handle)->get_device(vap_handle));

  if (result == -EEXIST) {
    try++;
    if (try < 10) {
      msleep(10);
      goto try_load_again;
    }
  }
  if (result != 0)
  {
    ELOG_S("Firmware (%s) is missing", fname);
    fw_entry = NULL;
  }
  else
  {
    ILOG3_SDP("%s firmware: size=0x%x, data=0x%p",
        fname, (unsigned int)fw_entry->size, fw_entry->data);
  }

  return fw_entry;
}

int __MTLK_IFUNC
mtlk_df_fw_load_file (mtlk_vap_manager_t *vap_mgr, const char *name,
                      mtlk_df_fw_file_buf_t *fb)
{
  int                    res      = MTLK_ERR_UNKNOWN;
  const struct firmware *fw_entry = NULL;

  MTLK_ASSERT(NULL != vap_mgr);
  MTLK_ASSERT(NULL != name);
  MTLK_ASSERT(NULL != fb);

  fw_entry = _mtlk_df_fw_request_firmware(vap_mgr, name);

  if (fw_entry) {
    fb->buffer  = fw_entry->data;
    fb->size    = fw_entry->size;
    fb->context = HANDLE_T(fw_entry);
    res         = MTLK_ERR_OK;
  }

  return res;
}


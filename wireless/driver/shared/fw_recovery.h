/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
 * FW RECOVERY header file
 *
 *
 */
#ifndef __FW_RECOVERY_H__
#define __FW_RECOVERY_H__

typedef enum {
  RCVRY_TYPE_NONE,
  RCVRY_TYPE_FAST,
  RCVRY_TYPE_FULL,
  RCVRY_TYPE_COMPLETE,
  RCVRY_TYPE_UNDEF,
  RCVRY_TYPE_LAST
} rcvry_type_e;

/* configuration structure */
typedef struct _rcvry_cfg_t {
  uint8 fast_rcvry_num;         /* consecutive number of allowed FAST recovery */
  uint8 full_rcvry_num;         /* consecutive number of allowed FULL recovery */
  uint8 complete_rcvry;         /* allowed complete recovery */
  uint32 fail_time_interval;    /* time interval after FW full or fast recovery where
                                   FW stuck considered as consecutive fail */
  uint8 fw_dump;                /* dump FW after crash */
} rcvry_cfg_t;

mtlk_handle_t __MTLK_IFUNC
mtlk_rcvry_create(mtlk_vap_manager_t *vap_manager, mtlk_wss_t *parent_wss);

void __MTLK_IFUNC
mtlk_rcvry_delete(mtlk_handle_t rcvry_handle);

int __MTLK_IFUNC
mtlk_rcvry_set_cfg(mtlk_handle_t rcvry_handle, rcvry_cfg_t *rcvry_cfg);

int __MTLK_IFUNC
mtlk_rcvry_get_cfg(mtlk_handle_t rcvry_handle, rcvry_cfg_t *rcvry_cfg);

void __MTLK_IFUNC
rcvry_set_forced_type(mtlk_handle_t rcvry_handle, rcvry_type_e recovery_type);

void __MTLK_IFUNC
rcvry_initiate(mtlk_handle_t rcvry_handle);

BOOL __MTLK_IFUNC
rcvry_is_dump_in_progress(mtlk_handle_t rcvry_handle);

BOOL __MTLK_IFUNC
rcvry_is_in_progress(mtlk_handle_t rcvry_handle);

BOOL __MTLK_IFUNC
rcvry_is_scheduled(mtlk_handle_t rcvry_handle);

BOOL __MTLK_IFUNC
rcvry_is_initialized(mtlk_handle_t rcvry_handle);

#endif /* __FW_RECOVERY_H__ */

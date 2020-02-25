/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/

#ifndef _MTLK_POWERSAVE_H_
#define _MTLK_POWERSAVE_H_

/** 
*\file mtlk_ps.h
*\brief Power Save
*/

#include "mtlkdfdefs.h"
#include "mhi_ieee_address.h"
#include "mtlkqos.h"

#define LOG_LOCAL_GID   GID_PS
#define LOG_LOCAL_FID   0

struct _sta_db;

typedef struct _mtlk_ps_status_t {
  uint32 uapsd_enabled_stas;
  uint32 delivery_enabled_acs;
} mtlk_ps_status_t;

typedef struct _mtlk_ps_peer_status_t {
  IEEE_ADDR mac_addr;
  BOOL      ps_mode_enabled;
  BOOL      uapsd_enabled[NTS_PRIORITIES];
  uint32    limit_per_ac[NTS_PRIORITIES];
  uint32    used_per_ac[NTS_PRIORITIES];
} mtlk_ps_peer_status_t;

int __MTLK_IFUNC mtlk_ps_get_status(struct _sta_db *stadb, mtlk_clpb_t *clpb);

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#endif //_MTLK_POWERSAVE_H_

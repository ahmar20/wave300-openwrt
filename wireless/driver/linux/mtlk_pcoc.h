/******************************************************************************

                               Copyright (c) 2013
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
 *
 *
 *
 * Power management functionality implementation in compliance with
 * Code of Conduct on Energy Consumption of Broadband Equipment (a.k.a CoC)
 *
 * On-the fly CPU frequency switch on AR10
 *
 */


#ifndef __MTLK_PCOC_H__
#define __MTLK_PCOC_H__

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/**********************************************************************
 * Public declaration
***********************************************************************/
typedef struct _mtlk_pcoc_t mtlk_pcoc_t;

typedef struct _mtlk_pcoc_params_t
{
  uint32 interval;
  uint32 limit_lower;
  uint32 limit_upper;
} __MTLK_IDATA mtlk_pcoc_params_t;

typedef struct _mtlk_pcoc_cfg_t
{
  mtlk_vap_handle_t vap_handle;
  mtlk_pcoc_params_t default_params;
} __MTLK_IDATA mtlk_pcoc_cfg_t;

/**********************************************************************
 * Public function declaration
***********************************************************************/
mtlk_pcoc_t* __MTLK_IFUNC
mtlk_pcoc_create(const mtlk_pcoc_cfg_t *cfg);

void __MTLK_IFUNC
mtlk_pcoc_delete(mtlk_pcoc_t *pcoc_obj);

mtlk_pcoc_params_t * __MTLK_IFUNC
mtlk_pcoc_get_params(mtlk_pcoc_t *pcoc_obj);

int __MTLK_IFUNC
mtlk_pcoc_set_params(mtlk_pcoc_t *pcoc_obj, const mtlk_pcoc_params_t *params);

BOOL __MTLK_IFUNC
mtlk_pcoc_is_enabled_adm(mtlk_pcoc_t *pcoc_obj);

int __MTLK_IFUNC
mtlk_pcoc_set_enabled_adm(mtlk_pcoc_t *pcoc_obj, const BOOL is_enabled);

int __MTLK_IFUNC
mtlk_pcoc_set_enabled_op(mtlk_pcoc_t *pcoc_obj, const BOOL is_enabled);

BOOL __MTLK_IFUNC
mtlk_pcoc_is_active(mtlk_pcoc_t *pcoc_obj);

uint8 __MTLK_IFUNC
mtlk_pcoc_get_traffic_state(mtlk_pcoc_t *pcoc_obj);

int __MTLK_IFUNC
mtlk_pcoc_test_pmcu_cb(mtlk_pcoc_t *pcoc_obj, uint32 pmcu_debug);

void __MTLK_IFUNC
mtlk_pcoc_on_rcvry_isol(mtlk_pcoc_t *pcoc_obj);

int __MTLK_IFUNC
mtlk_pcoc_on_rcvry_configure(mtlk_pcoc_t *pcoc_obj);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_PCOC_H__ */

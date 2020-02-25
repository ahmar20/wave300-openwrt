/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/

#include "mtlkinc.h"

#include "mtlk_sq.h"
#include "mtlk_ps.h"
#include "stadb.h"

#define LOG_LOCAL_GID   GID_PS
#define LOG_LOCAL_FID   1

int __MTLK_IFUNC
mtlk_ps_get_status(sta_db *stadb, mtlk_clpb_t *clpb)
{
  int                   res = MTLK_ERR_UNKNOWN;
  int                   idx;
  mtlk_stadb_iterator_t iter;
  const sta_entry      *sta = NULL;
  mtlk_ps_status_t      status;
  mtlk_ps_peer_status_t peer_status;

  ASSERT( stadb != NULL );

  status.uapsd_enabled_stas = 0;
  status.delivery_enabled_acs = 0;

  sta = mtlk_stadb_iterate_first(stadb, &iter);
  if (sta) {
    do {
      peer_status.mac_addr = *mtlk_sta_get_addr(sta);
      peer_status.ps_mode_enabled = mtlk_osal_atomic_get(&sta->sq_peer_ctx.ps_mode_enabled);

      /* ILOG0_Y("Processing STA %Y", &peer_status.mac_addr); */

      for(idx = 0; idx < NTS_PRIORITIES; idx++) {
        peer_status.uapsd_enabled[idx] = mtlk_osal_atomic_get(&sta->sq_peer_ctx.limit_per_ac[idx]);
        peer_status.limit_per_ac[idx] = peer_status.ps_mode_enabled && peer_status.uapsd_enabled[idx]
          ? mtlk_osal_atomic_get(&sta->sq_peer_ctx.limit_per_ac[idx])
          : mtlk_osal_atomic_get(&sta->sq_peer_ctx.limit);
        peer_status.used_per_ac[idx] = mtlk_osal_atomic_get(&sta->sq_peer_ctx.used_per_ac[idx]);

        /* ILOG0_DD("AC[%d] limit = %d", idx, peer_status.limit_per_ac[idx]); */
        /* ILOG0_DD("AC[%d] used  = %d", idx, peer_status.used_per_ac[idx]); */

        if (mtlk_osal_atomic_get(&sta->sq_peer_ctx.limit_per_ac[idx])) {
          ++ status.delivery_enabled_acs;
          /* ILOG0_D("Delivery enabled STA = '%d'", status.delivery_enabled_acs); */
        }
      }

      if (sta->sq_peer_ctx.uapsd_max_sp) {
        ++ status.uapsd_enabled_stas;
        /* ILOG0_D("U-APSD enabled STA = '%d'", status.uapsd_enabled_stas); */
      }

      res = mtlk_clpb_push(clpb, &peer_status, sizeof(peer_status));
      if (MTLK_ERR_OK != res) {
        goto err_push;
      }

      sta = mtlk_stadb_iterate_next(&iter);

    } while (sta);
    mtlk_stadb_iterate_done(&iter);
  }

  res = mtlk_clpb_push(clpb, &status, sizeof(status));
  if (MTLK_ERR_OK != res) {
    goto err_push;
  }

  goto finish;

 err_push:
  ELOG_V("!!! ERROR !!!");
  mtlk_clpb_purge(clpb);
 finish:

  return res;
}



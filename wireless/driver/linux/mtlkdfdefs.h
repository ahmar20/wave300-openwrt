/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MTLK_DF_DEFS_H__
#define __MTLK_DF_DEFS_H__

typedef struct _mtlk_user_request_t mtlk_user_request_t; /*!< User request object provided to core */

#define _mtlk_core_t nic
typedef struct _mtlk_core_t     mtlk_core_t;

typedef struct _mtlk_hw_t       mtlk_hw_t;
typedef struct _mtlk_df_t       mtlk_df_t;
typedef struct _mtlk_pdb_t      mtlk_pdb_t;
typedef struct _mtlk_txmm_t     mtlk_txmm_t;

typedef struct _sta_entry       sta_entry;

typedef struct net_device       mtlk_ndev_t;
typedef struct sk_buff          mtlk_nbuf_t;
typedef struct sk_buff_head     mtlk_nbuf_list_t;
typedef struct sk_buff          mtlk_nbuf_list_entry_t;

#endif /* __MTLK_DF_DEFS_H__ */


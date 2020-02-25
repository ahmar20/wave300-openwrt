/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id$
 *
 * 
 *
 * Written by: Grygorii Strashko
 *
 * Private DF definitions
 *
 */


#ifndef __MTLK_DF_PRIV_H__
#define __MTLK_DF_PRIV_H__

#include "mtlkdfdefs.h"
#include "mtlk_df_user_priv.h"
#include "mtlk_clipboard.h"
#include "mtlk_coreui.h"

/* Proc_FS node type - Node contains entries */
typedef struct _mtlk_df_proc_fs_node_t mtlk_df_proc_fs_node_t;

/***
 * DF PROC OS dependent definitions
 ***/
/* sequential print support */
#define _mtlk_seq_entry_t seq_file

/* sequential print support - context */
typedef struct _mtlk_seq_entry_t mtlk_seq_entry_t;

/**********************************************************************
 * DF PROC FS utilities
 **********************************************************************/

#define mtlk_aux_seq_printf seq_printf
#define mtlk_aux_seq_write seq_write

/* read proc handler type */
typedef int (*mtlk_df_proc_entry_read_f)(char *page, char **start, off_t off,
                                        int count, int *eof, void *data);

/* write proc handler type */
typedef int (*mtlk_df_proc_entry_write_f)(struct file *file, const char __user *buffer,
                                         unsigned long count, void *data);

/* seq file show handler type */
typedef int (*mtlk_df_proc_seq_entry_show_f)(mtlk_seq_entry_t *seq_ctx, void *data);

mtlk_df_proc_fs_node_t* __MTLK_IFUNC
mtlk_df_proc_node_create(const uint8 *name, mtlk_df_proc_fs_node_t* parent);

void __MTLK_IFUNC
mtlk_df_proc_node_delete(mtlk_df_proc_fs_node_t* proc_node);

int __MTLK_IFUNC
mtlk_df_proc_node_add_ro_entry(char *name,
                               mtlk_df_proc_fs_node_t *parent_node,
                               void *df,
                               mtlk_df_proc_entry_read_f  read_func);

int __MTLK_IFUNC
mtlk_df_proc_node_add_wo_entry(char *name,
                               mtlk_df_proc_fs_node_t *parent_node,
                               void *df,
                               mtlk_df_proc_entry_write_f write_func);

int  __MTLK_IFUNC
mtlk_df_proc_node_add_rw_entry(char *name,
                               mtlk_df_proc_fs_node_t *parent_node,
                               void *df,
                               mtlk_df_proc_entry_read_f  read_func,
                               mtlk_df_proc_entry_write_f write_func);

int __MTLK_IFUNC
mtlk_df_proc_node_add_seq_entry(char *name,
                                mtlk_df_proc_fs_node_t *parent_node,
                                void *df,
                                mtlk_df_proc_seq_entry_show_f show_func);

void __MTLK_IFUNC
mtlk_df_proc_node_remove_entry(char *name,
                               mtlk_df_proc_fs_node_t *parent_node);

void* __MTLK_IFUNC
mtlk_df_proc_seq_entry_get_df(mtlk_seq_entry_t *seq_ctx);

//NOTICE caused problems with gcc -Werror -Wall
//int mtlk_aux_seq_printf(mtlk_seq_entry_t *seq_ctx, const char *fmt, ...);
//int mtlk_aux_seq_write(mtlk_seq_entry_t *seq_ctx, const void *data, size_t len);

mtlk_df_proc_fs_node_t* mtlk_dfg_get_drv_proc_node(void);

/*! \brief   Returns TRUE if the DF belongs to a Slave Core.

    \param   df          DF object
*/
BOOL
mtlk_df_is_slave(mtlk_df_t *df);


/*! \brief   Returns TRUE if the DF belongs to a AP.

    \param   df          DF object
*/
BOOL
mtlk_df_is_ap(mtlk_df_t *df);

/*! \fn      mtlk_df_user_t* mtlk_df_get_user(mtlk_df_t *df)

    \brief   Returns pointer to DF USER object.

    \param   df          DF object
*/
mtlk_df_user_t*
mtlk_df_get_user(mtlk_df_t *df);

int
_mtlk_df_user_process_core_retval_void(int processing_result,
                                       mtlk_clpb_t* execution_result,
                                       mtlk_core_tx_req_id_t core_req,
                                       BOOL delete_clipboard_on_success);

int
_mtlk_df_user_process_core_retval(int processing_result,
                                  mtlk_clpb_t* execution_result,
                                  mtlk_core_tx_req_id_t core_req,
                                  BOOL delete_clipboard_on_success);

int
_mtlk_df_user_invoke_core(mtlk_df_t* df,
                          mtlk_core_tx_req_id_t req_id,
                          mtlk_clpb_t **ppclpb,
                          const void* data,
                          size_t data_size);

typedef void __MTLK_IFUNC (*_invoke_async_clb_t)(mtlk_handle_t user_context,
                                                 int           processing_result,
                                                 mtlk_clpb_t  *pclpb);
void
_mtlk_df_user_invoke_core_async(mtlk_df_t* df,
                                mtlk_core_tx_req_id_t req_id,
                                const void* data,
                                size_t data_size,
                                _invoke_async_clb_t clb,
                                mtlk_handle_t ctx);

int
_mtlk_df_user_pull_core_data(mtlk_df_t* df,
                             mtlk_core_tx_req_id_t core_req,
                             BOOL is_void_request,
                             void **out_data,
                             uint32* out_data_length,
                             mtlk_handle_t *hdata);

void
_mtlk_df_user_free_core_data(mtlk_df_t* df,
                             mtlk_handle_t data_handle);

int
_mtlk_df_user_push_core_data(mtlk_df_t* df,
                             mtlk_core_tx_req_id_t core_req,
                             BOOL is_void_request,
                             void *in_data,
                             uint32 in_data_length);

void*
_mtlk_df_user_alloc_core_data(mtlk_df_t* df,
                              uint32 data_length);

#endif /* __MTLK_DF_PRIV_H__ */

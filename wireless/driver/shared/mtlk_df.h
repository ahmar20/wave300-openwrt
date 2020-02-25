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
 * Driver framework debug API.
 *
 * Originally written by Andrii Tseglytskyi
 *
 */

#ifndef __DF_UI__
#define __DF_UI__

#include "mtlkdfdefs.h"
#include "mtlk_vap_manager.h"
#include "mtlkirbd.h"
#include "mtlk_wss.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/**********************************************************************
 * Functions declaration
***********************************************************************/
/*! \fn      mtlk_df_t* mtlk_df_create(mtlk_vap_handle_t vap_handle)
    
    \brief   Allocates and initiates DF object.

    \return  pointer to created DF
*/
mtlk_df_t* mtlk_df_create(mtlk_vap_handle_t vap_handle);

/*! \fn      void mtlk_df_delete(mtlk_df_t *df)
    
    \brief   Cleanups and deletes DF object.

    \param   df          DF object 
*/
void mtlk_df_delete(mtlk_df_t *df);

/*! \fn      void mtlk_df_start(mtlk_df_t *df)
    
    \brief   Starts the DF object.

    \param   df          DF object
    \param   reason      Stop reason (in case of error during start)
*/
int mtlk_df_start(mtlk_df_t *df, mtlk_vap_manager_interface_e intf);

/*! \fn      void mtlk_df_stop(mtlk_df_t *df)
    
    \brief   Stops the DF object.

    \param   df          DF object
    \param   reason      Stop reason
*/
void mtlk_df_stop(mtlk_df_t *df, mtlk_vap_manager_interface_e intf);

mtlk_vap_manager_t*
mtlk_df_get_vap_manager(const mtlk_df_t* df);

mtlk_vap_handle_t
mtlk_df_get_vap_handle(const mtlk_df_t* df);

const char* mtlk_df_get_name(mtlk_df_t *df);
int         mtlk_df_down(mtlk_df_t *df);
int         mtlk_df_up(mtlk_df_t *df);

/*! \fn      mtlk_df_t* mtlk_df_get_df_by_dfg_entry(mtlk_dlist_entry_t *df_entry)

    \brief   Returns pointer to DF object, expanded from node in DFG list

    \param   df_entry list entry for DF object
*/
mtlk_df_t* mtlk_df_get_df_by_dfg_entry(mtlk_dlist_entry_t *df_entry);

/**********************************************************************
 * DF USER utilities (CORE->DF->DF_USER->User space)
 **********************************************************************/
/**

*\brief Outer world interface subsidiary of driver framework

*\defgroup DFUSER DF user
*\{
*/

enum BCL_UNIT_TYPE {
  BCL_UNIT_INT_RAM = 0,
  BCL_UNIT_AFE_RAM,
  BCL_UNIT_RFIC_RAM,
  BCL_UNIT_EEPROM,
  BCL_UNIT_MAX = 10
};


/*! Notifies DF UI about user request processing completion

    \param   req     Request object.
    \param   result  Processing result.

    \warning
    Do not garble request processing result with request execution result.
    Request processing result indicates whether request was \b processed
    successfully. In case processing result is MTK_ERR_OK, caller may examine
    request execution result and collect request execution resulting data.

*/
void __MTLK_IFUNC
mtlk_df_ui_req_complete(mtlk_user_request_t *req, int result);

void __MTLK_IFUNC
mtlk_df_ui_set_mac_addr(mtlk_df_t *df, const uint8* mac_addr);

const uint8* __MTLK_IFUNC
mtlk_df_ui_get_mac_addr(mtlk_df_t* df);

BOOL __MTLK_IFUNC
mtlk_df_ui_is_promiscuous(mtlk_df_t *df);

/***
 * Requests from Core
 ***/
/* Data transfer functions */
int __MTLK_IFUNC
mtlk_df_ui_indicate_rx_data(mtlk_df_t *df_user, mtlk_nbuf_t *nbuf);

BOOL __MTLK_IFUNC
mtlk_df_ui_check_is_mc_group_member(mtlk_df_t *df, const uint8* group_addr);

int __MTLK_IFUNC
mtlk_df_user_ppa_register (mtlk_vap_handle_t vap_handle);

/***
 * Notifications from Core
 ***/
void __MTLK_IFUNC
mtlk_df_ui_notify_tx_start(mtlk_df_t *df);

/* Wireless subsystem access API*/
void __MTLK_IFUNC
mtlk_df_ui_notify_association(mtlk_df_t *df, const uint8 *mac);

void __MTLK_IFUNC
mtlk_df_ui_notify_disassociation(mtlk_df_t *df);

void __MTLK_IFUNC
mtlk_df_ui_notify_node_connect(mtlk_df_t *df, const uint8 *node_addr);

void __MTLK_IFUNC
mtlk_df_ui_notify_node_disconect(mtlk_df_t *df, const uint8 *node_addr);

void __MTLK_IFUNC
mtlk_df_ui_notify_secure_node_connect(mtlk_df_t *df_user, const uint8 *node_addr,
                                        const uint8 *rsnie, size_t rsnie_len);

void __MTLK_IFUNC
mtlk_df_ui_notify_scan_complete(mtlk_df_t *df);

typedef enum
{
  MIC_FAIL_PAIRWISE = 0,
  MIC_FAIL_GROUP
} mtlk_df_ui_mic_fail_type_t;

void __MTLK_IFUNC
mtlk_df_ui_notify_mic_failure(mtlk_df_t *df,
                                const uint8 *src_mac,
                                mtlk_df_ui_mic_fail_type_t fail_type);

void __MTLK_IFUNC
mtlk_df_ui_notify_notify_rmmod(const char *wlanitrf);

void __MTLK_IFUNC
mtlk_df_ui_notify_notify_fw_hang(mtlk_df_t *df, uint32 fw_cpu, uint32 sw_watchdog_data);

void __MTLK_IFUNC
mtlk_df_on_rcvry_isol(mtlk_df_t *df);

void __MTLK_IFUNC
mtlk_df_on_rcvry_restore(mtlk_df_t *df);

/* Remove MAC addr from switch MAC table */
void __MTLK_IFUNC
mtlk_df_user_ppa_remove_mac_addr(mtlk_df_t *df, const uint8 *mac_addr);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __DF_UI__ */

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
 * 20/40 coexistence feature
 * Provides transition between modes (20MHz->20/40MHz and vice versa)
 *
 * AP:
 * The scan exemption policy manager will deal with the requests by the associated stations to be exempt 
 * from the periodic OBSS scanning. 
 * Its operation will depend on the current value of a number of MIBs.
 *
 * STA:
 * The scan exemption request / response manager will generate scan exemption requests 
 * and process the respective responses from the associated AP
 *
 * This file relates to both (AP and STA)
 */
#ifndef __SCEXEMPT_H__
#define __SCEXEMPT_H__

#ifndef COEX_20_40_C
#error This file can only be included from one of the 20/40 coexistence implementation (.c) files
#endif

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/**************************************************************************************/
/****************************************  AP  ****************************************/
/**************************************************************************************/

struct _mtlk_20_40_coexistence_sm ;

typedef struct _mtlk_sta_exemption_descriptor
{
  IEEE_ADDR                       sta_addr;
  uint16                          padding;
  BOOL                            slot_used;
  BOOL                            supports_coexistence;
  BOOL                            exempt;
  BOOL                            intolerant;
  BOOL                            legacy;
} mtlk_sta_exemption_descriptor_t;


typedef struct _mtlk_scan_exemption_policy_manager
{
  struct _mtlk_20_40_coexistence_sm *parent_csm;
  mtlk_20_40_csm_xfaces_t           *xfaces;
  mtlk_sta_exemption_descriptor_t   *sta_exemption_descriptors;
  uint32                            descriptor_array_size;
  uint32                            min_num_of_non_exempted_sta;
}mtlk_scan_exemption_policy_manager_t;

int __MTLK_IFUNC mtlk_sepm_init (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  struct _mtlk_20_40_coexistence_sm *parent_csm, mtlk_20_40_csm_xfaces_t *xfaces, uint32 descriptor_array_size);

void __MTLK_IFUNC mtlk_sepm_cleanup (mtlk_scan_exemption_policy_manager_t *sepm_mgr);

void __MTLK_IFUNC mtlk_sepm_register_station (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  const IEEE_ADDR *sta_addr, BOOL supports_coexistence, BOOL exempt, BOOL intolerant, BOOL legacy);

void __MTLK_IFUNC mtlk_sepm_unregister_station (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  const IEEE_ADDR *sta_addr);

int __MTLK_IFUNC mtlk_sepm_process_exemption_request (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  const IEEE_ADDR *sta_addr);

BOOL __MTLK_IFUNC mtlk_sepm_register_station_intolerance (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  const IEEE_ADDR *sta_addr);

BOOL __MTLK_IFUNC mtlk_sepm_is_assoc_sta (mtlk_scan_exemption_policy_manager_t *sepm_mgr,
  const IEEE_ADDR *sta_addr);

BOOL __MTLK_IFUNC mtlk_sepm_is_intolerant_or_legacy_station_connected (mtlk_scan_exemption_policy_manager_t *sepm_mgr);

BOOL __MTLK_IFUNC mtlk_sepm_is_intolerant_station_connected(mtlk_scan_exemption_policy_manager_t *sepm_mgr);

uint32 __MTLK_IFUNC mtlk_sepm_get_min_non_exempted_sta (mtlk_scan_exemption_policy_manager_t *sepm_mgr);

int __MTLK_IFUNC mtlk_sepm_set_min_non_exempted_sta (mtlk_scan_exemption_policy_manager_t *sepm_mgr, uint32 min_num_of_non_exempted_sta);

/**************************************************************************************/
/****************************************  STA  ***************************************/
/**************************************************************************************/

typedef enum{
  MTLK_OBSM_STATE_NOT_STARTED,
  MTLK_OBSM_STATE_STARTED,
  MTLK_OBSM_STATE_CONNECTED,
  MTLK_OBSM_STATE_CONNECTED_EXEMPTION_REQUESTED,
  MTLK_OBSM_STATE_CONNECTED_EXEMPTION_RECEIVED_ACTIVE,
  MTLK_OBSM_STATE_CONNECTED_EXEMPTION_RECEIVED_OVERRIDDEN,
} obsm_states_e;

typedef struct _mtlk_obss_scan_manager
{
  struct _mtlk_20_40_coexistence_sm *parent_csm;
  mtlk_20_40_csm_xfaces_t           *xfaces;
  obsm_states_e                      state;
  BOOL                               interested_in_exemption;
  BOOL                               scan_in_progress;
  mtlk_osal_timestamp_t              last_scan_ts;
  mtlk_osal_timestamp_t              next_scan_due_time_ts;
  mtlk_osal_timer_t                  scan_timer;
  mtlk_20_40_coexistence_element     coex_el;
  mtlk_20_40_obss_scan_results_t     scan_results;

  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_START_STATUS;
} mtlk_obss_scan_manager_t;

int __MTLK_IFUNC mtlk_obsm_init (mtlk_obss_scan_manager_t *obsm_mgr,
  struct _mtlk_20_40_coexistence_sm *parent_csm, mtlk_20_40_csm_xfaces_t *xfaces);
  
int __MTLK_IFUNC mtlk_obsm_start(mtlk_obss_scan_manager_t *obsm_mgr);

void __MTLK_IFUNC mtlk_obsm_stop(mtlk_obss_scan_manager_t *obsm_mgr);

void __MTLK_IFUNC mtlk_obsm_cleanup (mtlk_obss_scan_manager_t *obsm_mgr);

void __MTLK_IFUNC mtlk_obsm_sta_notify_connection_to_ap(mtlk_obss_scan_manager_t *obsm_mgr, BOOL supports_coexistence, BOOL scan_exemption_received);

void __MTLK_IFUNC mtlk_obsm_sta_notify_disconnection_from_ap(mtlk_obss_scan_manager_t *obsm_mgr);

void __MTLK_IFUNC mtlk_obsm_sta_notify_switch_to_20_mode(mtlk_obss_scan_manager_t *obsm_mgr, uint16 channel);

void __MTLK_IFUNC mtlk_obsm_sta_notify_switch_to_40_mode(mtlk_obss_scan_manager_t *obsm_mgr, uint16 primary_channel, int secondary_channel_offset);

void __MTLK_IFUNC mtlk_obsm_set_request_scan_exemption_flag(mtlk_obss_scan_manager_t *obsm_mgr, BOOL must_request);

int __MTLK_IFUNC mtlk_obsm_process_response(mtlk_obss_scan_manager_t *obsm_mgr, BOOL exemption_granted);

void __MTLK_IFUNC mtlk_obsm_prepare_and_send_obss_scan_report(mtlk_obss_scan_manager_t *obsm_mgr, const IEEE_ADDR *addr, BOOL dont_lock);

void __MTLK_IFUNC mtlk_obsm_notify_scan_interval_updated(mtlk_obss_scan_manager_t *obsm_mgr, BOOL dont_lock);

BOOL __MTLK_IFUNC mtlk_obsm_is_scan_in_progress(mtlk_obss_scan_manager_t *obsm_mgr);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif

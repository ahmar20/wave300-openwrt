/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 * $Id: coex20_40.h 11780 2011-10-19 13:00:19Z bogoslav $
 *
 * 
 *
 * 20/40 coexistence feature
 * Provides transition between modes (20MHz->20/40MHz and vice versa)
 *
 * The 20/40 coexistence state machine module will export a complete façade-style interface to 
 * all externally accessible functions of the main module and its sub-modules. 
 * Calls that are to be processed by the auxiliary sub-modules rather than the main module itself, 
 * will be forwarded to the actual processor of the request. 
 * In order to implement that and in order to satisfy the internal needs of the state machine module, 
 * the auxiliary sub-modules will export interfaces of their own.
 *
 */
#ifndef __COEX20_40_H__
#define __COEX20_40_H__

#include "mhi_umi.h"
#include "mhi_umi_propr.h"
#include "mtlk_vap_manager.h"
#include "mtlk_wss.h"
#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

/* coexistence-related data elements (represented by auxiliary structures) */

typedef enum
{
  CSM_STATE_NOT_STARTED,
  CSM_STATE_20,
  CSM_STATE_20_40,
} eCSM_STATES;

typedef enum
{
  eAO_REGISTER,
  eAO_UNREGISTER,
  eAO_ENABLE,
  eAO_DISABLE,
} eABILITY_OPS;

typedef UMI_COEX_EL mtlk_20_40_coexistence_element;

typedef struct _mtlk_20_40_obss_scan_results
{
  UMI_INTOLERANT_CHANNELS_REPORT    intolerant_channels_report;
  /* TODO insert var's */
} mtlk_20_40_obss_scan_results_t ;/* (see UMI_INTOLERANT_CHANNELS_REPORT definition in the SW -> FW section) */

typedef struct _mtlk_20_40_bss_info
{
  uint16                channel;
  uint8                 secondary_channel_offset;
  uint8                 max_rssi;
  mtlk_osal_timestamp_t timestamp;
  BOOL                  is_2_4;
  BOOL                  is_ht;
  BOOL                  forty_mhz_intolerant;
} mtlk_20_40_bss_info_t;

typedef struct _mtlk_20_40_obss_scan_params
{
  uint16  uOBSSScanPassiveDwell;
  uint16  uOBSSScanActiveDwell;
  uint16  uBSSChannelWidthTriggerScanInterval;
  uint16  uOBSSScanPassiveTotalPerChannel;
  uint16  uOBSSScanActiveTotalPerChannel;
  uint16  uBSSWidthChannelTransitionDelayFactor;
  uint16  uOBSSScanActivityThreshold;
}mtlk_20_40_obss_scan_params;

struct _mtlk_get_channel_data_t;

typedef void (*switch_cb_mode_callback_type)(mtlk_handle_t context,
  uint16 primary_channel, int secondary_channel_offset);

typedef void __MTLK_IFUNC(*send_ce_callback_type)(mtlk_handle_t context,
  UMI_COEX_EL *coexistence_element);

typedef void __MTLK_IFUNC(*send_cmf_callback_type)(mtlk_handle_t context,
  const UMI_COEX_FRAME *coexistence_frame);

typedef void __MTLK_IFUNC(*send_exemption_policy_callback_type)(mtlk_handle_t context,
  BOOL must_grant_exemption_request);

typedef int __MTLK_IFUNC (*scan_async_callback_type)(mtlk_handle_t context,
  BOOL scan_5mhz_band);

typedef void __MTLK_IFUNC (*scan_set_bg_callback_type)(mtlk_handle_t context, BOOL is_background);

typedef void __MTLK_IFUNC scan_completed_notification_callback_type(mtlk_handle_t context);

typedef int __MTLK_IFUNC (*register_scan_completion_notification_callback_type)(mtlk_handle_t caller_context,
  mtlk_handle_t core_context, scan_completed_notification_callback_type *callback);

typedef void __MTLK_IFUNC (*bss_enumerator_callback_type)(mtlk_handle_t context, 
  mtlk_20_40_bss_info_t *bss_info);

typedef int __MTLK_IFUNC (*enumerate_bss_info_callback_type)(mtlk_handle_t caller_context,
  mtlk_handle_t core_context, bss_enumerator_callback_type callback, uint32 expiration_time);

typedef int __MTLK_IFUNC (*ability_control_callback_type)(mtlk_handle_t context,
  eABILITY_OPS operation, const uint32* ab_id_list, uint32 ab_id_num);

typedef uint16 __MTLK_IFUNC (*get_cur_channels_callback_type)(mtlk_handle_t context, int *secondary_channel_offset);

typedef unsigned long __MTLK_IFUNC (*modify_cache_expire_callback_type)(mtlk_handle_t core_context, unsigned long new_time, BOOL force_change);

typedef int __MTLK_IFUNC (*update_fw_obss_scan_params_callback_type)(mtlk_handle_t context);

typedef struct _mtlk_20_40_csm_xfaces
{
  mtlk_handle_t                                       context;
  mtlk_vap_handle_t                                   vap_handle;
  switch_cb_mode_callback_type                        switch_cb_mode;
  send_ce_callback_type                               send_ce;
  send_cmf_callback_type                              send_cmf;
  send_exemption_policy_callback_type                 send_exempt_policy;
  scan_async_callback_type                            scan_async;
  scan_set_bg_callback_type                           scan_set_background;
  register_scan_completion_notification_callback_type register_scan_completion_notification_callback;
  enumerate_bss_info_callback_type                    enumerate_bss_info;
  ability_control_callback_type                       ability_control;
  get_cur_channels_callback_type                      get_cur_channels;
  modify_cache_expire_callback_type                   modify_cache_expire_time;
  update_fw_obss_scan_params_callback_type            update_fw_obss_scan_params;
} mtlk_20_40_csm_xfaces_t;

/* interfaces */

struct _mtlk_20_40_coexistence_sm;

/* Initialization and de-initialization interfaces */ 

struct _mtlk_20_40_coexistence_sm* __MTLK_IFUNC mtlk_20_40_create(mtlk_20_40_csm_xfaces_t *xfaces, BOOL is_ap, uint32 max_number_of_connected_stations);

int __MTLK_IFUNC mtlk_20_40_start(struct _mtlk_20_40_coexistence_sm *coex_sm, eCSM_STATES initial_state, mtlk_wss_t *parent_wss);

void __MTLK_IFUNC mtlk_20_40_stop(struct _mtlk_20_40_coexistence_sm *coex_sm);

void __MTLK_IFUNC mtlk_20_40_delete(struct _mtlk_20_40_coexistence_sm *coex_sm);

void __MTLK_IFUNC mtlk_20_40_limit_to_20(struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL must_limit);

/* Interfaces for the command line handlers */

int __MTLK_IFUNC mtlk_20_40_enable_feature(struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL enable);
/* This function will either enable or disable the 20/40 coexistence feature in general. */

BOOL __MTLK_IFUNC mtlk_20_40_is_feature_enabled(struct _mtlk_20_40_coexistence_sm *coex_sm);
/* This function returns TRUE if the 20/40 coexistence feature is enabled or FALSE otherwise.*/

int __MTLK_IFUNC mtlk_20_40_declare_intolerance(struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL intolerant);
/* This function will instruct the state machine whether to declare 20/40 intolerance or not.*/

BOOL __MTLK_IFUNC mtlk_20_40_is_intolerance_declared(struct _mtlk_20_40_coexistence_sm *coex_sm);
/* This function returns TRUE if the STA/AP has declared itself 40 MHz intolerant, or FALSE otherwise. */

int __MTLK_IFUNC mtlk_20_40_set_ap_force_scan_params_on_assoc_sta(struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL ap_force_scan_params_on_assoc_sta);

BOOL __MTLK_IFUNC mtlk_20_40_get_ap_force_scan_params_on_assoc_sta(struct _mtlk_20_40_coexistence_sm *coex_sm);

int __MTLK_IFUNC mtlk_20_40_set_wait_for_scan_results_interval(struct _mtlk_20_40_coexistence_sm *coex_sm, uint16 wait_for_scan_results_interval);

uint16 __MTLK_IFUNC mtlk_20_40_get_wait_for_scan_results_interval(struct _mtlk_20_40_coexistence_sm *coex_sm);

int __MTLK_IFUNC mtlk_20_40_sta_force_scan_exemption_request (struct _mtlk_20_40_coexistence_sm *coex_sm, 
  BOOL request_exemption);
/* This interface will allow other modules to instruct the coexistence state machine of a station to request exemption from OBSS scanning. The second parameter (request_exemption) will determine whether the coexistence state machine of the station will generate coexistence elements with OBSS Scanning Exemption Request flag set to one or zero. */

BOOL __MTLK_IFUNC mtlk_20_40_sta_is_scan_exemption_request_forced (struct _mtlk_20_40_coexistence_sm *coex_sm);
/* This function returns TRUE if the station is configured to request exemption from OBSS scanning, or FALSE otherwise. */

int __MTLK_IFUNC mtlk_20_40_set_transition_delay_factor (struct _mtlk_20_40_coexistence_sm *coex_sm, 
  uint16 delay_factor);
/* The delay factor is the minimum number of scans that the AP must perform (by itself or by an associated station) before it makes a decision to move from 20 to 20/40 CB mode. */

uint16 __MTLK_IFUNC mtlk_20_40_get_transition_delay_factor (struct _mtlk_20_40_coexistence_sm *coex_sm);
/* The function returns the current transition delay factor. */

int __MTLK_IFUNC mtlk_20_40_set_scan_interval (struct _mtlk_20_40_coexistence_sm *coex_sm,
  uint16 scan_interval);
/* The scan interval is the interval in seconds between two consecutive OBSS scans. */

uint16 __MTLK_IFUNC mtlk_20_40_get_scan_interval (struct _mtlk_20_40_coexistence_sm *coex_sm);
/*The function returns the current interval between two consecutive OBSS scans. */

int __MTLK_IFUNC mtlk_20_40_set_scan_activity_threshold(struct _mtlk_20_40_coexistence_sm *coex_sm, uint16 activity_threshold);

uint16 __MTLK_IFUNC mtlk_20_40_get_scan_activity_threshold(struct _mtlk_20_40_coexistence_sm *coex_sm);

int __MTLK_IFUNC mtlk_20_40_set_scan_passive_dwell(struct _mtlk_20_40_coexistence_sm *coex_sm, uint16 scan_passive_dwell);

uint16 __MTLK_IFUNC mtlk_20_40_get_scan_passive_dwell(struct _mtlk_20_40_coexistence_sm *coex_sm);

int __MTLK_IFUNC mtlk_20_40_set_scan_active_dwell(struct _mtlk_20_40_coexistence_sm *coex_sm, uint16 scan_active_dwell);

uint16 __MTLK_IFUNC mtlk_20_40_get_scan_active_dwell(struct _mtlk_20_40_coexistence_sm *coex_sm);

int __MTLK_IFUNC mtlk_20_40_set_passive_total_per_channel(struct _mtlk_20_40_coexistence_sm *coex_sm, uint16 passive_total_per_channel);

uint16 __MTLK_IFUNC mtlk_20_40_get_passive_total_per_channel(struct _mtlk_20_40_coexistence_sm *coex_sm);

int __MTLK_IFUNC mtlk_20_40_set_active_total_per_channel(struct _mtlk_20_40_coexistence_sm *coex_sm, uint16 active_total_per_channel);

uint16 __MTLK_IFUNC mtlk_20_40_get_active_total_per_channel(struct _mtlk_20_40_coexistence_sm *coex_sm);

uint32 __MTLK_IFUNC mtlk_20_40_get_min_non_exempted_sta(struct _mtlk_20_40_coexistence_sm *coex_sm);

int __MTLK_IFUNC mtlk_20_40_set_min_non_exempted_sta(struct _mtlk_20_40_coexistence_sm *coex_sm, uint32 min_num_of_non_exempted_sta);

/* Additional interfaces */
void __MTLK_IFUNC mtlk_20_40_ap_process_coexistence_element (struct _mtlk_20_40_coexistence_sm *coex_sm, 
  const mtlk_20_40_coexistence_element *coex_el, const IEEE_ADDR *src_addr);

void __MTLK_IFUNC mtlk_20_40_sta_process_coexistence_element (struct _mtlk_20_40_coexistence_sm *coex_sm, 
  mtlk_20_40_coexistence_element *coex_el, const IEEE_ADDR *src_addr, BOOL individually_addressed, BOOL assoc_res_process, BOOL is_from_assoc_ap);

BOOL __MTLK_IFUNC mtlk_20_40_is_intolerant_station_connected(struct _mtlk_20_40_coexistence_sm *coex_sm);

void __MTLK_IFUNC mtlk_20_40_ap_process_obss_scan_results (struct _mtlk_20_40_coexistence_sm *coex_sm,
  UMI_INTOLERANT_CHANNEL_DESCRIPTOR *intolerant_channels_descriptor);
/* This function will be called by the frame parser of on AP when an OBSS scan results are received from one of the associated stations. */

void __MTLK_IFUNC mtlk_20_40_ap_notify_non_ht_beacon_received (struct _mtlk_20_40_coexistence_sm *coex_sm, uint16 channel);

void __MTLK_IFUNC mtlk_20_40_ap_notify_intolerant_or_legacy_station_connected (struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL dont_lock);

void __MTLK_IFUNC mtlk_20_40_ap_notify_last_40_incapable_station_disconnected(struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL dont_lock);

void __MTLK_IFUNC mtlk_20_40_sta_notify_connection_to_ap(struct _mtlk_20_40_coexistence_sm *coex_sm, int spectrum_mode, BOOL supports_coexistence, BOOL scan_exemption_received);

void __MTLK_IFUNC mtlk_20_40_sta_notify_disconnection_from_ap(struct _mtlk_20_40_coexistence_sm *coex_sm);

void __MTLK_IFUNC mtlk_20_40_sta_notify_switch_to_20_mode (struct _mtlk_20_40_coexistence_sm *coex_sm,
  uint16 channel);

void __MTLK_IFUNC mtlk_20_40_sta_notify_switch_to_40_mode (struct _mtlk_20_40_coexistence_sm *coex_sm,
  uint16 primary_channel, int secondary_channel_offset);

void __MTLK_IFUNC mtlk_20_40_register_station (struct _mtlk_20_40_coexistence_sm *coex_sm,
  const IEEE_ADDR *sta_addr, BOOL supports_coexistence, BOOL exempt, BOOL intolerant, BOOL legacy);

void __MTLK_IFUNC mtlk_20_40_unregister_station (struct _mtlk_20_40_coexistence_sm *coex_sm, 
  const IEEE_ADDR *sta_addr);

BOOL __MTLK_IFUNC mtlk_20_40_is_20_40_operation_permitted(struct _mtlk_20_40_coexistence_sm *coex_sm,
  uint16 primary_channel, uint8 secondary_channel_offset);

void __MTLK_IFUNC mtlk_20_40_set_intolerance_at_first_scan_flag(struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL intolerant, BOOL dont_lock);

void __MTLK_IFUNC mtlk_20_40_sta_apply_associated_ap_parameters(struct _mtlk_20_40_coexistence_sm *coex_sm, mtlk_20_40_obss_scan_params *received_params);

void __MTLK_IFUNC mtlk_20_40_sta_restore_obss_scan_parameters(struct _mtlk_20_40_coexistence_sm *coex_sm);

void __MTLK_IFUNC mtlk_20_40_sta_intolerance_detected(struct _mtlk_20_40_coexistence_sm *coex_sm, BOOL dont_lock);

#ifdef BT_ACS_DEBUG
void __MTLK_IFUNC mtlk_20_40_clear_intolerant_db(struct _mtlk_20_40_coexistence_sm *coex_sm);
#endif /* BT_ACS_DEBUG */

void __MTLK_IFUNC mtlk_20_40_ap_on_fast_rcvry_isol(struct _mtlk_20_40_coexistence_sm *coex_sm);
void __MTLK_IFUNC mtlk_20_40_ap_on_full_rcvry_isol(struct _mtlk_20_40_coexistence_sm *coex_sm);
void __MTLK_IFUNC mtlk_20_40_ap_on_rcvry_restore(struct _mtlk_20_40_coexistence_sm *coex_sm);
int __MTLK_IFUNC mtlk_20_40_ap_on_fast_rcvry_configure(struct _mtlk_20_40_coexistence_sm *coex_sm);
int __MTLK_IFUNC mtlk_20_40_ap_on_full_rcvry_configure(struct _mtlk_20_40_coexistence_sm *coex_sm,
                                                       eCSM_STATES initial_state,
                                                       mtlk_wss_t *parent_wss);

void  __MTLK_IFUNC mtlk_20_40_set_rssi_threshold(struct _mtlk_20_40_coexistence_sm *coex_sm, int32 rssi_threshold);
int32 __MTLK_IFUNC mtlk_20_40_get_rssi_threshold(struct _mtlk_20_40_coexistence_sm *coex_sm);
BOOL  __MTLK_IFUNC mtlk_20_40_check_rssi_threshold(struct _mtlk_20_40_coexistence_sm *coex_sm, uint32 rssi);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif

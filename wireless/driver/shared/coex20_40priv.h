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
 * 20/40 coexistence feature - private portion
 *
 * This file defines the private portion of the 20/40 coexistence state
 * machine's interface meant only for the state machine itself and its
 * child modules
 */

#ifndef __COEX20_40CHLD_H__
#define __COEX20_40CHLD_H__

#ifndef COEX_20_40_C
#error This file can only be included from one of the 20/40 coexistence implementation (.c) files
#endif

#include "coex20_40.h"
#include "coexlve.h"
#include "scexempt.h"
#include "coexfrgen.h"
#include "cbsmgr.h"
#include "mtlkstartup.h"

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

#define CE2040_NUMBER_OF_CHANNELS_IN_2G4_BAND   14
#define CE2040_FIRST_CHANNEL_NUMBER_IN_2G4_BAND 1


/* statistics */
typedef enum{
  MTLK_COEX_20_40_NOF_COEX_EL_RECEIVED,                            /* received coexistence element */
  MTLK_COEX_20_40_NOF_COEX_EL_SCAN_EXEMPTION_REQUESTED,            /* received coexistence element with SCAN_EXEMPTION_REQUEST bit = 1 */
  MTLK_COEX_20_40_NOF_COEX_EL_SCAN_EXEMPTION_GRANTED,              /* sent coexistence element with SCAN_EXEMPTION_GRANT bit = 1 */
  MTLK_COEX_20_40_NOF_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED,      /* sent coexistence element with SCAN_EXEMPTION_GRANT bit = 0 */
  MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_20_TO_40,                     /* switch channel message sent to FW (20MHz to 40MHz) */
  MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_40_TO_20,                     /* switch channel message sent to FW (40MHz to 20MHz) */
  MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_40_TO_40,                     /* switch channel message sent to FW (40MHz to 40MHz) */
  
  MTLK_COEX_20_40_CNT_LAST
} coex_20_40_info_cnt_id_e;


typedef struct _mtlk_coex_intolerant_detection
{
  BOOL                              intolerant_detected;
  mtlk_osal_timestamp_t             intolerant_detection_ts;
  BOOL                              request_for_20_mhz_detected;
  mtlk_osal_timestamp_t             request_for_20_mhz_detection_ts;
} mtlk_coex_intolerant_detection_t;

typedef struct _mtlk_coex_intolerant_channels_db
{
  BOOL                              primary;
  mtlk_osal_timestamp_t             primary_detection_ts;
  BOOL                              secondary;
  mtlk_osal_timestamp_t             secodnary_detection_ts;
  BOOL                              intolerant;
  mtlk_osal_timestamp_t             intolerant_detection_ts;
} mtlk_coex_intolerant_channels_db_t;

typedef struct _mtlk_coex_intolerant_db
{
  mtlk_coex_intolerant_detection_t      general_intolerance_data;
  mtlk_coex_intolerant_channels_db_t    channels_list[CE2040_NUMBER_OF_CHANNELS_IN_2G4_BAND];
  MTLK_DECLARE_INIT_STATUS;
} mtlk_coex_intolerant_db_t;

typedef struct _mtlk_coex_scan_parameters
{
  uint16                                scan_passive_dwell;             /* in Transmission Unit */
  uint16                                scan_active_dwell;              /* in Transmission Unit */
  uint16                                scan_passive_total_per_channel; /* in Transmission Unit */
  uint16                                scan_active_total_per_channel;  /* in Transmission Unit */
  uint16                                delay_factor;
  uint16                                obss_scan_interval;
  uint16                                scan_activity_threshold;
  BOOL                                  already_saved;
} mtlk_coex_scan_parameters_t;

/* state machine */
typedef struct _mtlk_20_40_coexistence_sm
{
  mtlk_osal_spinlock_t                  lock;
  /* MIBs */
  mtlk_atomic_t                         coexistence_mode;
  mtlk_atomic_t                         rssi_threshold;
  mtlk_atomic_t                         intolerance_mode;
  mtlk_atomic_t                         scan_passive_dwell;             /* in Transmission Unit */
  mtlk_atomic_t                         scan_active_dwell;              /* in Transmission Unit */
  mtlk_atomic_t                         scan_passive_total_per_channel; /* in Transmission Unit */
  mtlk_atomic_t                         scan_active_total_per_channel;  /* in Transmission Unit */
  mtlk_atomic_t                         delay_factor;
  mtlk_atomic_t                         obss_scan_interval;
  mtlk_atomic_t                         scan_activity_threshold;
  /* State Machine attributes */
  mtlk_coex_scan_parameters_t           sta_original_scan_parameters;
  unsigned long                         sta_original_cache_expire;
  mtlk_atomic_t                         ap_force_scan_params_on_assoc_sta;
  mtlk_atomic_t                         wait_for_scan_results_interval;
  mtlk_atomic_t                         limited_to_20;
  mtlk_atomic_t                         exemption_req;
  mtlk_atomic_t                         intolerance_detected_at_first_scan;
  mtlk_atomic_t                         is_ap;
  mtlk_osal_timer_t                     transition_timer;
  mtlk_coex_intolerant_db_t             intolerance_db;
  mtlk_atomic_t                         current_csm_state;
  mtlk_20_40_csm_xfaces_t               xfaces;

  /* sub-modules */
  mtlk_local_variable_evaluator         coexlve;
  mtlk_cb_switch_manager                cbsm;
  mtlk_scan_exemption_policy_manager_t  ap_scexmpt;
  mtlk_obss_scan_manager_t              sta_scexmpt;
  mtlk_coex_frame_gen                   frgen;

  /* statistics */
  mtlk_wss_t                            *wss;
  mtlk_wss_cntr_handle_t                *wss_hcntrs[MTLK_COEX_20_40_CNT_LAST];

  MTLK_DECLARE_INIT_STATUS;
  MTLK_DECLARE_START_STATUS;
} __MTLK_IDATA mtlk_20_40_coexistence_sm_t;

/* Interfaces for the child modules */
void __MTLK_IFUNC mtlk_20_40_perform_idb_update(mtlk_20_40_coexistence_sm_t *coex_sm);
uint32 __MTLK_IFUNC mtlk_20_40_calc_transition_timer(mtlk_20_40_coexistence_sm_t *coex_sm);
BOOL __MTLK_IFUNC mtlk_20_40_is_coex_el_intolerant_bit_detected(mtlk_20_40_coexistence_sm_t *coex_sm, BOOL ignore_timeouts);
BOOL __MTLK_IFUNC mtlk_20_40_is_coex_el_20_mhz_request_bit_detected(mtlk_20_40_coexistence_sm_t *coex_sm, BOOL ignore_timeouts);
void __MTLK_IFUNC mtlk_20_40_clear_intolerant_db(mtlk_20_40_coexistence_sm_t *coex_sm);
void __MTLK_IFUNC mtlk_20_40_lock(mtlk_20_40_coexistence_sm_t *coex_sm);
void __MTLK_IFUNC mtlk_20_40_unlock(mtlk_20_40_coexistence_sm_t *coex_sm);

/* statistics - promote counter function */
/* statistic counters will be modified with checking ALLOWED option */
#define MTLK_COEX_20_40_NOF_COEX_EL_RECEIVED_ALLOWED                        MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_RECEIVED_ALLOWED
#define MTLK_COEX_20_40_NOF_COEX_EL_SCAN_EXEMPTION_REQUESTED_ALLOWED        MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_REQUESTED_ALLOWED
#define MTLK_COEX_20_40_NOF_COEX_EL_SCAN_EXEMPTION_GRANTED_ALLOWED          MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_GRANTED_ALLOWED
#define MTLK_COEX_20_40_NOF_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED_ALLOWED  MTLK_WWSS_WLAN_STAT_ID_NOF_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED_ALLOWED
#define MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_20_TO_40_ALLOWED                 MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_20_TO_40_ALLOWED
#define MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_40_TO_20_ALLOWED                 MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_40_TO_20_ALLOWED
#define MTLK_COEX_20_40_NOF_CHANNEL_SWITCH_40_TO_40_ALLOWED                 MTLK_WWSS_WLAN_STAT_ID_NOF_CHANNEL_SWITCH_40_TO_40_ALLOWED

#define mtlk_coex_20_40_inc_cnt(coex, id)         { if (id##_ALLOWED) __mtlk_coex_20_40_inc_cnt(coex, id); }

void __MTLK_IFUNC __mtlk_coex_20_40_inc_cnt(mtlk_20_40_coexistence_sm_t *coex_sm, coex_20_40_info_cnt_id_e cnt_id);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif

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
* Cross - platfrom core module code
*/

#ifndef _MTLK_CORE_IFACE_H_
#define _MTLK_CORE_IFACE_H_

struct nic;
struct _mtlk_eeprom_data_t;
struct _mtlk_dot11h_t;
struct _mtlk_vap_handle_t;

#ifdef MTCFG_RF_MANAGEMENT_MTLK
struct _mtlk_rf_mgmt_t;
#endif

/* Core "getters" */
struct _mtlk_eeprom_data_t* __MTLK_IFUNC mtlk_core_get_eeprom(struct nic* core);
struct _mtlk_dot11h_t*      __MTLK_IFUNC mtlk_core_get_dfs(struct nic* core);
uint8                       __MTLK_IFUNC mtlk_core_get_country_code (struct nic *core);
BOOL                        __MTLK_IFUNC mtlk_core_get_dot11d (struct nic *core);
uint16                      __MTLK_IFUNC mtlk_core_get_sq_size(struct nic *nic, uint16 access_category);
uint8                       __MTLK_IFUNC mtlk_core_get_cur_bonding(struct nic *core);
uint8                       __MTLK_IFUNC mtlk_core_get_user_bonding(struct nic *core);
int                         __MTLK_IFUNC mtlk_core_set_cur_bonding(mtlk_core_t *core, uint8 bonding);

uint8  __MTLK_IFUNC mtlk_core_get_network_mode_cur(mtlk_core_t *core);
uint8  __MTLK_IFUNC mtlk_core_get_network_mode_cfg(mtlk_core_t *core);
uint8  __MTLK_IFUNC mtlk_core_get_freq_band_cur(struct nic *core);
uint8  __MTLK_IFUNC mtlk_core_get_freq_band_cfg(struct nic *core);
uint8  __MTLK_IFUNC mtlk_core_get_is_ht_cur(mtlk_core_t *core);
uint8  __MTLK_IFUNC mtlk_core_get_is_ht_cfg(mtlk_core_t *core);
BOOL   __MTLK_IFUNC mtlk_core_get_interfdet_mode_cfg(mtlk_core_t *core);
BOOL   __MTLK_IFUNC mtlk_core_get_interfdet_scan_report_en(mtlk_core_t *core);

/* Move the following prototype to frame master interface
   when frame module design is introduced */
#ifdef MTCFG_RF_MANAGEMENT_MTLK
struct _mtlk_rf_mgmt_t*     __MTLK_IFUNC mtlk_get_rf_mgmt(mtlk_handle_t context);
#endif

void    __MTLK_IFUNC mtlk_core_sta_country_code_update_from_bss(struct nic* core, uint8 country_code);
int16   __MTLK_IFUNC mtlk_calc_tx_power_lim_wrapper(mtlk_handle_t usr_data, int8 spectrum_mode, uint8 channel);
int16   __MTLK_IFUNC mtlk_scan_calc_tx_power_lim_wrapper(mtlk_handle_t usr_data, int8 spectrum_mode, uint8 reg_domain, uint8 channel);
int16   __MTLK_IFUNC mtlk_get_antenna_gain_wrapper(mtlk_handle_t usr_data, uint8 channel);
int     __MTLK_IFUNC mtlk_reload_tpc_wrapper (uint8 channel, mtlk_handle_t usr_data);
uint8   __MTLK_IFUNC mtlk_core_is_device_busy(mtlk_handle_t context);
void    __MTLK_IFUNC mtlk_core_notify_scan_complete(struct _mtlk_vap_handle_t *vap_handle);
BOOL    __MTLK_IFUNC mtlk_core_is_stopping(struct nic *core);
int     __MTLK_IFUNC mtlk_core_wds_on_beacon(struct nic *core, IEEE_ADDR *addr);
uint32  __MTLK_IFUNC mtlk_core_wds_on_timer(mtlk_osal_timer_t *timer, mtlk_handle_t wds_peer_handle);
uint32  __MTLK_IFUNC mtlk_core_wds_on_host_disconnect_tmr(mtlk_osal_timer_t *timer, mtlk_handle_t host_handle);
uint32  __MTLK_IFUNC mtlk_core_ta_on_timer(mtlk_osal_timer_t *timer, mtlk_handle_t ta_handle);
int     __MTLK_IFUNC detect_replay(mtlk_core_t *core, mtlk_nbuf_t *nbuf, u8 *last_rc, BOOL is_mgmt);
int     __MTLK_IFUNC get_rsc_buf(mtlk_core_t* core, mtlk_nbuf_t *nbuf, int off);

void    __MTLK_IFUNC core_schedule_recovery_task(struct _mtlk_vap_handle_t *vap_handle, void *task, mtlk_handle_t rcvry_handle, int vap_num);
int     __MTLK_IFUNC core_on_rcvry_configure(mtlk_core_t *core, uint32 rcvry_net_state, uint32 preactivate, uint32 rcvry_type);
void    __MTLK_IFUNC core_on_rcvry_isol(mtlk_core_t *core, uint32 rcvry_type);
void    __MTLK_IFUNC core_on_rcvry_isol_irbd_unregister(mtlk_core_t *core);
int     __MTLK_IFUNC core_on_rcvry_restore(mtlk_core_t *core);
void    __MTLK_IFUNC core_on_rcvry_error(mtlk_core_t *core);
BOOL    __MTLK_IFUNC core_get_is_mac_fatal_pending(mtlk_core_t *core);
void    __MTLK_IFUNC core_set_is_mac_fatal_pending(mtlk_core_t *core, BOOL pending);
void    __MTLK_IFUNC mtlk_core_store_calibration_channel_bit_map(mtlk_core_t *core, uint32 *storedCalibrationChannelBitMap);

/**
*\defgroup CORE_SERIALIZED_TASKS Core serialization facility
*\brief Core interface for scheduling tasks on demand of outer world

*\{

*/

/*! Core serialized task callback prototype

    \param   object           Handle of receiver object
    \param   data             Pointer to the data buffer provided by caller
    \param   data_size        Size of data buffer provided by caller
    \return  MTLK_ERR_... code indicating whether function invocation succeeded

    \warning
    Do not garble function invocation result with execution result.
    Execution result indicates whether request was \b processed
    successfully. In case invocation result is MTK_ERR_OK, caller may examine
    Execution result and collect resulting data.
*/
typedef int __MTLK_IFUNC (*mtlk_core_task_func_t)(mtlk_handle_t object,
                                                  const void *data,
                                                  uint32 data_size);

/*! Function for scheduling serialized task on demand of internal core activities

    \param   nic              Pointer to the core object
    \param   object           Handle of receiver object
    \param   func             Task callback
    \param   data             Pointer to the data buffer provided by caller
    \param   data_size        Size of data buffer provided by caller

*/
int __MTLK_IFUNC mtlk_core_schedule_internal_task_ex(struct nic* core,
                                                     mtlk_handle_t object,
                                                     mtlk_core_task_func_t func,
                                                     const void *data, size_t size,
                                                     mtlk_slid_t issuer_slid);

#define mtlk_core_schedule_internal_task(core, object, func, data, size) \
  mtlk_core_schedule_internal_task_ex((core), (object), (func), (data), (size), MTLK_SLID)

int __MTLK_IFUNC mtlk_core_schedule_user_task_ex(struct nic* core,
                                                 mtlk_handle_t object,
                                                 mtlk_core_task_func_t func,
                                                 const void *data, size_t size,
                                                 mtlk_slid_t issuer_slid);

#define mtlk_core_schedule_user_task(core, object, func, data, size) \
  mtlk_core_schedule_user_task_ex((core), (object), (func), (data), (size), MTLK_SLID)


struct _mtlk_20_40_coexistence_sm * mtlk_core_get_coex_sm(mtlk_core_t *core);

BOOL mtlk_core_is_20_40_active(struct nic* core);
BOOL __MTLK_IFUNC mtlk_core_is_interfdet_enabled(struct nic *core);
void __MTLK_IFUNC mtlk_core_interfdet_enable(struct nic *core, BOOL enable_flag);
int  __MTLK_IFUNC mtlk_core_stop_lm(struct nic *core);
int __MTLK_IFUNC mtlk_core_set_tx_antennas(mtlk_core_t *core, uint8 num_tx_antennas);
void __MTLK_IFUNC mtlk_core_set_pm_enabled (mtlk_core_t *core, BOOL enabled);

/*\}*/
#endif //_MTLK_CORE_IFACE_H_

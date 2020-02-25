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

#ifdef MTCFG_PMCU_SUPPORT

#include "mtlkinc.h"
#include "mhi_umi.h"
#include "core.h"
#include "mtlk_coreui.h"
#include "mtlk_pcoc.h"

#ifdef  CONFIG_IFX_PMCU
  #include <asm-mips/ifx/ifx_pmcu.h>
#elif defined CONFIG_LTQ_CPU_FREQ
  #include <linux/cpufreq.h>
  #include <cpufreq/ltq_cpufreq.h>
  #include <cpufreq/ltq_cpufreq_pmcu_compatible.h>
#endif

#define LOG_LOCAL_GID   GID_PCOC
#define LOG_LOCAL_FID   1

/**********************************************************************
 * Local definitions
***********************************************************************/

#define LIMIT_LOWER_MIN   1
#define LIMIT_LOWER_MAX   1499999
#define LIMIT_UPPER_MIN   2
#define LIMIT_UPPER_MAX   1500000
#define INTERVAL_MIN      1
#define INTERVAL_MAX      100

typedef enum {
  PCOC_TRAFFIC_LOW  = 0,
  PCOC_TRAFFIC_HIGH = 1,
  PCOC_TRAFFIC_UNDEF
} _mtlk_pcoc_traffic_state_t;

struct _mtlk_pcoc_t
{
  mtlk_vap_handle_t          vap_handle;
  mtlk_handle_t              ta_crit_handle;
  mtlk_pcoc_params_t         params;
  BOOL                       is_enabled_adm;
  BOOL                       is_enabled_op;
  BOOL                       is_active;
  _mtlk_pcoc_traffic_state_t traffic_state;
#ifdef CONFIG_IFX_PMCU
  IFX_PMCU_STATE_t           power_state;
#elif defined CONFIG_LTQ_CPU_FREQ
  enum ltq_cpufreq_state     power_state;
#endif
  mtlk_osal_spinlock_t       lock;

  MTLK_DECLARE_INIT_STATUS;
};

static const mtlk_ability_id_t _pcoc_abilities[] = {
  MTLK_CORE_REQ_GET_PCOC_CFG,
  MTLK_CORE_REQ_SET_PCOC_CFG
};

/* PMCU part begin */

static mtlk_pcoc_t *_mtlk_g_pcoc_obj = NULL;

static void _mtlk_set_g_pcoc_obj(mtlk_pcoc_t *ptr)
{
  _mtlk_g_pcoc_obj = ptr;
}
static IFX_PMCU_RETURN_t _mtlk_pcoc_pmcu_cb_main ( IFX_PMCU_STATE_t newState,
                                                   BOOL change )
{
  IFX_PMCU_RETURN_t res_pmcu = IFX_PMCU_RETURN_SUCCESS;

  if(_mtlk_g_pcoc_obj == NULL)
  {
    /* This should never happen */
    MTLK_ASSERT(0);
    res_pmcu = IFX_PMCU_RETURN_DENIED;
    goto end;
  }

  mtlk_osal_lock_acquire(&_mtlk_g_pcoc_obj->lock);

  if(_mtlk_g_pcoc_obj->is_active == TRUE)
  {
    if((_mtlk_g_pcoc_obj->traffic_state == PCOC_TRAFFIC_LOW) ||
       (newState == IFX_PMCU_STATE_D0))
    {
      if(change)
      {
        _mtlk_g_pcoc_obj->power_state = newState;
      }
      ILOG2_V("IFX_PMCU_RETURN_SUCCESS");
    }
    else
    {
      res_pmcu = IFX_PMCU_RETURN_DENIED;
      ILOG2_V("IFX_PMCU_RETURN_DENIED");
    }
  }
  else
  {
    /* This should never happen */
    MTLK_ASSERT(0);
    res_pmcu = IFX_PMCU_RETURN_DENIED;
  }

  mtlk_osal_lock_release(&_mtlk_g_pcoc_obj->lock);

end:

  return res_pmcu;
}

/* Callback to be called before module changes it's state to new */
static IFX_PMCU_RETURN_t _mtlk_pcoc_pmcu_cb_pre ( IFX_PMCU_MODULE_t pmcuModule,
                                           IFX_PMCU_STATE_t newState,
                                           IFX_PMCU_STATE_t oldState)
{
  ILOG2_DD("PMCU pre CB module %d new state %d", pmcuModule, newState);

  return _mtlk_pcoc_pmcu_cb_main(newState, FALSE);
}

/* Callback used to change module's power state */
static IFX_PMCU_RETURN_t _mtlk_pcoc_pmcu_cb_state_change ( IFX_PMCU_STATE_t pmcuState )
{
  ILOG2_D("PMCU state_change CB new state %d", pmcuState);

  return _mtlk_pcoc_pmcu_cb_main(pmcuState, TRUE);
}

/* Callback to be called after module changes it's state to new state */
static IFX_PMCU_RETURN_t _mtlk_pcoc_pmcu_cb_post ( IFX_PMCU_MODULE_t pmcuModule,
                                            IFX_PMCU_STATE_t newState,
                                            IFX_PMCU_STATE_t oldState)
{
  ILOG2_DD("PMCU post CB module %d new state %d", pmcuModule, newState);

  return IFX_PMCU_RETURN_SUCCESS;
}

/* Optional: Callback used to get module's power state */
static IFX_PMCU_RETURN_t _mtlk_pcoc_pmcu_cb_state_get ( IFX_PMCU_STATE_t *pmcuState )
{
  IFX_PMCU_RETURN_t res_pmcu = IFX_PMCU_RETURN_SUCCESS;

  if(_mtlk_g_pcoc_obj == NULL)
  {
    /* This should never happen */
    MTLK_ASSERT(0);
    res_pmcu = IFX_PMCU_RETURN_NOACTIVITY;
    goto end;
  }

  mtlk_osal_lock_acquire(&_mtlk_g_pcoc_obj->lock);

  if(_mtlk_g_pcoc_obj->is_active == TRUE)
  {
    *pmcuState = _mtlk_g_pcoc_obj->power_state;
  }
  else
  {
    /* This should never happen */
    MTLK_ASSERT(0);
    res_pmcu = IFX_PMCU_RETURN_NOACTIVITY;
  }

  mtlk_osal_lock_release(&_mtlk_g_pcoc_obj->lock);

end:

  ILOG2_DD("PMCU state_get CB state %d res_pmcu %d", *pmcuState, res_pmcu);

  return res_pmcu;
}

#ifdef CONFIG_IFX_PMCU

/* Callback used to enable/disable the power features of the module */
static IFX_PMCU_RETURN_t _mtlk_pcoc_pmcu_cb_feature_switch (IFX_PMCU_PWR_STATE_ENA_t pmcuPwrStateEna )
{
  return IFX_PMCU_RETURN_NOACTIVITY;
}

static IFX_PMCU_MODULE_DEP_t _mtlk_pcoc_pmcu_dep_list =
{
  1,
  {
    {
      IFX_PMCU_MODULE_CPU,
      0,
      IFX_PMCU_STATE_D0,
      IFX_PMCU_STATE_D0D3,
      IFX_PMCU_STATE_D0D3,
      IFX_PMCU_STATE_D0D3
    }
  }
};

static IFX_PMCU_REGISTER_t _mtlk_pcoc_pmcu_register_data = {
  .pmcuModule                  = IFX_PMCU_MODULE_WLAN,
  .pmcuModuleNr                = 0,
  .pmcuModuleDep               = &_mtlk_pcoc_pmcu_dep_list,
  .pre                         = _mtlk_pcoc_pmcu_cb_pre,
  .ifx_pmcu_state_change       = _mtlk_pcoc_pmcu_cb_state_change,
  .post                        = _mtlk_pcoc_pmcu_cb_post,
  .ifx_pmcu_state_get          = _mtlk_pcoc_pmcu_cb_state_get,
  .ifx_pmcu_pwr_feature_switch = _mtlk_pcoc_pmcu_cb_feature_switch
};

static int
_mtlk_pcoc_pmcu_register (void)
{
  int res = MTLK_ERR_OK;
  int res_pmcu;

  res_pmcu = ifx_pmcu_register(&_mtlk_pcoc_pmcu_register_data);
  if(res_pmcu != IFX_PMCU_RETURN_SUCCESS)
  {
    ELOG_D("PMCU registration failed. Error code is %d.", res_pmcu);
    res = MTLK_ERR_UNKNOWN;
  }

  return res;
}

static int
_mtlk_pcoc_pmcu_unregister (void)
{
  int res = MTLK_ERR_OK;
  int res_pmcu;

  res_pmcu = ifx_pmcu_unregister(&_mtlk_pcoc_pmcu_register_data);
  if(res_pmcu != IFX_PMCU_RETURN_SUCCESS)
  {
    ELOG_D("PMCU unregistration failed. Error code is %d.", res_pmcu);
    res = MTLK_ERR_UNKNOWN;
  }

  return res;
}

#elif defined CONFIG_LTQ_CPU_FREQ

struct ltq_cpufreq_module_info mtlk_pcoc_feature_fss = {
  .name                            = "WLAN frequency scaling support",
  .pmcuModule                      = LTQ_CPUFREQ_MODULE_WLAN,
  .pmcuModuleNr                    = 0,
  .powerFeatureStat                = 1,
  .ltq_cpufreq_state_get           = _mtlk_pcoc_pmcu_cb_state_get,
  .ltq_cpufreq_pwr_feature_switch  = NULL,
};

/* keep track of frequency transitions */
static int
mtlk_pcoc_cpufreq_notifier(struct notifier_block *nb, unsigned long val, void *data)
{
  struct cpufreq_freqs *freq = data;
  enum ltq_cpufreq_state new_State, old_State;
  int ret;

  new_State = ltq_cpufreq_get_ps_from_khz(freq->new);
  if(new_State == LTQ_CPUFREQ_PS_UNDEF) {
    return NOTIFY_STOP_MASK | (LTQ_CPUFREQ_MODULE_WLAN<<4);
  }
  old_State = ltq_cpufreq_get_ps_from_khz(freq->old);
  if(old_State == LTQ_CPUFREQ_PS_UNDEF) {
    return NOTIFY_STOP_MASK | (LTQ_CPUFREQ_MODULE_WLAN<<4);
  }
  if (val == CPUFREQ_PRECHANGE){
    ret = _mtlk_pcoc_pmcu_cb_pre(LTQ_CPUFREQ_MODULE_WLAN, new_State, old_State);
    if (ret < 0) {
      return NOTIFY_STOP_MASK | (LTQ_CPUFREQ_MODULE_WLAN<<4);
    }
    ret = _mtlk_pcoc_pmcu_cb_state_change(new_State);
    if (ret < 0) {
      return NOTIFY_STOP_MASK | (LTQ_CPUFREQ_MODULE_WLAN<<4);
    }
  } else if (val == CPUFREQ_POSTCHANGE){
    ret = _mtlk_pcoc_pmcu_cb_post(LTQ_CPUFREQ_MODULE_WLAN, new_State, old_State);
    if (ret < 0) {
      return NOTIFY_STOP_MASK | (LTQ_CPUFREQ_MODULE_WLAN<<4);
    }
  }else{
    return NOTIFY_OK | (LTQ_CPUFREQ_MODULE_WLAN<<4);
  }
  return NOTIFY_OK | (LTQ_CPUFREQ_MODULE_WLAN<<4);
}

static struct notifier_block mtlk_pcoc_cpufreq_notifier_block = {
  .notifier_call = mtlk_pcoc_cpufreq_notifier
};

static int
_mtlk_pcoc_pmcu_register (void)
{
  int res_cpufreq;
  struct ltq_cpufreq* mtlk_cpufreq_p;

  res_cpufreq = cpufreq_register_notifier(&mtlk_pcoc_cpufreq_notifier_block,
                                            CPUFREQ_TRANSITION_NOTIFIER);
  mtlk_cpufreq_p = ltq_cpufreq_get();
  if((res_cpufreq < 0) || (mtlk_cpufreq_p == NULL))
  {
    ELOG_D("CPUFREQ registration failed. Error code is %d.", res_cpufreq);
    return MTLK_ERR_UNKNOWN;
  }
  list_add_tail(&mtlk_pcoc_feature_fss.list, &mtlk_cpufreq_p->list_head_module);
  return MTLK_ERR_OK;
}

static int
_mtlk_pcoc_pmcu_unregister (void)
{
  if(cpufreq_unregister_notifier(&mtlk_pcoc_cpufreq_notifier_block,
                                 CPUFREQ_TRANSITION_NOTIFIER)) {
    ELOG_V("CPUFREQ unregistration failed.");
    return MTLK_ERR_UNKNOWN;
  }
  list_del(&mtlk_pcoc_feature_fss.list);
  return MTLK_ERR_OK;
}

#endif

static int
_mtlk_pcoc_pmcu_request_state ( IFX_PMCU_STATE_t new_state )
{
  int res = MTLK_ERR_OK;
  int res_pmcu;

  ILOG1_D("Requesting PMCU power mode %d", new_state);
#ifdef CONFIG_IFX_PMCU
  res_pmcu = ifx_pmcu_state_req(IFX_PMCU_MODULE_WLAN, 0, new_state);
#elif defined CONFIG_LTQ_CPU_FREQ
  res_pmcu = ltq_cpufreq_state_req(LTQ_CPUFREQ_MODULE_WLAN, 0, new_state);
#endif
  if(res_pmcu != IFX_PMCU_RETURN_SUCCESS)
  {
    ELOG_DD("PMCU mode request failed. New state is %d, error code is %d.",
            new_state,
            res_pmcu);
    res = MTLK_ERR_UNKNOWN;
  }

  return res;
}

/* PMCU part end */

static int
_mtlk_pcoc_set_interval (mtlk_pcoc_t *pcoc_obj)
{
  int res = MTLK_ERR_OK;
  ta_crit_pcoc_cfg_t ta_crit_cfg;

  MTLK_ASSERT(NULL != pcoc_obj);

  ta_crit_cfg.interval = pcoc_obj->params.interval;

  res = mtlk_ta_pcoc_cfg(mtlk_vap_get_ta(pcoc_obj->vap_handle), &ta_crit_cfg);

  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: Wrong PCoC configuration: interval %d doesn't fit for TA",
            mtlk_vap_get_oid(pcoc_obj->vap_handle), ta_crit_cfg.interval);
  }

  return res;
}

static void
_mtlk_pcoc_ta_process (mtlk_pcoc_t *pcoc_obj, uint32 sum_tp)
{
  _mtlk_pcoc_traffic_state_t current_state, new_state;

  MTLK_ASSERT(NULL != pcoc_obj);

  mtlk_osal_lock_acquire(&pcoc_obj->lock);

  current_state = pcoc_obj->traffic_state;
  new_state = current_state;
  switch (current_state) {
    case PCOC_TRAFFIC_LOW:
      if (sum_tp > pcoc_obj->params.limit_upper) {
        ILOG2_D("CID-%04x: PCoC state -> HIGH",
          mtlk_vap_get_oid(pcoc_obj->vap_handle));

        new_state = PCOC_TRAFFIC_HIGH;
        _mtlk_pcoc_pmcu_request_state(IFX_PMCU_STATE_D0);
      }
      break;
    case PCOC_TRAFFIC_HIGH:
      if (sum_tp < pcoc_obj->params.limit_lower) {
        ILOG2_D("CID-%04x: PCoC state -> LOW",
          mtlk_vap_get_oid(pcoc_obj->vap_handle));

        new_state = PCOC_TRAFFIC_LOW;
#ifdef CONFIG_LTQ_CPU_FREQ
        _mtlk_pcoc_pmcu_request_state(IFX_PMCU_STATE_D3);
#endif
      }
      break;
    default:
      /* This should never happen */
      MTLK_ASSERT(0);
  }
  pcoc_obj->traffic_state = new_state;

  mtlk_osal_lock_release(&pcoc_obj->lock);
}

/* Timer entry point. Called from serializer tough. */
static void
_mtlk_pcoc_ta_callback (mtlk_handle_t clb_ctx, mtlk_handle_t clb_crit)
{
  mtlk_pcoc_t *pcoc_obj;
  uint32 sum_tp;

  pcoc_obj = HANDLE_T_PTR(mtlk_pcoc_t, clb_ctx);
  MTLK_ASSERT(NULL != pcoc_obj);
  sum_tp = *((uint32 *)clb_crit);

  _mtlk_pcoc_ta_process(pcoc_obj, sum_tp);
}

static int
_mtlk_pcoc_start_or_stop (mtlk_pcoc_t *pcoc_obj)
{
  int res = MTLK_ERR_OK;

  if (pcoc_obj->is_enabled_adm && pcoc_obj->is_enabled_op) {
    if (!pcoc_obj->is_active) {

      ILOG2_D("CID-%04x: PCoC activation",
        mtlk_vap_get_oid(pcoc_obj->vap_handle));

      mtlk_osal_lock_acquire(&pcoc_obj->lock);
      pcoc_obj->power_state = IFX_PMCU_STATE_D0;
      pcoc_obj->traffic_state = PCOC_TRAFFIC_HIGH;
      pcoc_obj->is_active = TRUE;
      mtlk_osal_lock_release(&pcoc_obj->lock);

      _mtlk_pcoc_pmcu_register();
      _mtlk_pcoc_pmcu_request_state(IFX_PMCU_STATE_D0);

      mtlk_ta_crit_start(pcoc_obj->ta_crit_handle);
    }
  }
  else {
    if (pcoc_obj->is_active) {

      ILOG2_D("CID-%04x: PCoC deactivation",
        mtlk_vap_get_oid(pcoc_obj->vap_handle));

      mtlk_ta_crit_stop(pcoc_obj->ta_crit_handle);
#ifdef CONFIG_LTQ_CPU_FREQ
      _mtlk_pcoc_pmcu_request_state(IFX_PMCU_STATE_D3);
#endif
      _mtlk_pcoc_pmcu_unregister();

      mtlk_osal_lock_acquire(&pcoc_obj->lock);
      pcoc_obj->power_state = IFX_PMCU_STATE_INVALID;
      pcoc_obj->traffic_state = PCOC_TRAFFIC_UNDEF;
      pcoc_obj->is_active = FALSE;
      mtlk_osal_lock_release(&pcoc_obj->lock);
    }
  }

  return res;
}

MTLK_INIT_STEPS_LIST_BEGIN(pcoc)
  MTLK_INIT_STEPS_LIST_ENTRY(pcoc, LOCK_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(pcoc, CONFIG_TA)
  MTLK_INIT_STEPS_LIST_ENTRY(pcoc, REG_CALLBACK)
  MTLK_INIT_STEPS_LIST_ENTRY(pcoc, REG_ABILITIES)
  MTLK_INIT_STEPS_LIST_ENTRY(pcoc, EN_ABILITIES)
  MTLK_INIT_STEPS_LIST_ENTRY(pcoc, SET_GLOBAL_OBJ)
MTLK_INIT_INNER_STEPS_BEGIN(pcoc)
MTLK_INIT_STEPS_LIST_END(pcoc);

static void
_mtlk_pcoc_cleanup (mtlk_pcoc_t *pcoc_obj)
{
  MTLK_ASSERT(NULL != pcoc_obj);
  MTLK_ASSERT(NULL != _mtlk_g_pcoc_obj);

  MTLK_CLEANUP_BEGIN(pcoc, MTLK_OBJ_PTR(pcoc_obj))
    MTLK_CLEANUP_STEP(pcoc, SET_GLOBAL_OBJ, MTLK_OBJ_PTR(pcoc_obj),
                      _mtlk_set_g_pcoc_obj, (NULL));
    MTLK_CLEANUP_STEP(pcoc, EN_ABILITIES, MTLK_OBJ_PTR(pcoc_obj),
                      mtlk_abmgr_disable_ability_set,
                      (mtlk_vap_get_abmgr(pcoc_obj->vap_handle),
                      _pcoc_abilities, ARRAY_SIZE(_pcoc_abilities)));
    MTLK_CLEANUP_STEP(pcoc, REG_ABILITIES, MTLK_OBJ_PTR(pcoc_obj),
                      mtlk_abmgr_unregister_ability_set,
                      (mtlk_vap_get_abmgr(pcoc_obj->vap_handle),
                      _pcoc_abilities, ARRAY_SIZE(_pcoc_abilities)));
    MTLK_CLEANUP_STEP(pcoc, REG_CALLBACK, MTLK_OBJ_PTR(pcoc_obj),
                      mtlk_ta_crit_unregister, (pcoc_obj->ta_crit_handle));
    MTLK_CLEANUP_STEP(pcoc, CONFIG_TA, MTLK_OBJ_PTR(pcoc_obj),
                      MTLK_NOACTION, ());
    MTLK_CLEANUP_STEP(pcoc, LOCK_INIT, MTLK_OBJ_PTR(pcoc_obj),
                      mtlk_osal_lock_cleanup, (&pcoc_obj->lock));
  MTLK_CLEANUP_END(pcoc, MTLK_OBJ_PTR(pcoc_obj));
}

static int
_mtlk_pcoc_init (mtlk_pcoc_t *pcoc_obj, const mtlk_pcoc_cfg_t *cfg)
{
  ta_crit_pcoc_cfg_t ta_crit_cfg;
  uint32 res;

  MTLK_ASSERT(NULL != pcoc_obj);
  MTLK_ASSERT(NULL == _mtlk_g_pcoc_obj);

  pcoc_obj->vap_handle = cfg->vap_handle;
  pcoc_obj->params = cfg->default_params;
  pcoc_obj->traffic_state = PCOC_TRAFFIC_UNDEF;
  pcoc_obj->power_state = IFX_PMCU_STATE_INVALID;
  pcoc_obj->is_enabled_adm = TRUE;
  ta_crit_cfg.interval = cfg->default_params.interval;

  MTLK_INIT_TRY(pcoc, MTLK_OBJ_PTR(pcoc_obj))
    MTLK_INIT_STEP(pcoc, LOCK_INIT, MTLK_OBJ_PTR(pcoc_obj),
                   mtlk_osal_lock_init, (&pcoc_obj->lock));
    MTLK_INIT_STEP_EX(pcoc, CONFIG_TA, MTLK_OBJ_PTR(pcoc_obj),
                      mtlk_ta_pcoc_cfg,
                      (mtlk_vap_get_ta(pcoc_obj->vap_handle), &ta_crit_cfg),
                      res,
                      res == MTLK_ERR_OK, MTLK_ERR_PARAMS);
    MTLK_INIT_STEP_EX(pcoc, REG_CALLBACK, MTLK_OBJ_PTR(pcoc_obj),
                      mtlk_ta_crit_register,
                      (mtlk_vap_get_ta(pcoc_obj->vap_handle),
                       TA_CRIT_ID_PCOC,
                       _mtlk_pcoc_ta_callback,
                       HANDLE_T(pcoc_obj)),
                       pcoc_obj->ta_crit_handle,
                       pcoc_obj->ta_crit_handle != MTLK_INVALID_HANDLE, MTLK_ERR_NO_MEM);
    MTLK_INIT_STEP(pcoc, REG_ABILITIES, MTLK_OBJ_PTR(pcoc_obj),
                   mtlk_abmgr_register_ability_set,
                   (mtlk_vap_get_abmgr(pcoc_obj->vap_handle),
                    _pcoc_abilities, ARRAY_SIZE(_pcoc_abilities)));
    MTLK_INIT_STEP_VOID(pcoc, EN_ABILITIES, MTLK_OBJ_PTR(pcoc_obj),
                        mtlk_abmgr_enable_ability_set,
                        (mtlk_vap_get_abmgr(pcoc_obj->vap_handle),
                         _pcoc_abilities, ARRAY_SIZE(_pcoc_abilities)));
    MTLK_INIT_STEP_VOID(pcoc, SET_GLOBAL_OBJ, MTLK_OBJ_PTR(pcoc_obj),
                        _mtlk_set_g_pcoc_obj, (pcoc_obj));
  MTLK_INIT_FINALLY(pcoc, MTLK_OBJ_PTR(pcoc_obj))
  MTLK_INIT_RETURN(pcoc, MTLK_OBJ_PTR(pcoc_obj), _mtlk_pcoc_cleanup, (pcoc_obj));
}

/*****************************************************************************
* Function implementation
******************************************************************************/
mtlk_pcoc_t* __MTLK_IFUNC
mtlk_pcoc_create (const mtlk_pcoc_cfg_t *cfg)
{
  mtlk_pcoc_t *pcoc_obj;

  ILOG2_D("CID-%04x: PCoC initialization",
    mtlk_vap_get_oid(cfg->vap_handle));

  pcoc_obj = mtlk_osal_mem_alloc(sizeof(mtlk_pcoc_t), MTLK_MEM_TAG_DFS);

  if (NULL != pcoc_obj)
  {
    memset(pcoc_obj, 0, sizeof(mtlk_pcoc_t));
    if (MTLK_ERR_OK != _mtlk_pcoc_init(pcoc_obj, cfg)) {
      mtlk_osal_mem_free(pcoc_obj);
      pcoc_obj = NULL;
    }
  }

  return pcoc_obj;
}

void __MTLK_IFUNC
mtlk_pcoc_delete (mtlk_pcoc_t *pcoc_obj)
{
  MTLK_ASSERT(NULL != pcoc_obj);

  ILOG2_D("CID-%04x: PCoC delete",
    mtlk_vap_get_oid(pcoc_obj->vap_handle));

  _mtlk_pcoc_cleanup(pcoc_obj);

  mtlk_osal_mem_free(pcoc_obj);
}

mtlk_pcoc_params_t * __MTLK_IFUNC
mtlk_pcoc_get_params (mtlk_pcoc_t *pcoc_obj)
{
  MTLK_ASSERT(NULL != pcoc_obj);

  return &pcoc_obj->params;
}

int __MTLK_IFUNC
mtlk_pcoc_set_params (mtlk_pcoc_t *pcoc_obj, const mtlk_pcoc_params_t *params)
{
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(NULL != pcoc_obj);

  ILOG2_D("CID-%04x: Setting PCoC parameters",
    mtlk_vap_get_oid(pcoc_obj->vap_handle));

  if ((params->interval < INTERVAL_MIN) || (params->interval > INTERVAL_MAX)) {
    ELOG_DDD("CID-%04x: Wrong PCoC configuration. Interval must be in range %d..%d",
           mtlk_vap_get_oid(pcoc_obj->vap_handle),
           INTERVAL_MIN,
           INTERVAL_MAX
          );
    res = MTLK_ERR_PARAMS;
    goto end;
  }
  else if ((params->limit_lower < LIMIT_LOWER_MIN) ||
           (params->limit_lower == LIMIT_LOWER_MAX)) {
    ELOG_DDD("CID-%04x: Wrong PCoC configuration. Lower limit must be %d..%d",
           mtlk_vap_get_oid(pcoc_obj->vap_handle),
           LIMIT_LOWER_MIN,
           LIMIT_LOWER_MAX
          );
    res = MTLK_ERR_PARAMS;
    goto end;
  }
  else if ((params->limit_upper < LIMIT_UPPER_MIN) ||
           (params->limit_upper == LIMIT_UPPER_MAX)) {
    ELOG_DDD("CID-%04x: Wrong PCoC configuration. Upper limit must be %d..%d",
           mtlk_vap_get_oid(pcoc_obj->vap_handle),
           LIMIT_UPPER_MIN,
           LIMIT_UPPER_MAX
          );
    res = MTLK_ERR_PARAMS;
    goto end;
  }
  else if ((params->limit_lower >= params->limit_upper)) {
    ELOG_D("CID-%04x: Wrong PCoC configuration. Lower threshold must be less than upper.",
           mtlk_vap_get_oid(pcoc_obj->vap_handle));
    res = MTLK_ERR_PARAMS;
    goto end;
  }

  mtlk_osal_lock_acquire(&pcoc_obj->lock);

  memcpy(&pcoc_obj->params, params, sizeof(mtlk_pcoc_params_t));
  res = _mtlk_pcoc_set_interval(pcoc_obj);
  if (MTLK_ERR_OK != res) {
    res = MTLK_ERR_PARAMS;
  }

  mtlk_osal_lock_release(&pcoc_obj->lock);

end:

  return res;
}

BOOL __MTLK_IFUNC
mtlk_pcoc_is_enabled_adm (mtlk_pcoc_t *pcoc_obj)
{
  MTLK_ASSERT(NULL != pcoc_obj);

  return pcoc_obj->is_enabled_adm;
}

int __MTLK_IFUNC
mtlk_pcoc_set_enabled_adm (mtlk_pcoc_t *pcoc_obj, const BOOL is_enabled)
{
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(NULL != pcoc_obj);

  ILOG2_DD("CID-%04x: PCoC enabled administratively: %d",
    mtlk_vap_get_oid(pcoc_obj->vap_handle), is_enabled);

  pcoc_obj->is_enabled_adm = is_enabled;
  res = _mtlk_pcoc_start_or_stop(pcoc_obj);

  return res;
}

int __MTLK_IFUNC
mtlk_pcoc_set_enabled_op (mtlk_pcoc_t *pcoc_obj, const BOOL is_enabled)
{
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(NULL != pcoc_obj);

  ILOG2_DD("CID-%04x: PCoC enabled operationally: %d",
    mtlk_vap_get_oid(pcoc_obj->vap_handle), is_enabled);

  pcoc_obj->is_enabled_op = is_enabled;
  res = _mtlk_pcoc_start_or_stop(pcoc_obj);

  return res;
}

BOOL __MTLK_IFUNC
mtlk_pcoc_is_active(mtlk_pcoc_t *pcoc_obj)
{
  BOOL res;

  MTLK_ASSERT(NULL != pcoc_obj);

  mtlk_osal_lock_acquire(&pcoc_obj->lock);
  res = pcoc_obj->is_active;
  mtlk_osal_lock_release(&pcoc_obj->lock);

  return res;
}

uint8 __MTLK_IFUNC
mtlk_pcoc_get_traffic_state(mtlk_pcoc_t *pcoc_obj)
{
  uint8 res;

  MTLK_ASSERT(NULL != pcoc_obj);

  mtlk_osal_lock_acquire(&pcoc_obj->lock);
  res = (uint8) pcoc_obj->traffic_state;
  mtlk_osal_lock_release(&pcoc_obj->lock);

  return res;
}

int __MTLK_IFUNC
mtlk_pcoc_test_pmcu_cb(mtlk_pcoc_t *pcoc_obj, uint32 pmcu_debug)
{
  int res = MTLK_ERR_OK;
  IFX_PMCU_STATE_t pmcu_state;
  IFX_PMCU_RETURN_t pmcu_res;

  switch(pmcu_debug)
  {
    case 0:
      pmcu_state = IFX_PMCU_STATE_D0;
      break;
    case 1:
      pmcu_state = IFX_PMCU_STATE_D1;
      break;
    case 2:
      pmcu_state = IFX_PMCU_STATE_D2;
      break;
    case 3:
      pmcu_state = IFX_PMCU_STATE_D3;
      break;
    default:
      res = MTLK_ERR_PARAMS;
      goto end;
  }

  pmcu_res = _mtlk_pcoc_pmcu_cb_main(pmcu_state, FALSE);

  if(pmcu_res != IFX_PMCU_RETURN_SUCCESS)
  {
    res = MTLK_ERR_PROHIB;
  }

end:

  return res;
}

void __MTLK_IFUNC
mtlk_pcoc_on_rcvry_isol (mtlk_pcoc_t *pcoc_obj)
{
  if (pcoc_obj) {
    (void)mtlk_pcoc_set_enabled_op(pcoc_obj, FALSE);
  }
}

int __MTLK_IFUNC
mtlk_pcoc_on_rcvry_configure (mtlk_pcoc_t *pcoc_obj)
{
  int res = MTLK_ERR_OK;

  if (pcoc_obj) {
    res = mtlk_pcoc_set_enabled_op(pcoc_obj, TRUE);
  }

  return res;
}

#endif /*MTCFG_PMCU_SUPPORT*/

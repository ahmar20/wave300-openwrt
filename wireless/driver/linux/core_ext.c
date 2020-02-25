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
 * Core functionality (extension)
 *
 */

#define LOG_LOCAL_GID   GID_CORE

#undef  LOG_LOCAL_FID
#define LOG_LOCAL_FID   5

/* 802.11w begin */

static int
_mtlk_core_set_dot11w_pmf (mtlk_core_t *core, mtlk_pdb_id_t param_id, uint32 param_value)
{
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(core != NULL);

  if (param_value > 1) {
    ELOG_D("Incorrect value: %d. Must be 0 or 1.", param_value);
    res = MTLK_ERR_PARAMS;
    goto end;
  }

  MTLK_CORE_PDB_SET_INT(core, param_id, param_value);

end:

  return res;
}

static int
_mtlk_core_set_dot11w_saq_tmout (mtlk_core_t *core, mtlk_pdb_id_t param_id, uint32 param_value, uint16 obj_id)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *core_master = NULL;

  MTLK_ASSERT(core != NULL);

  core_master = mtlk_core_get_master(core);
  MTLK_ASSERT(core_master != NULL);

  if (param_value == 0) {
    ELOG_D("Incorrect value: %d. Must be 1..4294967295", param_value);
    res = MTLK_ERR_PARAMS;
    goto end;
  }

  res = mtlk_set_mib_value_uint32(mtlk_vap_get_txmm(core_master->vap_handle), obj_id, param_value);

  if(res != MTLK_ERR_OK)
  {
    ELOG_DD("Failed to set MIB id %d value %d", obj_id, param_value);
    goto end;
  }

  MTLK_CORE_PDB_SET_INT(core_master, param_id, param_value);

end:

  return res;
}

static int
_mtlk_core_set_dot11w_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  uint32 _dot11w_cfg_size;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_dot11w_cfg_t *_dot11w_cfg = NULL;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  _dot11w_cfg = mtlk_clpb_enum_get_next(clpb, &_dot11w_cfg_size);
  MTLK_ASSERT(NULL != _dot11w_cfg);
  MTLK_ASSERT(sizeof(*_dot11w_cfg) == _dot11w_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_CHECK_ITEM_AND_CALL(_dot11w_cfg, pmf_activated, _mtlk_core_set_dot11w_pmf,
                                 (core, PARAM_DB_CORE_PMF_ACTIVATED, _dot11w_cfg->pmf_activated), res);
    MTLK_CFG_CHECK_ITEM_AND_CALL(_dot11w_cfg, pmf_required, _mtlk_core_set_dot11w_pmf,
                                 (core, PARAM_DB_CORE_PMF_REQUIRED, _dot11w_cfg->pmf_required), res);
    MTLK_CFG_CHECK_ITEM_AND_CALL(_dot11w_cfg, saq_retr_tmout, _mtlk_core_set_dot11w_saq_tmout,
                                 (core, PARAM_DB_CORE_SAQ_RETR_TMOUT,
                                  _dot11w_cfg->saq_retr_tmout, MIB_SAQUERY_RETRY_TIMEOUT), res);
    MTLK_CFG_CHECK_ITEM_AND_CALL(_dot11w_cfg, saq_max_tmout, _mtlk_core_set_dot11w_saq_tmout,
                                 (core, PARAM_DB_CORE_SAQ_MAX_TMOUT,
                                  _dot11w_cfg->saq_max_tmout, MIB_SAQUERY_MAX_TIMEOUT), res);
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int
_mtlk_core_get_dot11w_cfg (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_dot11w_cfg_t _dot11w_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_core_t *core_master = mtlk_core_get_master(core);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  mtlk_txmm_t *txmm;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  txmm = mtlk_vap_get_txmm(core_master->vap_handle);
  memset(&_dot11w_cfg, 0, sizeof(_dot11w_cfg));

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_SET_ITEM(&_dot11w_cfg, pmf_activated,
                      MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_PMF_ACTIVATED));
    MTLK_CFG_SET_ITEM(&_dot11w_cfg, pmf_required,
                      MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_PMF_REQUIRED));
    MTLK_CFG_SET_ITEM_BY_FUNC(&_dot11w_cfg, saq_retr_tmout, mtlk_get_mib_value_uint32,
                      (txmm, MIB_SAQUERY_RETRY_TIMEOUT, &_dot11w_cfg.saq_retr_tmout), res);
    MTLK_CFG_SET_ITEM_BY_FUNC(&_dot11w_cfg, saq_max_tmout,  mtlk_get_mib_value_uint32,
                      (txmm, MIB_SAQUERY_MAX_TIMEOUT,   &_dot11w_cfg.saq_max_tmout),  res);
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &_dot11w_cfg, sizeof(_dot11w_cfg));
  }

  return res;
}

/* 802.11w end */

static int
_mtlk_core_set_recovery_cfg (mtlk_handle_t hcore, const void *data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_rcvry_cfg_t *_rcvry_cfg = NULL;
  uint32 _rcvry_cfg_size;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **)data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  _rcvry_cfg = mtlk_clpb_enum_get_next(clpb, &_rcvry_cfg_size);
  MTLK_ASSERT(NULL != _rcvry_cfg);
  MTLK_ASSERT(sizeof(*_rcvry_cfg) == _rcvry_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_CHECK_ITEM_AND_CALL(_rcvry_cfg, recovery_cfg, mtlk_rcvry_set_cfg,
                                 (mtlk_vap_manager_get_rcvry(mtlk_vap_get_manager(core->vap_handle)),
                                  &_rcvry_cfg->recovery_cfg), res);
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));
}

static int
_mtlk_core_get_recovery_cfg (mtlk_handle_t hcore, const void *data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_rcvry_cfg_t _rcvry_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&_rcvry_cfg, 0, sizeof(_rcvry_cfg));

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_SET_ITEM_BY_FUNC(&_rcvry_cfg, recovery_cfg, mtlk_rcvry_get_cfg,
                              (mtlk_vap_manager_get_rcvry(mtlk_vap_get_manager(core->vap_handle)),
                               &_rcvry_cfg.recovery_cfg), res);
  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &_rcvry_cfg, sizeof(_rcvry_cfg));
  }
  return res;
}

static int
_mtlk_core_stop_lm(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);

  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), &res);
  if (!man_entry) {
    ELOG_D("CID-%04x: Can't stop lower MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NO_MEM;
    goto end;
  }

  man_entry->id           = UM_LM_STOP_REQ;
  man_entry->payload_size = 0;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Can't stop lower MAC, timed-out", mtlk_vap_get_oid(core->vap_handle));
  }

  mtlk_txmm_msg_cleanup(&man_msg);
end:
  return res;
}

static int
_mtlk_core_mac_calibrate(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);

  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), &res);
  if (!man_entry) {
    ELOG_D("CID-%04x: Can't calibrate due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NO_MEM;
    goto end;
  }

  man_entry->id           = UM_PER_CHANNEL_CALIBR_REQ;
  man_entry->payload_size = 0;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Can't calibrate, timed-out", mtlk_vap_get_oid(core->vap_handle));
  }

  mtlk_txmm_msg_cleanup(&man_msg);
end:
  return res;
}

static int
_mtlk_core_get_iw_generic(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  UMI_GENERIC_MAC_REQUEST *req_df_cfg = NULL;
  UMI_GENERIC_MAC_REQUEST *pdata = NULL;
  uint32 req_df_cfg_size;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), &res);
  if (!man_entry) {
    ELOG_D("CID-%04x: Can't send request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NO_MEM;
    goto end;
  }

  req_df_cfg = mtlk_clpb_enum_get_next(clpb, &req_df_cfg_size);

  MTLK_ASSERT(NULL != req_df_cfg);
  MTLK_ASSERT(sizeof(*req_df_cfg) == req_df_cfg_size);

  pdata = (UMI_GENERIC_MAC_REQUEST*)man_entry->payload;
  man_entry->id           = UM_MAN_GENERIC_MAC_REQ;
  man_entry->payload_size = sizeof(*pdata);

  pdata->opcode=  cpu_to_le32(req_df_cfg->opcode);
  pdata->size=  cpu_to_le32(req_df_cfg->size);
  pdata->action=  cpu_to_le32(req_df_cfg->action);
  pdata->res0=  cpu_to_le32(req_df_cfg->res0);
  pdata->res1=  cpu_to_le32(req_df_cfg->res1);
  pdata->res2=  cpu_to_le32(req_df_cfg->res2);
  pdata->retStatus=  cpu_to_le32(req_df_cfg->retStatus);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Can't send generic request to MAC, timed-out", mtlk_vap_get_oid(core->vap_handle));
    goto err_send;
  }

  /* Send response to DF user */
  pdata->opcode=  cpu_to_le32(pdata->opcode);
  pdata->size=  cpu_to_le32(pdata->size);
  pdata->action=  cpu_to_le32(pdata->action);
  pdata->res0=  cpu_to_le32(pdata->res0);
  pdata->res1=  cpu_to_le32(pdata->res1);
  pdata->res2=  cpu_to_le32(pdata->res2);
  pdata->retStatus=  cpu_to_le32(pdata->retStatus);

  res = mtlk_clpb_push(clpb, pdata, sizeof(*pdata));

err_send:
  mtlk_txmm_msg_cleanup(&man_msg);
end:
  return res;
}

static int
_mtlk_core_set_leds_cfg(mtlk_core_t *core, UMI_SET_LED *leds_cfg)
{
  int res = MTLK_ERR_OK;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry = NULL;
  UMI_SET_LED* pdata = NULL;

  MTLK_ASSERT(NULL != leds_cfg);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), &res);
  if (!man_entry) {
    ELOG_D("CID-%04x: No man entry to set MAC leds cfg (GPIO)", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NO_MEM;
    goto end;
  }

  pdata = (UMI_SET_LED*)man_entry->payload;
  man_entry->id           = UM_MAN_SET_LED_REQ;
  man_entry->payload_size = sizeof(*leds_cfg);

  pdata->u8BasebLed = leds_cfg->u8BasebLed;
  pdata->u8LedStatus = leds_cfg->u8LedStatus;

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: MAC Control GPIO set timeout", mtlk_vap_get_oid(core->vap_handle));
  }

  mtlk_txmm_msg_cleanup(&man_msg);

end:
  return res;
}

static int
_mtlk_core_ctrl_mac_gpio(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  UMI_SET_LED *leds_cfg = NULL;
  uint32 leds_cfg_size;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  leds_cfg = mtlk_clpb_enum_get_next(clpb, &leds_cfg_size);

  MTLK_ASSERT(NULL != leds_cfg);
  MTLK_ASSERT(sizeof(*leds_cfg) == leds_cfg_size);

  return _mtlk_core_set_leds_cfg(core, leds_cfg);
}

static int
_mtlk_core_gen_dataex_get_connection_stats (mtlk_core_t *core,
                                            WE_GEN_DATAEX_REQUEST *preq,
                                            mtlk_clpb_t *clpb)
{
  int res = MTLK_ERR_OK;
  WE_GEN_DATAEX_CONNECTION_STATUS dataex_conn_status;
  WE_GEN_DATAEX_RESPONSE resp;
  int nof_connected;
  const sta_entry *sta = NULL;
  mtlk_stadb_iterator_t iter;

  memset(&resp, 0, sizeof(resp));
  memset(&dataex_conn_status, 0, sizeof(dataex_conn_status));

  resp.ver = WE_GEN_DATAEX_PROTO_VER;
  resp.status = WE_GEN_DATAEX_SUCCESS;
  resp.datalen = sizeof(WE_GEN_DATAEX_CONNECTION_STATUS);

  if (preq->datalen < resp.datalen) {
    return MTLK_ERR_NO_MEM;
  }

  memset(&dataex_conn_status, 0, sizeof(WE_GEN_DATAEX_CONNECTION_STATUS));

  nof_connected = 0;

  sta = mtlk_stadb_iterate_first(&core->slow_ctx->stadb, &iter);
  if (sta) {
    do {
      WE_GEN_DATAEX_DEVICE_STATUS dataex_dev_status;
      dataex_dev_status.u32RxCount    = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_PACKETS_RECEIVED);
      dataex_dev_status.u32TxCount    = mtlk_sta_get_cnt(sta, MTLK_STAI_CNT_PACKETS_SENT);

      resp.datalen += sizeof(WE_GEN_DATAEX_DEVICE_STATUS);
      if (preq->datalen < resp.datalen) {
        res = MTLK_ERR_NO_MEM;
        break;
      }

      res = mtlk_clpb_push(clpb, &dataex_dev_status, sizeof(WE_GEN_DATAEX_DEVICE_STATUS));
      if (MTLK_ERR_OK != res) {
        break;
      }

      nof_connected++;

      sta = mtlk_stadb_iterate_next(&iter);
    } while (sta);
    mtlk_stadb_iterate_done(&iter);
  }

  if (MTLK_ERR_OK == res) {
    dataex_conn_status.u32NumOfConnections = nof_connected;
    res = mtlk_clpb_push(clpb, &dataex_conn_status, sizeof(WE_GEN_DATAEX_CONNECTION_STATUS));
  }

  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &resp, sizeof(resp));
  }
  return res;
}

static int
_mtlk_core_gen_dataex_get_status (mtlk_core_t *core,
                                  WE_GEN_DATAEX_REQUEST *preq,
                                  mtlk_clpb_t *clpb)
{
  int res = MTLK_ERR_OK;
  const sta_entry *sta;
  WE_GEN_DATAEX_RESPONSE resp;
  WE_GEN_DATAEX_STATUS status;
  mtlk_stadb_iterator_t iter;

  memset(&resp, 0, sizeof(resp));
  memset(&status, 0, sizeof(status));

  if (preq->datalen < sizeof(status)) {
    resp.status = WE_GEN_DATAEX_DATABUF_TOO_SMALL;
    resp.datalen = sizeof(status);
    goto end;
  }

  resp.ver = WE_GEN_DATAEX_PROTO_VER;
  resp.status = WE_GEN_DATAEX_SUCCESS;
  resp.datalen = sizeof(status);

  memset(&status, 0, sizeof(status));

  status.security_on = 0;
  status.wep_enabled = 0;

  sta = mtlk_stadb_iterate_first(&core->slow_ctx->stadb, &iter);
  if (sta) {
    do {
      /* Check global WEP enabled flag only if some STA connected */
      if ((mtlk_sta_get_cipher(sta) != IW_ENCODE_ALG_NONE) || core->slow_ctx->wep_enabled) {
        status.security_on = 1;
        if (core->slow_ctx->wep_enabled) {
          status.wep_enabled = 1;
        }
        break;
      }
      sta = mtlk_stadb_iterate_next(&iter);
    } while (sta);
    mtlk_stadb_iterate_done(&iter);
  }

  status.scan_started = mtlk_core_scan_is_running(core);
  if (!mtlk_vap_is_slave_ap(core->vap_handle) && (mtlk_scan_is_initialized(&core->slow_ctx->scan))) {
    status.frequency_band = mtlk_core_get_freq_band_cur(core);
  }
  else {
    status.frequency_band = MTLK_HW_BAND_NONE;
  }
  status.link_up = (mtlk_core_get_net_state(core) == NET_STATE_CONNECTED) ? 1 : 0;

  res = mtlk_clpb_push(clpb, &status, sizeof(status));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &resp, sizeof(resp));
  }

end:
  return res;
}

static int
_mtlk_core_gen_dataex_send_mac_leds (mtlk_core_t *core,
                                     mtlk_core_ui_gen_data_t *req,
                                     mtlk_clpb_t *clpb)
{
  int res = 0;
  WE_GEN_DATAEX_RESPONSE resp;
  UMI_SET_LED leds_cfg;
  WE_GEN_DATAEX_LED *leds_status = NULL;

  MTLK_ASSERT(NULL != req);

  memset(&resp, 0, sizeof(resp));
  memset(&leds_cfg, 0, sizeof(leds_cfg));

  leds_status = &req->leds_status;

  resp.ver = WE_GEN_DATAEX_PROTO_VER;
  if (req->request.datalen < sizeof(WE_GEN_DATAEX_LED)) {
    resp.status = WE_GEN_DATAEX_DATABUF_TOO_SMALL;
    resp.datalen = sizeof(WE_GEN_DATAEX_LED);
    goto end;
  }

  ILOG2_DD("u8BasebLed = %d, u8LedStatus = %d", req->leds_status.u8BasebLed,
    req->leds_status.u8LedStatus);

  resp.datalen = sizeof(leds_status);

  leds_cfg.u8LedStatus = req->leds_status.u8LedStatus;
  leds_cfg.u8BasebLed  = req->leds_status.u8BasebLed;

  res = _mtlk_core_set_leds_cfg(core, &leds_cfg);
  if (MTLK_ERR_OK != res) {
    resp.status = WE_GEN_DATAEX_FAIL;
  }
  else {
    resp.status = WE_GEN_DATAEX_SUCCESS;
  }

  res = mtlk_clpb_push(clpb, &resp, sizeof(resp));

end:
  return res;
}

static int
_mtlk_core_gen_data_exchange(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_ui_gen_data_t *req = NULL;
  uint32 req_size;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  req = mtlk_clpb_enum_get_next(clpb, &req_size);

  MTLK_ASSERT(NULL != req);
  MTLK_ASSERT(sizeof(*req) == req_size);

  switch (req->request.cmd_id) {
    case WE_GEN_DATAEX_CMD_CONNECTION_STATS:
      res = _mtlk_core_gen_dataex_get_connection_stats(core, &req->request, clpb);
      break;
    case WE_GEN_DATAEX_CMD_STATUS:
      res = _mtlk_core_gen_dataex_get_status(core, &req->request, clpb);
      break;
    case WE_GEN_DATAEX_CMD_LEDS_MAC:
      res = _mtlk_core_gen_dataex_send_mac_leds(core, req, clpb);
      break;
    default:
      MTLK_ASSERT(0);
   }

  return res;
}

uint32
mtlk_core_get_available_bitrates (struct nic *nic)
{
  uint8 net_mode;
  uint32 mask = 0;

  /* Get all needed MIBs */
  if (nic->net_state == NET_STATE_CONNECTED)
    net_mode =  mtlk_core_get_network_mode_cur(nic);
  else
    net_mode = mtlk_core_get_network_mode_cfg(nic);
  mask = get_operate_rate_set(net_mode);
  ILOG3_D("Configuration mask: 0x%08x", mask);

  return mask;
}

static int
_mtlk_core_store_gen_ie (struct nic *nic, u8 *ie, u16 ie_len, u8 ie_type)
{
  if (ie_type >= UMI_WPS_IE_NUM) {
    return MTLK_ERR_NOT_SUPPORTED;
  }

  memset(nic->slow_ctx->gen_ie[ie_type], 0, sizeof(nic->slow_ctx->gen_ie[ie_type]));
  nic->slow_ctx->gen_ie_len[ie_type] = ie_len;

  if (ie && ie_len) {
    memcpy(nic->slow_ctx->gen_ie[ie_type], ie, ie_len);
  }

  return MTLK_ERR_OK;
}

int
mtlk_core_set_gen_ie (struct nic *nic, u8 *ie, u16 ie_len, u8 ie_type)
{
  int res = 0;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry;
  UMI_GENERIC_IE   *ie_req;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg,
                                                 mtlk_vap_get_txmm(nic->vap_handle),
                                                 NULL);
  if (!man_entry) {
    ELOG_D("CID-%04x: No man entry available", mtlk_vap_get_oid(nic->vap_handle));
    res = -EAGAIN;
    goto end;
  }

  man_entry->id = UM_MAN_SET_IE_REQ;
  man_entry->payload_size = sizeof(UMI_GENERIC_IE);
  ie_req = (UMI_GENERIC_IE*) man_entry->payload;
  memset(ie_req, 0, sizeof(*ie_req));

  ie_req->u8Type = ie_type;

  if (ie_len > sizeof(ie_req->au8IE)) {
    ELOG_DDD("CID-%04x: invalid IE length (%i > %i)", mtlk_vap_get_oid(nic->vap_handle),
        ie_len, (int)sizeof(ie_req->au8IE));
    res = -EINVAL;
    goto end;
  }
  if (ie && ie_len)
    memcpy(ie_req->au8IE, ie, ie_len);
  ie_req->u16Length = cpu_to_le16(ie_len);

  if (mtlk_txmm_msg_send_blocked(&man_msg,
                                 MTLK_MM_BLOCKED_SEND_TIMEOUT) != MTLK_ERR_OK) {
    ELOG_D("CID-%04x: cannot set IE to MAC", mtlk_vap_get_oid(nic->vap_handle));
    res = -EINVAL;
  }

end:
  if (man_entry)
    mtlk_txmm_msg_cleanup(&man_msg);

  return res;
}

static int
mtlk_core_is_band_supported(mtlk_core_t *nic, unsigned band)
{
  if (band == MTLK_HW_BAND_BOTH && mtlk_vap_is_ap(nic->vap_handle)) // AP can't be dual-band
    return MTLK_ERR_UNKNOWN;

  return mtlk_eeprom_is_band_supported(mtlk_core_get_eeprom(nic), band);
}

int
mtlk_core_update_network_mode(mtlk_core_t* nic, uint8 net_mode)
{
  mtlk_core_t *core = nic;
  uint8 band_new = net_mode_to_band(net_mode);

  MTLK_ASSERT(NULL != nic);

  if (mtlk_core_is_band_supported(core, band_new) != MTLK_ERR_OK) {
    if (band_new == MTLK_HW_BAND_BOTH) {
      /*
       * Just in case of single-band hardware
       * continue to use `default' frequency band,
       * which is de facto correct.
       */
      ELOG_D("CID-%04x: dualband isn't supported", mtlk_vap_get_oid(core->vap_handle));
      return MTLK_ERR_OK;
    } else {
      ELOG_DS("CID-%04x: %s band isn't supported", mtlk_vap_get_oid(core->vap_handle),
              mtlk_eeprom_band_to_string(net_mode_to_band(net_mode)));
      return MTLK_ERR_NOT_SUPPORTED;
    }
  }

  ILOG1_S("Set Network Mode to %s", net_mode_to_string(net_mode));

  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_NET_MODE_CUR, net_mode);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_NET_MODE_CFG, net_mode);

  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_IS_HT_CUR, is_ht_net_mode(net_mode));
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_IS_HT_CFG, is_ht_net_mode(net_mode));

  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_FREQ_BAND_CFG, band_new);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_FREQ_BAND_CUR, band_new);

  /* The set of supported bands may be changed by this request.           */
  /* Scan cache to be cleared to throw out BSS from unsupported now bands */
  mtlk_cache_clear(&core->slow_ctx->cache);
  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_core_get_ta_cfg (mtlk_handle_t hcore,
                           const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_ta_cfg_t *ta_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  ta_cfg = mtlk_osal_mem_alloc(sizeof(*ta_cfg), MTLK_MEM_TAG_CLPB);
  if(ta_cfg == NULL) {
    ELOG_V("Can't allocate clipboard data");
    return MTLK_ERR_NO_MEM;
  }

  memset(ta_cfg, 0, sizeof(*ta_cfg));

  MTLK_CFG_SET_ITEM(ta_cfg, timer_resolution,
    mtlk_ta_get_timer_resolution_ticks(mtlk_vap_get_ta(core->vap_handle)));

  MTLK_CFG_SET_ITEM_BY_FUNC_VOID(ta_cfg, debug_info, mtlk_ta_get_debug_info,
                                 (mtlk_vap_get_ta(core->vap_handle), &ta_cfg->debug_info));

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push_nocopy(clpb, ta_cfg, sizeof(*ta_cfg));
    if(MTLK_ERR_OK != res) {
      mtlk_osal_mem_free(ta_cfg);
    }
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_ta_cfg (mtlk_handle_t hcore,
                       const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_ta_cfg_t *ta_cfg;
  uint32  ta_cfg_size;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  ta_cfg = mtlk_clpb_enum_get_next(clpb, &ta_cfg_size);

  MTLK_ASSERT(NULL != ta_cfg);
  MTLK_ASSERT(sizeof(*ta_cfg) == ta_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_CHECK_ITEM_AND_CALL(ta_cfg, timer_resolution, mtlk_ta_set_timer_resolution_ticks,
                                 (mtlk_vap_get_ta(core->vap_handle), ta_cfg->timer_resolution), res);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));

}

static int __MTLK_IFUNC
_mtlk_core_get_uapsd_mode (mtlk_handle_t hcore,
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_uapsd_mode_cfg_t uapsd_mode_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&uapsd_mode_cfg, 0, sizeof(uapsd_mode_cfg));

  MTLK_CFG_SET_ITEM(&uapsd_mode_cfg, uapsd_enabled, core->uapsd_enabled);

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &uapsd_mode_cfg, sizeof(uapsd_mode_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_uapsd_mode (mtlk_handle_t hcore,
                           const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_uapsd_mode_cfg_t *uapsd_mode_cfg;
  uint32  uapsd_mode_cfg_size;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  uapsd_mode_cfg = mtlk_clpb_enum_get_next(clpb, &uapsd_mode_cfg_size);

  MTLK_ASSERT(NULL != uapsd_mode_cfg);
  MTLK_ASSERT(sizeof(*uapsd_mode_cfg) == uapsd_mode_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_GET_ITEM(uapsd_mode_cfg, uapsd_enabled, core->uapsd_enabled);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));

}

static int __MTLK_IFUNC
_mtlk_core_get_uapsd_cfg (mtlk_handle_t hcore,
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_uapsd_cfg_t uapsd_cfg;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  memset(&uapsd_cfg, 0, sizeof(uapsd_cfg));

  MTLK_CFG_SET_ITEM(&uapsd_cfg, uapsd_max_sp, core->uapsd_max_sp);

  res = mtlk_clpb_push(clpb, &res, sizeof(res));
  if (MTLK_ERR_OK == res) {
    res = mtlk_clpb_push(clpb, &uapsd_cfg, sizeof(uapsd_cfg));
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_core_set_uapsd_cfg (mtlk_handle_t hcore,
                          const void* data, uint32 data_size)
{
  uint32 res = MTLK_ERR_OK;
  mtlk_uapsd_cfg_t *uapsd_cfg;
  uint32  uapsd_cfg_size;
  mtlk_core_t *core = (mtlk_core_t*)hcore;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  uapsd_cfg = mtlk_clpb_enum_get_next(clpb, &uapsd_cfg_size);

  MTLK_ASSERT(NULL != uapsd_cfg);
  MTLK_ASSERT(sizeof(*uapsd_cfg) == uapsd_cfg_size);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_CHECK_ITEM_AND_CALL(uapsd_cfg, uapsd_max_sp, mtlk_core_set_uapsd_max_sp,
                                 (core, uapsd_cfg->uapsd_max_sp), res);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  return mtlk_clpb_push(clpb, &res, sizeof(res));

}

int
mtlk_handle_bar (mtlk_handle_t context, uint8 *ta, uint8 tid, uint16 ssn)
{
  struct nic *nic = (struct nic *)context;
  int res;
  sta_entry *sta = NULL;

  nic->pstats.bars_cnt++;

  if (tid >= NTS_TIDS) {
    ELOG_DD("CID-%04x: Received BAR with wrong TID (%u)", mtlk_vap_get_oid(nic->vap_handle), tid);
    return -1;
  }

  sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, ta);
  if (sta == NULL) {
    ELOG_DY("CID-%04x: Received BAR from unknown peer %Y", mtlk_vap_get_oid(nic->vap_handle), ta);
    return -1;
  }

  ILOG2_YD("Received BAR from %Y TID %u", ta, tid);

  res = mtlk_sta_process_bar(sta, tid, ssn);

  mtlk_sta_decref(sta); /* De-reference of find */

  if(res != 0) {
    ELOG_DYD("CID-%04x: Failed to process BAR (STA %Y TID %u)", mtlk_vap_get_oid(nic->vap_handle), ta, tid);
  }

  return res;
}

#ifdef MTCFG_RF_MANAGEMENT_MTLK
mtlk_rf_mgmt_t* __MTLK_IFUNC
mtlk_get_rf_mgmt (mtlk_handle_t context)
{
  struct nic *nic = (struct nic *)context;

  return nic->rf_mgmt;
}
#endif

static void
_mtlk_update_prev_sub_nbuf(mtlk_nbuf_t * nbuf, mtlk_nbuf_t **sub_nbuf_stored, mtlk_nbuf_t **sub_nbuf_fisrt)
{
  mtlk_nbuf_t *sub_nbuf = *sub_nbuf_stored;

  if(sub_nbuf)
    mtlk_nbuf_priv_set_sub_frame(mtlk_nbuf_priv(sub_nbuf), nbuf);
  else
    *sub_nbuf_fisrt = nbuf;

  *sub_nbuf_stored = nbuf;
}

static int
mtlk_parse_a_msdu(mtlk_core_t* nic, mtlk_nbuf_t *nbuf, int a_msdu_len, mtlk_nbuf_t **sub_nbuf_fisrt)
{
  int msdu_len, subpacket_len, pad;
  struct ethhdr *ether_header;
  mtlk_nbuf_t *sub_nbuf;
  mtlk_nbuf_t *sub_nbuf_stored = NULL;
  int res = MTLK_ERR_NOT_IN_USE;
  const int AMSDU_MAX_MSDU_SIZE = 2304; /* See IEEE Std 802.11-2012, p. 417 */

  *sub_nbuf_fisrt = nbuf;

  ILOG5_D("Parsing A-MSDU: length %d", a_msdu_len);

  while (a_msdu_len) {

    if(a_msdu_len < sizeof(struct ethhdr))
    {
      WLOG_D("Invalid A-MSDU: subpacket header too short: %d", a_msdu_len);
      goto on_error;
    }

    ether_header = (struct ethhdr *)nbuf->data;
    msdu_len = ntohs(ether_header->h_proto);

    if(msdu_len > AMSDU_MAX_MSDU_SIZE)
    {
      WLOG_DD("Invalid A-MSDU: MSDU length > %d, a_msdu_len = %d",
              AMSDU_MAX_MSDU_SIZE, a_msdu_len);
      goto on_error;
    }

    subpacket_len = msdu_len + sizeof(struct ethhdr);

    ILOG5_D("A-MSDU subpacket: length = %d", subpacket_len);

    a_msdu_len -= subpacket_len;
    if (a_msdu_len < 0) {
      WLOG_DD("Invalid A-MSDU: subpacket too long (%d > %d)",
              subpacket_len, a_msdu_len + subpacket_len);
      goto on_error;
    } else if (a_msdu_len == 0) {
      /* The last MSDU in A-MSDU. Use original nbuf. */
      sub_nbuf = nbuf;
      pad = 0;
    } else {
      /* Not the last MSDU in A-MSDU. Clone original nbuf. */
      sub_nbuf = mtlk_df_nbuf_clone_no_priv(mtlk_vap_get_manager(nic->vap_handle), nbuf);
      /* skip padding */
      pad = (4 - (subpacket_len & 0x03)) & 0x03;
      a_msdu_len -= pad;
      if (sub_nbuf) {
        mtlk_nbuf_priv_set_src_sta(mtlk_nbuf_priv(sub_nbuf),
                                   mtlk_nbuf_priv_get_src_sta(mtlk_nbuf_priv(nbuf)));
      }
      else {
        WLOG_D("Cannot clone A-MSDU (len=%d)", a_msdu_len);
        goto on_error;
      }
      mtlk_df_nbuf_pull(nbuf, subpacket_len + pad);
    }

    /* cut everything after data */
    mtlk_df_nbuf_trim(sub_nbuf, subpacket_len);
    /* for A-MSDU case we need to skip LLC/SNAP header */
    memmove(sub_nbuf->data + sizeof(mtlk_snap_hdr_t) + sizeof(mtlk_llc_hdr_t),
      ether_header, ETH_ALEN * 2);
    mtlk_df_nbuf_pull(sub_nbuf, sizeof(mtlk_snap_hdr_t) + sizeof(mtlk_llc_hdr_t));
    analyze_rx_packet(nic, sub_nbuf);
    mtlk_mc_parse(nic, sub_nbuf);
    /* Link this nbuf (clone or original) to previous clone (if any) */
    _mtlk_update_prev_sub_nbuf(sub_nbuf, &sub_nbuf_stored, sub_nbuf_fisrt);
  }

  return MTLK_ERR_OK;

on_error:

  /* Have we made any clone so far? */
  if(*sub_nbuf_fisrt != nbuf)
  {
    /* Link first cloned nbuf to original nbuf
       because they will be freed in this order */
    mtlk_nbuf_priv_set_sub_frame(mtlk_nbuf_priv(nbuf), *sub_nbuf_fisrt);
  }

  return res;
}

/*
 * Definitions and macros below are used only for the packet's header transformation
 * For more information, please see following documents:
 *   - IEEE 802.1H standard
 *   - IETF RFC 1042
 *   - IEEE 802.11n standard draft 5 Annex M
 * */

#define _8021H_LLC_HI4BYTES             0xAAAA0300
#define _8021H_LLC_LO2BYTES_CONVERT     0x0000
#define RFC1042_LLC_LO2BYTES_TUNNEL     0x00F8

/* Default ISO/IEC conversion
 * we need to keep full LLC header and store packet length in the T/L subfield */
#define _8021H_CONVERT(ether_header, nbuf, data_offset) \
  data_offset -= sizeof(struct ethhdr); \
  ether_header = (struct ethhdr *)(nbuf->data + data_offset); \
  ether_header->h_proto = htons(nbuf->len - data_offset - sizeof(struct ethhdr))

/* 802.1H encapsulation
 * we need to remove LLC header except the 'type' field */
#define _8021H_DECAPSULATE(ether_header, nbuf, data_offset) \
  data_offset -= sizeof(struct ethhdr) - (sizeof(mtlk_snap_hdr_t) + sizeof(mtlk_llc_hdr_t)); \
  ether_header = (struct ethhdr *)(nbuf->data + data_offset)

static int
handle_rx_ind (mtlk_core_t *nic, mtlk_nbuf_t *nbuf, uint16 msdulen,
               const MAC_RX_ADDITIONAL_INFO_T *mac_rx_info)
{
  int res = MTLK_ERR_OK; /* Do not free nbuf */
  int off;
  mtlk_nbuf_priv_t *nbuf_priv;
  unsigned char fromDS, toDS;
  uint16 seq = 0, frame_ctl;
  uint16 frame_subtype;
  uint16 priority = 0, qos = 0;
  unsigned int a_msdu = 0;
  unsigned char *cp, *addr1, *addr2;
  sta_entry *sta = NULL;
  uint8 key_idx;
  int bridge_mode;

  ILOG4_V("Rx indication");
  bridge_mode = MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE);;

  // Set the size of the nbuff data

  if (msdulen > mtlk_df_nbuf_get_tail_room_size(nbuf)) {
    ELOG_DDD("CID-%04x: msdulen > nbuf size ->> %d > %d", mtlk_vap_get_oid(nic->vap_handle),
          msdulen,
          mtlk_df_nbuf_get_tail_room_size(nbuf));
  }

  mtlk_df_nbuf_put(nbuf, msdulen);

  // Get pointer to private area
  nbuf_priv = mtlk_nbuf_priv(nbuf);

  /* store vap index in private data,
   * Each packet must be aligned with the corresponding VAP */
  mtlk_nbuf_priv_set_vap_handle(nbuf_priv, nic->vap_handle);

/*


802.11n data frame from AP:

        |----------------------------------------------------------------|
 Bytes  |  2   |  2    |  6  |  6  |  6  |  2  | 6?  | 2?  | 0..2312 | 4 |
        |------|-------|-----|-----|-----|-----|-----|-----|---------|---|
 Descr. | Ctl  |Dur/ID |Addr1|Addr2|Addr3| Seq |Addr4| QoS |  Frame  |fcs|
        |      |       |     |     |     | Ctl |     | Ctl |  data   |   |
        |----------------------------------------------------------------|
Total: 28-2346 bytes

Existance of Addr4 in frame is optional and depends on To_DS From_DS flags.
Existance of QoS_Ctl is also optional and depends on Ctl flags.
(802.11n-D1.0 describes also HT Control (0 or 4 bytes) field after QoS_Ctl
but we don't support this for now.)

Interpretation of Addr1/2/3/4 depends on To_DS From_DS flags:

To DS From DS   Addr1   Addr2   Addr3   Addr4
---------------------------------------------
0       0       DA      SA      BSSID   N/A
0       1       DA      BSSID   SA      N/A
1       0       BSSID   SA      DA      N/A
1       1       RA      TA      DA      SA


frame data begins with 8 bytes of LLC/SNAP:

        |-----------------------------------|
 Bytes  |  1   |   1  |  1   |    3   |  2  |
        |-----------------------------------|
 Descr. |        LLC         |     SNAP     |
        |-----------------------------------+
        | DSAP | SSAP | Ctrl |   OUI  |  T  |
        |-----------------------------------|
        |  AA  |  AA  |  03  | 000000 |     |
        |-----------------------------------|

From 802.11 data frame that we receive from MAC we are making
Ethernet DIX (II) frame.

Ethernet DIX (II) frame format:

        |------------------------------------------------------|
 Bytes  |  6  |  6  | 2 |         46 - 1500               |  4 |
        |------------------------------------------------------|
 Descr. | DA  | SA  | T |          Data                   | FCS|
        |------------------------------------------------------|

So we overwrite 6 bytes of LLC/SNAP with SA.

*/

  ILOG4_V("Munging IEEE 802.11 header to be Ethernet DIX (II), irreversible!");
  cp = (unsigned char *) nbuf->data;

  mtlk_dump(4, cp, 112, "dump of recvd .11n packet");

  // Chop the last four bytes (FCS)
  mtlk_df_nbuf_trim(nbuf, nbuf->len-4);

  frame_ctl = mtlk_wlan_pkt_get_frame_ctl(cp);
  addr1 = WLAN_GET_ADDR1(cp);
  addr2 = WLAN_GET_ADDR2(cp);

  ILOG4_D("frame control - %04x", frame_ctl);

  /* Try to find source MAC of transmitter */
  sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, addr2);
  mtlk_nbuf_priv_set_src_sta(nbuf_priv, sta);
  mtlk_nbuf_priv_set_rcn_bits(nbuf_priv, mac_rx_info->u8RSN);

  /*
  Excerpts from "IEEE P802.11e/D13.0, January 2005" p.p. 22-23
  Type          Subtype     Description
  -------------------------------------------------------------
  00 Management 0000        Association request
  00 Management 0001        Association response
  00 Management 0010        Reassociation request
  00 Management 0011        Reassociation response
  00 Management 0100        Probe request
  00 Management 0101        Probe response
  00 Management 0110-0111   Reserved
  00 Management 1000        Beacon
  00 Management 1001        Announcement traffic indication message (ATIM)
  00 Management 1010        Disassociation
  00 Management 1011        Authentication
  00 Management 1100        Deauthentication
  00 Management 1101        Action
  00 Management 1101-1111   Reserved
  01 Control    0000-0111   Reserved
  01 Control    1000        Block Acknowledgement Request (BlockAckReq)
  01 Control    1001        Block Acknowledgement (BlockAck)
  01 Control    1010        Power Save Poll (PS-Poll)
  01 Control    1011        Request To Send (RTS)
  01 Control    1100        Clear To Send (CTS)
  01 Control    1101        Acknowledgement (ACK)
  01 Control    1110        Contention-Free (CF)-End
  01 Control    1111        CF-End + CF-Ack
  10 Data       0000        Data
  10 Data       0001        Data + CF-Ack
  10 Data       0010        Data + CF-Poll
  10 Data       0011        Data + CF-Ack + CF-Poll
  10 Data       0100        Null function (no data)
  10 Data       0101        CF-Ack (no data)
  10 Data       0110        CF-Poll (no data)
  10 Data       0111        CF-Ack + CF-Poll (no data)
  10 Data       1000        QoS Data
  10 Data       1001        QoS Data + CF-Ack
  10 Data       1010        QoS Data + CF-Poll
  10 Data       1011        QoS Data + CF-Ack + CF-Poll
  10 Data       1100        QoS Null (no data)
  10 Data       1101        Reserved
  10 Data       1110        QoS CF-Poll (no data)
  10 Data       1111        QoS CF-Ack + CF-Poll (no data)
  11 Reserved   0000-1111   Reserved
  */

  // FIXME: ADD DEFINITIONS!!!!
  // XXX, klogg: see frame.h

  switch(WLAN_FC_GET_TYPE(frame_ctl))
  {
  case IEEE80211_FTYPE_DATA:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_DAT);
    mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_DAT_FRAMES_RECEIVED);
    // Normal data
    break;
  case IEEE80211_FTYPE_MGMT:
    res = mtlk_process_man_frame(HANDLE_T(nic), sta, &nic->slow_ctx->scan, &nic->slow_ctx->cache,
        nic->slow_ctx->aocs, nbuf, mac_rx_info);
    mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_MAN_FRAMES_RECEIVED);

    if(res != MTLK_ERR_OK)
    {
      goto end;
    }

    frame_subtype = (frame_ctl & FRAME_SUBTYPE_MASK) >> FRAME_SUBTYPE_SHIFT;
    ILOG4_D("Subtype is %d", frame_subtype);

    if ((frame_subtype == MAN_TYPE_BEACON && !mtlk_vap_is_ap(nic->vap_handle)) ||
        frame_subtype == MAN_TYPE_PROBE_RES ||
        frame_subtype == MAN_TYPE_PROBE_REQ) {

      // Workaraund for WPS (wsc-1.7.0) - send channel instead of the Dur/ID field */
      *(uint16 *)(nbuf->data + 2) = mac_rx_info->u8Channel;

      mtlk_nl_send_brd_msg(nbuf->data, nbuf->len, GFP_ATOMIC,
                             NETLINK_SIMPLE_CONFIG_GROUP, NL_DRV_CMD_MAN_FRAME);
    }

    res = MTLK_ERR_NOT_IN_USE;
    goto end;
  case IEEE80211_FTYPE_CTL:
    CPU_STAT_SPECIFY_TRACK(CPU_STAT_ID_RX_CTL);
    mtlk_process_ctl_frame(HANDLE_T(nic), nbuf->data, nbuf->len);
    mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_CTL_FRAMES_RECEIVED);
    res = MTLK_ERR_NOT_IN_USE; /* Free nbuf */
    goto end;
  default:
    ILOG2_D("Unknown header type, frame_ctl %04x", frame_ctl);
    res = MTLK_ERR_NOT_IN_USE; /* Free nbuf */
    goto end;
  }

  ILOG4_Y("802.11n rx TA: %Y", addr2);
  ILOG4_Y("802.11n rx RA: %Y", addr1);

  if (sta == NULL) {
    ILOG2_V("SOURCE of RX packet not found!");
    res = MTLK_ERR_NOT_IN_USE; /* Free nbuf */
    goto end;
  }
  mtlk_sta_update_rx_rate(sta, mac_rx_info);

  /* Peers are updated on any data packet including NULL */
  mtlk_sta_on_frame_arrived(sta, mac_rx_info->aRssi);

  seq = mtlk_wlan_pkt_get_seq(cp);
  ILOG3_D("seq %d", seq);

  if (WLAN_FC_IS_NULL_PKT(frame_ctl)) {
    ILOG3_D("Null data packet, frame ctl - 0x%04x", frame_ctl);
    res = MTLK_ERR_NOT_IN_USE; /* Free nbuf */
    goto end;
  }

  off = mtlk_wlan_get_hdrlen(frame_ctl);
  ILOG3_D("80211_hdrlen - %d", off);
  if (WLAN_FC_IS_QOS_PKT(frame_ctl)) {
    u16 qos_ctl = mtlk_wlan_pkt_get_qos_ctl(cp, off);
    priority = WLAN_QOS_GET_PRIORITY(qos_ctl);
    a_msdu = WLAN_QOS_GET_MSDU(qos_ctl);
  }

  qos = mtlk_qos_get_ac_by_tid(priority);
#ifdef MTLK_DEBUG_CHARIOT_OOO
  nbuf_priv->seq_qos = qos;
#endif

#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
  if (mtlk_vap_is_ap(nic->vap_handle)) {
    mtlk_aocs_on_rx_msdu(mtlk_core_get_master(nic)->slow_ctx->aocs, qos);
  }
#endif

  fromDS = WLAN_FC_GET_FROMDS(frame_ctl);
  toDS   = WLAN_FC_GET_TODS(frame_ctl);
  ILOG3_DD("FromDS %d, ToDS %d", fromDS, toDS);

  /* Check if packet was directed to us */
  if (0 == MTLK_CORE_HOT_PATH_PDB_CMP_MAC(nic, CORE_DB_CORE_MAC_ADDR, addr1)) {
    mtlk_nbuf_priv_set_flags(nbuf_priv, MTLK_NBUFF_DIRECTED);
  }

  nic->pstats.ac_rx_counter[qos]++;
  nic->pstats.sta_session_rx_packets++;

  /* data offset should account also security info if it is there */
  off += _get_rsc_buf(nic, nbuf, off);
  /* See CCMP/TKIP frame header in standard - 8.3.2.2/8.3.3.2 clause */
  key_idx = (mtlk_nbuf_priv_get_rsc_buf_byte(nbuf_priv, 3) & 0300) >> 6;

  /* process regular MSDU */
  if (likely(!a_msdu)) {
    struct ethhdr *ether_header;
    /* Raw LLC header split into 3 parts to make processing more convenient */
    struct llc_hdr_raw_t {
      uint32 hi4bytes;
      uint16 lo2bytes;
      uint16 ether_type;
    } *llc_hdr_raw = (struct llc_hdr_raw_t *)(nbuf->data + off);

    if (llc_hdr_raw->hi4bytes == __constant_htonl(_8021H_LLC_HI4BYTES)) {
      switch (llc_hdr_raw->lo2bytes) {
      case __constant_htons(_8021H_LLC_LO2BYTES_CONVERT):
        switch (llc_hdr_raw->ether_type) {
        /* AppleTalk and IPX encapsulation - integration service STT (see table M.1) */
        case __constant_htons(ETH_P_AARP):
        case __constant_htons(ETH_P_IPX):
          _8021H_CONVERT(ether_header, nbuf, off);
          break;
        /* default encapsulation
         * TODO: make sure it will be the shortest path */
        default:
          _8021H_DECAPSULATE(ether_header, nbuf, off);
          break;
        }
        break;
      case __constant_htons(RFC1042_LLC_LO2BYTES_TUNNEL):
        _8021H_DECAPSULATE(ether_header, nbuf, off);
        break;
      default:
        _8021H_CONVERT(ether_header, nbuf, off);
        break;
      }
    } else {
      _8021H_CONVERT(ether_header, nbuf, off);
    }
    mtlk_df_nbuf_pull(nbuf, off);
    ether_header = (struct ethhdr *)nbuf->data;

    /* save SRC/DST MAC addresses from 802.11 header to 802.3 header */
    mtlk_wlan_get_mac_addrs(cp, fromDS, toDS, ether_header->h_source, ether_header->h_dest);

    /* Check if packet received from WDS HOST
     * (HOST mac address is not equal to sender's MAC address) */
    if ((bridge_mode == BR_MODE_WDS) &&
        memcmp(ether_header->h_source, mtlk_sta_get_addr(sta)->au8Addr, ETH_ALEN)) {

      /* On AP we need to update HOST's entry in database of registered
       * HOSTs behind connected STAs */
      if (mtlk_vap_is_ap(nic->vap_handle)) {
         mtlk_hstdb_update_host(&nic->slow_ctx->hstdb, ether_header->h_source, sta);

      /* On STA we search if this HOST registered in STA's database of
       * connected HOSTs (that are behind this STA) */
      } else if (mtlk_hstdb_find_sta(&nic->slow_ctx->hstdb, ether_header->h_source) != NULL) {
          ILOG4_V("Packet from our own host received from AP");
          res = MTLK_ERR_NOT_IN_USE; /* Free nbuf */
          goto end;
      }
    }

#ifdef MTLK_DEBUG_IPERF_PAYLOAD_RX
    debug_ooo_analyze_packet(TRUE, nbuf, seq);
#endif

    analyze_rx_packet(nic, nbuf);
    mtlk_mc_parse(nic, nbuf);

    /* try to reorder packet */
    if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_DIRECTED) &&
        (mtlk_vap_is_ap(nic->vap_handle) || WLAN_FC_IS_QOS_PKT(frame_ctl))) {
      if(!mtlk_sta_reorder_packet(sta, priority, seq, nbuf))
      {
        /* We have received QoS packet while reordering is not open */
        if(MTLK_BFIELD_GET(mac_rx_info->u8HwInfo, HW_INFO_AGGR))
        {
          /* This is aggregated packet. We should send DELBA. */
          mtlk_sta_on_qos_without_reord(sta, priority);
        }
      }
    } else {
      mtlk_detect_replay_or_sendup(nic, nbuf, nic->group_rsc[key_idx]);
    }

  /* process A-MSDU */
  } else {
    mtlk_nbuf_t *nbuf_first;

    mtlk_df_nbuf_pull(nbuf, off);
    /* parse A-MSDU return first packet to send to OS */
    res = mtlk_parse_a_msdu(nic, nbuf, nbuf->len, &nbuf_first);
    if (MTLK_ERR_OK != res) {
      goto end;
    }

    /* try to reorder packet */
    if (mtlk_nbuf_priv_check_flags(mtlk_nbuf_priv(nbuf_first), MTLK_NBUFF_DIRECTED) &&
        (mtlk_vap_is_ap(nic->vap_handle) || WLAN_FC_IS_QOS_PKT(frame_ctl))) {
            mtlk_sta_reorder_packet(sta, priority, seq, nbuf_first);
    } else {
        mtlk_detect_replay_or_sendup(nic, nbuf_first, nic->group_rsc[key_idx]);
    }
  }

end:
  if (sta) {
    mtlk_sta_decref(sta); /* De-reference of find */
  }
  return res;
}

static int __MTLK_IFUNC
_handle_bss_created(mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);
  MTLK_ASSERT(size == sizeof(UMI_NETWORK_EVENT));

  ILOG1_Y("Network created, BSSID %Y", ((UMI_NETWORK_EVENT *)payload)->sBSSID.au8Addr);
  _mtlk_core_trigger_connect_complete_event(nic, TRUE);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_bss_connecting(mtlk_handle_t core_object, const void *payload, uint32 size)
{
  MTLK_UNREFERENCED_PARAM(core_object);
  MTLK_UNREFERENCED_PARAM(payload);
  MTLK_UNREFERENCED_PARAM(size);
  ILOG1_V("Connecting to network...");

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_bss_connected(mtlk_handle_t core_object, const void *payload, uint32 size)
{
  MTLK_UNREFERENCED_PARAM(core_object);
  MTLK_ASSERT(size == sizeof(UMI_NETWORK_EVENT));

  ILOG1_Y("Network created, BSSID %Y", ((UMI_NETWORK_EVENT *)payload)->sBSSID.au8Addr);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_bss_failed(mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);
  const UMI_NETWORK_EVENT *network_event = (const UMI_NETWORK_EVENT *)payload;
  uint16 reason;
  MTLK_ASSERT(size == sizeof(UMI_NETWORK_EVENT));

  reason = network_event->u8Reason;
  WLOG_DD("CID-%04x: Failed to create/connect to network, reason %d",
           mtlk_vap_get_oid(nic->vap_handle), reason);

  // AP is dead? Force user to rescan to see this BSS again
  if (reason == UMI_BSS_JOIN_FAILED) {
    MTLK_ASSERT(!mtlk_vap_is_ap(nic->vap_handle));
    mtlk_cache_remove_bss_by_bssid(&nic->slow_ctx->cache,
                                   network_event->sBSSID.au8Addr);
  }

  _mtlk_core_trigger_connect_complete_event(nic, FALSE);

  return MTLK_ERR_OK;
};

static int __MTLK_IFUNC
_mtlk_handle_unknown_network_ind(mtlk_handle_t core_object, const void *payload, uint32 size)
{
  MTLK_ASSERT(size == sizeof(uint16));

  ELOG_DD("CID-%04x: Unrecognised network event %d",
      mtlk_vap_get_oid(HANDLE_T_PTR(mtlk_core_t, core_object)->vap_handle), *(uint16 *)payload);
  return MTLK_ERR_OK;
}

static void
_handle_network_event (struct nic *nic, UMI_NETWORK_EVENT *psNetwork)
{
  uint16 id = MAC_TO_HOST16(psNetwork->u16BSSstatus);

  switch (id)
  {
  case UMI_BSS_CREATED:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _handle_bss_created,
                          HANDLE_T(nic), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  case UMI_BSS_CONNECTING:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _handle_bss_connecting,
                          HANDLE_T(nic), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  case UMI_BSS_CONNECTED:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _handle_bss_connected,
                          HANDLE_T(nic), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  case UMI_BSS_FAILED:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _handle_bss_failed,
                          HANDLE_T(nic), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  case UMI_BSS_CHANNEL_SWITCH_DONE:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, mtlk_dot11h_handle_channel_switch_done,
                          HANDLE_T(mtlk_core_get_dfs(nic)), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  case UMI_BSS_CHANNEL_PRE_SWITCH_DONE:
    MTLK_ASSERT(!mtlk_vap_is_slave_ap(nic->vap_handle));
    _mtlk_process_hw_task(nic, SYNCHRONOUS, mtlk_dot11h_handle_channel_pre_switch_done,
                          HANDLE_T(mtlk_core_get_dfs(nic)), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  case UMI_BSS_CHANNEL_SWITCH_NORMAL:
  case UMI_BSS_CHANNEL_SWITCH_SILENT:
    MTLK_ASSERT(!mtlk_vap_is_slave_ap(nic->vap_handle));
    _mtlk_process_hw_task(nic, SYNCHRONOUS, mtlk_dot11h_handle_channel_switch_ind,
                          HANDLE_T(mtlk_core_get_dfs(nic)), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  case UMI_BSS_RADAR_NORM:
  case UMI_BSS_RADAR_HOP:
    MTLK_ASSERT(!mtlk_vap_is_slave_ap(nic->vap_handle));
    _mtlk_process_hw_task(nic, SYNCHRONOUS, mtlk_dot11h_handle_radar_ind,
                          HANDLE_T(mtlk_core_get_dfs(nic)), psNetwork, sizeof(UMI_NETWORK_EVENT));
    break;

  default:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_unknown_network_ind,
                          HANDLE_T(nic), &id, sizeof(uint16));
    break;
  }
}

static int
_mtlk_core_set_wep_key_blocked (struct nic      *nic,
                                const IEEE_ADDR *addr,
                                uint16           key_idx)
{
  int               res             = MTLK_ERR_UNKNOWN;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t *man_entry       = NULL;
  UMI_SET_KEY      *umi_key;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg,
                                                 mtlk_vap_get_txmm(nic->vap_handle),
                                                 &res);
  if (!man_entry) {
    ELOG_DD("CID-%04x: No man entry available (res = %d)", mtlk_vap_get_oid(nic->vap_handle), res);
    goto end;
  }

  umi_key = (UMI_SET_KEY*)man_entry->payload;
  memset(umi_key, 0, sizeof(*umi_key));

  man_entry->id           = UM_MAN_SET_KEY_REQ;
  man_entry->payload_size = sizeof(*umi_key);

  umi_key->u16CipherSuite     = HOST_TO_MAC16(UMI_RSN_CIPHER_SUITE_WEP40);
  umi_key->sStationID         = *addr;
  if (mtlk_osal_eth_is_broadcast(addr->au8Addr))
    umi_key->u16KeyType = cpu_to_le16(UMI_RSN_GROUP_KEY);
  else
    umi_key->u16KeyType = cpu_to_le16(UMI_RSN_PAIRWISE_KEY);
  umi_key->u16DefaultKeyIndex = HOST_TO_MAC16(key_idx);

  memcpy(umi_key->au8Tk1,
         nic->slow_ctx->wep_keys.sKey[key_idx].au8KeyData,
         nic->slow_ctx->wep_keys.sKey[key_idx].u8KeyLength);

  mtlk_dump(4, umi_key, sizeof(*umi_key), "dump of UMI_SET_KEY");

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: mtlk_mm_send failed: %d", mtlk_vap_get_oid(nic->vap_handle), res);
    goto end;
  }

  umi_key->u16Status = MAC_TO_HOST16(umi_key->u16Status);

  if (umi_key->u16Status != UMI_OK) {
    ELOG_DYD("CID-%04x: %Y: status is %d", mtlk_vap_get_oid(nic->vap_handle), addr, umi_key->u16Status);
    res = MTLK_ERR_MAC;
    goto end;
  }

  res = MTLK_ERR_OK;

end:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return res;
}

static void
_mtlk_core_notify_ap_of_station_connection(struct nic       *nic,
                                           const IEEE_ADDR  *addr,
                                           const UMI_RSN_IE *rsn_ie,
                                           BOOL supports_20_40,
                                           BOOL received_scan_exemption,
                                           BOOL is_intolerant,
                                           BOOL is_legacy)
{

  const uint8 *rsnie     = rsn_ie->au8RsnIe;
  uint8        rsnie_id  = rsnie[0];

  /*
   * WARNING! When there is RSN IE, we send address
   * in IWEVREGISTERED event not in wrqu.addr.sa_data as usual,
   * but in extra along with RSN IE data.
   * Why? Because iwreq_data is union and there is no other way
   * to send address and RSN IE in one event.
   * IWEVREGISTERED event is handled in hostAPd only
   * so there might be not any collessions with such non-standart
   * implementation of it.
   */
  mtlk_dump(4, rsnie, sizeof(rsn_ie->au8RsnIe), "dump of RSNIE:");

  mtlk_20_40_register_station(mtlk_core_get_coex_sm(nic),
                              addr,
                              supports_20_40,
                              received_scan_exemption,
                              is_intolerant,
                              FALSE); /* Temporary fix. Legacy flag shouldn't be considered by coex. module.
                                         see WAVE400_SW-194 for more details.
                                         AV TODO: Remove legacy flag from Coex. if no problem will be detected on QA */

  if (rsnie_id) {
    mtlk_df_ui_notify_secure_node_connect(
        mtlk_vap_get_df(nic->vap_handle),
        addr->au8Addr, rsnie, rsnie[1] + 2);
  } else {
    mtlk_df_ui_notify_node_connect(mtlk_vap_get_df(nic->vap_handle), addr->au8Addr);
  }
}

static void
_mtlk_core_notify_ap_of_station_disconnection (struct nic       *nic,
                                               const IEEE_ADDR  *addr)
{
  mtlk_20_40_unregister_station(mtlk_core_get_coex_sm(nic), addr);
  mtlk_df_ui_notify_node_disconect(mtlk_vap_get_df(nic->vap_handle), addr->au8Addr);
}

static void
_mtlk_core_connect_sta_blocked (struct nic       *nic,
                                const UMI_CONNECTION_EVENT *psConnect,
                                BOOL              reconnect)
{
  sta_entry          *sta;
  const IEEE_ADDR    *addr = &psConnect->sStationID;
  const IEEE_ADDR    *prev_bssid = &psConnect->sPrevBSSID;
  const UMI_RSN_IE   *rsn_ie = &psConnect->sRSNie;
  BOOL                is_ht_capable = FALSE;
  BOOL                intolerant = FALSE;
  BOOL                is_tkip = FALSE;
  struct wpa_ie_data  d;
  uint32              sta_id = (uint32) MAC_TO_HOST16(psConnect->u16Sid);

  MTLK_UNREFERENCED_PARAM(prev_bssid);

  MTLK_ASSERT(mtlk_vap_is_ap(nic->vap_handle) || !_mtlk_core_has_connections(nic));

  /* check TKIP */
  if (rsn_ie->au8RsnIe[0]) {
    if (wpa_parse_wpa_ie(rsn_ie->au8RsnIe, rsn_ie->au8RsnIe[1] + 2, &d) == MTLK_ERR_OK) {
      if (d.pairwise_cipher == UMI_RSN_CIPHER_SUITE_TKIP) {
        ILOG1_DY("CID-%04x: Station %Y connecting in TKIP", mtlk_vap_get_oid(nic->vap_handle), addr);
        mtlk_dump(2, rsn_ie->au8RsnIe, sizeof(rsn_ie->au8RsnIe), "Content of UMI_CONNECTED RSN IE");
        is_tkip = TRUE;
      }
    }
    else{
      ELOG_D("CID-%04x: Can not parse WPA/RSN IE", mtlk_vap_get_oid(nic->vap_handle));
      mtlk_dump(0, rsn_ie->au8RsnIe, sizeof(rsn_ie->au8RsnIe), "Content of UMI_CONNECTED RSN IE");
    }
  }

  if(psConnect->u8HTmode && (psConnect->u32SupportedRates & LM_PHY_11N_RATE_MSK) && (!is_tkip))
  {
    is_ht_capable = TRUE;
  }
  else
  {
    is_ht_capable = FALSE;
  }

  /* Remove MAC addr from switch MAC table before adding to stadb */
  mtlk_df_user_ppa_remove_mac_addr(mtlk_vap_get_df(nic->vap_handle), addr->au8Addr);

  sta = mtlk_stadb_add_sta(&nic->slow_ctx->stadb, addr->au8Addr,
                           (mtlk_core_get_is_ht_cur(nic) && is_ht_capable), sta_id);

  if (sta == NULL)
    goto add_sta_fail;

  if (mtlk_vap_is_ap(nic->vap_handle))
  {
    sta_capabilities sta_capabilities;

    if (!reconnect) {
      ILOG1_DYS("CID-%04x: Station %Y (%sN) has connected",
                mtlk_vap_get_oid(nic->vap_handle),
                addr,
                is_ht_capable ? "" : "non-");
    }
    else {
      ILOG1_DYSY("CID-%04x: Station %Y (%sN) has reconnected. Previous BSS was %Y",
                 mtlk_vap_get_oid(nic->vap_handle),
                 addr, is_ht_capable ? "" : "non-",
                 prev_bssid);
    }

    if (psConnect->u8PeerAP) {
      /* Peer AP */
      sta->peer_ap = 1;

      if (nic->slow_ctx->peerAPs_key_idx) {
        mtlk_sta_set_cipher(sta, IW_ENCODE_ALG_WEP);
        _mtlk_core_set_wep_key_blocked(nic, addr, nic->slow_ctx->peerAPs_key_idx - 1);
      }
    }
    else {
      /* Regular STA */
      if (nic->slow_ctx->wep_enabled && !nic->slow_ctx->wps_in_progress) {
        mtlk_sta_set_cipher(sta, IW_ENCODE_ALG_WEP);
        _mtlk_core_set_wep_key_blocked(nic, addr, nic->slow_ctx->default_key);
      }

      if ((psConnect->fortyMHzIntolerant == TRUE) || (psConnect->twentyMHzBssWidthRequest == TRUE)) {
        intolerant = TRUE;
      }

      _mtlk_core_notify_ap_of_station_connection(nic, addr, rsn_ie, psConnect->twentyFortyBssCoexistenceManagementSupport, psConnect->obssScanningExemptionGrant, intolerant, !is_ht_capable);
    }

    mtlk_frame_process_sta_capabilities(&sta_capabilities,
        psConnect->sPeersCapabilities.u8LantiqProprietry,
        MAC_TO_HOST16(psConnect->sPeersCapabilities.u16HTCapabilityInfo),
        psConnect->sPeersCapabilities.AMPDU_Parameters,
        MAC_TO_HOST32(psConnect->sPeersCapabilities.tx_bf_capabilities),
        psConnect->boWMEsupported,
        MAC_TO_HOST32(psConnect->u32SupportedRates),
        mtlk_core_get_freq_band_cur(nic));

    mtlk_sta_set_capabilities(sta, &sta_capabilities);

    if (nic->uapsd_enabled) {
      BOOL uapsd_enabled = FALSE;
      mtlk_sq_peer_ctx_t *ppeer = &sta->sq_peer_ctx;

      ppeer->uapsd_max_sp = MTLK_BFIELD_GET(psConnect->uapsdParams, IND_UAPSD_MAX_SP_LENGTH);
      if (0 == ppeer->uapsd_max_sp) {
        ppeer->uapsd_max_sp = UAPSD_MAX_SP_USER_MAX;
      }
      ppeer->uapsd_max_sp = MIN(ppeer->uapsd_max_sp, nic->uapsd_max_sp) + UAPSD_MAX_SP_ADDITION;

      UAPSD_SET_AC_LIMIT_AND_UAPSD_ENABLED(VO, ppeer->uapsd_max_sp);
      UAPSD_SET_AC_LIMIT_AND_UAPSD_ENABLED(VI, ppeer->uapsd_max_sp);
      UAPSD_SET_AC_LIMIT_AND_UAPSD_ENABLED(BK, ppeer->uapsd_max_sp);
      UAPSD_SET_AC_LIMIT_AND_UAPSD_ENABLED(BE, ppeer->uapsd_max_sp);

      if(!uapsd_enabled)
      {
        ppeer->uapsd_max_sp = 0;
      }
    }

  } // End of is AP
  else  // STA
  {
    mtlk_pdb_set_mac(mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_BSSID,
                     addr->au8Addr);

    if (!reconnect) {
      ILOG1_YS("connected to %Y (%sN)",
                       addr,
                       is_ht_capable ? "" : "non-");
      nic->pstats.sta_session_rx_packets = 0;
      nic->pstats.sta_session_tx_packets = 0;
    }
    else {
      ILOG1_YY("connected to %Y, previous BSSID was %Y",
                      addr,
                      prev_bssid);
    }

    mtlk_df_ui_notify_association(mtlk_vap_get_df(nic->vap_handle), addr->au8Addr);
    if (mtlk_core_is_20_40_active(nic))
    {
      mtlk_20_40_sta_notify_connection_to_ap(mtlk_core_get_coex_sm(nic), psConnect->fortyMHzMode, psConnect->twentyFortyBssCoexistenceManagementSupport, psConnect->obssScanningExemptionGrant);
    }
    mtlk_scan_set_load_progmodel_policy(&nic->slow_ctx->scan, mtlk_core_is_20_40_active(nic) ? FALSE : TRUE);

#ifdef PHASE_3
    mtlk_start_cache_update(nic);
#endif

    // make BSS we've connected to persistent in cache until we're disconnected
    mtlk_cache_set_persistent(&nic->slow_ctx->cache,
                              addr->au8Addr,
                              TRUE);
  }

  if (nic->slow_ctx->rsnie.au8RsnIe[0] && !sta->peer_ap) {
    /* Filter is already enabled by default */
  } else if (!mtlk_vap_is_ap(nic->vap_handle) && nic->slow_ctx->wps_in_progress) {
    mtlk_sta_set_packets_filter(sta, MTLK_PCKT_FLTR_ALLOW_802_1X);
    ILOG1_Y("%Y: turn on 802.1x filtering due to WPS", mtlk_sta_get_addr(sta));
  } else {
    mtlk_sta_set_packets_filter(sta, MTLK_PCKT_FLTR_ALLOW_ALL);
    ILOG1_Y("%Y: turn off 802.1x filtering", mtlk_sta_get_addr(sta));
    mtlk_sta_on_security_negotiated(sta);
  }

  mtlk_qos_reset_acm_bits(&nic->qos);

  if ( BR_MODE_WDS == MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE)){
    mtlk_hstdb_start_all_by_sta(&nic->slow_ctx->hstdb, sta);
  }

  mtlk_hstdb_remove_host_by_addr(&nic->slow_ctx->hstdb, addr);

  if (mtlk_vap_is_ap(nic->vap_handle) &&
      BR_MODE_WDS == MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE)){
    wds_peer_connect(&nic->slow_ctx->wds_mng, sta, addr);
  }

  /* notify Traffic Analyzer about new STA */
  mtlk_ta_on_connect(mtlk_vap_get_ta(nic->vap_handle), sta);

  mtlk_sta_decref(sta); /* De-reference by creator */

  return;

add_sta_fail:

  if (mtlk_vap_is_ap(nic->vap_handle) &&
      BR_MODE_WDS == MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE)){
    wds_peer_connect(&nic->slow_ctx->wds_mng, NULL, addr);
  }

  return;
}

static int
_mtlk_core_disconnect_sta_blocked (struct nic       *nic,
                                   const IEEE_ADDR  *addr,
                                   uint16            reason,
                                   uint16            fail_reason,
                                   uint16            reason_code)

{
  uint32      net_state  = mtlk_core_get_net_state(nic);

  MTLK_ASSERT(addr != NULL);

  /* Check is STA is a peer AP */
  if (mtlk_vap_is_ap(nic->vap_handle) &&
      BR_MODE_WDS == MTLK_CORE_HOT_PATH_PDB_GET_INT(nic, CORE_DB_CORE_BRIDGE_MODE)){
    int res, terminate;
    res = wds_peer_disconnect(&nic->slow_ctx->wds_mng, addr, &terminate);
    if (terminate) return res;
  }

  if (net_state != NET_STATE_CONNECTED) {
    ILOG1_DYDDD("CID-%04x: Failed to connect to %Y for reason %d fail_reason %d reason_code %d",
              mtlk_vap_get_oid(nic->vap_handle),
              addr, reason, fail_reason, reason_code);

    /* This is wrong variable to check. Must be "reason", not "fail_reason".
       I will leave this place as-is to not break present logic */
    if (fail_reason == UMI_BSS_JOIN_FAILED) {
      /* AP is dead? Force user to rescan to see this BSS again */
      mtlk_cache_remove_bss_by_bssid(&nic->slow_ctx->cache, addr->au8Addr);
    }

    _mtlk_core_trigger_connect_complete_event(nic, FALSE);
  }
  else {
    if (mtlk_vap_is_ap(nic->vap_handle)) {
      ILOG1_DYDDD("CID-%04x: STA %Y disconnected for reason %d fail_reason %d reason_code %d",
                mtlk_vap_get_oid(nic->vap_handle),
                addr, reason, fail_reason, reason_code);
    } else {
      ILOG1_DYDDD("CID-%04x: Disconnected from BSS %Y for reason %d fail_reason %d reason_code %d",
                mtlk_vap_get_oid(nic->vap_handle),
                addr, reason, fail_reason, reason_code);

      if (!mtlk_vap_is_slave_ap(nic->vap_handle)) {
        /* We could have BG scan doing right now.
           It must be terminated because scan module
           is not ready for background scan to normal scan
           mode switch on the fly.
         */
        scan_terminate(&nic->slow_ctx->scan);

        /* Since we disconnected switch scan mode to normal */
        mtlk_scan_set_background(&nic->slow_ctx->scan, FALSE);
      }
    }
  }

  /* send disconnect request */
  return _mtlk_core_send_disconnect_req_blocked(nic, addr, reason, fail_reason, reason_code);
}


static int __MTLK_IFUNC
_mtlk_core_ap_disconnect_sta(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  int res = MTLK_ERR_PARAMS;
  mtlk_core_ui_mlme_cfg_t *mlme_cfg;
  uint32 size;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if (!mtlk_vap_is_ap(nic->vap_handle)) {
    res = MTLK_ERR_NOT_SUPPORTED;
    goto finish;
  }

  mlme_cfg = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == mlme_cfg) || (sizeof(*mlme_cfg) != size) ) {
    ELOG_D("CID-%04x: Failed to get MLME configuration parameters from CLPB", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_UNKNOWN;
    goto finish;
  }

  if (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED) {
    ILOG1_Y("STA (%Y), not connected - request rejected", mlme_cfg->sta_addr.au8Addr);
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  res = _mtlk_core_disconnect_sta_blocked(nic,
                                           (const IEEE_ADDR  *)mlme_cfg->sta_addr.au8Addr,
                                           UMI_BSS_UNSPECIFIED,
                                           FM_STATUSCODE_USER_REQUEST,
                                           mlme_cfg->reason_code);
finish:
  return res;
}

static int __MTLK_IFUNC
_mtlk_core_ap_disconnect_all (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  int res = MTLK_ERR_OK;
  const sta_entry *sta = NULL;
  mtlk_stadb_iterator_t iter;
  mtlk_core_ui_mlme_cfg_t *mlme_cfg;
  uint32 size;


  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(mtlk_vap_is_ap(nic->vap_handle));

  if (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED) {
    ILOG1_V("AP is down - request rejected");
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  mlme_cfg = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == mlme_cfg) || (sizeof(*mlme_cfg) != size) ) {
    ELOG_D("CID-%04x: Failed to get MLME configuration parameters from CLPB", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_UNKNOWN;
    goto finish;
  }

  sta = mtlk_stadb_iterate_first(&nic->slow_ctx->stadb, &iter);
  if (sta) {
    do {
      if (!sta->peer_ap){
        res = _mtlk_core_disconnect_sta_blocked(nic,
                                                mtlk_sta_get_addr(sta),
                                                UMI_BSS_UNSPECIFIED,
                                                FM_STATUSCODE_USER_REQUEST,
                                                mlme_cfg->reason_code);
        if (res != MTLK_ERR_OK) {
          ELOG_YD("STA (%Y) disconnection failed (%d)", mtlk_sta_get_addr(sta), res);
          break;
        }
      }
      sta = mtlk_stadb_iterate_next(&iter);
    } while (sta);
    mtlk_stadb_iterate_done(&iter);
  }

finish:
  return res;
}

static int _mtlk_core_on_association_event(mtlk_vap_handle_t vap_handle,
                                           IEEE_ADDR         mac_addr,
                                           uint16            fail_reason)
{
  mtlk_association_event_t assoc_event;

  memset(&assoc_event, 0, sizeof(mtlk_association_event_t));
  mtlk_osal_copy_eth_addresses((uint8*)&assoc_event.mac_addr, mac_addr.au8Addr);
  assoc_event.status = fail_reason;

  return mtlk_wssd_send_event(mtlk_vap_get_irbd(vap_handle),
                              MTLK_WSSA_DRV_EVENT_ASSOCIATION,
                              &assoc_event,
                              sizeof(mtlk_association_event_t));
}

static int _mtlk_core_on_authentication_event(mtlk_vap_handle_t vap_handle,
                                              IEEE_ADDR         mac_addr,
                                              uint16            reason,
                                              uint16            fail_reason)
{
  mtlk_authentication_event_t auth_event;

  memset(&auth_event, 0, sizeof(mtlk_authentication_event_t));

  mtlk_osal_copy_eth_addresses((uint8*)&auth_event.mac_addr, mac_addr.au8Addr);
  auth_event.auth_type = reason;
  auth_event.status = fail_reason;

  return mtlk_wssd_send_event(mtlk_vap_get_irbd(vap_handle),
                              MTLK_WSSA_DRV_EVENT_AUTHENTICATION,
                              &auth_event,
                              sizeof(mtlk_authentication_event_t));
}

static int _mtlk_core_on_peer_disconnect(mtlk_core_t *core,
                                        sta_entry   *sta,
                                        uint16       reason,
                                        uint16       fail_reason)
{
  mtlk_disconnection_event_t disconnect_event;
  BOOL ap_mode;

  MTLK_ASSERT(core != NULL);
  MTLK_ASSERT(sta != NULL);

  ap_mode = mtlk_vap_is_ap(core->vap_handle);

  memset(&disconnect_event, 0, sizeof(disconnect_event));
  memcpy(&disconnect_event.mac_addr, mtlk_sta_get_addr(sta), sizeof(disconnect_event.mac_addr));

  disconnect_event.reason = fail_reason;
  disconnect_event.initiator = ap_mode ? MTLK_DI_AP : MTLK_DI_STA;

  switch (fail_reason)
  {
    case FM_STATUSCODE_AGED_OUT:
    case FM_STATUSCODE_INACTIVITY:
    case FM_STATUSCODE_USER_REQUEST:
    case FM_STATUSCODE_PEER_PARAMS_CHANGED:
      /* Disconnect initiated by this side */
      break;

    case FM_STATUSCODE_MAC_ADDRESS_FILTER:
        if (UMI_BSS_DEAUTHENTICATED == reason)
        {
          break;
        }
        /* Drop down to default */

    default:
      /* Disconnect initiated by other side */
      disconnect_event.initiator = ap_mode ? MTLK_DI_STA : MTLK_DI_AP;
  }

  mtlk_sta_get_peer_stats(sta, &disconnect_event.peer_stats);
  mtlk_sta_get_peer_capabilities(sta, &disconnect_event.peer_capabilities);

  return mtlk_wssd_send_event(mtlk_vap_get_irbd(core->vap_handle),
                              MTLK_WSSA_DRV_EVENT_DISCONNECTION,
                              &disconnect_event,
                              sizeof(disconnect_event));
}

static int __MTLK_IFUNC
_handle_sta_connection_event (mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);
  const UMI_CONNECTION_EVENT *psConnect =
    (const UMI_CONNECTION_EVENT *)payload;
  uint16 reason = MAC_TO_HOST16(psConnect->u16Reason);
  uint16 fail_reason = MAC_TO_HOST16(psConnect->u16FailReason);
  uint16 event = MAC_TO_HOST16(psConnect->u16Event);

  MTLK_ASSERT(size == sizeof(*psConnect));

  mtlk_dump(5, psConnect, sizeof(UMI_CONNECTION_EVENT), "UMI_CONNECTION_EVENT:");

  if (nic->is_stopped) {
    ILOG5_V("Connection event while core is down");
    return MTLK_ERR_OK; // do not process
  }

  switch (event) {
  case UMI_CONNECTED:
    ILOG2_DDY("CID-%04x: UMI_CONNECTED. Reason %d, %Y",
      mtlk_vap_get_oid(nic->vap_handle), reason, psConnect->sStationID.au8Addr);

    _mtlk_core_connect_sta_blocked(nic, psConnect, FALSE);
    if (FM_STATUSCODE_ASSOCIATED == reason) {
      _mtlk_core_on_association_event(nic->vap_handle, psConnect->sStationID,
                                      FM_STATUSCODE_SUCCESSFUL);
    }
    break;
  case UMI_RECONNECTED:
    ILOG2_DDY("CID-%04x: UMI_RECONNECTED. Reason %d, %Y",
      mtlk_vap_get_oid(nic->vap_handle), reason, psConnect->sStationID.au8Addr);

    _mtlk_core_connect_sta_blocked(nic, psConnect, TRUE);
    break;
  case UMI_DISCONNECTED:
    ILOG2_DDDY("CID-%04x: UMI_DISCONNECTED. Reason %d, FailReason %d, %Y",
      mtlk_vap_get_oid(nic->vap_handle), reason, fail_reason, psConnect->sStationID.au8Addr);

    if((fail_reason == FM_STATUSCODE_RESERVED) && (reason == UMI_BSS_DISASSOCIATED))
    {
      /* Do not send UMI_DISCONNECT back to FW */
    }
    else if((fail_reason == FM_STATUSCODE_MAC_ADDRESS_FILTER) && (reason == UMI_BSS_AUTH_FAILED))
    {
      /* Blacklisted station tried to connect */
    }
    else
    {
      _mtlk_core_disconnect_sta_blocked(nic, &psConnect->sStationID,
                                        reason, fail_reason, FRAME_REASON_UNSPECIFIED);
    }

    if ((UMI_BSS_ASSOC_FAILED == reason) ||
        ((fail_reason == FM_STATUSCODE_MAC_ADDRESS_FILTER) && (reason == UMI_BSS_AUTH_FAILED))) {
      _mtlk_core_on_association_event(nic->vap_handle, psConnect->sStationID, fail_reason);
    }
    break;
  case UMI_AUTHENTICATION:
    _mtlk_core_on_authentication_event(nic->vap_handle, psConnect->sStationID,
                                       reason,
                                       fail_reason);
    break;
  case UMI_ASSOCIATION:
    MTLK_ASSERT(UMI_BSS_ASSOC_FAILED == reason);
    _mtlk_core_on_association_event(nic->vap_handle,
                                    psConnect->sStationID,
                                    fail_reason);
    break;
  default:
    ELOG_DD("CID-%04x: Unrecognized connection event %d", mtlk_vap_get_oid(nic->vap_handle), event);
    break;
  }

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_fw_connection_event_indication(mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);

  if(!mtlk_vap_is_ap(nic->vap_handle)) {
    _mtlk_core_trigger_connect_complete_event(nic, TRUE);
  }
  _mtlk_process_hw_task(nic, SERIALIZABLE, _handle_sta_connection_event,
                        core_object, payload, size);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_vap_removed_ind (mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);

  MTLK_UNREFERENCED_PARAM(payload);
  ILOG1_V("VAP removal indication received");
  mtlk_osal_event_set(&nic->slow_ctx->vap_removed_event);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
handleDisconnectSta (mtlk_handle_t core_object, const void *payload, uint32 size)
{
  struct nic *nic = HANDLE_T_PTR(struct nic, core_object);
  int                                   res;
  struct mtlk_core_disconnect_sta_data *data =
    (struct mtlk_core_disconnect_sta_data *)payload;

  MTLK_ASSERT(size == sizeof(*data));

  res = _mtlk_core_disconnect_sta_blocked(nic, &data->addr, UMI_BSS_UNSPECIFIED, data->fail_reason, data->reason_code);

  if (data->res) {
    *data->res = res;
  }
  if (data->done_evt) {
    mtlk_osal_event_set(data->done_evt);
  }

  return MTLK_ERR_OK;
}

static int
_handle_dynamic_param_ind(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);
  UMI_DYNAMIC_PARAM_TABLE *param_table = (UMI_DYNAMIC_PARAM_TABLE *) data;
  int i;

  MTLK_ASSERT(sizeof(UMI_DYNAMIC_PARAM_TABLE) == data_size);

  for (i = 0; i < NTS_PRIORITIES; i++)
    ILOG5_DD("Set ACM bit for priority %d: %d", i, param_table->ACM_StateTable[i]);

  mtlk_qos_set_acm_bits(&nic->qos, param_table->ACM_StateTable);
  return MTLK_ERR_OK;
}

static int
_handle_security_alert_ind(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);
  UMI_SECURITY_ALERT *usa = (UMI_SECURITY_ALERT*)data;
  MTLK_ASSERT(sizeof(UMI_SECURITY_ALERT) == data_size);

  switch (usa->u16EventCode) {
  case UMI_RSN_EVENT_TKIP_MIC_FAILURE:
    {
      mtlk_df_ui_mic_fail_type_t mic_fail_type =
          (UMI_RSN_PAIRWISE_KEY == usa->u16KeyType) ? MIC_FAIL_PAIRWISE : MIC_FAIL_GROUP;

      mtlk_df_ui_notify_mic_failure(
          mtlk_vap_get_df(nic->vap_handle), usa->sStationID.au8Addr, mic_fail_type);

      _mtlk_core_on_mic_failure(nic, mic_fail_type);
    }
    break;
  }
  return MTLK_ERR_OK;
}

/* steps for init and cleanup */
MTLK_INIT_STEPS_LIST_BEGIN(core_slow_ctx)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, DFS)
#ifdef EEPROM_DATA_VALIDATION
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, EEPROM)
#endif /* EEPROM_DATA_VALIDATION */
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, SERIALIZER)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, SET_NIC_CFG)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, WATCHDOG_TIMER_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, CONNECT_EVENT_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, VAP_REMOVED_EVENT_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, STADB_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core_slow_ctx, HSTDB_INIT)
MTLK_INIT_INNER_STEPS_BEGIN(core_slow_ctx)
MTLK_INIT_STEPS_LIST_END(core_slow_ctx);

static void __MTLK_IFUNC
_mtlk_slow_ctx_cleanup(struct nic_slow_ctx *slow_ctx, struct nic* nic)
{
  MTLK_ASSERT(NULL != slow_ctx);
  MTLK_ASSERT(NULL != nic);

  MTLK_CLEANUP_BEGIN(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx))

    MTLK_CLEANUP_STEP(core_slow_ctx, HSTDB_INIT, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_hstdb_cleanup, (&slow_ctx->hstdb));

    MTLK_CLEANUP_STEP(core_slow_ctx, STADB_INIT, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_stadb_cleanup, (&slow_ctx->stadb));

    MTLK_CLEANUP_STEP(core_slow_ctx, VAP_REMOVED_EVENT_INIT, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_osal_event_cleanup, (&slow_ctx->vap_removed_event));

    MTLK_CLEANUP_STEP(core_slow_ctx, CONNECT_EVENT_INIT, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_osal_event_cleanup, (&slow_ctx->connect_event));

    MTLK_CLEANUP_STEP(core_slow_ctx, WATCHDOG_TIMER_INIT, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_osal_timer_cleanup, (&slow_ctx->mac_watchdog_timer));

    MTLK_CLEANUP_STEP(core_slow_ctx, SET_NIC_CFG, MTLK_OBJ_PTR(slow_ctx),
                      MTLK_NOACTION, ());

    MTLK_CLEANUP_STEP(core_slow_ctx, SERIALIZER, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_serializer_cleanup, (&slow_ctx->serializer));

#ifdef EEPROM_DATA_VALIDATION
    MTLK_CLEANUP_STEP(core_slow_ctx, EEPROM, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_eeprom_fw_access_cleanup, (nic->vap_handle));
#endif /* EEPROM_DATA_VALIDATION */

    MTLK_CLEANUP_STEP(core_slow_ctx, DFS, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_dfs_delete, (slow_ctx->dot11h) )

  MTLK_CLEANUP_END(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx));
}

static int __MTLK_IFUNC
_mtlk_slow_ctx_init(struct nic_slow_ctx *slow_ctx, struct nic* nic)
{
  MTLK_ASSERT(NULL != slow_ctx);
  MTLK_ASSERT(NULL != nic);

  memset(slow_ctx, 0, sizeof(struct nic_slow_ctx));
  slow_ctx->nic = nic;

  MTLK_INIT_TRY(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx))

    MTLK_INIT_STEP_EX_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, DFS, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_dfs_create, (),
                      slow_ctx->dot11h, NULL != slow_ctx->dot11h, MTLK_ERR_NO_MEM);

#ifdef EEPROM_DATA_VALIDATION
    MTLK_INIT_STEP_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, EEPROM, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_eeprom_fw_access_init, (nic->vap_handle));
#endif /* EEPROM_DATA_VALIDATION */

    MTLK_INIT_STEP(core_slow_ctx, SERIALIZER, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_serializer_init, (&slow_ctx->serializer, _MTLK_CORE_NUM_PRIORITIES));

    MTLK_INIT_STEP_VOID(core_slow_ctx, SET_NIC_CFG, MTLK_OBJ_PTR(slow_ctx),
                        mtlk_mib_set_nic_cfg, (nic));

    MTLK_INIT_STEP_IF(!mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, WATCHDOG_TIMER_INIT, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_osal_timer_init, (&slow_ctx->mac_watchdog_timer,
                                             mac_watchdog_timer_handler,
                                             HANDLE_T(nic)));

    MTLK_INIT_STEP(core_slow_ctx, CONNECT_EVENT_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_osal_event_init, (&slow_ctx->connect_event));

    MTLK_INIT_STEP(core_slow_ctx, VAP_REMOVED_EVENT_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_osal_event_init, (&slow_ctx->vap_removed_event));

    MTLK_INIT_STEP(core_slow_ctx, STADB_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_stadb_init, (&slow_ctx->stadb, nic->vap_handle));

    MTLK_INIT_STEP(core_slow_ctx, HSTDB_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_hstdb_init, (&slow_ctx->hstdb, nic->vap_handle));

    slow_ctx->last_pm_spectrum = -1;
    slow_ctx->last_pm_freq = MTLK_HW_BAND_NONE;

    /* Initialize WEP keys */
    slow_ctx->wep_keys.sKey[0].u8KeyLength =
    slow_ctx->wep_keys.sKey[1].u8KeyLength =
    slow_ctx->wep_keys.sKey[2].u8KeyLength =
    slow_ctx->wep_keys.sKey[3].u8KeyLength =
    MIB_WEP_KEY_WEP1_LENGTH;

    nic->slow_ctx->tx_limits.num_tx_antennas = DEFAULT_NUM_TX_ANTENNAS;
    nic->slow_ctx->tx_limits.num_rx_antennas = DEFAULT_NUM_RX_ANTENNAS;

    nic->slow_ctx->deactivate_ts = INVALID_DEACTIVATE_TIMESTAMP;

  MTLK_INIT_FINALLY(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx))
  MTLK_INIT_RETURN(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx), _mtlk_slow_ctx_cleanup, (nic->slow_ctx, nic))
}

static int
_mtlk_core_sq_init(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);
  /* in case if it is Virtual AP, we get Master SQ from outside, otherwise, create it */
  if (mtlk_vap_is_slave_ap(core->vap_handle)) {
    mtlk_core_t *master_nic = mtlk_core_get_master(core);
    core->sq               = master_nic->sq;
    core->sq_flush_tasklet = master_nic->sq_flush_tasklet;
    return MTLK_ERR_OK;
  }
  return sq_init(core);
}

static void
_mtlk_core_sq_cleanup(mtlk_core_t *core)
{
  /* do nothing in case of slave core */
  if (mtlk_vap_is_slave_ap(core->vap_handle)) {
    core->sq               = NULL;
    core->sq_flush_tasklet = NULL;
  }
  else {
    sq_cleanup(core);
  }
}

static void
_mtlk_core_ps_init(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  mtlk_osal_atomic_set(&core->stas_in_ps_mode, 0);
  core->uapsd_enabled = UAPSD_ENABLED_DEFAULT;
  core->uapsd_max_sp = UAPSD_MAX_SP_USER_DEFAULT;
}

static void
_mtlk_core_hw_flctrl_delete(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  if(!mtlk_vap_is_slave_ap(core->vap_handle)) {
    MTLK_ASSERT(core->hw_tx_flctrl != NULL);
    mtlk_flctrl_cleanup(core->hw_tx_flctrl);
    mtlk_osal_mem_free(core->hw_tx_flctrl);
  }
  core->hw_tx_flctrl = NULL;
}

static void  __MTLK_IFUNC
_mtlk_core_hw_flctrl_start_data (mtlk_handle_t ctx)
{
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, ctx);

  mtlk_sq_tx_enable(core->sq);
  mtlk_sq_schedule_flush(core);
}

static void __MTLK_IFUNC
_mtlk_core_hw_flctrl_stop_data (mtlk_handle_t ctx)
{
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, ctx);

  mtlk_sq_tx_disable(core->sq);
}

static int
_mtlk_core_hw_flctrl_create(mtlk_core_t *core)
{
  int result = MTLK_ERR_OK;
  MTLK_ASSERT(NULL != core);

  /* Allocate and init Flow Control object only for Master AP or STA */
  if(!mtlk_vap_is_slave_ap(core->vap_handle)) {
    mtlk_flctrl_api_t hw_flctrl_cfg;

    core->hw_tx_flctrl = mtlk_osal_mem_alloc(sizeof(mtlk_flctrl_t), MTLK_MEM_TAG_FLCTRL);
    if (NULL == core->hw_tx_flctrl) {
      result = MTLK_ERR_NO_MEM;
      goto err_flctrl_alloc;
    }

    hw_flctrl_cfg.ctx        = HANDLE_T(core);
    hw_flctrl_cfg.start_data = _mtlk_core_hw_flctrl_start_data;
    hw_flctrl_cfg.stop_data  = _mtlk_core_hw_flctrl_stop_data;
    result = mtlk_flctrl_init(core->hw_tx_flctrl, &hw_flctrl_cfg);
    if (MTLK_ERR_OK != result) {
      goto err_flctrl_init;
    }
  }
  else {
    /* In case of Slave AP, use Master AP Flow Control Object */
    core->hw_tx_flctrl = mtlk_core_get_master(core)->hw_tx_flctrl;
  }

  return MTLK_ERR_OK;

err_flctrl_init:
  mtlk_osal_mem_free(core->hw_tx_flctrl);
  core->hw_tx_flctrl = NULL;
err_flctrl_alloc:
  return result;
}

/* steps for init and cleanup */
MTLK_INIT_STEPS_LIST_BEGIN(core)
  MTLK_INIT_STEPS_LIST_ENTRY(core, CORE_PDB_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core, SLOW_CTX_ALLOC)
  MTLK_INIT_STEPS_LIST_ENTRY(core, SLOW_CTX_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core, SQ_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core, SQ_BROADCAST_CTX_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core, L2NAT_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core, NET_STATE_LOCK_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core, QOS_INIT)
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  MTLK_INIT_STEPS_LIST_ENTRY(core, RF_MGMT_CREATE)
#endif
  MTLK_INIT_STEPS_LIST_ENTRY(core, FLCTRL_CREATE)
  MTLK_INIT_STEPS_LIST_ENTRY(core, TXMM_EEPROM_ASYNC_MSGS_INIT)
  MTLK_INIT_STEPS_LIST_ENTRY(core, PS_INIT)
MTLK_INIT_INNER_STEPS_BEGIN(core)
MTLK_INIT_STEPS_LIST_END(core);

static void __MTLK_IFUNC
_mtlk_core_cleanup(struct nic* nic)
{
  int i;

  MTLK_ASSERT(NULL != nic);
  MTLK_CLEANUP_BEGIN(core, MTLK_OBJ_PTR(nic))
    MTLK_CLEANUP_STEP(core, PS_INIT, MTLK_OBJ_PTR(nic),
                      MTLK_NOACTION, ())

    for (i = 0; i < ARRAY_SIZE(nic->txmm_async_eeprom_msgs); i++) {
      MTLK_CLEANUP_STEP_LOOP(core, TXMM_EEPROM_ASYNC_MSGS_INIT, MTLK_OBJ_PTR(nic),
                             mtlk_txmm_msg_cleanup, (&nic->txmm_async_eeprom_msgs[i]));
    }

    MTLK_CLEANUP_STEP(core, FLCTRL_CREATE, MTLK_OBJ_PTR(nic),
                      _mtlk_core_hw_flctrl_delete, (nic))

#ifdef MTCFG_RF_MANAGEMENT_MTLK
    MTLK_CLEANUP_STEP(core, RF_MGMT_CREATE, MTLK_OBJ_PTR(nic),
                      mtlk_rf_mgmt_delete, (nic->rf_mgmt))
#endif

    MTLK_CLEANUP_STEP(core, QOS_INIT, MTLK_OBJ_PTR(nic),
                      mtlk_qos_cleanup, (&nic->qos, nic->vap_handle));

    MTLK_CLEANUP_STEP(core, NET_STATE_LOCK_INIT, MTLK_OBJ_PTR(nic),
                      mtlk_osal_lock_cleanup, (&nic->net_state_lock));

    MTLK_CLEANUP_STEP(core, L2NAT_INIT, MTLK_OBJ_PTR(nic),
                      mtlk_l2nat_cleanup, (&nic->l2nat, nic));

    MTLK_CLEANUP_STEP(core, SQ_BROADCAST_CTX_INIT, MTLK_OBJ_PTR(nic),
                      mtlk_sq_peer_ctx_cleanup, (nic->sq, &nic->sq_broadcast_ctx));

    MTLK_CLEANUP_STEP(core, SQ_INIT, MTLK_OBJ_PTR(nic),
                      _mtlk_core_sq_cleanup, (nic));

    MTLK_CLEANUP_STEP(core, SLOW_CTX_INIT, MTLK_OBJ_PTR(nic),
                      _mtlk_slow_ctx_cleanup, (nic->slow_ctx, nic));

    MTLK_CLEANUP_STEP(core, SLOW_CTX_ALLOC, MTLK_OBJ_PTR(nic),
                      kfree_tag, (nic->slow_ctx));

    MTLK_CLEANUP_STEP(core, CORE_PDB_INIT, MTLK_OBJ_PTR(nic),
        mtlk_core_pdb_fast_handles_close, (nic->pdb_hot_path_handles));

  MTLK_CLEANUP_END(core, MTLK_OBJ_PTR(nic));
}

static int __MTLK_IFUNC
_mtlk_core_init(struct nic* nic, mtlk_vap_handle_t vap_handle, mtlk_df_t*   df)
{
  int txem_cnt = 0;

  MTLK_ASSERT(NULL != nic);

  MTLK_INIT_TRY(core, MTLK_OBJ_PTR(nic))
    /* set initial net state */
    nic->net_state   = NET_STATE_HALTED;
    nic->vap_handle  = vap_handle;

    MTLK_INIT_STEP(core, CORE_PDB_INIT, MTLK_OBJ_PTR(nic),
        mtlk_core_pdb_fast_handles_open, (mtlk_vap_get_param_db(nic->vap_handle), nic->pdb_hot_path_handles));

    MTLK_INIT_STEP_EX(core, SLOW_CTX_ALLOC, MTLK_OBJ_PTR(nic),
                      kmalloc_tag, (sizeof(struct nic_slow_ctx), GFP_KERNEL, MTLK_MEM_TAG_CORE),
                      nic->slow_ctx, NULL != nic->slow_ctx, MTLK_ERR_NO_MEM);

    MTLK_INIT_STEP(core, SLOW_CTX_INIT, MTLK_OBJ_PTR(nic),
                   _mtlk_slow_ctx_init, (nic->slow_ctx, nic));

    MTLK_INIT_STEP(core, SQ_INIT, MTLK_OBJ_PTR(nic),
                   _mtlk_core_sq_init, (nic));

    MTLK_INIT_STEP_VOID(core, SQ_BROADCAST_CTX_INIT, MTLK_OBJ_PTR(nic),
                        mtlk_sq_peer_ctx_init, (nic->sq, &nic->sq_broadcast_ctx, MTLK_SQ_TX_LIMIT_DEFAULT, vap_handle));

    MTLK_INIT_STEP(core, L2NAT_INIT, MTLK_OBJ_PTR(nic),
                   mtlk_l2nat_init, (&nic->l2nat, nic));

    MTLK_INIT_STEP(core, NET_STATE_LOCK_INIT, MTLK_OBJ_PTR(nic),
                   mtlk_osal_lock_init, (&nic->net_state_lock));

    MTLK_INIT_STEP(core, QOS_INIT, MTLK_OBJ_PTR(nic),
                   mtlk_qos_init, (&nic->qos, nic->vap_handle));

#ifdef MTCFG_RF_MANAGEMENT_MTLK
    MTLK_INIT_STEP_EX(core, RF_MGMT_CREATE, MTLK_OBJ_PTR(nic),
                      mtlk_rf_mgmt_create, (), nic->rf_mgmt,
                      nic->rf_mgmt != NULL, MTLK_ERR_UNKNOWN);
#endif

    MTLK_INIT_STEP(core, FLCTRL_CREATE, MTLK_OBJ_PTR(nic),
                   _mtlk_core_hw_flctrl_create, (nic))

    for (txem_cnt = 0; txem_cnt < ARRAY_SIZE(nic->txmm_async_eeprom_msgs); txem_cnt++) {
      MTLK_INIT_STEP_LOOP(core, TXMM_EEPROM_ASYNC_MSGS_INIT, MTLK_OBJ_PTR(nic),
                          mtlk_txmm_msg_init, (&nic->txmm_async_eeprom_msgs[txem_cnt]));
    }

    MTLK_INIT_STEP_VOID(core, PS_INIT, MTLK_OBJ_PTR(nic),
                        _mtlk_core_ps_init, (nic))

    nic->is_stopped = TRUE;

  MTLK_INIT_FINALLY(core, MTLK_OBJ_PTR(nic))
  MTLK_INIT_RETURN(core, MTLK_OBJ_PTR(nic), _mtlk_core_cleanup, (nic))
}

static int _mtlk_core_update_param_db(struct nic* nic)
{
    /* reset TWO_ANT_TX_ENABLE if antenna limits is not 3x3 */
    if (!_mtlk_core_tx_limits_antennas_is_3x3(nic)) {
        MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_TWO_ANT_TX_ENABLE, 0);
    }

    return MTLK_ERR_OK;
}

static int _mtlk_core_create_20_40(struct nic* nic)
{
  mtlk_20_40_csm_xfaces_t xfaces;
  struct _mtlk_20_40_coexistence_sm *coex_sm;
  BOOL is_ap;
  uint32 max_number_of_connected_stations;
  int ret_val = MTLK_ERR_NO_MEM;

  xfaces.context = (mtlk_handle_t)nic;
  xfaces.vap_handle = nic->vap_handle;
  xfaces.switch_cb_mode = &_mtlk_core_switch_cb_mode_callback;
  xfaces.send_ce = &_mtlk_core_send_ce_callback;
  xfaces.send_cmf = &_mtlk_core_send_cmf_callback;
  xfaces.send_exempt_policy = &_mtlk_core_send_exemption_policy_callback;
  xfaces.scan_async = &_mtlk_core_scan_async_callback;
  xfaces.scan_set_background = &_mtlk_core_scan_set_obss_callback;
  xfaces.register_scan_completion_notification_callback = &_mtlk_core_register_scan_completion_notification_callback;
#ifdef BT_ACS_DEBUG
  xfaces.enumerate_bss_info = (mtlk_core_get_freq_band_cfg(nic) == MTLK_HW_BAND_2_4_GHZ)
    ? &_mtlk_core_enumerate_bss_info_by_aocs_callback : &_mtlk_core_enumerate_bss_info_callback;
#else
  xfaces.enumerate_bss_info = &_mtlk_core_enumerate_bss_info_callback;
#endif /* BT_ACS_DEBUG */
  xfaces.ability_control = &_mtlk_core_ability_control_callback;
  xfaces.get_cur_channels = &_mtlk_core_get_cur_channels_callback;
  xfaces.modify_cache_expire_time = &_mtlk_core_modify_cache_expire_callback;
  xfaces.update_fw_obss_scan_params = &_mtlk_core_update_fw_obss_scan_parameters_callback;

  is_ap = mtlk_vap_is_ap(nic->vap_handle);
  if (is_ap) {
    max_number_of_connected_stations = mtlk_core_get_max_stas_supported_by_fw(nic);
  } else {
    max_number_of_connected_stations = 0;
  }
  coex_sm = mtlk_20_40_create(&xfaces, is_ap, max_number_of_connected_stations);

  if (coex_sm) {
    mtlk_core_set_coex_sm(nic, coex_sm);
    ret_val = MTLK_ERR_OK;
  }

  return ret_val;
}

static void _mtlk_core_delete_20_40(struct nic* nic)
{
  struct _mtlk_20_40_coexistence_sm *coex_sm = mtlk_core_get_coex_sm(nic);
  if (coex_sm)
  {
    mtlk_20_40_delete(coex_sm);
    mtlk_core_set_coex_sm(nic, NULL);
  }
}

struct _mtlk_20_40_coexistence_sm * mtlk_core_get_coex_sm(mtlk_core_t *core)
{
  return mtlk_core_get_master(core)->coex_20_40_sm;
}

BOOL mtlk_core_is_20_40_active(struct nic* nic)
{
  BOOL res = FALSE;

  if(mtlk_vap_is_ap(nic->vap_handle))
  {
    res = (mtlk_20_40_is_feature_enabled(mtlk_core_get_coex_sm(nic)) && (mtlk_core_get_freq_band_cfg(nic) == MTLK_HW_BAND_2_4_GHZ)) ? TRUE : FALSE;
  }
  else
  {
    if (mtlk_core_get_net_state(nic) == NET_STATE_CONNECTED)
    {
      res = (mtlk_20_40_is_feature_enabled(mtlk_core_get_coex_sm(nic)) && (mtlk_core_get_freq_band_cur(nic) == MTLK_HW_BAND_2_4_GHZ)) ? TRUE : FALSE;
    }
    else
    {
      res = mtlk_20_40_is_feature_enabled(mtlk_core_get_coex_sm(nic)) ? TRUE : FALSE;
    }

  }

  return res;
}

void __MTLK_IFUNC
mtlk_core_interfdet_enable (struct nic *core, BOOL enable_flag)
{
  MTLK_ASSERT(mtlk_vap_is_master_ap(core->vap_handle));
  core->is_interfdet_enabled = enable_flag;
}

BOOL __MTLK_IFUNC
mtlk_core_is_interfdet_enabled (struct nic *core)
{
  BOOL res = FALSE;

  MTLK_ASSERT(core != NULL);

  if (mtlk_vap_is_master_ap(core->vap_handle)) {
    res = (BOOL)mtlk_core_get_master(core)->is_interfdet_enabled;
  }

  return res;
}

int __MTLK_IFUNC
mtlk_core_api_init (mtlk_vap_handle_t vap_handle, mtlk_core_api_t *core_api, mtlk_df_t* df)
{
  int res;
  mtlk_core_t *core;

  MTLK_ASSERT(NULL != core_api);

  /* initialize function table */
  core_api->vft = &core_vft;

  core = mtlk_fast_mem_alloc(MTLK_FM_USER_CORE, sizeof(mtlk_core_t));
  if(NULL == core) {
    return MTLK_ERR_NO_MEM;
  }

  memset(core, 0, sizeof(mtlk_core_t));

  res = _mtlk_core_init(core, vap_handle, df);
  if (MTLK_ERR_OK != res) {
    mtlk_fast_mem_free(core);
    return res;
  }

  core_api->core_handle = HANDLE_T(core);

  return MTLK_ERR_OK;
}

static int
mtlk_core_master_set_default_band(struct nic *nic)
{
  uint8 freq_band_cfg = MTLK_HW_BAND_NONE;
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(nic->vap_handle));

  if (mtlk_core_is_band_supported(nic, MTLK_HW_BAND_BOTH) == MTLK_ERR_OK) {
    freq_band_cfg = MTLK_HW_BAND_BOTH;
  } else if (mtlk_core_is_band_supported(nic, MTLK_HW_BAND_5_2_GHZ) == MTLK_ERR_OK) {
    freq_band_cfg = MTLK_HW_BAND_5_2_GHZ;
  } else if (mtlk_core_is_band_supported(nic, MTLK_HW_BAND_2_4_GHZ) == MTLK_ERR_OK) {
    freq_band_cfg = MTLK_HW_BAND_2_4_GHZ;
  } else {
    ELOG_D("CID-%04x: None of the bands is supported", mtlk_vap_get_oid(nic->vap_handle));
    return MTLK_ERR_UNKNOWN;
  }

  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_FREQ_BAND_CFG, freq_band_cfg);
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_FREQ_BAND_CUR, freq_band_cfg);

  return MTLK_ERR_OK;
}

static int
mtlk_core_set_default_net_mode(struct nic *nic)
{
  uint8 freq_band_cfg = MTLK_HW_BAND_NONE;
  uint8 net_mode = NUM_OF_NETWORK_MODES;
  mtlk_core_t *core_master = NULL;

  MTLK_ASSERT(nic != NULL);

  if(!mtlk_vap_is_slave_ap(nic->vap_handle))
  {
    /* Set default mode based on band
     * for Master AP or STA */
    freq_band_cfg = mtlk_core_get_freq_band_cfg(nic);
    net_mode = get_net_mode(freq_band_cfg, TRUE);
  }
  else
  {
    /* Copy default mode from Master VAP.
     * This is important while scripts
     * are not ready for network mode per VAP. */
    core_master = mtlk_core_get_master(nic);
    MTLK_ASSERT(core_master != NULL);
    net_mode = mtlk_core_get_network_mode_cfg(core_master);
  }

  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_NET_MODE_CUR, net_mode);
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_NET_MODE_CFG, net_mode);

  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_IS_HT_CUR, is_ht_net_mode(net_mode));
  MTLK_CORE_PDB_SET_INT(nic, PARAM_DB_CORE_IS_HT_CFG, is_ht_net_mode(net_mode));

  return MTLK_ERR_OK;
}

static int
_mtlk_core_process_antennas_configuration(mtlk_core_t *nic)
{
  int err = MTLK_ERR_OK;
  uint8 tx_val_array[MTLK_NUM_ANTENNAS_BUFSIZE] = {1, 2, 3, 0};
  uint8 rx_val_array[MTLK_NUM_ANTENNAS_BUFSIZE] = {1, 2, 3, 0};

  uint8 num_tx_antennas = nic->slow_ctx->tx_limits.num_tx_antennas;
  uint8 num_rx_antennas = nic->slow_ctx->tx_limits.num_rx_antennas;

  /* determine number of TX antennas */
  if (2 == num_tx_antennas)
  {
    tx_val_array[2] = 0;
  }
  else if (3 != num_tx_antennas)
  {
    MTLK_ASSERT(!"Wrong number of TX antennas");
    return MTLK_ERR_UNKNOWN;
  }

  /* determine number of RX antennas */
  if (2 == num_rx_antennas)
  {
    rx_val_array[2] = 0;
  }
  else if (3 != num_rx_antennas)
  {
    MTLK_ASSERT(!"Wrong number of RX antennas");
    return MTLK_ERR_UNKNOWN;
  }

  err = MTLK_CORE_PDB_SET_BINARY(nic, PARAM_DB_CORE_TX_ANTENNAS, tx_val_array, MTLK_NUM_ANTENNAS_BUFSIZE);
  if (MTLK_ERR_OK != err)
  {
    ILOG2_V("Can not save TX antennas configuration in to the PDB");
    return err;
  }

  err = MTLK_CORE_PDB_SET_BINARY(nic, PARAM_DB_CORE_RX_ANTENNAS, rx_val_array, MTLK_NUM_ANTENNAS_BUFSIZE);
  if (MTLK_ERR_OK != err)
  {
    ILOG2_V("Can not save RX antennas configuration in to the PDB");
    return err;
  }

  return MTLK_ERR_OK;
}

static void
_mtlk_core_aocs_on_channel_change(mtlk_vap_handle_t vap_handle, int channel)
{
  mtlk_pdb_set_int(mtlk_vap_get_param_db(vap_handle), PARAM_DB_CORE_CHANNEL_CUR, channel);
  ILOG3_D("Channel changed to %d", channel);
}

static void
_mtlk_core_aocs_on_bonding_change(mtlk_vap_handle_t vap_handle, uint8 bonding)
{
  mtlk_core_set_cur_bonding(mtlk_vap_get_core(vap_handle), bonding);
}

static void
_mtlk_core_aocs_on_spectrum_change(mtlk_vap_handle_t vap_handle, int spectrum)
{
    mtlk_core_t *nic = mtlk_vap_get_core(vap_handle);
    mtlk_pdb_t  *pdb = mtlk_vap_get_param_db(vap_handle);

    if (mtlk_pdb_get_int(pdb, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE) != spectrum) {
        ILOG0_DS("CID-%04x: Spectrum changed to %s0MHz", mtlk_vap_get_oid(vap_handle), spectrum == SPECTRUM_20MHZ ? "2" : "4");
    }

    mtlk_pdb_set_int(pdb, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE, spectrum);

    mtlk_dot11h_set_spectrum_mode(mtlk_core_get_dfs(nic), spectrum);
}

MTLK_START_STEPS_LIST_BEGIN(core_slow_ctx)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, SERIALIZER_START)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, SET_MAC_MAC_ADDR)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, PARSE_EE_DATA)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, CACHE_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, INIT_TX_LIMIT_TABLES)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, PROCESS_ANTENNA_CFG)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, PROCESS_COC)
#ifdef MTCFG_PMCU_SUPPORT
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, PROCESS_PCOC)
#endif /*MTCFG_PMCU_SUPPORT*/
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, AOCS_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, SCAN_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, DOT11H_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, ADDBA_AGGR_LIM_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, ADDBA_REORD_LIM_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, ADDBA_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, WDS_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, SET_DEFAULT_BAND)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, SET_DEFAULT_NET_MODE)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, WATCHDOG_TIMER_START)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, SERIALIZER_ACTIVATE)
  MTLK_START_STEPS_LIST_ENTRY(core_slow_ctx, CORE_STAT_REQ_HANDLER)
MTLK_START_INNER_STEPS_BEGIN(core_slow_ctx)
MTLK_START_STEPS_LIST_END(core_slow_ctx);

static void
_mtlk_core_get_wlan_stats(mtlk_core_t* core, mtlk_wssa_drv_wlan_stats_t* stats)
{
/* minimal statistic */
  stats->TxPacketsSucceeded                                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_PACKETS_SENT);
  stats->RxPacketsSucceeded                                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_PACKETS_RECEIVED);
  stats->TxBytesSucceeded                                   = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BYTES_SENT);
  stats->RxBytesSucceeded                                   = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BYTES_RECEIVED);
  stats->UnicastPacketsSent                                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_UNICAST_PACKETS_SENT);
  stats->UnicastPacketsReceived                             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_UNICAST_PACKETS_RECEIVED);
  stats->MulticastPacketsSent                               = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_MULTICAST_PACKETS_SENT);
  stats->MulticastPacketsReceived                           = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_MULTICAST_PACKETS_RECEIVED);
  stats->BroadcastPacketsSent                               = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BROADCAST_PACKETS_SENT);
  stats->BroadcastPacketsReceived                           = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BROADCAST_PACKETS_RECEIVED);
}

#if MTLK_MTIDL_WLAN_STAT_FULL
static void
_mtlk_core_get_debug_wlan_stats(mtlk_core_t* core, mtlk_wssa_drv_debug_wlan_stats_t* stats)
{
/* minimal statistic */
  stats->TxPacketsSucceeded                                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_PACKETS_SENT);
  stats->RxPacketsSucceeded                                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_PACKETS_RECEIVED);
  stats->TxBytesSucceeded                                   = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BYTES_SENT);
  stats->RxBytesSucceeded                                   = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BYTES_RECEIVED);
  stats->UnicastPacketsSent                                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_UNICAST_PACKETS_SENT);
  stats->UnicastPacketsReceived                             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_UNICAST_PACKETS_RECEIVED);
  stats->MulticastPacketsSent                               = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_MULTICAST_PACKETS_SENT);
  stats->MulticastPacketsReceived                           = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_MULTICAST_PACKETS_RECEIVED);
  stats->BroadcastPacketsSent                               = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BROADCAST_PACKETS_SENT);
  stats->BroadcastPacketsReceived                           = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BROADCAST_PACKETS_RECEIVED);

  /* full statistic */
  stats->RxPacketsDiscardedDrvTooOld                        = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_PACKETS_DISCARDED_DRV_TOO_OLD);
  stats->RxPacketsDiscardedDrvDuplicate                     = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_PACKETS_DISCARDED_DRV_DUPLICATE);
  stats->TxPacketsDiscardedDrvNoPeers                       = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_NO_PEERS);
  stats->TxPacketsDiscardedDrvACM                           = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_ACM);
  stats->TxPacketsDiscardedEapolCloned                      = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_EAPOL_CLONED);
  stats->TxPacketsDiscardedDrvUnknownDestinationDirected    = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_DIRECTED);
  stats->TxPacketsDiscardedDrvUnknownDestinationMcast       = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_UNKNOWN_DESTINATION_MCAST);
  stats->TxPacketsDiscardedDrvNoResources                   = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DRV_NO_RESOURCES);
  stats->TxPacketsDiscardedDrvSQOverflow                    = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_SQ_OVERFLOW);
  stats->TxPacketsDiscardedDrvEAPOLFilter                   = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_EAPOL_FILTER);
  stats->TxPacketsDiscardedDrvDropAllFilter                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_DROP_ALL_FILTER);
  stats->TxPacketsDiscardedDrvTXQueueOverflow               = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_TX_QUEUE_OVERFLOW);
  stats->RxPackets802_1x                                    = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_802_1X_PACKETS_RECEIVED);
  stats->TxPackets802_1x                                    = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_802_1X_PACKETS_SENT);
  stats->PairwiseMICFailurePackets                          = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_PAIRWISE_MIC_FAILURE_PACKETS);
  stats->GroupMICFailurePackets                             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_GROUP_MIC_FAILURE_PACKETS);
  stats->UnicastReplayedPackets                             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_UNICAST_REPLAYED_PACKETS);
  stats->MulticastReplayedPackets                           = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_MULTICAST_REPLAYED_PACKETS);
  stats->FwdRxPackets                                       = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_FWD_RX_PACKETS);
  stats->FwdRxBytes                                         = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_FWD_RX_BYTES);
  stats->MulticastBytesSent                                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_MULTICAST_BYTES_SENT);
  stats->MulticastBytesReceived                             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_MULTICAST_BYTES_RECEIVED);
  stats->BroadcastBytesSent                                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BROADCAST_BYTES_SENT);
  stats->BroadcastBytesReceived                             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_BROADCAST_BYTES_RECEIVED);
  stats->DATFramesReceived                                  = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_DAT_FRAMES_RECEIVED);
  stats->CTLFramesReceived                                  = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_CTL_FRAMES_RECEIVED);
  stats->MANFramesReceived                                  = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_MAN_FRAMES_RECEIVED);

  stats->CoexElReceived                                     = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_COEX_EL_RECEIVED);
  stats->ScanExRequested                                    = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_COEX_EL_SCAN_EXEMPTION_REQUESTED);
  stats->ScanExGranted                                      = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_COEX_EL_SCAN_EXEMPTION_GRANTED);
  stats->ScanExGrantCancelled                               = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_COEX_EL_SCAN_EXEMPTION_GRANT_CANCELLED);
  stats->SwitchChannel20To40                                = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_CHANNEL_SWITCH_20_TO_40);
  stats->SwitchChannel40To20                                = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_CHANNEL_SWITCH_40_TO_20);
  stats->SwitchChannel40To40                                = _mtlk_core_get_cnt(mtlk_core_get_master(core), MTLK_CORE_CNT_CHANNEL_SWITCH_40_TO_40);

  /* Aggregation info */
  stats->Aggregation.AggrActive                 = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_AGGR_ACTIVE);
  stats->Aggregation.ReordActive                = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_REORD_ACTIVE);
  stats->Aggregation.RxAddbaReqReceived         = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_ADDBA_REQ_RECEIVED);
  stats->Aggregation.RxAddbaResCfmdFail         = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_ADDBA_RES_CFMD_FAIL);
  stats->Aggregation.RxAddbaResCfmdSuccess      = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_ADDBA_RES_CFMD_SUCCESS);
  stats->Aggregation.RxAddbaResLost             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_ADDBA_RES_LOST);
  stats->Aggregation.RxAddbaResNegativeSent     = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_ADDBA_RES_NEGATIVE_SENT);
  stats->Aggregation.RxAddbaResNotCfmd          = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_ADDBA_RES_NOT_CFMD);
  stats->Aggregation.RxAddbaResPositiveSent     = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_ADDBA_RES_POSITIVE_SENT);
  stats->Aggregation.RxAddbaResReached          = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_ADDBA_RES_REACHED);
  stats->Aggregation.RxAddbaResRetransmissions  = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_ADDBA_RES_RETRANSMISSIONS);
  stats->Aggregation.RxBarWithoutReordering     = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_BAR_WITHOUT_REORDERING);
  stats->Aggregation.RxPktWithoutReordering     = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_AGGR_PKT_WITHOUT_REORDERING);
  stats->Aggregation.RxDelbaReqCfmdFail         = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_DELBA_REQ_CFMD_FAIL);
  stats->Aggregation.RxDelbaReqCfmdSuccess      = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_DELBA_REQ_CFMD_SUCCESS);
  stats->Aggregation.RxDelbaReqLost             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_DELBA_REQ_LOST);
  stats->Aggregation.RxDelbaReqNotCfmd          = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_DELBA_REQ_NOT_CFMD);
  stats->Aggregation.RxDelbaReqRcv              = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_DELBA_REQ_RCV);
  stats->Aggregation.RxDelbaReqReached          = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_DELBA_REQ_REACHED);
  stats->Aggregation.RxDelbaReqRetransmissions  = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_DELBA_REQ_RETRANSMISSIONS);
  stats->Aggregation.RxDelbaReqSent             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_DELBA_REQ_SENT);
  stats->Aggregation.RxDelbaSentByTimeout       = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_RX_DELBA_SENT_BY_TIMEOUT);

  stats->Aggregation.TxAckOnBarDetected         = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_ACK_ON_BAR_DETECTED);
  stats->Aggregation.TxAddbaReqCfmdFail         = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_ADDBA_REQ_CFMD_FAIL);
  stats->Aggregation.TxAddbaReqCfmdSuccess      = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_ADDBA_REQ_CFMD_SUCCESS);
  stats->Aggregation.TxAddbaReqNotCfmd          = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_ADDBA_REQ_NOT_CFMD);
  stats->Aggregation.TxAddbaReqSent             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_ADDBA_REQ_SENT);
  stats->Aggregation.TxAddbaResRcvNegative      = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_ADDBA_RES_RCV_NEGATIVE);
  stats->Aggregation.TxAddbaResRcvPositive      = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_ADDBA_RES_RCV_POSITIVE);
  stats->Aggregation.TxAddbaResTimeout          = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_ADDBA_RES_TIMEOUT);
  stats->Aggregation.TxCloseAggrCfmdFail        = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_CLOSE_AGGR_CFMD_FAIL);
  stats->Aggregation.TxCloseAggrCfmdSuccess     = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_CLOSE_AGGR_CFMD_SUCCESS);
  stats->Aggregation.TxCloseAggrNotCfmd         = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_CLOSE_AGGR_NOT_CFMD);
  stats->Aggregation.TxCloseAggrSent            = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_CLOSE_AGGR_SENT);
  stats->Aggregation.TxDelbaReqCfmdFail         = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_DELBA_REQ_CFMD_FAIL);
  stats->Aggregation.TxDelbaReqCfmdSuccess      = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_DELBA_REQ_CFMD_SUCCESS);
  stats->Aggregation.TxDelbaReqLost             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_DELBA_REQ_LOST);
  stats->Aggregation.TxDelbaReqNotCfmd          = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_DELBA_REQ_NOT_CFMD);
  stats->Aggregation.TxDelbaReqRcv              = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_DELBA_REQ_RCV);
  stats->Aggregation.TxDelbaReqReached          = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_DELBA_REQ_REACHED);
  stats->Aggregation.TxDelbaReqRetransmissions  = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_DELBA_REQ_RETRANSMISSIONS);
  stats->Aggregation.TxDelbaReqSent             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_DELBA_REQ_SENT);
  stats->Aggregation.TxOpenAggrCfmdFail         = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_OPEN_AGGR_CFMD_FAIL);
  stats->Aggregation.TxOpenAggrCfmdSuccess      = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_OPEN_AGGR_CFMD_SUCCESS);
  stats->Aggregation.TxOpenAggrNotCfmd          = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_OPEN_AGGR_NOT_CFMD);
  stats->Aggregation.TxOpenAggrSent             = _mtlk_core_get_cnt(core, MTLK_CORE_CNT_TX_OPEN_AGGR_SENT);
}
#endif /* MTLK_MTIDL_WLAN_STAT_FULL */

static void __MTLK_IFUNC
_mtlk_core_stat_handle_request(mtlk_irbd_t       *irbd,
                               mtlk_handle_t      context,
                               const mtlk_guid_t *evt,
                               void              *buffer,
                               uint32            *size)
{
  struct nic_slow_ctx  *slow_ctx = HANDLE_T_PTR(struct nic_slow_ctx, context);
  mtlk_wssa_info_hdr_t *hdr = (mtlk_wssa_info_hdr_t *) buffer;

  MTLK_UNREFERENCED_PARAM(evt);

  if(sizeof(mtlk_wssa_info_hdr_t) > *size)
    return;

  if(MTIDL_SRC_DRV == hdr->info_source)
  {
    switch(hdr->info_id)
    {
    case MTLK_WSSA_DRV_STATUS_WLAN:
      {
        if(sizeof(mtlk_wssa_drv_wlan_stats_t) + sizeof(mtlk_wssa_info_hdr_t) > *size)
        {
          hdr->processing_result = MTLK_ERR_BUF_TOO_SMALL;
        }
        else
        {
          _mtlk_core_get_wlan_stats(slow_ctx->nic, (mtlk_wssa_drv_wlan_stats_t*) &hdr[1]);
          hdr->processing_result = MTLK_ERR_OK;
          *size = sizeof(mtlk_wssa_drv_wlan_stats_t) + sizeof(mtlk_wssa_info_hdr_t);
        }
      }
      break;
#if MTLK_MTIDL_WLAN_STAT_FULL
    case MTLK_WSSA_DRV_DEBUG_STATUS_WLAN:
      {
        if(sizeof(mtlk_wssa_drv_debug_wlan_stats_t) + sizeof(mtlk_wssa_info_hdr_t) > *size)
        {
          hdr->processing_result = MTLK_ERR_BUF_TOO_SMALL;
        }
        else
        {
          _mtlk_core_get_debug_wlan_stats(slow_ctx->nic, (mtlk_wssa_drv_debug_wlan_stats_t*) &hdr[1]);
          hdr->processing_result = MTLK_ERR_OK;
          *size = sizeof(mtlk_wssa_drv_debug_wlan_stats_t) + sizeof(mtlk_wssa_info_hdr_t);
        }
      }
      break;
#endif /* MTLK_MTIDL_WLAN_STAT_FULL */
    default:
      {
        hdr->processing_result = MTLK_ERR_NO_ENTRY;
        *size = sizeof(mtlk_wssa_info_hdr_t);
      }
    }
  }
  else
  {
    hdr->processing_result = MTLK_ERR_NO_ENTRY;
    *size = sizeof(mtlk_wssa_info_hdr_t);
  }
}

static void __MTLK_IFUNC
_mtlk_slow_ctx_stop(struct nic_slow_ctx *slow_ctx, struct nic* nic)
{
  MTLK_ASSERT(NULL != slow_ctx);
  MTLK_ASSERT(NULL != nic);

  MTLK_STOP_BEGIN(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx))
    MTLK_STOP_STEP(core_slow_ctx, CORE_STAT_REQ_HANDLER, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_wssd_unregister_request_handler, (mtlk_vap_get_irbd(nic->vap_handle), slow_ctx->stat_irb_handle));

    MTLK_STOP_STEP(core_slow_ctx, SERIALIZER_ACTIVATE, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_serializer_stop, (&slow_ctx->serializer))

    MTLK_STOP_STEP(core_slow_ctx, WATCHDOG_TIMER_START, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_osal_timer_cancel_sync, (&slow_ctx->mac_watchdog_timer))

    MTLK_STOP_STEP(core_slow_ctx, SET_DEFAULT_NET_MODE, MTLK_OBJ_PTR(slow_ctx),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core_slow_ctx, SET_DEFAULT_BAND, MTLK_OBJ_PTR(slow_ctx),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core_slow_ctx, WDS_INIT, MTLK_OBJ_PTR(slow_ctx),
                   wds_cleanup, (&slow_ctx->wds_mng))

    MTLK_STOP_STEP(core_slow_ctx, ADDBA_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_addba_cleanup, (&slow_ctx->addba))

    MTLK_STOP_STEP(core_slow_ctx, ADDBA_REORD_LIM_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_reflim_cleanup, (&slow_ctx->addba_lim_reord))

    MTLK_STOP_STEP(core_slow_ctx, ADDBA_AGGR_LIM_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_reflim_cleanup, (&slow_ctx->addba_lim_aggr))

    MTLK_STOP_STEP(core_slow_ctx, DOT11H_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_dot11h_cleanup, (mtlk_core_get_dfs(nic)))

    MTLK_STOP_STEP(core_slow_ctx, SCAN_INIT, MTLK_OBJ_PTR(slow_ctx),
                    mtlk_scan_cleanup, (&slow_ctx->scan))

    MTLK_STOP_STEP(core_slow_ctx, AOCS_INIT, MTLK_OBJ_PTR(slow_ctx),
                    mtlk_aocs_delete, (slow_ctx->aocs))

#ifdef MTCFG_PMCU_SUPPORT
    MTLK_STOP_STEP(core_slow_ctx, PROCESS_PCOC, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_pcoc_delete, (slow_ctx->pcoc_mngmt))
#endif /*MTCFG_PMCU_SUPPORT*/

    MTLK_STOP_STEP(core_slow_ctx, PROCESS_COC, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_coc_delete, (slow_ctx->coc_mngmt))

    MTLK_STOP_STEP(core_slow_ctx, PROCESS_ANTENNA_CFG, MTLK_OBJ_PTR(slow_ctx),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core_slow_ctx, INIT_TX_LIMIT_TABLES, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_cleanup_tx_limit_tables, (&slow_ctx->tx_limits))

    MTLK_STOP_STEP(core_slow_ctx, CACHE_INIT, MTLK_OBJ_PTR(slow_ctx),
                   mtlk_cache_cleanup, (&slow_ctx->cache))

    MTLK_STOP_STEP(core_slow_ctx, PARSE_EE_DATA, MTLK_OBJ_PTR(slow_ctx),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core_slow_ctx, SET_MAC_MAC_ADDR, MTLK_OBJ_PTR(slow_ctx),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core_slow_ctx, SERIALIZER_START, MTLK_OBJ_PTR(slow_ctx),
                   MTLK_NOACTION, ())
  MTLK_STOP_END(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx))
}

static int __MTLK_IFUNC
_mtlk_slow_ctx_start(struct nic_slow_ctx *slow_ctx, struct nic* nic)
{
  int cache_param;
  struct mtlk_scan_config scan_cfg;
  mtlk_aocs_wrap_api_t aocs_api;
  mtlk_aocs_init_t aocs_ini_data;
  mtlk_dot11h_wrap_api_t dot11h_api;
  mtlk_coc_cfg_t  coc_cfg;
#ifdef MTCFG_PMCU_SUPPORT
  mtlk_pcoc_cfg_t  pcoc_cfg;
  BOOL             pcoc_required = FALSE;
#endif /*MTCFG_PMCU_SUPPORT*/
  mtlk_txmm_t *txmm = mtlk_vap_get_txmm(nic->vap_handle);
  mtlk_reflim_t *addba_lim_aggr  = NULL;
  mtlk_reflim_t *addba_lim_reord = NULL;
  mtlk_eeprom_data_t *eeprom_data;
  BOOL is_dut = mtlk_vap_is_dut(nic->vap_handle);
  BOOL is_slave_ap = mtlk_vap_is_slave_ap(nic->vap_handle);

  MTLK_ASSERT(NULL != slow_ctx);
  MTLK_ASSERT(NULL != nic);

  MTLK_START_TRY(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx))
    MTLK_START_STEP(core_slow_ctx, SERIALIZER_START, MTLK_OBJ_PTR(slow_ctx),
                    mtlk_serializer_start, (&slow_ctx->serializer))

    eeprom_data = mtlk_core_get_eeprom(nic);

    MTLK_START_STEP_IF(!is_dut && !is_slave_ap, core_slow_ctx, SET_MAC_MAC_ADDR, MTLK_OBJ_PTR(slow_ctx),
                       _mtlk_core_set_mac_addr, (nic, (char *)mtlk_eeprom_get_nic_mac_addr(eeprom_data)))

    MTLK_START_STEP_IF(!is_dut && !is_slave_ap, core_slow_ctx, PARSE_EE_DATA, MTLK_OBJ_PTR(slow_ctx),
                       mtlk_eeprom_check_ee_data, (eeprom_data, txmm, mtlk_vap_is_ap(nic->vap_handle)))

    _mtlk_core_country_code_set_default(nic);

    if (mtlk_vap_is_ap(nic->vap_handle)) {
      cache_param = 0;
    } else {
      cache_param = SCAN_CACHE_AGEING;
    }

    MTLK_START_STEP(core_slow_ctx, CACHE_INIT, MTLK_OBJ_PTR(slow_ctx),
                    mtlk_cache_init, (&slow_ctx->cache, cache_param))

    MTLK_START_STEP_IF(!is_dut && !is_slave_ap, core_slow_ctx, INIT_TX_LIMIT_TABLES, MTLK_OBJ_PTR(slow_ctx),
                       mtlk_init_tx_limit_tables,
                       (&slow_ctx->tx_limits,
                       MAC_TO_HOST16( mtlk_eeprom_get_vendor_id(eeprom_data) ),
                       MAC_TO_HOST16( mtlk_eeprom_get_device_id(eeprom_data) ),
                       mtlk_eeprom_get_nic_type(eeprom_data),
                       mtlk_eeprom_get_nic_revision(eeprom_data) ) )

    MTLK_START_STEP_IF(!is_dut && !is_slave_ap, core_slow_ctx, PROCESS_ANTENNA_CFG, MTLK_OBJ_PTR(slow_ctx),
                       _mtlk_core_process_antennas_configuration, (nic));

    coc_cfg.hw_antenna_cfg.num_tx_antennas = nic->slow_ctx->tx_limits.num_tx_antennas;
    coc_cfg.hw_antenna_cfg.num_rx_antennas = nic->slow_ctx->tx_limits.num_rx_antennas;
    coc_cfg.default_auto_cfg.interval_1x1   = 10; /* 1s */
    coc_cfg.default_auto_cfg.interval_2x2   = 50; /* 5s */
    coc_cfg.default_auto_cfg.interval_3x3   = 50; /* 5s */
    coc_cfg.default_auto_cfg.high_limit_1x1 = 1000;  /* byte/sec */
    coc_cfg.default_auto_cfg.low_limit_2x2  = 5000;  /* byte/sec */
    coc_cfg.default_auto_cfg.high_limit_2x2 = 5000;  /* byte/sec */
    coc_cfg.default_auto_cfg.low_limit_3x3  = 5000;  /* byte/sec */
    coc_cfg.txmm = txmm;
    coc_cfg.vap_handle = nic->vap_handle;

    MTLK_START_STEP_EX_IF(!is_slave_ap, core_slow_ctx, PROCESS_COC, MTLK_OBJ_PTR(slow_ctx),
                          mtlk_coc_create, (&coc_cfg),
                          slow_ctx->coc_mngmt, slow_ctx->coc_mngmt != NULL, MTLK_ERR_NO_MEM);
#ifdef MTCFG_PMCU_SUPPORT

    if (_mtlk_core_is_master_on_g35(nic))
    {
      pcoc_required = TRUE;
      pcoc_cfg.default_params.interval       = 10;    /* 1s */
      pcoc_cfg.default_params.limit_lower    = 1000;  /* kbps */
      pcoc_cfg.default_params.limit_upper    = 2000;  /* kbps */
      pcoc_cfg.vap_handle = nic->vap_handle;
    }

    MTLK_START_STEP_EX_IF((!is_slave_ap) && pcoc_required, core_slow_ctx, PROCESS_PCOC, MTLK_OBJ_PTR(slow_ctx),
                          mtlk_pcoc_create, (&pcoc_cfg),
                          slow_ctx->pcoc_mngmt, slow_ctx->pcoc_mngmt != NULL, MTLK_ERR_NO_MEM);
#endif /*MTCFG_PMCU_SUPPORT*/

    aocs_api.on_channel_change = _mtlk_core_aocs_on_channel_change;
    aocs_api.on_bonding_change = _mtlk_core_aocs_on_bonding_change;
    aocs_api.on_spectrum_change = _mtlk_core_aocs_on_spectrum_change;

    aocs_ini_data.api = &aocs_api;
    aocs_ini_data.scan_data = &slow_ctx->scan;
    aocs_ini_data.cache = &slow_ctx->cache;
    aocs_ini_data.dot11h = mtlk_core_get_dfs(nic);
    aocs_ini_data.txmm = txmm;
    aocs_ini_data.disable_sm_channels = mtlk_eeprom_get_disable_sm_channels(mtlk_core_get_eeprom(nic));

    slow_ctx->aocs = NULL;

    MTLK_START_STEP_EX_IF(mtlk_vap_is_master_ap(nic->vap_handle), core_slow_ctx, AOCS_INIT, MTLK_OBJ_PTR(slow_ctx),
                          mtlk_aocs_create, (&aocs_ini_data, nic->vap_handle),
                          slow_ctx->aocs, slow_ctx->aocs != NULL, MTLK_ERR_NO_MEM)

    scan_cfg.txmm = txmm;
    scan_cfg.aocs = slow_ctx->aocs;
    scan_cfg.hw_tx_flctrl = nic->hw_tx_flctrl;
    scan_cfg.bss_cache = &slow_ctx->cache;
    scan_cfg.bss_data  = &slow_ctx->bss_data;

    MTLK_START_STEP_IF(!is_slave_ap, core_slow_ctx, SCAN_INIT, MTLK_OBJ_PTR(slow_ctx),
                       mtlk_scan_init, (&slow_ctx->scan, scan_cfg, nic->vap_handle))

    dot11h_api.aocs         = slow_ctx->aocs;
    dot11h_api.txmm         = txmm;
    dot11h_api.hw_tx_flctrl = nic->hw_tx_flctrl;

    MTLK_START_STEP_IF(!is_slave_ap, core_slow_ctx, DOT11H_INIT, MTLK_OBJ_PTR(slow_ctx),
                      mtlk_dot11h_init, (mtlk_core_get_dfs(nic), NULL, &dot11h_api, nic->vap_handle))

    MTLK_START_STEP_VOID_IF(mtlk_vap_is_master(nic->vap_handle), core_slow_ctx, ADDBA_AGGR_LIM_INIT, MTLK_OBJ_PTR(slow_ctx),
                            mtlk_reflim_init, (&slow_ctx->addba_lim_aggr, MTLK_ADDBA_DEF_MAX_AGGR_SUPPORTED))

    MTLK_START_STEP_VOID_IF(mtlk_vap_is_master(nic->vap_handle), core_slow_ctx, ADDBA_REORD_LIM_INIT, MTLK_OBJ_PTR(slow_ctx),
                            mtlk_reflim_init, (&slow_ctx->addba_lim_reord, MTLK_ADDBA_DEF_MAX_REORD_SUPPORTED))

    if (mtlk_vap_is_master(nic->vap_handle)) {
      addba_lim_aggr  = &slow_ctx->addba_lim_aggr;
      addba_lim_reord = &slow_ctx->addba_lim_reord;
    }
    else {
      /* Limits are shared for all the VAPs on same HW */
      addba_lim_aggr  = &mtlk_core_get_master(nic)->slow_ctx->addba_lim_aggr;
      addba_lim_reord = &mtlk_core_get_master(nic)->slow_ctx->addba_lim_reord;
    }

    MTLK_START_STEP(core_slow_ctx, ADDBA_INIT, MTLK_OBJ_PTR(slow_ctx),
                    mtlk_addba_init, (&slow_ctx->addba, txmm, addba_lim_aggr, addba_lim_reord, &slow_ctx->cfg.addba, nic->vap_handle))

    MTLK_START_STEP_IF(mtlk_vap_is_ap(nic->vap_handle), core_slow_ctx, WDS_INIT, MTLK_OBJ_PTR(slow_ctx),
                       wds_init, (&slow_ctx->wds_mng, nic->vap_handle))

    MTLK_START_STEP_IF(!is_dut && !mtlk_vap_is_slave_ap(nic->vap_handle), core_slow_ctx, SET_DEFAULT_BAND, MTLK_OBJ_PTR(slow_ctx),
                       mtlk_core_master_set_default_band, (nic))

    MTLK_START_STEP(core_slow_ctx, SET_DEFAULT_NET_MODE, MTLK_OBJ_PTR(slow_ctx),
                    mtlk_core_set_default_net_mode, (nic))

    MTLK_START_STEP_IF(!is_slave_ap, core_slow_ctx, WATCHDOG_TIMER_START, MTLK_OBJ_PTR(slow_ctx),
                       mtlk_osal_timer_set,
                       (&slow_ctx->mac_watchdog_timer,
                       MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_PERIOD_MS)))

    MTLK_START_STEP_VOID(core_slow_ctx, SERIALIZER_ACTIVATE, MTLK_OBJ_PTR(slow_ctx),
                         MTLK_NOACTION, ())

    MTLK_START_STEP_EX(core_slow_ctx, CORE_STAT_REQ_HANDLER, MTLK_OBJ_PTR(slow_ctx),
                       mtlk_wssd_register_request_handler, (mtlk_vap_get_irbd(nic->vap_handle),
                                                            _mtlk_core_stat_handle_request, HANDLE_T(slow_ctx)),
                       slow_ctx->stat_irb_handle, slow_ctx->stat_irb_handle != NULL, MTLK_ERR_UNKNOWN);

  MTLK_START_FINALLY(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx))
  MTLK_START_RETURN(core_slow_ctx, MTLK_OBJ_PTR(slow_ctx), _mtlk_slow_ctx_stop, (slow_ctx, nic))
}

MTLK_START_STEPS_LIST_BEGIN(core)
  MTLK_START_STEPS_LIST_ENTRY(core, WSS_CREATE)
  MTLK_START_STEPS_LIST_ENTRY(core, WSS_HCTNRs)
  MTLK_START_STEPS_LIST_ENTRY(core, SET_NET_STATE_IDLE)
  MTLK_START_STEPS_LIST_ENTRY(core, FLCTRL_ID_REGISTER)
  MTLK_START_STEPS_LIST_ENTRY(core, SLOW_CTX_START)
  MTLK_START_STEPS_LIST_ENTRY(core, DF_USER_SET_MAC_ADDR)
  MTLK_START_STEPS_LIST_ENTRY(core, RESET_STATS)
  MTLK_START_STEPS_LIST_ENTRY(core, SQ_START)
  MTLK_START_STEPS_LIST_ENTRY(core, MC_INIT)
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  MTLK_START_STEPS_LIST_ENTRY(core, RF_MGMT_START)
#endif
  MTLK_START_STEPS_LIST_ENTRY(core, DUT_REGISTER)
  MTLK_START_STEPS_LIST_ENTRY(core, ADD_VAP)
  MTLK_START_STEPS_LIST_ENTRY(core, ABILITIES_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core, SET_NET_STATE_READY)
  MTLK_START_STEPS_LIST_ENTRY(core, COEX_20_40_INIT)
  MTLK_START_STEPS_LIST_ENTRY(core, UPDATE_PARAM_DB)
  MTLK_START_STEPS_LIST_ENTRY(core, SET_VAP_MIBS)
MTLK_START_INNER_STEPS_BEGIN(core)
MTLK_START_STEPS_LIST_END(core);

static void
_mtlk_core_stop (mtlk_vap_handle_t vap_handle)
{
  mtlk_core_t *nic = mtlk_vap_get_core (vap_handle);
  mtlk_hw_state_e hw_state = mtlk_core_get_hw_state(nic);
  int i;

  ILOG0_D("CID-%04x: stop", mtlk_vap_get_oid(vap_handle));

  /*send RMMOD event to application*/
  if ((hw_state != MTLK_HW_STATE_EXCEPTION) &&
      (hw_state != MTLK_HW_STATE_APPFATAL) &&
      (hw_state != MTLK_HW_STATE_MAC_ASSERTED)) {
    ILOG4_V("RMMOD send event");
    mtlk_df_ui_notify_notify_rmmod(mtlk_df_get_name(mtlk_vap_get_df(vap_handle)));
  }

  MTLK_STOP_BEGIN(core, MTLK_OBJ_PTR(nic))
    MTLK_STOP_STEP(core, SET_VAP_MIBS, MTLK_OBJ_PTR(nic),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core, UPDATE_PARAM_DB, MTLK_OBJ_PTR(nic),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core, COEX_20_40_INIT, MTLK_OBJ_PTR(nic),
                   _mtlk_core_delete_20_40, (nic))

    MTLK_STOP_STEP(core, SET_NET_STATE_READY, MTLK_OBJ_PTR(nic),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core, ABILITIES_INIT, MTLK_OBJ_PTR(nic),
                   mtlk_core_abilities_unregister, (nic))

    MTLK_STOP_STEP(core, ADD_VAP, MTLK_OBJ_PTR(nic),
                   mtlk_mbss_send_vap_delete, (nic))

    MTLK_STOP_STEP(core, DUT_REGISTER, MTLK_OBJ_PTR(nic),
                   mtlk_dut_core_unregister, (nic));

#ifdef MTCFG_RF_MANAGEMENT_MTLK
    MTLK_STOP_STEP(core, RF_MGMT_START, MTLK_OBJ_PTR(nic),
                   mtlk_rf_mgmt_stop, (nic->rf_mgmt))
#endif

    MTLK_STOP_STEP(core, MC_INIT, MTLK_OBJ_PTR(nic),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core, SQ_START, MTLK_OBJ_PTR(nic),
                   mtlk_sq_stop, (nic->sq));

    MTLK_STOP_STEP(core, RESET_STATS, MTLK_OBJ_PTR(nic),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core, DF_USER_SET_MAC_ADDR, MTLK_OBJ_PTR(nic),
                   MTLK_NOACTION, ())

    MTLK_STOP_STEP(core, SLOW_CTX_START, MTLK_OBJ_PTR(nic),
                   _mtlk_slow_ctx_stop, (nic->slow_ctx, nic))

    MTLK_STOP_STEP(core, FLCTRL_ID_REGISTER, MTLK_OBJ_PTR(nic),
                   mtlk_flctrl_unregister, (nic->hw_tx_flctrl, nic->flctrl_id))

    nic->flctrl_id = 0;

    MTLK_STOP_STEP(core, SET_NET_STATE_IDLE, MTLK_OBJ_PTR(nic),
                   MTLK_NOACTION, ())
    MTLK_STOP_STEP(core, WSS_HCTNRs, MTLK_OBJ_PTR(nic),
                   mtlk_wss_cntrs_close, (nic->wss, nic->wss_hcntrs, ARRAY_SIZE(nic->wss_hcntrs)))
    MTLK_STOP_STEP(core, WSS_CREATE, MTLK_OBJ_PTR(nic),
                   mtlk_wss_delete, (nic->wss));
  MTLK_STOP_END(core, MTLK_OBJ_PTR(nic))

  for (i = 0; i < ARRAY_SIZE(nic->txmm_async_eeprom_msgs); i++) {
    mtlk_txmm_msg_cancel(&nic->txmm_async_eeprom_msgs[i]);
  }
}

static int
_mtlk_core_start (mtlk_vap_handle_t vap_handle)
{
  mtlk_core_t       *nic = mtlk_vap_get_core (vap_handle);
  uint8             mac_addr[ETH_ALEN];
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  mtlk_rf_mgmt_cfg_t rf_mgmt_cfg = {0};
#endif
  MTLK_ASSERT(ARRAY_SIZE(nic->wss_hcntrs) == MTLK_CORE_CNT_LAST);
  MTLK_ASSERT(ARRAY_SIZE(_mtlk_core_wss_id_map) == MTLK_CORE_CNT_LAST);

  MTLK_START_TRY(core, MTLK_OBJ_PTR(nic))
    MTLK_START_STEP_EX(core, WSS_CREATE, MTLK_OBJ_PTR(nic),
                       mtlk_wss_create, (mtlk_vap_get_hw_wss(vap_handle), _mtlk_core_wss_id_map, ARRAY_SIZE(_mtlk_core_wss_id_map)),
                       nic->wss, nic->wss != NULL, MTLK_ERR_NO_MEM);

    MTLK_START_STEP(core, WSS_HCTNRs, MTLK_OBJ_PTR(nic),
                    mtlk_wss_cntrs_open, (nic->wss, _mtlk_core_wss_id_map, nic->wss_hcntrs, MTLK_CORE_CNT_LAST));
    MTLK_START_STEP(core, SET_NET_STATE_IDLE, MTLK_OBJ_PTR(nic),
                    mtlk_core_set_net_state, (nic, NET_STATE_IDLE))

    nic->flctrl_id = 0;

    MTLK_START_STEP(core, FLCTRL_ID_REGISTER, MTLK_OBJ_PTR(nic),
                    mtlk_flctrl_register, (nic->hw_tx_flctrl, &nic->flctrl_id))

    MTLK_START_STEP(core, SLOW_CTX_START, MTLK_OBJ_PTR(nic),
                    _mtlk_slow_ctx_start, (nic->slow_ctx, nic))

    mtlk_pdb_get_mac(
        mtlk_vap_get_param_db(vap_handle), PARAM_DB_CORE_MAC_ADDR, mac_addr);

    MTLK_START_STEP_VOID(core, DF_USER_SET_MAC_ADDR, MTLK_OBJ_PTR(nic),
                         mtlk_df_ui_set_mac_addr, (mtlk_vap_get_df(vap_handle), mac_addr))

    MTLK_START_STEP_VOID(core, RESET_STATS, MTLK_OBJ_PTR(nic),
                         _mtlk_core_reset_stats_internal, (nic))

    /* The Master DF is always used here */
    MTLK_START_STEP_IF(!mtlk_vap_is_slave_ap(vap_handle), core, SQ_START, MTLK_OBJ_PTR(nic),
                       mtlk_sq_start, (nic->sq, mtlk_vap_get_manager(vap_handle)) );

    MTLK_START_STEP_VOID(core, MC_INIT, MTLK_OBJ_PTR(nic),
                         mtlk_mc_init, (nic))

#ifdef MTCFG_RF_MANAGEMENT_MTLK
    rf_mgmt_cfg.txmm  = mtlk_vap_get_txmm(vap_handle);
    rf_mgmt_cfg.stadb = &nic->slow_ctx->stadb;
    rf_mgmt_cfg.irbd  = mtlk_vap_get_irbd(vap_handle);
    rf_mgmt_cfg.context = HANDLE_T(nic);
    rf_mgmt_cfg.device_is_busy = mtlk_core_is_device_busy;

    mtlk_rf_mgmt_configure(nic->rf_mgmt, &rf_mgmt_cfg);
    MTLK_START_STEP(core, RF_MGMT_START, MTLK_OBJ_PTR(nic),
                    mtlk_rf_mgmt_start, (nic->rf_mgmt));
#endif

    MTLK_START_STEP(core, DUT_REGISTER, MTLK_OBJ_PTR(nic),
                    mtlk_dut_core_register, (nic));

    MTLK_START_STEP_IF(mtlk_vap_is_ap(vap_handle),
                       core, ADD_VAP, MTLK_OBJ_PTR(nic),
                       mtlk_mbss_send_vap_add, (nic))

    MTLK_START_STEP(core, ABILITIES_INIT, MTLK_OBJ_PTR(nic),
                    mtlk_core_abilities_register, (nic))

    MTLK_START_STEP(core, SET_NET_STATE_READY, MTLK_OBJ_PTR(nic),
                    mtlk_core_set_net_state, (nic, NET_STATE_READY))

    MTLK_START_STEP_IF(mtlk_vap_is_master(nic->vap_handle), core, COEX_20_40_INIT, MTLK_OBJ_PTR(nic),
                       _mtlk_core_create_20_40, (nic))

    MTLK_START_STEP(core, UPDATE_PARAM_DB, MTLK_OBJ_PTR(nic),
                    _mtlk_core_update_param_db, (nic))

    mtlk_core_init_defaults(nic);

    MTLK_START_STEP_IF(mtlk_vap_is_ap(vap_handle),
                       core, SET_VAP_MIBS, MTLK_OBJ_PTR(nic),
                       mtlk_set_vap_mibs, (nic))

  MTLK_START_FINALLY(core, MTLK_OBJ_PTR(nic))
  MTLK_START_RETURN(core, MTLK_OBJ_PTR(nic), _mtlk_core_stop, (vap_handle))
}

static int
_mtlk_core_release_tx_data (mtlk_vap_handle_t vap_handle, const mtlk_core_release_tx_data_t *data)
{
  int res = MTLK_ERR_UNKNOWN;
  mtlk_core_t *nic = mtlk_vap_get_core (vap_handle);
  mtlk_nbuf_t *nbuf = data->nbuf;
  unsigned short qos = 0;
  mtlk_nbuf_priv_t *nbuf_priv = mtlk_nbuf_priv(nbuf);
  sta_entry *sta = NULL; /* NOTE: nbuf is referencing STA, so it is safe to use this STA
                          * while nbuf isn't released. */
  mtlk_sq_peer_ctx_t *sq_ppeer = NULL;

  /* All the packets on AP and non BC packets on STA */
  if (mtlk_vap_is_ap(nic->vap_handle) ||
     mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_UNICAST | MTLK_NBUFF_RMCAST)) {
    sta = mtlk_nbuf_priv_get_dst_sta(nbuf_priv);
  }

#if defined(MTCFG_PER_PACKET_STATS) && defined (MTCFG_TSF_TIMER_ACCESS_ENABLED)
  mtlk_nbuf_priv_stats_set(nbuf_priv, MTLK_NBUF_STATS_TS_FW_OUT, mtlk_hw_get_timestamp(vap_handle));
#endif

  // check if NULL packet confirmed
  if (data->size == 0) {
    ILOG9_V("Confirmation for NULL nbuf");
    goto FINISH;
  }

  qos = mtlk_qos_get_ac_by_tid(data->access_category);

  if ((qos != (uint16)-1) && (nic->pstats.ac_used_counter[qos] > 0))
    --nic->pstats.ac_used_counter[qos];

  res = MTLK_ERR_OK;

FINISH:
  if (data->resources_free) {
    if (__UNLIKELY(!mtlk_flctrl_is_data_flowing(nic->hw_tx_flctrl))) {
      ILOG2_V("mtlk_flctrl_wake on OS TX queue wake");
      mtlk_flctrl_start_data(nic->hw_tx_flctrl, nic->flctrl_id);
    } else {
      mtlk_sq_schedule_flush(nic);
    }
  }

  // If confirmed (or failed) unicast packet to known STA
  if (NULL != sta ) {
    /* this is unicast or reliable multicast being transmitted */
    sq_ppeer = &sta->sq_peer_ctx;

    if (__LIKELY(data->status == UMI_OK)) {
      /* Update STA's timestamp on successful (confirmed by ACK) TX */
      mtlk_sta_on_packet_sent(sta, nbuf, data->nof_retries);

#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
      if (mtlk_vap_is_ap(vap_handle) && (qos != (uint16)-1)) {
        mtlk_aocs_on_tx_msdu_returned(mtlk_core_get_master(nic)->slow_ctx->aocs, qos);
      }
#endif
    } else {
      mtlk_sta_on_packet_dropped(sta, MTLK_TX_DISCARDED_FW);
    }
  } else {
    MTLK_ASSERT(FALSE == mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_UNICAST | MTLK_NBUFF_RMCAST));
    /* this should be broadcast or non-reliable multicast packet */
    sq_ppeer = &nic->sq_broadcast_ctx;
    if (__LIKELY(data->status == UMI_OK)) {
      if(mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_MULTICAST)) {
        mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_MULTICAST_PACKETS_SENT);
        mtlk_core_add_cnt(nic, MTLK_CORE_CNT_MULTICAST_BYTES_SENT, mtlk_df_nbuf_get_data_length(nbuf));
      }
      else if (mtlk_nbuf_priv_check_flags(nbuf_priv, MTLK_NBUFF_BROADCAST)) {
        mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_BROADCAST_PACKETS_SENT);
        mtlk_core_add_cnt(nic, MTLK_CORE_CNT_BROADCAST_BYTES_SENT, mtlk_df_nbuf_get_data_length(nbuf));
      }

      mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_PACKETS_SENT);
      mtlk_core_add_cnt(nic, MTLK_CORE_CNT_BYTES_SENT, mtlk_df_nbuf_get_data_length(nbuf));
      nic->pstats.tx_bcast_nrmcast++;
    } else {
      mtlk_core_inc_cnt(nic, MTLK_CORE_CNT_TX_PACKETS_DISCARDED_FW);
    }
  }

  /* use access_category from nbuf_priv instead of the qos */
  mtlk_sq_on_tx_cfm(sq_ppeer, mtlk_nbuf_priv_get_tx_pck_ac(nbuf_priv));

#if defined(MTCFG_PRINT_PER_PACKET_STATS)
  mtlk_nbuf_priv_stats_dump(nbuf_priv);
#endif

  /* Release net buffer
   * WARNING: we can't do it before since we use STA referenced by this packet on FINISH.
   */
  mtlk_df_nbuf_free(mtlk_vap_get_manager(nic->vap_handle), nbuf);

  /* update used Tx MSDUs counter */
#ifndef MBSS_FORCE_NO_CHANNEL_SWITCH
  if (qos != (uint16)-1) {
    if(mtlk_vap_is_ap(vap_handle)) {
      mtlk_aocs_msdu_tx_dec_nof_used(mtlk_core_get_master(nic)->slow_ctx->aocs, qos);
    }
  }
#endif

  return res;
}

static int
_mtlk_core_handle_rx_data (mtlk_vap_handle_t vap_handle, mtlk_core_handle_rx_data_t *data)
{
  mtlk_nbuf_t *nbuf = data->nbuf;
  ASSERT(nbuf != 0);

  mtlk_df_nbuf_put(nbuf, data->offset);
  mtlk_df_nbuf_pull(nbuf, data->offset);

  return handle_rx_ind(mtlk_vap_get_core (vap_handle), nbuf, (uint16)data->size, data->info);
}

void __MTLK_IFUNC
mtlk_core_set_pm_enabled (mtlk_core_t *core, BOOL enabled)
{
  uint32 limit, stas_in_ps_mode;
  mtlk_sq_peer_ctx_t *ppeer;

  MTLK_ASSERT(NULL != core);

  /* Increase/decrease counter for number of STAs in PS mode */
  if (enabled) {
    mtlk_osal_atomic_inc(&core->stas_in_ps_mode);
  } else {
    mtlk_osal_atomic_dec(&core->stas_in_ps_mode);
  }

  ppeer = &core->sq_broadcast_ctx;
  stas_in_ps_mode = mtlk_osal_atomic_get(&core->stas_in_ps_mode);

  /* Calculate queue limit depending of PS mode */
  limit = stas_in_ps_mode ? MIN(MTLK_PACKETS_IN_MAC_DURING_PM, ppeer->limit_cfg)
                          : ppeer->limit_cfg;

  mtlk_osal_atomic_set(&ppeer->limit, limit);

  mtlk_osal_atomic_set(&ppeer->ps_mode_enabled, (stas_in_ps_mode > 0) ? TRUE : FALSE);
}

static int __MTLK_IFUNC
_handle_pm_update_event(mtlk_handle_t object, const void *data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);
  const UMI_PM_UPDATE *pm_update = (const UMI_PM_UPDATE *)data;
  sta_entry *sta;
  BOOL enabled = pm_update->newPowerMode == UMI_STATION_IN_PS;

  MTLK_ASSERT(sizeof(UMI_PM_UPDATE) == data_size);

  ILOG2_DY("Power management mode changed to %d for %Y",
        pm_update->newPowerMode, pm_update->sStationID.au8Addr);

  sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, pm_update->sStationID.au8Addr);
  if (sta == NULL) {
    ILOG2_Y("PM update event received from STA %Y which is not known",
          pm_update->sStationID.au8Addr);
    return MTLK_ERR_OK;
  }

  if (mtlk_osal_atomic_get(&sta->sq_peer_ctx.ps_mode_enabled) != enabled) { /* skip duplicated pm events */
    mtlk_sta_set_pm_enabled(sta, enabled);
    mtlk_core_set_pm_enabled(nic, enabled);
  }

  mtlk_sta_decref(sta); /* De-reference of find */

  if (pm_update->newPowerMode == UMI_STATION_ACTIVE)
    mtlk_sq_schedule_flush(nic);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_logger_init_failed_event(mtlk_handle_t object, const void *data, uint32 data_size)
{
  MTLK_UNREFERENCED_PARAM(object);
  MTLK_UNREFERENCED_PARAM(data);
  MTLK_UNREFERENCED_PARAM(data_size);

  MTLK_ASSERT(0 == data_size);

  ELOG_V("Firmware log will be unavailable due to firmware logger init failure");

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_fw_debug_trace_event(mtlk_handle_t object, const void *data, uint32 data_size)
{
  UmiDbgTraceInd_t *UmiDbgTraceInd = (UmiDbgTraceInd_t *) data;
  MTLK_UNREFERENCED_PARAM(object);
  MTLK_ASSERT(sizeof(UmiDbgTraceInd_t) >= data_size);
  MTLK_ASSERT(MAX_DBG_TRACE_DATA >= MAC_TO_HOST32(UmiDbgTraceInd->length));

  UmiDbgTraceInd->au8Data[MAX_DBG_TRACE_DATA - 1] = 0; // make sure it is NULL-terminated (although it should be without this)

  ILOG0_S("DBG TRACE: %s", UmiDbgTraceInd->au8Data);

  return MTLK_ERR_OK;
}

static int
_handle_ba_tx_status_event(mtlk_handle_t object, const void *data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);
  UMI_BA_TX_STATUS *ba_tx = (UMI_BA_TX_STATUS*)data;
  sta_entry *sta;

  MTLK_ASSERT(NULL != nic);
  MTLK_ASSERT(NULL != data);
  MTLK_ASSERT(sizeof(UMI_BA_TX_STATUS) == data_size);

  ILOG2_DYDDDD("CID-%04x: BA status received for STA %Y TID=%d UMID=0x%04x I=%d ST=%d",
    mtlk_vap_get_oid(nic->vap_handle),
    ba_tx->sDA.au8Addr,
    MAC_TO_HOST16(ba_tx->u16AccessProtocol),
    MAC_TO_HOST16(ba_tx->u16MessageId),
    ba_tx->u8Initiator,
    ba_tx->u8Status);

  /* Try to find source MAC of transmitter */
  sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, ba_tx->sDA.au8Addr);
  if (NULL == sta)
  {
    ILOG1_DY("CID-%04x: BA status to unknown STA %Y, ignored",
            mtlk_vap_get_oid(nic->vap_handle), ba_tx->sDA.au8Addr);
    return MTLK_ERR_OK;
  }

  mtlk_addba_peer_on_ba_tx_status(mtlk_sta_get_addb_peer(sta), ba_tx);

  mtlk_sta_decref(sta); /* De-reference of find */

  return MTLK_ERR_OK;
}

static int
_handle_ack_on_bar_event (mtlk_handle_t object, const void *data, uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);
  uint16 tid;
  UMI_ACK_ON_BAR_EVENT *ack_on_bar = (UMI_ACK_ON_BAR_EVENT*)data;
  sta_entry *sta;

  MTLK_ASSERT(NULL != nic);
  MTLK_ASSERT(NULL != data);
  MTLK_ASSERT(sizeof(UMI_ACK_ON_BAR_EVENT) == data_size);

  tid = MAC_TO_HOST16(ack_on_bar->u16AccessProtocol);

  ILOG2_DYD("CID-%04x: ACK on BAR received for STA %Y TID=%d",
    mtlk_vap_get_oid(nic->vap_handle), ack_on_bar->sDA.au8Addr, tid);

  /* Try to find source MAC of transmitter */
  sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, ack_on_bar->sDA.au8Addr);
  if (NULL == sta)
  {
    ILOG1_DY("CID-%04x: ACK on BAR to unknown STA %Y, ignored",
             mtlk_vap_get_oid(nic->vap_handle), ack_on_bar->sDA.au8Addr);
    return MTLK_ERR_OK;
  }

  mtlk_addba_peer_on_ack_on_bar(mtlk_sta_get_addb_peer(sta), tid);

  mtlk_sta_decref(sta); /* De-reference of find */

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_aocs_tcp_event(mtlk_handle_t core_object, const void *data, uint32 data_size)
{
  mtlk_core_t* core = HANDLE_T_PTR(mtlk_core_t, core_object);

  MTLK_UNREFERENCED_PARAM(data_size);

  if (mtlk_vap_is_master_ap(core->vap_handle)) {
    mtlk_aocs_indicate_event(core->slow_ctx->aocs,
                             MTLK_AOCS_EVENT_TCP_IND, (void*) data, data_size);
  }

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_fw_interference_ind (mtlk_handle_t core_object, const void *data, uint32 data_size)
{
  int8 det_threshold;
  mtlk_core_t* core = HANDLE_T_PTR(mtlk_core_t, core_object);
  const UMI_INTERFERER *interferer_ind = (const UMI_INTERFERER *)data;

  MTLK_ASSERT(sizeof(UMI_INTERFERER) == data_size);

  if (mtlk_vap_is_master_ap(core->vap_handle)) {
    if (core->is_stopped) {
      ILOG5_V("UMI_INTERFERER event while core is down");
      return MTLK_ERR_OK; /* do not process */
    }

    if (!mtlk_core_is_interfdet_enabled(core)) {
      ILOG5_V("UMI_INTERFERER event while interference detection is deactivated");
      return MTLK_ERR_OK; /* do not process */
    }

    if (SPECTRUM_40MHZ == _mtlk_core_get_user_spectrum_mode(core)){
      /* Use 40MHz thresold if user choose 40MHz explicitly */
      det_threshold = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_INTERFDET_40MHZ_DETECTION_THRESHOLD);
    }else{
      /* Use 20MHZ threshold for 20MHz and 20/40 Auto and coexistance */
      det_threshold = MTLK_CORE_PDB_GET_INT(core, PARAM_DB_INTERFDET_20MHZ_DETECTION_THRESHOLD);
    }

    if (interferer_ind->maximumValue > det_threshold) {
      ILOG0_DDD("CID-%04x: Interference is detected on channel %d with Metric %d", mtlk_vap_get_oid(core->vap_handle),
                 interferer_ind->channel, interferer_ind->maximumValue);

      bt_acs_send_interf_event(interferer_ind->channel, interferer_ind->maximumValue);
      mtlk_aocs_indicate_event(core->slow_ctx->aocs,
                               MTLK_AOCS_EVENT_INTERF_IND, (void*)data, data_size);
    }
    mtlk_aocs_indicate_event(core->slow_ctx->aocs,
                             MTLK_AOCS_EVENT_INTERF_STAT, (void*)data, data_size);
  }

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_handle_fw_dfs_end (mtlk_handle_t core_object, const void *data, uint32 data_size)
{
  mtlk_core_t* core = HANDLE_T_PTR(mtlk_core_t, core_object);

  MTLK_UNREFERENCED_PARAM(data);
  MTLK_UNREFERENCED_PARAM(data_size);

  mtlk_dot11h_initial_wait_end(mtlk_core_get_dfs(core));
  (void)mtlk_coc_set_power_mode(core->slow_ctx->coc_mngmt,
                                mtlk_coc_get_auto_mode_cfg(core->slow_ctx->coc_mngmt));

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_process_mac_hang(mtlk_core_t* nic, mtlk_hw_state_e hw_state, uint32 fw_cpu)
{
  int res = MTLK_ERR_OK;

  if(!core_get_is_mac_fatal_pending(nic))
  {
    /* Firmware is already recovered */
    goto finish;
  }

  mtlk_core_set_net_state(nic, NET_STATE_HALTED);
  mtlk_set_hw_state(nic, hw_state);
  nic->slow_ctx->mac_stuck_detected_by_sw = 0;
  WLOG_DD("CID-%04x: MAC Hang detected, event = %d", mtlk_vap_get_oid(nic->vap_handle), hw_state);
  mtlk_df_ui_notify_notify_fw_hang(mtlk_vap_get_df(nic->vap_handle), fw_cpu, hw_state);

finish:

  return res;
}

static int
_mtlk_process_mac_fatal_log (mtlk_core_t *nic, APP_FATAL *app_fatal)
{
  int res = MTLK_ERR_OK;
  mtlk_log_event_t log_event;

  log_event.timestamp = MAC_TO_HOST32(app_fatal->uTimeStamp);
  log_event.info = LOG_MAKE_INFO(0,                                  /* version */
                                 MAC_TO_HOST32(app_fatal->OriginId), /* firmware OID */
                                 MAC_TO_HOST32(app_fatal->GroupId)); /* firmware GID */
  log_event.info_ex = LOG_MAKE_INFO_EX(MAC_TO_HOST32(app_fatal->FileId),             /* firmware FID */
                                       MAC_TO_HOST32(app_fatal->uCauseRegOrLineNum), /* firmware LID */
                                       0,                                            /* data size */
                                       MAC_TO_HOST32(app_fatal->FWinterface));       /* firmware wlanif */

#if (RTLOG_FLAGS & RTLF_REMOTE_ENABLED)
  mtlk_vap_get_hw_vft(nic->vap_handle)->set_prop(nic->vap_handle, MTLK_HW_LOG,
                                                 &log_event, sizeof(log_event) + 0 /* data size */);
#else
  MTLK_UNREFERENCED_PARAM(nic);
  MTLK_UNREFERENCED_PARAM(app_fatal);
#endif

  res = mtlk_nl_send_brd_msg(&log_event,
                             sizeof(log_event),
                             GFP_ATOMIC,
                             NETLINK_LOGSERVER_GROUP,
                             NL_DRV_IRBM_NOTIFY);

  if (MTLK_ERR_OK != res) {
    ELOG_V("Unable to notify LogServer");
  }

  return res;
}

static int __MTLK_IFUNC
_mtlk_handle_mac_exception(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  APP_FATAL *app_fatal = (APP_FATAL*)data;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);
  _mtlk_process_mac_hang(nic, MTLK_HW_STATE_EXCEPTION, MAC_TO_HOST32(app_fatal->uLmOrUm));

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_mac_exception_sync(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);
  MTLK_ASSERT(sizeof(APP_FATAL) == data_size);

  core_set_is_mac_fatal_pending(nic, TRUE);

  mtlk_cc_handle_mac_exception(nic->vap_handle, (const APP_FATAL*) data);
  _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_mac_exception, HANDLE_T(nic), data, data_size);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_mac_fatal(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  APP_FATAL *app_fatal = (APP_FATAL*)data;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);
  _mtlk_process_mac_fatal_log(nic, app_fatal);
  _mtlk_process_mac_hang(nic, MTLK_HW_STATE_APPFATAL, MAC_TO_HOST32(app_fatal->uLmOrUm));

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_mac_fatal_sync(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);
  MTLK_ASSERT(sizeof(APP_FATAL) == data_size);

  core_set_is_mac_fatal_pending(nic, TRUE);

  mtlk_cc_handle_mac_fatal(nic->vap_handle, (const APP_FATAL*) data);

  if (rcvry_is_in_progress(mtlk_vap_get_rcvry(nic->vap_handle))) {
    _mtlk_process_emergency_task(nic, _mtlk_handle_mac_fatal, HANDLE_T(nic), data, data_size);
  }
  else {
    _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_mac_fatal, HANDLE_T(nic), data, data_size);
  }

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_eeprom_failure_sync(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);
  MTLK_ASSERT(sizeof(EEPROM_FAILURE_EVENT) == data_size);

  mtlk_cc_handle_eeprom_failure(nic->vap_handle, (const EEPROM_FAILURE_EVENT*) data);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_generic_event(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  MTLK_ASSERT(sizeof(GENERIC_EVENT) == data_size);
  mtlk_cc_handle_generic_event(HANDLE_T_PTR(mtlk_core_t, object)->vap_handle, (GENERIC_EVENT*) data);
  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_algo_failure(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, object);

  MTLK_ASSERT(sizeof(CALIBR_ALGO_EVENT) == data_size);

  mtlk_cc_handle_algo_calibration_failure(nic->vap_handle, (const CALIBR_ALGO_EVENT*)data);

  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_dummy_event(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  MTLK_ASSERT(sizeof(DUMMY_EVENT) == data_size);
  mtlk_cc_handle_dummy_event(HANDLE_T_PTR(mtlk_core_t, object)->vap_handle, (const DUMMY_EVENT*) data);
  return MTLK_ERR_OK;
}

static int __MTLK_IFUNC
_mtlk_handle_unknown_event(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  MTLK_ASSERT(sizeof(uint32) == data_size);
  mtlk_cc_handle_unknown_event(HANDLE_T_PTR(mtlk_core_t, object)->vap_handle, *(uint32*)data);
  return MTLK_ERR_OK;
}

static void __MTLK_IFUNC
_mtlk_handle_mac_event(mtlk_core_t         *nic,
                       MAC_EVENT           *event)
{
  uint32 event_id = MAC_TO_HOST32(event->u32EventID) & 0xff;

  switch(event_id)
  {
  case EVENT_EXCEPTION:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _mtlk_handle_mac_exception_sync,
                          HANDLE_T(nic), &event->u.sAppFatalEvent, sizeof(APP_FATAL));
    break;
  case EVENT_EEPROM_FAILURE:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _mtlk_handle_eeprom_failure_sync,
                          HANDLE_T(nic), &event->u.sEepromEvent, sizeof(EEPROM_FAILURE_EVENT));
    break;
  case EVENT_APP_FATAL:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _mtlk_handle_mac_fatal_sync,
                          HANDLE_T(nic), &event->u.sAppFatalEvent, sizeof(APP_FATAL));
    break;
  case EVENT_GENERIC_EVENT:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_generic_event,
                          HANDLE_T(nic), &event->u.sGenericData, sizeof(GENERIC_EVENT));
    break;
  case EVENT_CALIBR_ALGO_FAILURE:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_algo_failure,
                          HANDLE_T(nic), &event->u.sCalibrationEvent, sizeof(CALIBR_ALGO_EVENT));
    break;
  case EVENT_DUMMY:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_dummy_event,
                          HANDLE_T(nic), &event->u.sDummyEvent, sizeof(DUMMY_EVENT));
    break;
  default:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_unknown_event,
                          HANDLE_T(nic), &event_id, sizeof(uint32));
    break;
  }
}

static int __MTLK_IFUNC
_mtlk_handle_unknown_ind_type(mtlk_handle_t object, const void *data,  uint32 data_size)
{
  MTLK_ASSERT(sizeof(uint32) == data_size);

  ILOG0_DD("CID-%04x:Unknown MAC indication type %u", mtlk_vap_get_oid(HANDLE_T_PTR(mtlk_core_t, object)->vap_handle),
        *(uint32*)data);

  return MTLK_ERR_OK;
}

static void
_mtlk_core_handle_rx_ctrl (mtlk_vap_handle_t    vap_handle,
                          uint32               id,
                          void                *payload,
                          uint32               payload_buffer_size)
{
  mtlk_core_t *nic = mtlk_vap_get_core (vap_handle);

  MTLK_ASSERT(NULL != nic);

  switch(id)
  {
  case MC_MAN_DYNAMIC_PARAM_IND:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _handle_dynamic_param_ind,
                          HANDLE_T(nic), payload, sizeof(UMI_DYNAMIC_PARAM_TABLE));
    break;
  case MC_MAN_MAC_EVENT_IND:
    _mtlk_handle_mac_event(nic, (MAC_EVENT*)payload);
    break;
  case MC_MAN_NETWORK_EVENT_IND:
    _handle_network_event(nic, (UMI_NETWORK_EVENT*)payload);
    break;
  case MC_MAN_CONNECTION_EVENT_IND:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _handle_fw_connection_event_indication,
                          HANDLE_T(nic), payload, sizeof(UMI_CONNECTION_EVENT));
    break;
  case MC_MAN_VAP_WAS_REMOVED_IND:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _handle_vap_removed_ind,
                          HANDLE_T(nic), payload, sizeof(UMI_DEACTIVATE_VAP));
    break;
  case MC_MAN_SECURITY_ALERT_IND:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _handle_security_alert_ind,
                          HANDLE_T(nic), payload, sizeof(UMI_SECURITY_ALERT));
    break;
  case MC_MAN_PM_UPDATE_IND:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _handle_pm_update_event,
                          HANDLE_T(nic), payload, sizeof(UMI_PM_UPDATE));
    break;
  case MC_MAN_AOCS_IND:
    _mtlk_process_hw_task(nic, SYNCHRONOUS, _handle_aocs_tcp_event,
                          HANDLE_T(nic), payload, payload_buffer_size);
    break;
  case MC_DBG_LOGGER_INIT_FAILD_IND:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _handle_logger_init_failed_event,
                          HANDLE_T(nic), payload, 0);
    break;
  case MC_MAN_TRACE_IND:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _handle_fw_debug_trace_event,
                          HANDLE_T(nic), payload, sizeof(UmiDbgTraceInd_t));
    break;
  case MC_MAN_BA_TX_STATUS_IND:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _handle_ba_tx_status_event,
                          HANDLE_T(nic), payload, sizeof(UMI_BA_TX_STATUS));
    break;
  case MC_MAN_ACK_ON_BAR_IND:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _handle_ack_on_bar_event,
                          HANDLE_T(nic), payload, sizeof(UMI_ACK_ON_BAR_EVENT));
    break;
  case MC_MAN_INTERFERER_IND:
    if (mtlk_core_scan_is_running(nic)){
      /* During scan the floor noise level must be updated directly, without serializer */  /* !!! ATT: please review this !!! */
      mtlk_aocs_scan_floor_noise_ind(nic->slow_ctx->aocs, (UMI_INTERFERER*)payload);
    }
    else{
      _mtlk_process_hw_task(nic, SERIALIZABLE, _handle_fw_interference_ind,
                            HANDLE_T(nic), payload, sizeof(UMI_INTERFERER));
    }

    break;
  case MC_MAN_DFS_END_IND:
      _mtlk_process_hw_task(nic, SERIALIZABLE, _handle_fw_dfs_end,
                            HANDLE_T(nic), payload, 0);
    break;
  default:
    _mtlk_process_hw_task(nic, SERIALIZABLE, _mtlk_handle_unknown_ind_type,
                          HANDLE_T(nic), &id, sizeof(uint32));
    break;
  }
}

void __MTLK_IFUNC
mtlk_core_handle_tx_ctrl (mtlk_vap_handle_t    vap_handle,
                          mtlk_user_request_t *req,
                          uint32               id,
                          mtlk_clpb_t         *data)
{
#define _MTLK_CORE_REQ_MAP_START(req_id)                                                \
  switch (req_id) {

#define _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(req_id, func)                                \
  case (req_id):                                                                        \
    _mtlk_process_user_task(nic, req, SERIALIZABLE, req_id, func, HANDLE_T(nic), data); \
    break;

#define _MTLK_CORE_HANDLE_REQ_SYNCHRONOUS(req_id, func)                                 \
  case (req_id):                                                                        \
  _mtlk_process_user_task(nic, req, SYNCHRONOUS, req_id, func, HANDLE_T(nic), data);    \
  break;

#define _MTLK_CORE_HANDLE_REQ_DUMPABLE(req_id, func)                                    \
  case (req_id):                                                                        \
  if (rcvry_is_dump_in_progress(mtlk_vap_get_rcvry(vap_handle))) {                      \
    _mtlk_process_user_task(nic, req, SYNCHRONOUS, req_id, func, HANDLE_T(nic), data);  \
  }                                                                                     \
  else {                                                                                \
    _mtlk_process_user_task(nic, req, SERIALIZABLE, req_id, func, HANDLE_T(nic), data); \
  }                                                                                     \
  break;

#define _MTLK_CORE_REQ_MAP_END()                                                        \
    default:                                                                            \
      MTLK_ASSERT(FALSE);                                                               \
  }

  mtlk_core_t *nic = mtlk_vap_get_core(vap_handle);

  MTLK_ASSERT(NULL != nic);
  MTLK_ASSERT(NULL != req);
  MTLK_ASSERT(NULL != data);

  _MTLK_CORE_REQ_MAP_START(id)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_AP_CAPABILITIES,       _mtlk_core_get_ap_capabilities)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_ACTIVATE_OPEN,             _mtlk_core_activate)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_CONNECT_STA,               _mtlk_core_connect_sta)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_DISCONNECT_STA,            _mtlk_core_hanle_disconnect_sta_req)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_AP_DISCONNECT_STA,         _mtlk_core_ap_disconnect_sta)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_AP_DISCONNECT_ALL,         _mtlk_core_ap_disconnect_all)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_DEACTIVATE,                _mtlk_core_deactivate)
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_START_SCANNING,            _mtlk_core_start_scanning);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_SCANNING_RES,          _mtlk_core_get_scanning_res);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_MAC_ADDR,              _mtlk_core_set_mac_addr_wrapper);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_MAC_ADDR,              _mtlk_core_get_mac_addr);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_STATUS,                _mtlk_core_get_status);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_RESET_STATS,               _mtlk_core_reset_stats);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_DELBA_REQ,             _mtlk_core_set_delba_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_ADDBA_CFG,             _mtlk_core_get_addba_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_ADDBA_CFG,             _mtlk_core_set_addba_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_ADDBA_STATE,           _mtlk_core_get_addba_state);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_WME_BSS_CFG,           _mtlk_core_get_wme_bss_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_WME_BSS_CFG,           _mtlk_core_set_wme_bss_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_WME_AP_CFG,            _mtlk_core_get_wme_ap_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_WME_AP_CFG,            _mtlk_core_set_wme_ap_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_AOCS_CFG,              _mtlk_core_get_aocs_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_AOCS_CFG,              _mtlk_core_set_aocs_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_DOT11H_CFG,            _mtlk_core_get_dot11h_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_DOT11H_CFG,            _mtlk_core_set_dot11h_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_DOT11H_AP_CFG,         _mtlk_core_get_dot11h_ap_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_DOT11H_AP_CFG,         _mtlk_core_set_dot11h_ap_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_L2NAT_CLEAR_TABLE,         _mtlk_core_l2nat_clear_table);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_ANTENNA_GAIN,          _mtlk_core_get_ant_gain);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_MIBS_CFG,              _mtlk_core_get_mibs_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_MIBS_CFG,              _mtlk_core_set_mibs_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_COUNTRY_CFG,           _mtlk_core_get_country_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_COUNTRY_CFG,           _mtlk_core_set_country_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_L2NAT_CFG,             _mtlk_core_get_l2nat_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_L2NAT_CFG,             _mtlk_core_set_l2nat_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_WDS_CFG,               _mtlk_core_set_wds_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_WDS_CFG,               _mtlk_core_get_wds_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_WDS_PEERAP,            _mtlk_core_get_wds_peer_ap);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_WDS_DBG,               _mtlk_core_set_wds_dbg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_DOT11D_CFG,            _mtlk_core_get_dot11d_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_DOT11D_CFG,            _mtlk_core_set_dot11d_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_MAC_WATCHDOG_CFG,      _mtlk_core_get_mac_wdog_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_MAC_WATCHDOG_CFG,      _mtlk_core_set_mac_wdog_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_STADB_CFG,             _mtlk_core_get_stadb_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_STADB_CFG,             _mtlk_core_set_stadb_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_SQ_CFG,                _mtlk_core_get_sq_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_SQ_CFG,                _mtlk_core_set_sq_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_CORE_CFG,              _mtlk_core_get_core_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_CORE_CFG,              _mtlk_core_set_core_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_MASTER_CFG,            _mtlk_core_get_master_specific_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_MASTER_CFG,            _mtlk_core_set_master_specific_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_EEPROM_CFG,            _mtlk_core_get_eeprom_cfg);
#ifdef EEPROM_DATA_VALIDATION
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_EEPROM_FW,             _mtlk_core_get_eeprom_fw);
#endif /* EEPROM_DATA_VALIDATION */
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_HSTDB_CFG,             _mtlk_core_get_hstdb_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_HSTDB_CFG,             _mtlk_core_set_hstdb_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_SCAN_CFG,              _mtlk_core_get_scan_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_SCAN_CFG,              _mtlk_core_set_scan_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_TX_POWER_LIMIT,        _mtlk_core_get_tx_power_limit_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_HW_DATA_CFG,           _mtlk_core_set_hw_data_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_QOS_CFG,               _mtlk_core_get_qos_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_QOS_CFG,               _mtlk_core_set_qos_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_COC_CFG,               _mtlk_core_get_coc_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_COC_CFG,               _mtlk_core_set_coc_cfg);
#ifdef MTCFG_PMCU_SUPPORT
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_PCOC_CFG,              _mtlk_core_get_pcoc_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_PCOC_CFG,              _mtlk_core_set_pcoc_cfg);
#endif /*MTCFG_PMCU_SUPPORT*/
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_AOCS_TBL,              _mtlk_core_get_aocs_table);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_AOCS_CHANNELS_TBL,     _mtlk_core_get_aocs_channels);
#ifdef BT_ACS_DEBUG
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_AOCS_CHANNELS_TBL_DBG, _mtlk_core_set_aocs_channels_dbg);
#endif /* BT_ACS_DEBUG */
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_AOCS_HISTORY,          _mtlk_core_get_aocs_history);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_AOCS_PENALTIES,        _mtlk_core_get_aocs_penalties);
#ifdef AOCS_DEBUG
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_AOCS_CL,               _mtlk_core_get_aocs_debug_update_cl);
#endif /* AOCS_DEBUG */
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_HW_LIMITS,             _mtlk_core_get_hw_limits);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_REG_LIMITS,            _mtlk_core_get_reg_limits);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_STOP_LM,                   _mtlk_core_stop_lm);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_MAC_CALIBRATE,             _mtlk_core_mac_calibrate);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_IW_GENERIC,            _mtlk_core_get_iw_generic);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_CTRL_MAC_GPIO,             _mtlk_core_ctrl_mac_gpio);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GEN_DATA_EXCHANGE,         _mtlk_core_gen_data_exchange);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_EE_CAPS,               _mtlk_core_get_ee_caps);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_L2NAT_STATS,           _mtlk_core_get_l2nat_stats);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_SQ_STATUS,             _mtlk_core_get_sq_status);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_MAC_ASSERT,            _mtlk_core_set_mac_assert);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_MC_IGMP_TBL,           _mtlk_core_get_mc_igmp_tbl);
    _MTLK_CORE_HANDLE_REQ_DUMPABLE(MTLK_CORE_REQ_GET_BCL_MAC_DATA,              _mtlk_core_bcl_mac_data_get);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_BCL_MAC_DATA,          _mtlk_core_bcl_mac_data_set);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_RANGE_INFO,            _mtlk_core_range_info_get);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_STADB_STATUS,          _mtlk_core_get_stadb_sta_list);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_WEP_ENC_CFG,           _mtlk_core_set_wep_enc_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_WEP_ENC_CFG,           _mtlk_core_get_wep_enc_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_AUTH_CFG,              _mtlk_core_set_auth_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_AUTH_CFG,              _mtlk_core_get_auth_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_GENIE_CFG,             _mtlk_core_set_genie_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_ENCEXT_CFG,            _mtlk_core_get_enc_ext_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_ENCEXT_CFG,            _mtlk_core_set_enc_ext_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_MBSS_ADD_VAP,              _mtlk_core_add_vap);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_MBSS_DEL_VAP,              _mtlk_core_del_vap);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_MBSS_SET_VARS,             _mtlk_core_set_mbss_vars);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_MBSS_GET_VARS,             _mtlk_core_get_mbss_vars);
    _MTLK_CORE_HANDLE_REQ_SYNCHRONOUS(MTLK_CORE_REQ_GET_SERIALIZER_INFO,        _mtlk_core_get_serializer_info);
    /* 20/40 coexistence feature */
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_COEX_20_40_RSSI_THR_CFG,   _mtlk_core_set_coex_20_40_mode_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_COEX_20_40_RSSI_THR_CFG,   _mtlk_core_get_coex_20_40_mode_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_COEX_20_40_MODE_CFG,   _mtlk_core_set_coex_20_40_mode_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_COEX_20_40_MODE_CFG,   _mtlk_core_get_coex_20_40_mode_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_COEX_20_40_AP_FORCE_PARAMS_CFG, _mtlk_core_set_coex_20_40_ap_force_params_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_COEX_20_40_AP_FORCE_PARAMS_CFG, _mtlk_core_get_coex_20_40_ap_force_params_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_COEX_20_40_STA_EXEMPTION_REQ_CFG, _mtlk_core_set_coex_20_40_exm_req_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_COEX_20_40_STA_EXEMPTION_REQ_CFG, _mtlk_core_get_coex_20_40_exm_req_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_COEX_20_40_AP_MIN_NUM_OF_EXM_STA_CFG, _mtlk_core_set_coex_20_40_min_num_exm_sta_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_COEX_20_40_AP_MIN_NUM_OF_EXM_STA_CFG, _mtlk_core_get_coex_20_40_min_num_exm_sta_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_COEX_20_40_TIMES_CFG,  _mtlk_core_set_coex_20_40_times_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_COEX_20_40_TIMES_CFG,  _mtlk_core_get_coex_20_40_times_cfg);
    /* Interference Detection */
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_INTERFDET_PARAMS_CFG,  _mtlk_core_set_interfdet_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_INTERFDET_MODE_CFG,    _mtlk_core_get_interfdet_mode_cfg);

    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_FW_LED_CFG,            _mtlk_core_set_fw_led_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_FW_LED_CFG,            _mtlk_core_get_fw_led_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_DBG_CLI,               _mtlk_core_simple_cli);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_FW_DEBUG,              _mtlk_core_fw_debug);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_FW_LOG_SEVERITY,       _mtlk_core_set_fw_log_severity);
    /* Traffic Analyzer */
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_TA_CFG,                _mtlk_core_set_ta_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_TA_CFG,                _mtlk_core_get_ta_cfg);

    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_ENHANCED11B_CFG,       _mtlk_core_set_enhanced11b_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_ENHANCED11B_CFG,       _mtlk_core_get_enhanced11b_cfg);

    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_11B_CFG,               _mtlk_core_set_11b_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_11B_CFG,               _mtlk_core_get_11b_cfg);

    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_DOT11W_CFG,            _mtlk_core_set_dot11w_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_DOT11W_CFG,            _mtlk_core_get_dot11w_cfg);

    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_RECOVERY_CFG,          _mtlk_core_set_recovery_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_RECOVERY_CFG,          _mtlk_core_get_recovery_cfg);

    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_MC_PS_SIZE_CFG,        _mtlk_core_set_mc_ps_size_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_MC_PS_SIZE_CFG,        _mtlk_core_get_mc_ps_size_cfg);

    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_UAPSD_MODE,            _mtlk_core_get_uapsd_mode);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_UAPSD_MODE,            _mtlk_core_set_uapsd_mode);

    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_SET_UAPSD_CFG,             _mtlk_core_set_uapsd_cfg);
    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_UAPSD_CFG,             _mtlk_core_get_uapsd_cfg);

    _MTLK_CORE_HANDLE_REQ_SERIALIZABLE(MTLK_CORE_REQ_GET_PS_STATUS,             _mtlk_core_get_ps_status);
  _MTLK_CORE_REQ_MAP_END()

#undef _MTLK_CORE_REQ_MAP_START
#undef _MTLK_CORE_HANDLE_REQ_SERIALIZABLE
#undef _MTLK_CORE_REQ_MAP_END
}

static int
_mtlk_core_get_prop (mtlk_vap_handle_t vap_handle, mtlk_core_prop_e prop_id, void* buffer, uint32 size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;

  switch (prop_id) {
  case MTLK_CORE_PROP_MAC_SW_RESET_ENABLED:
    if (buffer && size == sizeof(uint32))
    {
      uint32 *mac_sw_reset_enabled = (uint32 *)buffer;

      *mac_sw_reset_enabled = MTLK_CORE_PDB_GET_INT(mtlk_vap_get_core (vap_handle), PARAM_DB_CORE_MAC_SOFT_RESET_ENABLE);
      res = MTLK_ERR_OK;
    }
  case MTLK_CORE_PROP_IS_DUT:
    if (buffer && size == sizeof(BOOL))
    {
      BOOL *val = (BOOL *)buffer;
      *val = mtlk_is_dut_core_active(mtlk_vap_get_core(vap_handle));
      res = MTLK_ERR_OK;
    }
    break;
  case MTLK_CORE_PROP_IS_MAC_FATAL_PENDING:
    if (buffer && size == sizeof(BOOL))
    {
      BOOL *val = (BOOL *)buffer;
      *val = core_get_is_mac_fatal_pending(mtlk_vap_get_core(vap_handle));
      res = MTLK_ERR_OK;
    }
    break;
  default:
    break;
  }
  return res;
}

static int
_mtlk_core_set_prop (mtlk_vap_handle_t vap_handle,
                    mtlk_core_prop_e  prop_id,
                    void             *buffer,
                    uint32            size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_core_t *nic = mtlk_vap_get_core (vap_handle);

  switch (prop_id)
  {
  case MTLK_CORE_PROP_MAC_STUCK_DETECTED:
    if (buffer && size == sizeof(uint32))
    {
      uint32 *cpu_no = (uint32 *)buffer;
      nic->slow_ctx->mac_stuck_detected_by_sw = 1;
      mtlk_set_hw_state(nic, MTLK_HW_STATE_APPFATAL);
      mtlk_core_set_net_state(nic, NET_STATE_HALTED);
      mtlk_df_ui_notify_notify_fw_hang(mtlk_vap_get_df(nic->vap_handle), *cpu_no, MTLK_HW_STATE_APPFATAL);
    }
    break;
  case MTLK_CORE_PROP_IS_MAC_FATAL_PENDING:
    if (buffer && size == sizeof(BOOL))
    {
      BOOL val = *((BOOL*) buffer);
      core_set_is_mac_fatal_pending(mtlk_vap_get_core(vap_handle), val);
      res = MTLK_ERR_OK;
    }
    break;
  default:
    break;
  }

  return res;
}


void
mtlk_find_and_update_ap(mtlk_handle_t context, uint8 *addr, bss_data_t *bss_data)
{
  uint8 channel, is_ht;
  struct nic *nic = (struct nic *)context;
  int lost_beacons;
  sta_entry *sta = NULL;

  /* No updates in not connected state of for non-STA or during scan*/
  if (mtlk_vap_is_ap(nic->vap_handle) ||
      (mtlk_core_get_net_state(nic) != NET_STATE_CONNECTED) ||
      mtlk_core_scan_is_running(nic))
    return;

  /* Check wrong AP */
  sta = mtlk_stadb_find_sta(&nic->slow_ctx->stadb, addr);
  ILOG4_YP("Trying to find AP %Y, PTR is %p", addr, sta);
  if (sta == NULL) {
    nic->pstats.discard_nwi++;
    return;
  }

  /* Read setings for checks */
  is_ht = mtlk_core_get_is_ht_cur(nic);
  channel = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_CHANNEL_CUR);

  /* Check channel change */
  if (bss_data->channel != channel) {
    ILOG0_DYDD("CID-%04x: AP %Y changed its channel! (%u -> %u)", mtlk_vap_get_oid(nic->vap_handle), addr, channel, bss_data->channel);
    goto DISCONNECT;
  }

  /* Check HT capabilities change (only if HT is allowed in configuration) */
  if (mtlk_core_get_is_ht_cfg(nic) && !nic->slow_ctx->is_tkip && !nic->slow_ctx->wep_enabled &&
      (!!bss_data->is_ht != is_ht)) {
    ILOG0_DYS("CID-%04x: AP %Y changed its HT capabilities! (%s)", mtlk_vap_get_oid(nic->vap_handle),
        addr, is_ht ? "HT -> non-HT" : "non-HT -> HT");
    goto DISCONNECT;
  }

  /* Update lost beacons */
  lost_beacons = mtlk_sta_update_beacon_interval(sta, bss_data->beacon_interval);
  mtlk_sta_decref(sta); /* De-reference of find */
  nic->pstats.missed_beacon += lost_beacons;
  return;

DISCONNECT:
  mtlk_sta_decref(sta); /* De-reference of find */
  ILOG1_Y("Disconnecting AP %Y due to changed parameters", addr);
  _mtlk_core_schedule_disconnect_me(nic, FM_STATUSCODE_PEER_PARAMS_CHANGED, FRAME_REASON_UNSPECIFIED);
}

static signed int
find_acl_entry (IEEE_ADDR *list, IEEE_ADDR *mac, signed int *free_entry)
{
  int i;
  signed int idx;

  idx = -1;
  for (i = 0; i < MAX_ADDRESSES_IN_ACL; i++) {
    if (0 == mtlk_osal_compare_eth_addresses(mac->au8Addr, list[i].au8Addr)) {
      idx = i;
      break;
    }
  }
  if (NULL == free_entry)
	return idx;
  /* find first free entry */
  *free_entry = -1;
  for (i = 0; i < MAX_ADDRESSES_IN_ACL; i++) {
    if (mtlk_osal_is_zero_address(list[i].au8Addr)) {
      *free_entry = i;
      break;
    }
  }
  return idx;
}

int
mtlk_core_set_acl(struct nic *nic, IEEE_ADDR *mac, IEEE_ADDR *mac_mask)
{
  signed int idx, free_idx;
  IEEE_ADDR addr_tmp;

  if (mtlk_osal_is_zero_address(mac->au8Addr)) {
    ILOG2_V("Upload ACL list");
    return MTLK_ERR_OK;
  }

  /* Check pair MAC/MAC-mask consistency : MAC == (MAC & MAC-mask) */
  if (NULL != mac_mask) {
    mtlk_osal_eth_apply_mask(addr_tmp.au8Addr, mac->au8Addr, mac_mask->au8Addr);
    if (0 != mtlk_osal_compare_eth_addresses(addr_tmp.au8Addr, mac->au8Addr)) {
      WLOG_V("The ACL rule addition failed: "
           "The specified mask parameter is invalid. (MAC & MAC-Mask) != MAC.");
      return MTLK_ERR_PARAMS;
    }
  }

  idx = find_acl_entry(nic->slow_ctx->acl, mac, &free_idx);
  if (idx >= 0) {
    /* already on the list */
    WLOG_YD("MAC %Y is already on the ACL list at %d", mac->au8Addr, idx);
    return MTLK_ERR_OK;
  }
  if (free_idx < 0) {
    /* list is full */
    WLOG_V("ACL list is full");
    return MTLK_ERR_NO_RESOURCES;
  }
  /* add new entry */
  nic->slow_ctx->acl[free_idx] = *mac;
  if (NULL != mac_mask) {
    nic->slow_ctx->acl_mask[free_idx] = *mac_mask;
  } else {
    nic->slow_ctx->acl_mask[free_idx] = EMPTY_MAC_MASK;
  }

  ILOG2_YD("Added %Y to the ACL list at %d", mac->au8Addr, free_idx);
  return MTLK_ERR_OK;
}

int
mtlk_core_del_acl(struct nic *nic, IEEE_ADDR *mac)
{
  signed int idx;

  if (mtlk_osal_is_zero_address(mac->au8Addr)) {
    ILOG2_V("Delete ACL list");
    memset(nic->slow_ctx->acl, 0, sizeof(nic->slow_ctx->acl));
    return MTLK_ERR_OK;
  }
  idx = find_acl_entry(nic->slow_ctx->acl, mac, NULL);
  if (idx < 0) {
    /* not found on the list */
    WLOG_Y("MAC %Y is not on the ACL list", mac->au8Addr);
    return MTLK_ERR_PARAMS;
  }
  /* del entry */
  nic->slow_ctx->acl[idx] = EMPTY_MAC_ADDR;
  nic->slow_ctx->acl_mask[idx] = EMPTY_MAC_MASK;

  ILOG5_YD("Cleared %Y from the ACL list at %d", mac->au8Addr, idx);
  return MTLK_ERR_OK;
}

mtlk_handle_t __MTLK_IFUNC
mtlk_core_get_tx_limits_handle(mtlk_handle_t nic)
{
  return HANDLE_T(&(((struct nic*)nic)->slow_ctx->tx_limits));
}

int __MTLK_IFUNC
_mtlk_core_get_aocs_history(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_aocs_get_history(nic->slow_ctx->aocs, clpb);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_aocs_table(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_aocs_get_table(nic->slow_ctx->aocs, clpb);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_aocs_channels(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_aocs_get_channels(nic->slow_ctx->aocs, clpb);

  return res;
}

#ifdef BT_ACS_DEBUG
int __MTLK_IFUNC
_mtlk_core_set_aocs_channels_dbg(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  uint32 clpb_data_size;
  void* clpb_data;
  mtlk_aocs_channel_data_t *entry = NULL;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  clpb_data = mtlk_clpb_enum_get_next(clpb, &clpb_data_size);
  MTLK_ASSERT(NULL != clpb_data);
  MTLK_ASSERT(sizeof(mtlk_aocs_channel_data_t) == clpb_data_size);

  if (((mtlk_aocs_channel_data_t *)clpb_data)->stat.channel) {
    entry = (mtlk_aocs_channel_data_t *)mtlk_osal_mem_alloc(sizeof(*entry), MTLK_MEM_TAG_AOCS_ENTRY);
    if (NULL == entry) {
      res = MTLK_ERR_NO_MEM;
    } else {
      *entry = * (mtlk_aocs_channel_data_t *) clpb_data;
    }
  }
  mtlk_aocs_set_channels_dbg(nic->slow_ctx->aocs, entry);

  return res;
}
#endif  /* BT_ACS_DEBUG */

int __MTLK_IFUNC
_mtlk_core_get_aocs_penalties(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_aocs_get_penalties(nic->slow_ctx->aocs, clpb);

  return res;
}

#ifdef AOCS_DEBUG
int __MTLK_IFUNC
_mtlk_core_get_aocs_debug_update_cl(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  uint32 clpb_data_size;
  void* clpb_data;
  uint32 cl;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  clpb_data = mtlk_clpb_enum_get_next(clpb, &clpb_data_size);
  MTLK_ASSERT(NULL != clpb_data);
  MTLK_ASSERT(sizeof(uint32) == clpb_data_size);
  cl = *(uint32*) clpb_data;

  mtlk_aocs_debug_update_cl(nic->slow_ctx->aocs, cl);

  return res;
}
#endif /*AOCS_DEBUG*/

int __MTLK_IFUNC
_mtlk_core_get_hw_limits(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_channels_get_hw_limits(&nic->slow_ctx->tx_limits, clpb);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_reg_limits(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_channels_get_reg_limits(&nic->slow_ctx->tx_limits, clpb);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_ant_gain(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_channels_get_ant_gain(&nic->slow_ctx->tx_limits, clpb);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_l2nat_stats(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_l2nat_get_stats(nic, clpb);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_sq_status(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  BOOL get_peers_status;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  get_peers_status = (NET_STATE_CONNECTED == mtlk_core_get_net_state(nic));

  res = mtlk_sq_get_status(nic->sq, clpb, get_peers_status);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_ps_status(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;

  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_ps_get_status(&nic->slow_ctx->stadb, clpb);

  return res;
}

int __MTLK_IFUNC
_mtlk_core_set_mac_assert(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  uint32 clpb_data_size;
  void* clpb_data;
  uint32 assert_type;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  clpb_data = mtlk_clpb_enum_get_next(clpb, &clpb_data_size);
  MTLK_ASSERT(NULL != clpb_data);

  MTLK_ASSERT(sizeof(int) == clpb_data_size);
  assert_type = *(uint32*) clpb_data;

  ILOG0_DD("CID-%04x: Rise MAC assert (assert type=%d)", mtlk_vap_get_oid(nic->vap_handle), assert_type);

  switch (assert_type) {
    case MTLK_CORE_UI_ASSERT_TYPE_FW_LMIPS:
    case MTLK_CORE_UI_ASSERT_TYPE_FW_UMIPS:
     {
      uint32 mips_no;
      mips_no = (assert_type == MTLK_CORE_UI_ASSERT_TYPE_FW_UMIPS)?UMIPS:LMIPS;

      rcvry_set_forced_type(mtlk_vap_get_rcvry(nic->vap_handle), MTLK_CORE_UI_RCVRY_TYPE_NONE);

      res = mtlk_vap_get_hw_vft(nic->vap_handle)->set_prop(nic->vap_handle, MTLK_HW_DBG_ASSERT_FW, &mips_no, sizeof(mips_no));
      if (res != MTLK_ERR_OK) {
        ELOG_DDD("CID-%04x: Can't assert FW MIPS#%d (res=%d)", mtlk_vap_get_oid(nic->vap_handle), mips_no, res);
      }
    }
    break;

    case MTLK_CORE_UI_ASSERT_TYPE_DRV_DIV0:
    {
      volatile int do_bug = 0;
      do_bug = 1/do_bug;
      ILOG0_D("do_bug = %d", do_bug); /* To avoid compilation optimization */
      res = MTLK_ERR_OK;
    }
    break;

    case MTLK_CORE_UI_ASSERT_TYPE_DRV_BLOOP:
      while (1) {;}
      break;

    case MTLK_CORE_UI_ASSERT_TYPE_NONE:
    case MTLK_CORE_UI_ASSERT_TYPE_LAST:
    default:
      ILOG0_DD("CID-%04x: Unsupported assert type: %d", mtlk_vap_get_oid(nic->vap_handle), assert_type);
      res = MTLK_ERR_NOT_SUPPORTED;
      break;
  };

  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_mc_igmp_tbl(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_NOT_SUPPORTED;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_mc_dump_groups(nic, clpb);

  return res;
}

int
_mtlk_core_get_stadb_sta_list(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  mtlk_core_ui_get_stadb_status_req_t *get_stadb_status_req;
  uint32 size;
  hst_db *hstdb = NULL;
  uint8 group_cipher = FALSE;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if ( 0 == (mtlk_core_get_net_state(nic) & (NET_STATE_HALTED | NET_STATE_CONNECTED)) ) {
    mtlk_clpb_purge(clpb);
    return res;
  }

  get_stadb_status_req = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == get_stadb_status_req) || (sizeof(*get_stadb_status_req) != size) ) {
    res = MTLK_ERR_UNKNOWN;
    goto finish;
  }

  if (get_stadb_status_req->get_hostdb) {
    hstdb = &nic->slow_ctx->hstdb;
  }

  if (get_stadb_status_req->use_cipher) {
    group_cipher = nic->group_cipher;
  }

  mtlk_clpb_purge(clpb);
  res = mtlk_stadb_get_stat(&nic->slow_ctx->stadb, hstdb, clpb, group_cipher);

finish:
  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_ee_caps(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  res = mtlk_eeprom_get_caps(mtlk_core_get_eeprom(nic), clpb);

  return res;
}

mtlk_core_t * __MTLK_IFUNC
mtlk_core_get_master (mtlk_core_t *core)
{
  MTLK_ASSERT(core != NULL);

  return mtlk_vap_manager_get_master_core(mtlk_vap_get_manager(core->vap_handle));
}

uint8 __MTLK_IFUNC mtlk_core_is_device_busy(mtlk_handle_t context)
{

    mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, context);
    return (  MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_DFS_RADAR_DETECTION)
           && !mtlk_dot11h_can_switch_now(mtlk_core_get_dfs(nic)));
}

tx_limit_t* __MTLK_IFUNC
mtlk_core_get_tx_limits(mtlk_core_t *core)
{
  return &core->slow_ctx->tx_limits;
}

void
mtlk_core_configuration_dump(mtlk_core_t *core)
{
  static char *user_spectrum[] = {"20 MHz", "40 MHz", "Auto "};
  ILOG0_DS("CID-%04x: Country             : %s", mtlk_vap_get_oid(core->vap_handle), country_code_to_country(mtlk_core_get_country_code(core)));
  ILOG0_DD("CID-%04x: Domain              : %u", mtlk_vap_get_oid(core->vap_handle), country_code_to_domain(mtlk_core_get_country_code(core)));
  ILOG0_DS("CID-%04x: Network mode        : %s", mtlk_vap_get_oid(core->vap_handle), net_mode_to_string(mtlk_core_get_network_mode_cfg(core)));
  ILOG0_DS("CID-%04x: Band                : %s", mtlk_vap_get_oid(core->vap_handle), mtlk_eeprom_band_to_string(net_mode_to_band(mtlk_core_get_network_mode_cfg(core))));
  ILOG0_DS("CID-%04x: Prog Model Spectrum : %s MHz", mtlk_vap_get_oid(core->vap_handle), MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE) ? "40": "20");
  ILOG0_DS("CID-%04x: Selected Spectrum   : %s MHz", mtlk_vap_get_oid(core->vap_handle), MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE) ? "40": "20");
  ILOG0_DS("CID-%04x: User Sel. Spectrum  : %s", mtlk_vap_get_oid(core->vap_handle), user_spectrum[MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_USER_SPECTRUM_MODE)]);
  ILOG0_DS("CID-%04x: Bonding             : %s", mtlk_vap_get_oid(core->vap_handle),  MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_BONDING_MODE) == ALTERNATE_UPPER ? "upper" : (MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_BONDING_MODE) == ALTERNATE_LOWER ? "lower" : "none"));
  ILOG0_DS("CID-%04x: HT mode             : %s", mtlk_vap_get_oid(core->vap_handle),  mtlk_core_get_is_ht_cur(core) ? "enabled" : "disabled");
  ILOG0_DS("CID-%04x: SM enabled          : %s", mtlk_vap_get_oid(core->vap_handle),
      mtlk_eeprom_get_disable_sm_channels(mtlk_core_get_eeprom(core)) ? "disabled" : "enabled");
}


static void
_mtlk_core_prepare_stop(mtlk_vap_handle_t vap_handle)
{
  mtlk_core_t *nic = mtlk_vap_get_core (vap_handle);

  ILOG1_V("Core prepare stopping....");
  mtlk_osal_lock_acquire(&nic->net_state_lock);
  nic->is_stopping = TRUE;
  mtlk_osal_lock_release(&nic->net_state_lock);

  if (mtlk_vap_is_slave_ap(vap_handle)) {
    return;
  }

  rcvry_set_forced_type(mtlk_vap_get_rcvry(vap_handle), MTLK_CORE_UI_RCVRY_TYPE_NONE);

  if (NET_STATE_HALTED == mtlk_core_get_net_state(nic)) {
    if (mtlk_scan_is_running(&nic->slow_ctx->scan)) {
      scan_complete(&nic->slow_ctx->scan);
    }
  } else {
    scan_terminate_and_wait_completion(&nic->slow_ctx->scan);
  }
}

BOOL __MTLK_IFUNC
mtlk_core_is_stopping(mtlk_core_t *core)
{
  return (core->is_stopping || core->is_iface_stopping);
}

void
_mtlk_core_bswap_bcl_request (UMI_BCL_REQUEST *req, BOOL hdr_only)
{
  int i;

  req->Size    = cpu_to_le32(req->Size);
  req->Address = cpu_to_le32(req->Address);
  req->Unit    = cpu_to_le32(req->Unit);

  if (!hdr_only) {
    for (i = 0; i < ARRAY_SIZE(req->Data); i++) {
      req->Data[i] = cpu_to_le32(req->Data[i]);
    }
  }
}

int __MTLK_IFUNC
_mtlk_core_bcl_mac_data_get (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t* man_entry = NULL;
  int exception;
  mtlk_hw_state_e hw_state;
  UMI_BCL_REQUEST* preq;
  BOOL f_bswap_data = TRUE;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  /* Get BCL request from CLPB */
  preq = mtlk_clpb_enum_get_next(clpb, NULL);
  if (NULL == preq) {
    res = MTLK_ERR_PARAMS;
    goto finish;
  }

  /* Check MAC state */
  hw_state = mtlk_core_get_hw_state(core);
  exception = (((hw_state == MTLK_HW_STATE_EXCEPTION) ||
                (hw_state == MTLK_HW_STATE_APPFATAL) ||
                (hw_state == MTLK_HW_STATE_MAC_ASSERTED)) &&
               !core->slow_ctx->mac_stuck_detected_by_sw);

  /* if Core got here preq->Unit field wiath value greater or equal to BCL_UNIT_MAX -
   * the Core should not convert result data words in host format. */
  if (preq->Unit >= BCL_UNIT_MAX) {
    preq->Unit -= BCL_UNIT_MAX; /*Restore original field value*/
    f_bswap_data = FALSE;
  }

  ILOG3_SDDDD("Getting BCL over %s unit(%d) address(0x%x) size(%u) (%x)",
       exception ? "io" : "txmm",
       (int)preq->Unit,
       (unsigned int)preq->Address,
       (unsigned int)preq->Size,
       (unsigned int)preq->Data[0]);

  if (exception)
  {
    /* MAC is halted - send BCL request through IO */
    _mtlk_core_bswap_bcl_request(preq, TRUE);

    res = mtlk_vap_get_hw_vft(core->vap_handle)->get_prop(core->vap_handle, MTLK_HW_BCL_ON_EXCEPTION, preq, sizeof(*preq));

    if (MTLK_ERR_OK != res) {
      ELOG_D("CID-%04x: Can't get BCL", mtlk_vap_get_oid(core->vap_handle));
      goto err_push;
    }
  }
  else
  {
    /* MAC is in normal state - send BCL request through TXMM */
    man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
    if (!man_entry) {
      ELOG_D("CID-%04x: Can't send Get BCL request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
      res = MTLK_ERR_NO_RESOURCES;
      goto err_push;
    }

    _mtlk_core_bswap_bcl_request(preq, TRUE);

    memcpy((UMI_BCL_REQUEST*)man_entry->payload, preq, sizeof(*preq));

    man_entry->id           = UM_MAN_QUERY_BCL_VALUE;
    man_entry->payload_size = sizeof(*preq);

    res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);

    if (MTLK_ERR_OK != res) {
      ELOG_D("CID-%04x: Can't send Get BCL request to MAC, timed-out", mtlk_vap_get_oid(core->vap_handle));
      goto err_push;
    }

    /* Copy back results */
    memcpy(preq, (UMI_BCL_REQUEST*)man_entry->payload, sizeof(*preq));

    mtlk_txmm_msg_cleanup(&man_msg);
  }

  /* Send back results */
  _mtlk_core_bswap_bcl_request(preq, !f_bswap_data);

  mtlk_dump(3, preq, sizeof(*preq), "dump of the UM_MAN_QUERY_BCL_VALUE");

  res = mtlk_clpb_push(clpb, preq, sizeof(*preq));
  if (MTLK_ERR_OK != res) {
    goto err_push;
  }

  goto finish;

err_push:
  mtlk_clpb_purge(clpb);
finish:
  return res;
}

int __MTLK_IFUNC
_mtlk_core_bcl_mac_data_set (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  mtlk_txmm_msg_t   man_msg;
  mtlk_txmm_data_t* man_entry = NULL;
  int exception;
  mtlk_hw_state_e hw_state;
  UMI_BCL_REQUEST* preq = NULL;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  /* Read Set BCL request from CLPB */
  preq = mtlk_clpb_enum_get_next(clpb, NULL);
  if (NULL == preq) {
    res = MTLK_ERR_PARAMS;
    goto finish;
  }

  /* Check MAC state */
  hw_state = mtlk_core_get_hw_state(core);
  exception = (((hw_state == MTLK_HW_STATE_EXCEPTION) ||
                (hw_state == MTLK_HW_STATE_APPFATAL) ||
                (hw_state == MTLK_HW_STATE_MAC_ASSERTED)) &&
               !core->slow_ctx->mac_stuck_detected_by_sw);

  ILOG2_SDDDD("Setting BCL over %s unit(%d) address(0x%x) size(%u) (%x)",
       exception ? "io" : "txmm",
       (int)preq->Unit,
       (unsigned int)preq->Address,
       (unsigned int)preq->Size,
       (unsigned int)preq->Data[0]);

  mtlk_dump(3, preq, sizeof(*preq), "dump of the UM_MAN_SET_BCL_VALUE");

  if (exception)
  {
    /* MAC is halted - send BCL request through IO */
    _mtlk_core_bswap_bcl_request(preq, FALSE);

    res = mtlk_vap_get_hw_vft(core->vap_handle)->set_prop(core->vap_handle, MTLK_HW_BCL_ON_EXCEPTION, preq, sizeof(*preq));

    if (MTLK_ERR_OK != res) {
      ELOG_D("CID-%04x: Can't set BCL", mtlk_vap_get_oid(core->vap_handle));
      goto finish;
    }
  }
  else
  {
    /* MAC is in normal state - send BCL request through TXMM */
     man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
     if (!man_entry) {
       ELOG_D("CID-%04x: Can't send Set BCL request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
       res = MTLK_ERR_NO_RESOURCES;
       goto finish;
     }

     _mtlk_core_bswap_bcl_request(preq, FALSE);

     memcpy((UMI_BCL_REQUEST*)man_entry->payload, preq, sizeof(*preq));

     man_entry->id           = UM_MAN_SET_BCL_VALUE;
     man_entry->payload_size = sizeof(*preq);

     res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);

     if (MTLK_ERR_OK != res) {
       ELOG_D("CID-%04x: Can't send Set BCL request to MAC, timed-out", mtlk_vap_get_oid(core->vap_handle));
       goto finish;
     }

     mtlk_txmm_msg_cleanup(&man_msg);
  }

finish:
  return res;
}

static int
_mtlk_core_set_channel (mtlk_core_t *core, uint16 channel, uint8 spectrum_mode, BOOL userCfg)
{
  int res = MTLK_ERR_OK;
  mtlk_get_channel_data_t param;

  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  if (0 == channel) {
    ILOG1_D("CID-%04x: Enable channel auto-selection", mtlk_vap_get_oid(core->vap_handle));
  } else if (0 == mtlk_core_get_country_code(core)) {
    WLOG_DD("CID-%04x: Set channel to %i. (AP Workaround due to invalid Driver parameters setting at BSP startup)", mtlk_vap_get_oid(core->vap_handle),
            channel);
  } else {
    /* Check if channel is supported in current configuration */
    param.reg_domain = country_code_to_domain(mtlk_core_get_country_code(core));
    param.is_ht = TRUE;
    param.ap = mtlk_vap_is_ap(core->vap_handle);
    param.bonding = MTLK_CORE_PDB_GET_INT(core, userCfg ? PARAM_DB_CORE_USER_BONDING_MODE : PARAM_DB_CORE_SELECTED_BONDING_MODE);
    param.spectrum_mode = spectrum_mode;
    param.frequency_band = userCfg ? mtlk_core_get_freq_band_cfg(core) : mtlk_core_get_freq_band_cur(core);
    param.disable_sm_channels = mtlk_eeprom_get_disable_sm_channels(mtlk_core_get_eeprom(core));
    param.net_mode = userCfg ? mtlk_core_get_network_mode_cfg(core) : mtlk_core_get_network_mode_cur(core);

    if (param.bonding == (uint8)ALTERNATE_NONE){
      /* Force 20MHz in case of 40 MHz, but coexistence allows only 20MHz */
      param.spectrum_mode = SPECTRUM_20MHZ;
    }

    if (MTLK_ERR_OK == mtlk_check_channel(&param, channel)) {
      ILOG1_DD("CID-%04x: Set channel to %i", mtlk_vap_get_oid(core->vap_handle), channel);

    } else {
      WLOG_DD("CID-%04x: Channel (%i) is not supported in current configuration.", mtlk_vap_get_oid(core->vap_handle),
               channel);
      mtlk_core_configuration_dump(core);
      res = MTLK_ERR_PARAMS;
    }
  }

  if (MTLK_ERR_OK == res) {
    MTLK_CORE_PDB_SET_INT(core, userCfg ? PARAM_DB_CORE_CHANNEL_CFG : PARAM_DB_CORE_CHANNEL_CUR, channel);
  }

  return res;
}

static int
_mtlk_general_core_range_info_get (mtlk_core_t* nic, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_ui_range_info_t range_info;
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(nic->vap_handle));

  /* Get supported bitrates */
  {
    int avail = mtlk_core_get_available_bitrates(nic);
    uint32 short_cyclic_prefix = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_TX);
    int num_bitrates; /* Index in table returned to userspace */
    int value; /* Bitrate's value */
    int i; /* Bitrate index */
    int k, l; /* Counters, used for sorting */

    /* Array of bitrates is sorted and consist of only unique elements */
    num_bitrates = 0;
    for (i = BITRATE_FIRST; i <= BITRATE_LAST; i++) {
      if ((1 << i) & avail) {
        if(i == BITRATE_LAST)
        {
          short_cyclic_prefix |= MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SHORT_CYCLIC_PREFIX_RATE31);
        }
        value = mtlk_bitrate_get_value(i, MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE), short_cyclic_prefix);
        range_info.bitrates[num_bitrates] = value;
        k = num_bitrates;
        while (k && (range_info.bitrates[k-1] >= value)) k--; /* Position found */
        if ((k == num_bitrates) || (range_info.bitrates[k] != value)) {
          for (l = num_bitrates; l > k; l--)
            range_info.bitrates[l] = range_info.bitrates[l-1];
          range_info.bitrates[k] = value;
          num_bitrates++;
        }
      }
    }

    range_info.num_bitrates = num_bitrates;
  }

  /* Get supported channels */
  {
    uint8 band;
    mtlk_get_channel_data_t param;

    if (0 == mtlk_core_get_country_code(nic)) {
       WLOG_D("CID-%04x: Country is not selected. Channels information is not available.",
               mtlk_vap_get_oid(nic->vap_handle));
       goto finish;
    }

    if (!mtlk_vap_is_ap(nic->vap_handle) && (NET_STATE_CONNECTED == mtlk_core_get_net_state(nic)) )
      band = mtlk_core_get_freq_band_cur(nic);
    else
      band = mtlk_core_get_freq_band_cfg(nic);

    param.reg_domain = country_code_to_domain(mtlk_core_get_country_code(nic));
    param.is_ht = TRUE;
    param.ap = mtlk_vap_is_ap(nic->vap_handle);
    param.bonding = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_BONDING_MODE);
    param.spectrum_mode = MTLK_CORE_PDB_GET_INT(nic, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE);
    param.frequency_band = band;
    param.disable_sm_channels = mtlk_eeprom_get_disable_sm_channels(mtlk_core_get_eeprom(nic));

    range_info.num_channels = mtlk_get_avail_channels(&param, range_info.channels);
  }

finish:
  res = mtlk_clpb_push(clpb, &range_info, sizeof(range_info));
  if (MTLK_ERR_OK != res) {
    mtlk_clpb_purge(clpb);
  }

  return res;
}

static int
_mtlk_slave_core_range_info_get (mtlk_core_t* nic, const void* data, uint32 data_size)
{
  return (_mtlk_general_core_range_info_get (mtlk_core_get_master (nic), data, data_size));
}

int __MTLK_IFUNC
_mtlk_core_range_info_get (mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  mtlk_core_t *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  if (!mtlk_vap_is_slave_ap (core->vap_handle)) {
    return _mtlk_general_core_range_info_get (core, data, data_size);
  }
  else
    return _mtlk_slave_core_range_info_get (core, data, data_size);
}

int __MTLK_IFUNC
_mtlk_core_start_scanning(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_BUSY;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  char *essid = NULL;
  int net_state;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  net_state = mtlk_core_get_net_state(nic);

  /* allow scanning in net states ready and connected only */
  if ((net_state & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    WLOG1_DS("CID-%04x: Cannot start scanning in state %s", mtlk_vap_get_oid(nic->vap_handle), mtlk_net_state_to_string(net_state));
    goto CANNOT_SCAN;
  }
  if ((net_state == NET_STATE_CONNECTED) &&
      !mtlk_scan_is_background_scan_enabled(&nic->slow_ctx->scan)) {
    WLOG1_D("CID-%04x: BG scan is off - cannot start scanning", mtlk_vap_get_oid(nic->vap_handle));
    goto CANNOT_SCAN;
  }

  /* Although we won't start scan when the previous hasn't completed yet anyway
   * (scan module prohibits this) -
   * we need to return 0 in this case to indicate the scan
   * has been started successfully.
   * Otherwise wpa_supplicant will wait for scan completion for 3 seconds only
   * (which is not enough for us in the majority of cases to finish scan)
   * and iwlist will simply report error to the user.
   * If we return 0 - they start polling us for scan results, which is
   * an expected behavior.
   */
  res = MTLK_ERR_OK;
  if (mtlk_scan_is_running(&nic->slow_ctx->scan)) {
    ILOG1_D("CID-%04x: Scan in progress - cannot start scanning", mtlk_vap_get_oid(nic->vap_handle));
    goto CANNOT_SCAN;
  }
  if (mtlk_core_is_stopping(nic)) {
    WLOG1_D("CID-%04x: Core is being stopped - cannot start scanning", mtlk_vap_get_oid(nic->vap_handle));
    goto CANNOT_SCAN;
  }

  /* iwlist wlan0 scan <ESSID> */
  essid = mtlk_clpb_enum_get_next(clpb, NULL);

  /* Perform scanning */
  if (net_state == NET_STATE_CONNECTED)
    mtlk_scan_set_background(&nic->slow_ctx->scan, TRUE);
  else
    mtlk_scan_set_background(&nic->slow_ctx->scan, FALSE);
  res = mtlk_scan_async(&nic->slow_ctx->scan, mtlk_core_get_freq_band_cfg(nic), essid);

CANNOT_SCAN:
  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_scanning_res(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int         res = MTLK_ERR_OK;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  bss_data_t  bss_found;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if (mtlk_scan_is_running(&nic->slow_ctx->scan)) {
    ILOG1_D("CID-%04x: Can't get scan results - scan in progress", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }
  if (mtlk_core_is_stopping(nic)) {
    WLOG1_D("CID-%04x: Core is being stopped - cannot start scanning", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_IN_USE;
    goto finish;
  }

  mtlk_cache_rewind(&nic->slow_ctx->cache);
  while (mtlk_cache_get_next_bss(&nic->slow_ctx->cache, &bss_found, NULL, NULL)) {
    res = mtlk_clpb_push(clpb, &bss_found, sizeof(bss_found));
    if (MTLK_ERR_OK != res) {
      goto err_push;
    }
  }

  goto finish;

err_push:
  mtlk_clpb_purge(clpb);
finish:
  return res;
}

static int
_mtlk_core_set_wep (struct nic *nic, int wep_enabled)
{
  int res = MTLK_ERR_NOT_SUPPORTED;

  if ((!wep_enabled && !nic->slow_ctx->wep_enabled) ||
      (wep_enabled && nic->slow_ctx->wep_enabled)) { /* WEB state is not changed */
    res = MTLK_ERR_OK;
    goto end;
  }

  if (wep_enabled && is_ht_net_mode(mtlk_core_get_network_mode_cfg(nic))) {
    if (mtlk_vap_is_ap(nic->vap_handle)) {
      ELOG_DS("CID-%04x: AP: Can't set WEP for HT Network Mode (%s)", mtlk_vap_get_oid(nic->vap_handle),
           net_mode_to_string(mtlk_core_get_network_mode_cfg(nic)));
      goto end;
    }
    else if (!is_mixed_net_mode(mtlk_core_get_network_mode_cfg(nic))) {
      ELOG_DS("CID-%04x: STA: Can't set WEP for HT-only Network Mode (%s)", mtlk_vap_get_oid(nic->vap_handle),
        net_mode_to_string(mtlk_core_get_network_mode_cfg(nic)));
      goto end;
    }
  }

  res = mtlk_set_mib_value_uint8(mtlk_vap_get_txmm(nic->vap_handle),
                                 MIB_PRIVACY_INVOKED,
                                 wep_enabled);
  if (res != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Failed to enable WEP encryption (err=%d)", mtlk_vap_get_oid(nic->vap_handle), res);
    goto end;
  }
  nic->slow_ctx->wep_enabled = wep_enabled;
  if (wep_enabled) {
    ILOG1_V("WEP encryption enabled");
  }else {
    ILOG1_V("WEP encryption disabled");
  }

end:
  return res;
}

int __MTLK_IFUNC
_mtlk_core_set_wep_enc_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t *clpb = *(mtlk_clpb_t **) data;
  mtlk_core_ui_enc_cfg_t   *enc_cfg;
  uint32                    size;
  mtlk_txmm_t              *txmm = mtlk_vap_get_txmm(nic->vap_handle);
  IEEE_ADDR                 addr;
  MIB_WEP_KEY              *enc_cfg_wep_key;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if ((mtlk_core_get_net_state(nic) & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    ILOG1_D("CID-%04x: Invalid card state - request rejected", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }
  if (mtlk_core_scan_is_running(nic)) {
    ILOG1_D("CID-%04x: Can't set WEP configuration - scan in progress", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  enc_cfg = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == enc_cfg) || (sizeof(*enc_cfg) != size) ) {
    res = MTLK_ERR_UNKNOWN;
    goto finish;
  }

  /* New key always passed in array's slot 0 */
  enc_cfg_wep_key = &enc_cfg->wep_keys.sKey[0];

  if (( MAX_UINT8 != enc_cfg->key_id) && (enc_cfg_wep_key->u8KeyLength == 0)) { /* MAX_UINT8 => not specified */
    /* new key index is specified */
    res = mtlk_set_mib_value_uint8(txmm, MIB_WEP_DEFAULT_KEYID, enc_cfg->key_id);
    if (MTLK_ERR_OK != res) {
      ELOG_D("CID-%04x: Unable to set WEP TX key index", mtlk_vap_get_oid(nic->vap_handle));
      goto finish;
    }
    nic->slow_ctx->default_key = enc_cfg->key_id;
    ILOG1_D("Set WEP TX key index to %i", enc_cfg->key_id);
  }

  if (enc_cfg_wep_key->u8KeyLength != 0) {
    /* Set WEP key if specified */

    MIB_WEP_DEF_KEYS wep_keys;

    if ( MAX_UINT8 == enc_cfg->key_id ) {
      /* if key index not specified get current index from FW */
      res = mtlk_get_mib_value_uint8(txmm, MIB_WEP_DEFAULT_KEYID, &enc_cfg->key_id);
      if (MTLK_ERR_OK != res) {
        ELOG_D("CID-%04x: Unable to get WEP TX key index", mtlk_vap_get_oid(nic->vap_handle));
        goto finish;
      }
    }

    /* make a local copy of existing keys */
    memcpy(&wep_keys, &nic->slow_ctx->wep_keys, sizeof(wep_keys));

    /* Update specified key in local copy*/
    wep_keys.sKey[enc_cfg->key_id].u8KeyLength = enc_cfg_wep_key->u8KeyLength;

    memcpy(wep_keys.sKey[enc_cfg->key_id].au8KeyData,
           enc_cfg_wep_key->au8KeyData,
           enc_cfg_wep_key->u8KeyLength);

    /* Send all keys to FW */
    res = mtlk_set_mib_value_raw(txmm, MIB_WEP_DEFAULT_KEYS, (MIB_VALUE*)&wep_keys);
    if (res == MTLK_ERR_OK) {
      /* Update local copy of modified key */
      nic->slow_ctx->wep_keys.sKey[enc_cfg->key_id] = wep_keys.sKey[enc_cfg->key_id];
      ILOG2_D("Successfully set WEP key #%i", enc_cfg->key_id);
      mtlk_dump(2, wep_keys.sKey[enc_cfg->key_id].au8KeyData, wep_keys.sKey[enc_cfg->key_id].u8KeyLength, "");
    } else {
      ELOG_D("CID-%04x: Failed to set WEP key", mtlk_vap_get_oid(nic->vap_handle));
      goto finish;
    }
  }

  if ( MAX_UINT8 != enc_cfg->authentication ) { /* MAX_UINT8 => not specified */
    res = mtlk_set_mib_value_uint8(txmm, MIB_AUTHENTICATION_PREFERENCE, enc_cfg->authentication);
    if (MTLK_ERR_OK != res) {
      ELOG_DD("CID-%04x: Failed to switch access policy to %i", mtlk_vap_get_oid(nic->vap_handle), enc_cfg->authentication);
      goto finish;
    }
    nic->authentication = enc_cfg->authentication;
    ILOG1_D("Access policy switched to %i", enc_cfg->authentication);
  }

  if ( MAX_UINT8 != enc_cfg->wep_enabled ) { /* MAX_UINT8 => not specified */
    res = _mtlk_core_set_wep(nic, enc_cfg->wep_enabled);
    if (MTLK_ERR_OK != res) {
      ELOG_V("Failed to set WEP to Core");
      goto finish;
    }
  }

   /* Update RSN group key */
  memset(addr.au8Addr, 0xff, sizeof(addr.au8Addr));
  res = _mtlk_core_set_wep_key_blocked(nic, &addr, nic->slow_ctx->default_key);
  if (MTLK_ERR_OK != res) {
    ELOG_V("Failed to update group WEP key");
    goto finish;
  }

finish:
  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_wep_enc_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int                       res = MTLK_ERR_OK;
  mtlk_core_t               *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t               *clpb = *(mtlk_clpb_t **) data;
  mtlk_core_ui_enc_cfg_t    enc_cfg;
  mtlk_txmm_t               *txmm = mtlk_vap_get_txmm(nic->vap_handle);
  uint8                     tmp;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if ((mtlk_core_get_net_state(nic) & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    ILOG1_D("CID-%04x: Invalid card state - request rejected", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }
  if (mtlk_core_scan_is_running(nic)) {
    ILOG1_D("CID-%04x: Can't get WEP configuration - scan in progress", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  memset(&enc_cfg, 0, sizeof(enc_cfg));

  enc_cfg.wep_enabled = nic->slow_ctx->wep_enabled;

  if (nic->slow_ctx->wep_enabled) {
    /* Report access policy */
    res = mtlk_get_mib_value_uint8(txmm, MIB_AUTHENTICATION_PREFERENCE, &tmp);
    if (res != MTLK_ERR_OK) {
      ELOG_D("CID-%04x: Failed to read WEP access policy", mtlk_vap_get_oid(nic->vap_handle));
      goto finish;
    }
    enc_cfg.authentication = tmp;
  }

  enc_cfg.key_id = nic->slow_ctx->default_key;
  enc_cfg.wep_keys = nic->slow_ctx->wep_keys;

  res = mtlk_clpb_push(clpb, &enc_cfg, sizeof(enc_cfg));
  if (MTLK_ERR_OK != res) {
    mtlk_clpb_purge(clpb);
  }

finish:
  return res;
}


int __MTLK_IFUNC
_mtlk_core_set_auth_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int                       res = MTLK_ERR_OK;
  mtlk_core_t               *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t               *clpb = *(mtlk_clpb_t **) data;
  mtlk_core_ui_auth_cfg_t   *auth_cfg;
  uint32                    size;
  mtlk_txmm_t               *txmm = mtlk_vap_get_txmm(nic->vap_handle);

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if ((mtlk_core_get_net_state(nic) & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    ILOG1_D("CID-%04x: Invalid card state - request rejected", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_is_stopping(nic)) {
    ILOG1_D("CID-%04x: Can't set AUTH configuration - core is stopping", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_scan_is_running(nic)) {
    ILOG1_D("CID-%04x: Can't set AUTH configuration - scan in progress", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  auth_cfg = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == auth_cfg) || (sizeof(*auth_cfg) != size) ) {
    ELOG_D("CID-%04x: Failed to get AUTH configuration parameters from CLPB", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_UNKNOWN;
    goto finish;
  }

  if (0 <= auth_cfg->rsn_enabled) {
    res = mtlk_set_mib_rsn(txmm, auth_cfg->rsn_enabled);
    if (MTLK_ERR_OK != res) {
      ELOG_DD("CID-%04x: Failed to switch RSN state to %i", mtlk_vap_get_oid(nic->vap_handle), auth_cfg->rsn_enabled);
      goto finish;
    }
    nic->rsn_enabled = auth_cfg->rsn_enabled;
    ILOG1_DD("CID-%04x: RSN switched to %i", mtlk_vap_get_oid(nic->vap_handle), auth_cfg->rsn_enabled);
  }

  if (0 <= auth_cfg->wep_enabled) {
    res = _mtlk_core_set_wep(nic, auth_cfg->wep_enabled);
    if (MTLK_ERR_OK != res) {
      goto finish;
    }
  }

  if (0 <= auth_cfg->authentication) {
    res = mtlk_set_mib_value_uint8(txmm, MIB_AUTHENTICATION_PREFERENCE, auth_cfg->authentication);
    if (MTLK_ERR_OK != res) {
      ELOG_DD("CID-%04x: Failed to switch access policy to %i", mtlk_vap_get_oid(nic->vap_handle), auth_cfg->authentication);
      goto finish;
    }
    nic->authentication = auth_cfg->authentication;
    ILOG1_DD("CID-%04x: Access policy switched to %i", mtlk_vap_get_oid(nic->vap_handle), auth_cfg->authentication);
  }

finish:
  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_auth_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int                       res = MTLK_ERR_OK;
  mtlk_core_t               *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t               *clpb = *(mtlk_clpb_t **) data;
  mtlk_core_ui_auth_state_t auth_state;
  sta_entry                 *sta;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if ((mtlk_core_get_net_state(nic) & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    ILOG1_D("CID-%04x: Invalid card state - request rejected", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_is_stopping(nic)) {
    ILOG1_D("CID-%04x: Can't set AUTH configuration - core is stopping", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_scan_is_running(nic)) {
    ILOG1_D("CID-%04x: Can't set AUTH configuration - scan in progress", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  memset(&auth_state, 0, sizeof(auth_state));

  auth_state.rsnie = nic->slow_ctx->rsnie;
  auth_state.group_cipher = nic->group_cipher;
  auth_state.cipher_pairwise = -1;

  if (!mtlk_vap_is_ap(nic->vap_handle)) {
    sta = mtlk_stadb_get_ap(&nic->slow_ctx->stadb);
    if (!sta) {
      res = MTLK_ERR_PARAMS;
      goto finish;
    }

    auth_state.cipher_pairwise = mtlk_sta_get_cipher(sta);
    mtlk_sta_decref(sta); /* De-reference of get_ap */
  }

  res = mtlk_clpb_push(clpb, &auth_state, sizeof(auth_state));
  if (MTLK_ERR_OK != res) {
    mtlk_clpb_purge(clpb);
  }

finish:
  return res;
}

static void __MTLK_IFUNC
_mtlk_core_set_wps_in_progress(mtlk_core_t *core, uint8 wps_in_progress)
{
  core->slow_ctx->wps_in_progress = wps_in_progress;
  if (wps_in_progress)
    ILOG1_D("CID-%04x: WPS in progress", mtlk_vap_get_oid(core->vap_handle));
  else
    ILOG1_D("CID-%04x: WPS stopped", mtlk_vap_get_oid(core->vap_handle));
}

static int
_mtlk_core_set_rsn_enabled (mtlk_core_t *core, uint8 rsn_enabled)
{
  int res;

  res = mtlk_set_mib_rsn(mtlk_vap_get_txmm(core->vap_handle), rsn_enabled);
  if (res == MTLK_ERR_OK) {
    core->rsn_enabled = rsn_enabled;
  }

  return res;
}

int __MTLK_IFUNC
_mtlk_core_set_genie_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int                       res = MTLK_ERR_OK;
  mtlk_core_t               *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t               *clpb = *(mtlk_clpb_t **) data;
  mtlk_core_ui_genie_cfg_t  *genie_cfg;
  uint32                    size;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if ((mtlk_core_get_net_state(core) & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    ILOG1_D("CID-%04x:: Invalid card state - request rejected", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_is_stopping(core)) {
    ILOG1_D("CID-%04x: Can't set GEN_IE configuration - core is stopping", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_scan_is_running(core)) {
    ILOG1_D("CID-%04x: Can't set GEN_IE configuration - scan in progress", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  genie_cfg = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == genie_cfg) || (sizeof(*genie_cfg) != size) ) {
    ELOG_D("CID-%04x: Failed to get GEN_IE configuration parameters from CLPB", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_UNKNOWN;
    goto finish;
  }

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()

    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(genie_cfg, wps_in_progress,
        _mtlk_core_set_wps_in_progress, (core, genie_cfg->wps_in_progress));

    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(genie_cfg, rsnie_reset,
        memset, (&core->slow_ctx->rsnie, 0, sizeof(core->slow_ctx->rsnie)));

    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(genie_cfg, rsnie,
        memcpy, (&core->slow_ctx->rsnie, &genie_cfg->rsnie, sizeof(core->slow_ctx->rsnie)));

    MTLK_CFG_CHECK_ITEM_AND_CALL(genie_cfg, gen_ie_set, mtlk_core_set_gen_ie,
       (core, genie_cfg->gen_ie, genie_cfg->gen_ie_len, genie_cfg->gen_ie_type), res);
    MTLK_CFG_CHECK_ITEM_AND_CALL(genie_cfg, gen_ie_set, _mtlk_core_store_gen_ie,
       (core, genie_cfg->gen_ie, genie_cfg->gen_ie_len, genie_cfg->gen_ie_type), res);

    MTLK_CFG_CHECK_ITEM_AND_CALL(genie_cfg, rsn_enabled, _mtlk_core_set_rsn_enabled,
                                (core, genie_cfg->rsn_enabled), res);

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

finish:
  return res;
}

int __MTLK_IFUNC
_mtlk_core_get_enc_ext_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int                       res = MTLK_ERR_OK;
  mtlk_core_t               *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t               *clpb = *(mtlk_clpb_t **) data;
  mtlk_txmm_t               *txmm = mtlk_vap_get_txmm(nic->vap_handle);
  mtlk_txmm_msg_t           man_msg;
  mtlk_txmm_data_t          *man_entry = NULL;
  UMI_GROUP_PN              *umi_gpn;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if (0 != (mtlk_core_get_net_state(nic) & (NET_STATE_HALTED | NET_STATE_IDLE))) {
    ILOG1_D("CID-%04x: Invalid card state - request rejected", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_scan_is_running(nic)) {
    ILOG1_D("CID-%04x: Can't get WEP configuration - scan in progress", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, txmm, NULL);
  if (NULL == man_entry) {
    ELOG_D("CID-%04x: No man entry available", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NO_RESOURCES;
    goto finish;
  }

  umi_gpn = (UMI_GROUP_PN*)man_entry->payload;
  memset(umi_gpn, 0, sizeof(UMI_GROUP_PN));

  man_entry->id           = UM_MAN_GET_GROUP_PN_REQ;
  man_entry->payload_size = sizeof(UMI_GROUP_PN);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_D("CID-%04x: Timeout expired while waiting for CFM from MAC", mtlk_vap_get_oid(nic->vap_handle));
    goto finish;
  }

  umi_gpn->u16Status = le16_to_cpu(umi_gpn->u16Status);
  if (UMI_OK != umi_gpn->u16Status) {
    ELOG_DD("CID-%04x: GET_GROUP_PN_REQ failed: %u", mtlk_vap_get_oid(nic->vap_handle), umi_gpn->u16Status);
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  res = mtlk_clpb_push(clpb, umi_gpn, sizeof(*umi_gpn));
  if (MTLK_ERR_OK != res) {
    mtlk_clpb_purge(clpb);
  }

finish:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}

static void
_mtlk_core_set_rx_seq(struct nic *nic, uint16 idx, uint8* rx_seq)
{
  nic->group_rsc[idx][0] = rx_seq[5];
  nic->group_rsc[idx][1] = rx_seq[4];
  nic->group_rsc[idx][2] = rx_seq[3];
  nic->group_rsc[idx][3] = rx_seq[2];
  nic->group_rsc[idx][4] = rx_seq[1];
  nic->group_rsc[idx][5] = rx_seq[0];
}


int __MTLK_IFUNC
_mtlk_core_set_enc_ext_cfg(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int                       res = MTLK_ERR_OK;
  mtlk_core_t               *core = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t               *clpb = *(mtlk_clpb_t **) data;
  mtlk_txmm_msg_t           man_msg;
  mtlk_txmm_data_t          *man_entry = NULL;
  mtlk_txmm_t               *txmm = mtlk_vap_get_txmm(core->vap_handle);

  mtlk_core_ui_encext_cfg_t *encext_cfg;
  uint32                    size;
  UMI_SET_KEY               *umi_key;
  uint16                    alg_type = IW_ENCODE_ALG_NONE;
  uint16                    key_len = 0;
  uint16                    key_idx = 0;
  sta_entry                 *sta = NULL;
  mtlk_pckt_filter_e        sta_filter_stored = MTLK_PCKT_FLTR_ALLOW_ALL;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  if ((mtlk_core_get_net_state(core) & (NET_STATE_READY | NET_STATE_CONNECTED)) == 0) {
    ILOG1_D("CID-%04x: Invalid card state - request rejected", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_is_stopping(core)) {
    ILOG1_D("CID-%04x: Can't set ENC_EXT configuration - core is stopping", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  if (mtlk_core_scan_is_running(core)) {
    ILOG1_D("CID-%04x: Can't set ENC_EXT configuration - scan in progress", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NOT_READY;
    goto finish;
  }

  encext_cfg = mtlk_clpb_enum_get_next(clpb, &size);
  if ( (NULL == encext_cfg) || (sizeof(*encext_cfg) != size) ) {
    ELOG_D("CID-%04x: Failed to get ENC_EXT configuration parameters from CLPB", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_UNKNOWN;
    goto finish;
  }

  /* Prepare UMI message */
  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, txmm, NULL);
  if (!man_entry) {
    ELOG_D("CID-%04x: No man entry available", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NO_RESOURCES;
    goto finish;
  }

  umi_key = (UMI_SET_KEY*)man_entry->payload;
  memset(umi_key, 0, sizeof(*umi_key));

  man_entry->id           = UM_MAN_SET_KEY_REQ;
  man_entry->payload_size = sizeof(*umi_key);

  MTLK_CFG_START_CHEK_ITEM_AND_CALL()
    MTLK_CFG_GET_ITEM(encext_cfg, alg_type, alg_type);

    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(encext_cfg, sta_addr, memcpy,
        (umi_key->sStationID.au8Addr, encext_cfg->sta_addr.au8Addr, sizeof(umi_key->sStationID.au8Addr)));

    /* NB: key_idx now is one less than real value.
     * This means: 0-3,4-5 instead of 1-4,5-6 */
    MTLK_CFG_GET_ITEM(encext_cfg, key_idx, key_idx);

    /* Set default key */
    MTLK_CFG_CHECK_ITEM_AND_CALL(encext_cfg, default_key_idx,
        mtlk_set_mib_value_uint8, (txmm, MIB_WEP_DEFAULT_KEYID, encext_cfg->default_key_idx), res);

    MTLK_CFG_GET_ITEM(encext_cfg, default_key_idx, core->slow_ctx->default_key);

    MTLK_CFG_GET_ITEM(encext_cfg, key_len, key_len);

    /* Set WEP key if needed */
    if (alg_type == IW_ENCODE_ALG_WEP ||
       (alg_type == IW_ENCODE_ALG_NONE && core->slow_ctx->wep_enabled && key_idx < MIB_WEP_N_DEFAULT_KEYS))
          // IW_ENCODE_ALG_NONE - reset keys
    {
      /* Set WEP key */
      MIB_WEP_DEF_KEYS wep_keys;

      memcpy(&wep_keys, &core->slow_ctx->wep_keys, sizeof(wep_keys));

      wep_keys.sKey[key_idx].u8KeyLength = key_len;

      memset(wep_keys.sKey[key_idx].au8KeyData, 0,
             sizeof(wep_keys.sKey[key_idx].u8KeyLength));

      if (0 < key_len) {
        MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(encext_cfg, key, memcpy,
            (wep_keys.sKey[key_idx].au8KeyData, encext_cfg->key, key_len));
      }

      res = mtlk_set_mib_value_raw(txmm, MIB_WEP_DEFAULT_KEYS, (MIB_VALUE*)&wep_keys);
      if (res == MTLK_ERR_OK) {
        core->slow_ctx->wep_keys = wep_keys;
        ILOG1_D("Successfully set WEP key #%i", key_idx);
        mtlk_dump(1, wep_keys.sKey[key_idx].au8KeyData,
              wep_keys.sKey[key_idx].u8KeyLength, "");
      } else {
        ELOG_D("CID-%04x: Failed to set WEP key", mtlk_vap_get_oid(core->vap_handle));
        goto finish;
      }
    }

    /* Reset IGTK keys */
    if(
      alg_type == IW_ENCODE_ALG_NONE &&
      key_len == 0 &&
      (
        (key_idx == (UMI_RSN_IGTK_GM_KEY_INDEX - 1)) ||
        (key_idx == (UMI_RSN_IGTK_GN_KEY_INDEX - 1))
      )
    )
    {
      core->igtk_cipher = alg_type;
      core->igtk_key_len = key_len;
    }

    /* Enable disable WEP */
    res = _mtlk_core_set_wep(core, encext_cfg->wep_enabled);
    if (MTLK_ERR_OK != res) goto finish;

    /* Extract UNI key */
    /* The key has been copied into au8Tk1 array with UMI_RSN_TK1_LEN size.
     * But key can have UMI_RSN_TK1_LEN+UMI_RSN_TK2_LEN size - so
     * actually second part of key is copied into au8Tk2 array */
    MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(encext_cfg, key, memcpy,
              (umi_key->au8Tk1, encext_cfg->key, key_len));


    if (mtlk_osal_eth_is_broadcast(umi_key->sStationID.au8Addr))
    {
      umi_key->u16KeyType = cpu_to_le16(UMI_RSN_GROUP_KEY);

      if(alg_type == IW_ENCODE_ALG_AES_CMAC)
      {
        uint8 igtk_key_array_idx = (key_idx == (UMI_RSN_IGTK_GM_KEY_INDEX - 1)) ? 0 : 1;

        core->igtk_cipher = alg_type;
        core->igtk_key_len = key_len;
        memcpy(core->igtk_key[igtk_key_array_idx], encext_cfg->key, key_len);
      }
      else
      {
        core->group_cipher = alg_type;

        memset(core->group_rsc[key_idx], 0, sizeof(core->group_rsc[0]));

        MTLK_CFG_CHECK_ITEM_AND_CALL_VOID(encext_cfg, rx_seq,
            _mtlk_core_set_rx_seq, (core, key_idx, encext_cfg->rx_seq));
      }
    } else {
      umi_key->u16KeyType = cpu_to_le16(UMI_RSN_PAIRWISE_KEY);
    }

  MTLK_CFG_END_CHEK_ITEM_AND_CALL()

  if (MTLK_ERR_OK != res) {
    goto finish;
  }

  switch (alg_type) {
  case IW_ENCODE_ALG_NONE:
    /* IW_ENCODE_ALG_NONE - reset keys */
    umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_NONE);
    break;
  case IW_ENCODE_ALG_WEP:
    umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_WEP40);
    break;
  case IW_ENCODE_ALG_TKIP:
    umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_TKIP);
    break;
  case IW_ENCODE_ALG_CCMP:
    umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_CCMP);
    break;
  case IW_ENCODE_ALG_AES_CMAC:
    umi_key->u16CipherSuite = cpu_to_le16(UMI_RSN_CIPHER_SUITE_IGTK);
    break;
  }

  /* ant, 13 Apr 07: replay detection is performed by driver,
   * so MAC does not need this.
  if (ext->ext_flags & IW_ENCODE_EXT_RX_SEQ_VALID)
    memcpy(umi_key->au8RxSeqNum, ext->rx_seq, ARRAY_SIZE(umi_key->au8RxSeqNum));
    */
  /* ant: DO NOT SET THIS: supplicant is doing the job for us
   * (the job of swapping 16 bytes of umi_key->au8Tk2 in TKIP)
   * (umi_key->au8Tk2 is used in TKIP only)
  if (mtlk_vap_is_ap(nic->vap_handle))
    umi_key->u16StationRole = cpu_to_le16(UMI_RSN_AUTHENTICATOR);
  else
    umi_key->u16StationRole = cpu_to_le16(UMI_RSN_SUPPLICANT);
    */

  if (cpu_to_le16(UMI_RSN_PAIRWISE_KEY) == umi_key->u16KeyType) {
    sta = mtlk_stadb_find_sta(&core->slow_ctx->stadb, umi_key->sStationID.au8Addr);
    if (NULL == sta) {
    /* Supplicant reset keys for AP from which we just were disconnected */
      ILOG1_Y("There is no connection with %Y", umi_key->sStationID.au8Addr);
      goto finish;
    }

    mtlk_sta_set_cipher(sta, alg_type);
    mtlk_sta_zero_rod_reply_counters(sta);
    mtlk_sta_zero_pmf_replay_counters(sta);

    if (0 == key_len) {
      mtlk_sta_set_packets_filter(sta, MTLK_PCKT_FLTR_ALLOW_802_1X);
      ILOG1_Y("%Y: turn on 802.1x filtering", mtlk_sta_get_addr(sta));

    } else /*key_len != 0*/ if (!mtlk_vap_is_ap(core->vap_handle)) {
      /* Don't set the key until msg4 is sent to MAC */
      while (!mtlk_sta_sq_is_empty(sta))
        msleep(100);
    }
  }

  umi_key->u16DefaultKeyIndex = cpu_to_le16(key_idx);
  if (0 < key_len) {
    umi_key->au8TxSeqNum[0] = 1;
  }

  /* Set key/configure MAC */
  for (;;) {
    /* if WEP is configured then:
     * - new key should be set in MAC
     * - if default_key != new_key
     * ---- default key should be set in MAC*/
    uint16 status;
    /* Save UMI message */
    UMI_SET_KEY stored_umi_key = *(UMI_SET_KEY*)man_entry->payload;
    mtlk_txmm_data_t stored_man_entry = *man_entry;

    mtlk_dump(3, umi_key, sizeof(UMI_SET_KEY), "dump of UMI_SET_KEY msg:");

    if (NULL != sta) {
      sta_filter_stored  = mtlk_sta_get_packets_filter(sta);
      mtlk_sta_set_packets_filter(sta, MTLK_PCKT_FLTR_DISCARD_ALL);

      /* drop all packets in sta sendqueue */
      mtlk_sq_peer_ctx_cancel_all_packets(core->sq, mtlk_sta_get_sq(sta));

      /* Close TX aggregation */
      {
        mtlk_addba_peer_t *addba_peer = mtlk_sta_get_addb_peer(sta);
        mtlk_addba_delba_req_all(addba_peer);
      }

      /* wait till all messages to MAC to be confirmed */
      res = mtlk_sq_wait_all_packets_confirmed(mtlk_sta_get_sq(sta));
      if (MTLK_ERR_OK != res) {
        WLOG_DY("CID-%04x: STA:%Y wait time for all MSDUs confirmation expired",
                mtlk_vap_get_oid(core->vap_handle),
                mtlk_sta_get_addr(sta));

        mtlk_vap_get_hw_vft(core->vap_handle)->set_prop(core->vap_handle, MTLK_HW_RESET, NULL, 0);
        /* restore previous state of sta packets filter */
        mtlk_sta_set_packets_filter(sta, sta_filter_stored);
        goto finish;
      }
    }

    res = mtlk_txmm_msg_send_blocked(&man_msg,
                                     MTLK_MM_BLOCKED_SEND_TIMEOUT);
    if (NULL != sta) {
      /* restore previous state of sta packets filter */
      mtlk_sta_set_packets_filter(sta, sta_filter_stored);
    }

    if (MTLK_ERR_OK != res) {
      ELOG_DD("CID-%04x: mtlk_mm_send_blocked failed: %i", mtlk_vap_get_oid(core->vap_handle), res);
      goto finish;
    }

    status = le16_to_cpu(umi_key->u16Status);
    if (UMI_OK != status) {
      res = MTLK_ERR_UNKNOWN;
      switch (status) {
      case UMI_NOT_SUPPORTED:
        WLOG_D("CID-%04x: SIOCSIWENCODEEXT: RSN mode is disabled or an unsupported cipher suite was selected.", mtlk_vap_get_oid(core->vap_handle));
        res = MTLK_ERR_NOT_SUPPORTED;
        break;
      case UMI_STATION_UNKNOWN:
        WLOG_D("CID-%04x: SIOCSIWENCODEEXT: Unknown station was selected.", mtlk_vap_get_oid(core->vap_handle));
        break;
      default:
        WLOG_DDD("CID-%04x: invalid status of last msg %04x sending to MAC - %i", mtlk_vap_get_oid(core->vap_handle),
            man_entry->id, status);
      }
      goto finish;
    }

    if ((IW_ENCODE_ALG_WEP == alg_type) &&
        (umi_key->u16DefaultKeyIndex != cpu_to_le16(core->slow_ctx->default_key))) {

      ILOG1_D("reset tx key according to default key idx %i", core->slow_ctx->default_key);
      /* restore UMI message*/
      *(UMI_SET_KEY*)man_entry->payload = stored_umi_key;
      *man_entry = stored_man_entry;

      /* update UMI message*/
      umi_key->u16DefaultKeyIndex = cpu_to_le16(core->slow_ctx->default_key);
      memcpy(umi_key->au8Tk1,
          core->slow_ctx->wep_keys.sKey[core->slow_ctx->default_key].au8KeyData,
          core->slow_ctx->wep_keys.sKey[core->slow_ctx->default_key].u8KeyLength);
      /*Send UMI message*/
      continue;
    }
    break;
  }

  if (NULL != sta) {
    /* Now we have already set the keys =>
       we can start ADDBA and disable filter if required */
    mtlk_sta_on_security_negotiated(sta);

    if (key_len) {
      mtlk_sta_set_packets_filter(sta, MTLK_PCKT_FLTR_ALLOW_ALL);
      ILOG1_Y("%Y: turn off 802.1x filtering", mtlk_sta_get_addr(sta));
    }
  }

finish:
  if (NULL != sta) {
    mtlk_sta_decref(sta); /* De-reference of find */
  }

  if (NULL != man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return res;
}

static int
_mtlk_core_mac_get_channel_stats(mtlk_core_t *nic, mtlk_core_general_stats_t *general_stats)
{
  int                       res = MTLK_ERR_OK;
  mtlk_txmm_msg_t           man_msg;
  mtlk_txmm_data_t          *man_entry = NULL;
  UMI_GET_CHANNEL_STATUS    *pchannel_stats= NULL;

  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(general_stats != NULL);

  if (NET_STATE_HALTED == mtlk_core_get_net_state(nic)) { /* Do nothing if halted */
    goto finish;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), NULL);
  if (NULL == man_entry) {
    ELOG_D("CID-%04x: No man entry available", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NO_RESOURCES;
    goto finish;
  }

  man_entry->id = UM_MAN_GET_CHANNEL_STATUS_REQ;
  man_entry->payload_size = sizeof(UMI_GET_CHANNEL_STATUS);

  pchannel_stats = (UMI_GET_CHANNEL_STATUS *)man_entry->payload;
  memset(pchannel_stats, 0, sizeof(UMI_GET_CHANNEL_STATUS));

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: MAC Get Channel Status sending failure (%i)", mtlk_vap_get_oid(nic->vap_handle), res);
    goto finish;
  }

  general_stats->noise = pchannel_stats->u8GlobalNoise;
  general_stats->channel_load = pchannel_stats->u8ChannelLoad;

finish:
  if (NULL != man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}


static int
_mtlk_core_mac_get_peers_stats(mtlk_core_t *nic, mtlk_core_general_stats_t *general_stats)
{
  int                       res = MTLK_ERR_OK;
  mtlk_txmm_msg_t           man_msg;
  mtlk_txmm_data_t          *man_entry = NULL;
  UMI_GET_PEERS_STATUS      *ppeers_stats= NULL;
  uint8                     device_index = 0;
  uint8                     idx;

  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(general_stats != NULL);

  if (NET_STATE_HALTED == mtlk_core_get_net_state(nic)) { /* Do nothing if halted */
    goto finish;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), NULL);
  if (NULL == man_entry) {
    ELOG_D("CID-%04x: No man entry available", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NO_RESOURCES;
    goto finish;
  }

  do {
    man_entry->id = UM_MAN_GET_PEERS_STATUS_REQ;
    man_entry->payload_size = sizeof(UMI_GET_PEERS_STATUS);

    ppeers_stats = (UMI_GET_PEERS_STATUS *)man_entry->payload;
    memset(ppeers_stats, 0, sizeof(UMI_GET_PEERS_STATUS));
    ppeers_stats->u8DeviceIndex = device_index;

    res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
    if (MTLK_ERR_OK != res) {
      ELOG_DD("CID-%04x: MAC Get Peers Status sending failure (%i)", mtlk_vap_get_oid(nic->vap_handle), res);
      break;
    }

    for (idx = 0; idx < ppeers_stats->u8NumOfDeviceStatus; idx++) {
      sta_entry *sta = mtlk_stadb_find_sta(
                            &nic->slow_ctx->stadb,
                            (unsigned char*)&ppeers_stats->sDeviceStatus[idx].sMacAdd);

      if (sta != NULL) {
        mtlk_sta_update_fw_related_info(sta, &ppeers_stats->sDeviceStatus[idx]);
        mtlk_sta_decref(sta); /* De-reference of find */
      }
    }

    device_index = ppeers_stats->u8DeviceIndex;

  } while (0 != device_index);

finish:
  if (NULL != man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}

static int
_mtlk_core_mac_get_stats(mtlk_core_t *nic, mtlk_core_general_stats_t *general_stats)
{
  int                       res = MTLK_ERR_OK;
  mtlk_txmm_msg_t           man_msg;
  mtlk_txmm_data_t          *man_entry = NULL;
  UMI_GET_STATISTICS        *mac_stats;
  int                       idx;

  MTLK_ASSERT(nic != NULL);
  MTLK_ASSERT(general_stats != NULL);

  if (NET_STATE_HALTED == mtlk_core_get_net_state(nic)) { /* Do nothing if halted */
    goto finish;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txdm(nic->vap_handle), NULL);
  if (!man_entry) {
    ELOG_D("CID-%04x: Can't get statistics due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    res = MTLK_ERR_NO_RESOURCES;
    goto finish;
  }

  mac_stats = (UMI_GET_STATISTICS *)man_entry->payload;

  man_entry->id           = UM_DBG_GET_STATISTICS_REQ;
  man_entry->payload_size = sizeof(*mac_stats);
  mac_stats->u16Status       = 0;
  mac_stats->u16Ident        = HOST_TO_MAC16(GET_ALL_STATS);

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (MTLK_ERR_OK != res) {
    ELOG_DD("CID-%04x: MAC Get Stat sending failure (%i)", mtlk_vap_get_oid(nic->vap_handle), res);
  } else if (UMI_OK != le16_to_cpu(mac_stats->u16Status)) {
    ELOG_DD("CID-%04x: MAC Get Stat failure (%u)", mtlk_vap_get_oid(nic->vap_handle), le16_to_cpu(mac_stats->u16Status));
    res = MTLK_ERR_MAC;
  } else {
    for (idx = 0; idx < STAT_TOTAL_NUMBER; idx++) {
      general_stats->mac_stat.stat[idx] = le32_to_cpu(mac_stats->sStats.au32Statistics[idx]);
    }
  }

finish:
  if (NULL != man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  return res;
}


int
_mtlk_core_get_status(mtlk_handle_t hcore, const void* data, uint32 data_size)
{
  int                       res = MTLK_ERR_OK;
  mtlk_core_t               *nic = HANDLE_T_PTR(mtlk_core_t, hcore);
  mtlk_clpb_t               *clpb = *(mtlk_clpb_t **) data;
  mtlk_core_general_stats_t *general_stats;
  mtlk_txmm_stats_t         txm_stats;

  MTLK_ASSERT(sizeof(mtlk_clpb_t*) == data_size);

  general_stats = mtlk_osal_mem_alloc(sizeof(*general_stats), MTLK_MEM_TAG_CLPB);
  if(general_stats == NULL) {
    ELOG_V("Can't allocate clipboard data");
    res = MTLK_ERR_NO_MEM;
    goto err_finish;
  }
  memset(general_stats, 0, sizeof(*general_stats));

  /* Fill Core private statistic fields*/
  general_stats->core_priv_stats = nic->pstats;
  general_stats->tx_packets = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_PACKETS_SENT);
  general_stats->tx_bytes   = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_BYTES_SENT);
  general_stats->rx_packets = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_PACKETS_RECEIVED);
  general_stats->rx_bytes   = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_BYTES_RECEIVED);

  general_stats->unicast_replayed_packets = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_UNICAST_REPLAYED_PACKETS);
  general_stats->multicast_replayed_packets = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_MULTICAST_REPLAYED_PACKETS);
  general_stats->fwd_rx_packets = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_FWD_RX_PACKETS);
  general_stats->fwd_rx_bytes = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_FWD_RX_BYTES);
  general_stats->rx_dat_frames = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_DAT_FRAMES_RECEIVED);
  general_stats->rx_ctl_frames = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_CTL_FRAMES_RECEIVED);
  general_stats->rx_man_frames = _mtlk_core_get_cnt(nic, MTLK_CORE_CNT_MAN_FRAMES_RECEIVED);

  /* HW status fields */
  mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_FREE_TX_MSGS,
      &general_stats->tx_msdus_free, sizeof(general_stats->tx_msdus_free));
  mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_TX_MSGS_USED_PEAK,
      &general_stats->tx_msdus_usage_peak, sizeof(general_stats->tx_msdus_usage_peak));
  mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_BIST,
      &general_stats->bist_check_passed, sizeof(general_stats->bist_check_passed));
  mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_FW_BUFFERS_PROCESSED,
    &general_stats->fw_logger_packets_processed, sizeof(general_stats->fw_logger_packets_processed));
  mtlk_vap_get_hw_vft(nic->vap_handle)->get_prop(nic->vap_handle, MTLK_HW_FW_BUFFERS_DROPPED,
    &general_stats->fw_logger_packets_dropped, sizeof(general_stats->fw_logger_packets_dropped));

  mtlk_txmm_get_stats(mtlk_vap_get_txmm(nic->vap_handle), &txm_stats);
  general_stats->txmm_sent = txm_stats.nof_sent;
  general_stats->txmm_cfmd = txm_stats.nof_cfmed;
  general_stats->txmm_peak = txm_stats.used_peak;

  mtlk_txmm_get_stats(mtlk_vap_get_txdm(nic->vap_handle), &txm_stats);
  general_stats->txdm_sent = txm_stats.nof_sent;
  general_stats->txdm_cfmd = txm_stats.nof_cfmed;
  general_stats->txdm_peak = txm_stats.used_peak;

  if(mtlk_vap_is_master(nic->vap_handle)) {
    /* Get MAC statistic */
    res = _mtlk_core_mac_get_stats(nic, general_stats);
    if (MTLK_ERR_OK != res) {
      goto err_finish;
    }

    /* Get MAC channel information for master only*/
    res = _mtlk_core_mac_get_channel_stats(nic, general_stats);
    if (MTLK_ERR_OK != res) {
      goto err_finish;
    }

    /* Fill core status fields */
    general_stats->net_state = mtlk_core_get_net_state(nic);

    mtlk_pdb_get_mac(
        mtlk_vap_get_param_db(nic->vap_handle), PARAM_DB_CORE_BSSID, general_stats->bssid);

    if (!mtlk_vap_is_ap(nic->vap_handle) && (mtlk_core_get_net_state(nic) == NET_STATE_CONNECTED)) {
      sta_entry *sta = mtlk_stadb_get_ap(&nic->slow_ctx->stadb);

      if (NULL != sta) {
        general_stats->max_rssi = mtlk_sta_get_max_rssi(sta);

        mtlk_sta_decref(sta);  /* De-reference of get_ap */
      }
    }
  }

  /* Get MAC peers information */
  res = _mtlk_core_mac_get_peers_stats(nic, general_stats);
  if (MTLK_ERR_OK != res) {
    goto err_finish;
  }

  /* Return Core status & statistic data */
  res = mtlk_clpb_push_nocopy(clpb, general_stats, sizeof(*general_stats));
  if (MTLK_ERR_OK != res) {
    mtlk_clpb_purge(clpb);
    goto err_finish;
  }

  return MTLK_ERR_OK;

err_finish:
  if (general_stats) {
    mtlk_osal_mem_free(general_stats);
  }
  return res;
}


int
mtlk_core_set_cur_bonding(mtlk_core_t *core, uint8 bonding)
{
  int result = MTLK_ERR_OK;

  if ((bonding == ALTERNATE_UPPER) || (bonding == ALTERNATE_LOWER))
  {
    ILOG3_S("Bonding changed to %s", bonding == ALTERNATE_UPPER ? "upper" : (bonding == ALTERNATE_LOWER ? "lower" : "none"));
    MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_SELECTED_BONDING_MODE, bonding);
  } else {
    result = MTLK_ERR_PARAMS;
  }

  return result;
}

int __MTLK_IFUNC
mtlk_core_set_user_bonding(mtlk_core_t *core, uint8 bonding)
{
  int result = MTLK_ERR_OK;

  if ((bonding == ALTERNATE_UPPER) || (bonding == ALTERNATE_LOWER))
  {
    MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_USER_BONDING_MODE, bonding);
    if (NET_STATE_READY == mtlk_core_get_net_state(core)) { /* Check if interface down */
      MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_SELECTED_BONDING_MODE, bonding);
    }
  } else {
    result = MTLK_ERR_PARAMS;
  }

  return result;
}

uint8 __MTLK_IFUNC
mtlk_core_get_user_bonding(mtlk_core_t *core)
{
  return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_USER_BONDING_MODE);
}

uint8 __MTLK_IFUNC
mtlk_core_get_cur_bonding(mtlk_core_t *core)
{
  return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_BONDING_MODE);
}

BOOL __MTLK_IFUNC
mtlk_core_net_state_is_connected(uint16 net_state)
{
  return ((NET_STATE_CONNECTED == net_state) ? TRUE:FALSE);
}

uint8 __MTLK_IFUNC
mtlk_core_get_country_code(mtlk_core_t *core)
{
  return MTLK_CORE_PDB_GET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_COUNTRY_CODE);
}

static void
_mtlk_core_country_code_set_default(mtlk_core_t* core)
{
  uint8  country_code;

  if (mtlk_vap_is_master_ap(core->vap_handle)) {
    country_code = mtlk_eeprom_get_country_code(mtlk_core_get_eeprom(core));
    if (country_code) {
      MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_COUNTRY_CODE, country_code);
      ILOG1_DSD("CID-%04x: Country is set to (on init): %s(0x%02x)",
          mtlk_vap_get_oid(core->vap_handle), country_code_to_country(country_code), country_code);
    }
  }
  /* - Do nothing for slave VAP*/
  /* - Do nothing for STA (default value - 0)*/
}

static void
_mtlk_core_sta_country_code_set_default_on_activate(mtlk_core_t* core)
{
  uint8  country_code = mtlk_core_get_country_code(core);
  if (country_code == 0) {
     /*TODO: GS: Hide country_code processing */
    /* we must set up at least something */
    country_code = mtlk_eeprom_get_country_code(mtlk_core_get_eeprom(core));
    if (0 == country_code) {
      MTLK_CORE_PDB_SET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_COUNTRY_CODE, country_to_country_code("US"));
      WLOG_DSD("CID-%04x: Country isn't set. Set it to default (on activate): %s(0x%02x)",
          mtlk_vap_get_oid(core->vap_handle), "US", country_to_country_code("US"));
    }
  }
}

static void
_mtlk_core_sta_country_code_update_on_connect(mtlk_core_t* core, uint8 country_code)
{
  if (country_code) {
    MTLK_CORE_PDB_SET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_COUNTRY_CODE, country_code);
    ILOG1_DSD("CID-%04x: Country has been adopted (on connect): %s(0x%02x)",
        mtlk_vap_get_oid(core->vap_handle), country_code_to_country(country_code), country_code);
  }
}

void __MTLK_IFUNC
mtlk_core_sta_country_code_update_from_bss(mtlk_core_t* core, uint8 country_code)
{
  if ( mtlk_core_get_dot11d(core) /*802.11d mode on*/ &&
      (0 != country_code) /*802.11d becon with country IE recieved */ &&
      (0 == mtlk_core_get_country_code(core)) /*Country hasn't been set yet*/) {

    MTLK_CORE_PDB_SET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_COUNTRY_CODE, country_code);
    ILOG1_DSD("CID-%04x: Country has been adopted (from bss): %s(0x%02x)",
        mtlk_vap_get_oid(core->vap_handle), country_code_to_country(country_code), country_code);

    mtlk_scan_schedule_rescan(&mtlk_core_get_master(core)->slow_ctx->scan);
  }
}

static int
_mtlk_core_set_country_from_ui(mtlk_core_t *core, char *val)
{
  MTLK_ASSERT(!mtlk_vap_is_slave_ap(core->vap_handle));

  if (mtlk_core_get_net_state(core) != NET_STATE_READY) {
    return MTLK_ERR_BUSY;
  }

  if (mtlk_vap_is_ap(core->vap_handle) && mtlk_eeprom_get_country_code(mtlk_core_get_eeprom(core))) {
    ILOG1_D("CID-%04x: Can't change Country. It's read-only parameter.", mtlk_vap_get_oid(core->vap_handle));
    ILOG1_DSD("CID-%04x: Current Country value: %s(0x%02x)",
        mtlk_vap_get_oid(core->vap_handle), country_code_to_country(mtlk_core_get_country_code(core)), mtlk_core_get_country_code(core));
    return MTLK_ERR_NOT_SUPPORTED;
  }

  if (!mtlk_vap_is_ap(core->vap_handle) && mtlk_core_get_dot11d(core)) {
    ILOG1_D("CID-%04x: Can't change Country until 802.11d extension is enabled.", mtlk_vap_get_oid(core->vap_handle));
    return MTLK_ERR_NOT_SUPPORTED;
  }

  if (strncmp(val, "??", 2) && country_to_domain(val) == 0) {
    return MTLK_ERR_VALUE;
  }

  ILOG1_DSD("CID-%04x: Country is set to (from ui): %s(0x%02x)",
      mtlk_vap_get_oid(core->vap_handle), val, country_to_country_code(val));

  MTLK_CORE_PDB_SET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_COUNTRY_CODE, country_to_country_code(val));
  return MTLK_ERR_OK;
}

BOOL __MTLK_IFUNC mtlk_core_get_dot11d(mtlk_core_t *core)
{
  return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_DOT11D_ENABLED);
}

static int
_mtlk_core_set_is_dot11d(mtlk_core_t *core, BOOL is_dot11d)
{
  uint8  country_code = 0;

  if (NET_STATE_READY != mtlk_core_get_net_state(core)) {
    return MTLK_ERR_NOT_READY;
  }

  /* set country code */
  if (!mtlk_vap_is_ap(core->vap_handle)) {
    /* Switched on */
    if (is_dot11d && !mtlk_core_get_dot11d(core)) {
      country_code = 0;
    }
    /* Switched off */
    else if(!is_dot11d && mtlk_core_get_dot11d(core)) {
      country_code = mtlk_eeprom_get_country_code(mtlk_core_get_eeprom(core));
      if (0 == country_code) {

        country_code = country_to_country_code("US");
      }
    }

    MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_COUNTRY_CODE, country_code);
  }

  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_DOT11D_ENABLED, !!is_dot11d);

  return MTLK_ERR_OK;
}

uint8
mtlk_core_get_freq_band_cur(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  return MTLK_CORE_PDB_GET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_FREQ_BAND_CUR);
}

uint8
mtlk_core_get_freq_band_cfg(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  return MTLK_CORE_PDB_GET_INT(mtlk_core_get_master(core), PARAM_DB_CORE_FREQ_BAND_CFG);
}

BOOL __MTLK_IFUNC
mtlk_core_get_interfdet_mode_cfg (mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  return (BOOL)(MTLK_CORE_PDB_GET_INT(mtlk_core_get_master(core), PARAM_DB_INTERFDET_MODE) &&
                MTLK_HW_BAND_2_4_GHZ == mtlk_core_get_freq_band_cur(core));
}

BOOL __MTLK_IFUNC
mtlk_core_get_interfdet_scan_report_en (mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  return (BOOL)MTLK_CORE_PDB_GET_INT(mtlk_core_get_master(core), PARAM_DB_INTERFDET_MODE);
}

uint8
mtlk_core_get_network_mode_cur(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_NET_MODE_CUR);
}

uint8
mtlk_core_get_network_mode_cfg(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_NET_MODE_CFG);
}

void __MTLK_IFUNC
mtlk_core_notify_scan_complete(mtlk_vap_handle_t vap_handle)
{
  mtlk_df_ui_notify_scan_complete(mtlk_vap_get_df(vap_handle));
}

uint8
mtlk_core_get_is_ht_cur(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_IS_HT_CUR);
}

uint8
mtlk_core_get_is_ht_cfg(mtlk_core_t *core)
{
  MTLK_ASSERT(NULL != core);

  return MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_IS_HT_CFG);
}

static void _mtlk_core_switch_cb_mode_callback(mtlk_handle_t context, uint16 primary_channel, int secondary_channel_offset)
{
  mtlk_core_t *core;
  uint8 new_spectrum_mode;
  uint8 new_bonding, old_bonding;

  core = (mtlk_core_t*)context;

  MTLK_ASSERT(NULL != core);

  ILOG0_DSDS("\nCurrent Primary Channel = %d\n"
    "Current Spectrum Mode = %s\n"
    "About to switch channel with following parameters:\n"
    "Primary Channel = %d\n"
    "Channel bonding = %s\n",
    _mtlk_core_get_channel(core),
    MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE)?"40MHz":"20MHz",
    primary_channel,
    secondary_channel_offset == UMI_CHANNEL_SW_MODE_SCA ? "upper" : (secondary_channel_offset == UMI_CHANNEL_SW_MODE_SCB ? "lower" : "none"));

  if (secondary_channel_offset == UMI_CHANNEL_SW_MODE_SCN) {
    new_spectrum_mode = SPECTRUM_20MHZ;
    new_bonding = ALTERNATE_NONE;
  }
  else {
    new_spectrum_mode = SPECTRUM_40MHZ;
    if (secondary_channel_offset == UMI_CHANNEL_SW_MODE_SCA) {
      new_bonding = ALTERNATE_UPPER;
    }
    else {
      new_bonding = ALTERNATE_LOWER;
    }
  }

  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE, new_spectrum_mode);
  mtlk_aocs_set_spectrum_mode(core->slow_ctx->aocs, new_spectrum_mode);
  mtlk_dot11h_set_spectrum_mode(mtlk_core_get_dfs(core), new_spectrum_mode);

  mtlk_dot11h_20_40_coexistence_channel_switch(mtlk_core_get_dfs(core), primary_channel, new_bonding);
  ILOG2_V("Channel switch message sent to FW");

  old_bonding = mtlk_core_get_cur_bonding(mtlk_core_get_master(core));
  mtlk_core_set_cur_bonding(mtlk_core_get_master(core), new_bonding);
  if (MTLK_ERR_OK != _mtlk_core_set_channel(mtlk_core_get_master(core), primary_channel, new_spectrum_mode, FALSE)) {
    mtlk_core_set_cur_bonding(mtlk_core_get_master(core), old_bonding);
  }
}

static int __MTLK_IFUNC _mtlk_core_send_ce_serialized_callback(mtlk_handle_t context, const void *payload, uint32 size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *core;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry;

  ILOG2_V("Sending Coexistence Element");
  core = (mtlk_core_t*)context;

  MTLK_ASSERT(NULL != core);
  MTLK_ASSERT(NULL != payload);
  MTLK_ASSERT(sizeof(UMI_COEX_EL) == size);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (man_entry != NULL) {
     man_entry->id           = UM_MAN_SET_COEX_EL_TEMPLATE_REQ;
     man_entry->payload_size = size;
     memcpy(man_entry->payload, payload, size);

     res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
     if (res != MTLK_ERR_OK) {
       ELOG_DD("CID-%04x: Can't send UMI_COEX_EL request to MAC (err=%d)", mtlk_vap_get_oid(core->vap_handle), res);
     }
     mtlk_txmm_msg_cleanup(&man_msg);
   }
  else {
    ELOG_D("CID-%04x: Can't send UMI_COEX_EL request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NO_RESOURCES;
  }

  return res;
}

static void _mtlk_core_send_ce_callback(mtlk_handle_t context, UMI_COEX_EL *coexistence_element)
{
  mtlk_core_t *core;

  ILOG2_V("Scheduling Coexistence Element sending on serializer");
  core = (mtlk_core_t*)context;

  MTLK_ASSERT(NULL != core);
  mtlk_core_schedule_internal_task(core, context, _mtlk_core_send_ce_serialized_callback, coexistence_element, sizeof(*coexistence_element));
}

static int __MTLK_IFUNC _mtlk_core_send_cmf_serialized_callback(mtlk_handle_t context, const void *payload, uint32 size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *core;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry;

  ILOG2_V("Sending Coexistence Frame");
  core = (mtlk_core_t*)context;

  MTLK_ASSERT(NULL != core);
  MTLK_ASSERT(NULL != payload);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (man_entry != NULL) {
    memcpy(man_entry->payload, payload, size);
    man_entry->id           = UM_MAN_SEND_COEX_FRAME_REQ;
    man_entry->payload_size = size;
    res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
    if (res != MTLK_ERR_OK) {
        ELOG_V("Error sending coexistence management frame");
    }
    mtlk_txmm_msg_cleanup(&man_msg);
  }
  else{
    ELOG_D("CID-%04x: Can't send UM_MAN_SEND_COEX_FRAME_REQ request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NO_RESOURCES;
  }

  return res;
}

static void _mtlk_core_send_cmf_callback(mtlk_handle_t context, const UMI_COEX_FRAME *coexistence_frame)
{
  mtlk_core_t *core;

  ILOG2_V("Scheduling Coexistence Frame sending on serializer");
  core = (mtlk_core_t*)context;

  MTLK_ASSERT(NULL != core);
  mtlk_core_schedule_internal_task(core, context, _mtlk_core_send_cmf_serialized_callback, coexistence_frame, sizeof(*coexistence_frame));
}

static int _mtlk_core_send_exemption_policy_serialized_callback(mtlk_handle_t context, const void *payload, uint32 size)
{
  int res = MTLK_ERR_OK;
  mtlk_core_t *core;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry;

  ILOG2_V("Sending Exemption Policy To FW");
  core = (mtlk_core_t*)context;

  MTLK_ASSERT(NULL != core);
  MTLK_ASSERT(sizeof(BOOL) == size);

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (man_entry != NULL) {
      man_entry->id           = UM_MAN_SET_SCAN_EXEMPTION_POLICY_REQ;
      man_entry->payload_size = size;
      memcpy(man_entry->payload, payload, size);
      res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
      if (res != MTLK_ERR_OK) {
        ELOG_DD("CID-%04x: Can't send UMI_SCAN_EXEMPTION_POLICY request to MAC (err=%d)", mtlk_vap_get_oid(core->vap_handle), res);
      }
      mtlk_txmm_msg_cleanup(&man_msg);
  }
  else {
    ELOG_D("CID-%04x: Can't send UMI_SCAN_EXEMPTION_POLICY request to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(core->vap_handle));
    res = MTLK_ERR_NO_RESOURCES;
  }
  return res;
}

static void _mtlk_core_send_exemption_policy_callback(mtlk_handle_t context, BOOL must_grant_exemption_requests)
{
  mtlk_core_t *core;

  ILOG1_V("Scheduling Exemption Policy sending on serializer");
  core = (mtlk_core_t*)context;

  MTLK_ASSERT(NULL != core);
  mtlk_core_schedule_internal_task(core, context, _mtlk_core_send_exemption_policy_serialized_callback, &must_grant_exemption_requests, sizeof(must_grant_exemption_requests));
}

static int __MTLK_IFUNC _mtlk_core_scan_async_serialized_callback(mtlk_handle_t context, const void *payload, uint32 size)
{
  mtlk_core_t *core;
  const BOOL *scan_5mhz_band;

  core = (mtlk_core_t*)context;
  scan_5mhz_band = (const BOOL*)payload;

  mtlk_scan_async(&core->slow_ctx->scan, *scan_5mhz_band ? MTLK_HW_BAND_5_2_GHZ : MTLK_HW_BAND_2_4_GHZ, "");

  return MTLK_ERR_OK;
}

static int _mtlk_core_scan_async_callback(mtlk_handle_t context, BOOL scan_5mhz_band)
{
  mtlk_core_t *core;

  core = (mtlk_core_t*)context;

  MTLK_ASSERT(NULL != core);
  return mtlk_core_schedule_internal_task(core, context, _mtlk_core_scan_async_serialized_callback, &scan_5mhz_band, sizeof(scan_5mhz_band));
}

static void _mtlk_core_scan_set_obss_callback(mtlk_handle_t context, BOOL is_obss)
{
  mtlk_core_t *core;

  core = (mtlk_core_t*)context;

  MTLK_ASSERT(NULL != core);

  mtlk_scan_set_background(&core->slow_ctx->scan, is_obss);
  mtlk_scan_set_obss(&core->slow_ctx->scan, is_obss);
}

static int _mtlk_core_register_scan_completion_notification_callback(mtlk_handle_t caller_context, mtlk_handle_t core_context,
  scan_completed_notification_callback_type *callback)
{
  mtlk_core_t *core;

  core = (mtlk_core_t*)core_context;

  MTLK_ASSERT(NULL != core);

  mtlk_scan_register_scan_completed_notification_callback(&core->slow_ctx->scan, caller_context, callback);
  return MTLK_ERR_OK;
}

static int _mtlk_core_enumerate_bss_info_callback(mtlk_handle_t caller_context,
  mtlk_handle_t core_context, bss_enumerator_callback_type callback, uint32 expiration_time)
{
  bss_data_t bss_data;
  mtlk_core_t *core;
  uint32 original_cache_expire_time;
  int i = 0;

  core = (mtlk_core_t*)core_context;

  MTLK_ASSERT(NULL != core);

  original_cache_expire_time = mtlk_cache_get_expiration_time(&core->slow_ctx->cache);
  if (original_cache_expire_time < expiration_time)
  {
    mtlk_cache_set_expiration_time(&core->slow_ctx->cache, expiration_time);
  }
  mtlk_cache_rewind(&core->slow_ctx->cache);
  ILOG3_V("Accessing cache data .........");
  /* update channels array from cache (received bss's) */
  while (mtlk_cache_get_next_bss(&core->slow_ctx->cache, &bss_data, NULL, NULL))
  {
    mtlk_20_40_bss_info_t bss_info;
    ILOG3_D("Cache data No. %d:", i);
    ILOG3_D("Channel = %d", bss_data.channel);
    ILOG3_D("Sec.channel offset = %d", bss_data.secondary_channel_offset);
    ILOG3_D("Spectrum = %d", bss_data.spectrum);
    ILOG3_D("Is HT = %d", bss_data.is_ht);
    ILOG3_D("40MHz Intolerant = %d",bss_data.forty_mhz_intolerant);

    ++i;
    bss_info.channel = bss_data.channel;
    bss_info.secondary_channel_offset = bss_data.secondary_channel_offset;
    bss_info.timestamp = bss_data.received_timestamp;
    bss_info.is_2_4 = bss_data.is_2_4;
    bss_info.is_ht = bss_data.is_ht;
    bss_info.forty_mhz_intolerant = bss_data.forty_mhz_intolerant;
    bss_info.max_rssi = bss_data.max_rssi;
    (*callback)(caller_context, &bss_info);
  }
  if(i == 0)
    ILOG3_V("Processing cache data ended with no results - cache is empty!");
  /* restore cache expire time  */
  if (original_cache_expire_time < expiration_time)
  {
    mtlk_cache_set_expiration_time(&core->slow_ctx->cache, original_cache_expire_time);
  }
  return MTLK_ERR_OK;
}

#ifdef BT_ACS_DEBUG
static int _mtlk_core_enumerate_bss_info_by_aocs_callback
(mtlk_handle_t caller_context, mtlk_handle_t core_context,
 bss_enumerator_callback_type callback, uint32 expiration_time)
{
  mtlk_core_t *core;
  mtlk_aocs_t *aocs;
  mtlk_slist_entry_t *list_head;
  mtlk_slist_entry_t *list_entry;
  mtlk_aocs_channel_data_t *entry;

  core = (mtlk_core_t*)core_context;
  MTLK_ASSERT(NULL != core);

  aocs = core->slow_ctx->aocs;
  MTLK_ASSERT(NULL != aocs);

  mtlk_20_40_clear_intolerant_db(core->coex_20_40_sm);

  ILOG2_V("Accessing aocs channels data .........");
  /* update channels array from aocs channels */
  mtlk_slist_foreach(get_aocs_channel_list(aocs), list_entry, list_head) {
    mtlk_20_40_bss_info_t bss_info;
    entry = MTLK_LIST_GET_CONTAINING_RECORD(list_entry,
      mtlk_aocs_channel_data_t, link_entry);

    bss_info.channel = entry->stat.channel;
    bss_info.timestamp = mtlk_osal_timestamp();
    bss_info.is_2_4 = TRUE;
    bss_info.is_ht = ! entry->stat.forty_mhz_intolerant;
    bss_info.forty_mhz_intolerant = entry->stat.forty_mhz_intolerant;

    if (entry->stat.num_20mhz_bss) {
      bss_info.secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCN;
      (*callback)(caller_context, &bss_info);
    }
    if (entry->stat.num_40mhz_up_bss) {
      bss_info.secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCA;
      (*callback)(caller_context, &bss_info);
    }
    if (entry->stat.num_40mhz_lw_bss) {
      bss_info.secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCB;
      (*callback)(caller_context, &bss_info);
    }
  }

  return MTLK_ERR_OK;
}
#endif /* BT_ACS_DEBUG */

static int _mtlk_core_ability_control_callback(mtlk_handle_t context,
  eABILITY_OPS operation, const uint32* ab_id_list, uint32 ab_id_num)
{
  mtlk_core_t *core;
  int ret_val = MTLK_ERR_OK;

  ILOG2_V("called");

  core = (mtlk_core_t*)context;

  MTLK_ASSERT(NULL != core);

  switch (operation)
  {
    case eAO_REGISTER:
      ret_val = mtlk_abmgr_register_ability_set(mtlk_vap_get_abmgr(core->vap_handle), (mtlk_ability_id_t*)ab_id_list, ab_id_num);
      break;
    case eAO_UNREGISTER:
      mtlk_abmgr_unregister_ability_set(mtlk_vap_get_abmgr(core->vap_handle), (mtlk_ability_id_t*)ab_id_list, ab_id_num);
      break;
    case eAO_ENABLE:
      mtlk_abmgr_enable_ability_set(mtlk_vap_get_abmgr(core->vap_handle), (mtlk_ability_id_t*)ab_id_list, ab_id_num);
      break;
    case eAO_DISABLE:
      mtlk_abmgr_disable_ability_set(mtlk_vap_get_abmgr(core->vap_handle), (mtlk_ability_id_t*)ab_id_list, ab_id_num);
      break;
  }

  return ret_val;
}


static uint16 _mtlk_core_get_cur_channels_callback(mtlk_handle_t context, int *secondary_channel_offset)
{
  mtlk_core_t *core;

  ILOG2_V("called");

  core = (mtlk_core_t*)context;

  MTLK_ASSERT(NULL != core);

  if (mtlk_aocs_get_spectrum_mode(core->slow_ctx->aocs))
  {
    if (mtlk_core_get_cur_bonding(core) == ALTERNATE_UPPER)
    {
      *secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCA;
    }
    else
    {
      *secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCB;
    }
  }
  else
  {
    *secondary_channel_offset = UMI_CHANNEL_SW_MODE_SCN;
  }

  return _mtlk_core_get_channel(core);
}

static unsigned long _mtlk_core_modify_cache_expire_callback (mtlk_handle_t context, unsigned long new_time, BOOL force_change)
{
  mtlk_core_t *core;
  unsigned long old_time = new_time;


  core = (mtlk_core_t*)context;
  MTLK_ASSERT(NULL != core);

  if (mtlk_cache_get_expiration_time(&core->slow_ctx->cache) < new_time)
  {
    if (!force_change)/* AP forced sta its params */
    {
      old_time = mtlk_cache_get_expiration_time(&core->slow_ctx->cache);
    }
    mtlk_cache_set_expiration_time(&core->slow_ctx->cache, new_time);
  }

  return old_time;
}

static int _mtlk_core_update_fw_obss_scan_parameters_callback (mtlk_handle_t context)
{
  int res = MTLK_ERR_OK;
  mtlk_txmm_msg_t man_msg;
  mtlk_txmm_data_t *man_entry;
  UMI_OBSS_SCAN_PARAMS *umi_obss_scan_params_cfg = NULL;
  mtlk_core_t *core;
  struct _mtlk_20_40_coexistence_sm *coex_sm;

  core = (mtlk_core_t*)context;
  coex_sm= mtlk_core_get_coex_sm(core);

  MTLK_ASSERT(NULL != core);
  MTLK_ASSERT(NULL != coex_sm);

  ILOG2_V("Updating FW on OBSS scan parameters for associated STAs");

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(core->vap_handle), NULL);
  if (!man_entry) {
    res = MTLK_ERR_NO_RESOURCES;
    goto END;
  }

  man_entry->id = UM_MAN_SET_OBSS_SCAN_PARAMS_REQ;
  man_entry->payload_size = sizeof(UMI_OBSS_SCAN_PARAMS);

  umi_obss_scan_params_cfg = (UMI_OBSS_SCAN_PARAMS *)man_entry->payload;


  umi_obss_scan_params_cfg->u8_MustInclude = mtlk_20_40_get_ap_force_scan_params_on_assoc_sta(coex_sm);

  umi_obss_scan_params_cfg->uOBSSScanPassiveDwell = HOST_TO_MAC16(mtlk_20_40_get_scan_passive_dwell(coex_sm));
  umi_obss_scan_params_cfg->uOBSSScanActiveDwell = HOST_TO_MAC16(mtlk_20_40_get_scan_active_dwell(coex_sm));
  umi_obss_scan_params_cfg->uOBSSScanPassiveTotalPerChannel = HOST_TO_MAC16(mtlk_20_40_get_passive_total_per_channel(coex_sm));
  umi_obss_scan_params_cfg->uOBSSScanActiveTotalPerChannel = HOST_TO_MAC16(mtlk_20_40_get_active_total_per_channel(coex_sm));
  umi_obss_scan_params_cfg->uBSSWidthChannelTransitionDelayFactor = HOST_TO_MAC16(mtlk_20_40_get_transition_delay_factor(coex_sm));
  umi_obss_scan_params_cfg->uBSSChannelWidthTriggerScanInterval = HOST_TO_MAC16(mtlk_20_40_get_scan_interval(coex_sm));
  umi_obss_scan_params_cfg->uOBSSScanActivityThreshold = HOST_TO_MAC16(mtlk_20_40_get_scan_activity_threshold(coex_sm));

  res = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (res != MTLK_ERR_OK)
    ELOG_DD("CID-%04x: mtlk_mm_send_blocked failed: %i", mtlk_vap_get_oid(core->vap_handle), res);

END:
  mtlk_txmm_msg_cleanup(&man_msg);
  return res;
}

int __MTLK_IFUNC mtlk_core_on_channel_switch_done(mtlk_vap_handle_t vap_handle,
                                                  uint16            primary_channel,
                                                  uint8             secondary_channel_offset,
                                                  uint16            reason)
{
  mtlk_channel_switched_event_t switch_data;

  memset(&switch_data, 0, sizeof(mtlk_channel_switched_event_t));

  switch_data.primary_channel   = primary_channel;
  switch_data.secondary_channel = mtlk_channels_get_secondary_channel_no_by_offset(primary_channel, secondary_channel_offset);

  switch(reason)
  {
  case SWR_LOW_THROUGHPUT:
  case SWR_HIGH_SQ_LOAD:
  case SWR_CHANNEL_LOAD_CHANGED:
  case SWR_MAC_PRESSURE_TEST:
    switch_data.reason = WSSA_SWR_OPTIMIZATION;
    break;
  case SWR_RADAR_DETECTED:
    switch_data.reason = WSSA_SWR_RADAR;
    break;
  case SWR_INITIAL_SELECTION:
    switch_data.reason = WSSA_SWR_USER;
    break;
  case SWR_20_40_COEXISTENCE:
    switch_data.reason = WSSA_20_40_COEXISTENCE;
    break;
  case SWR_AP_SWITCHED:
    switch_data.reason = WSSA_SWR_AP_SWITCHED;
    break;
  case SWR_UNKNOWN:
  default:
    switch_data.reason = WSSA_SWR_UNKNOWN;
    break;
  }

  return mtlk_wssd_send_event(mtlk_vap_get_irbd(vap_handle),
                              MTLK_WSSA_DRV_EVENT_CH_SWITCH,
                              &switch_data,
                              sizeof(mtlk_channel_switched_event_t));
}


int __MTLK_IFUNC
mtlk_core_wds_on_beacon (mtlk_core_t *core, IEEE_ADDR *addr)
{
  wds_on_beacon_t *on_beacon;
  wds_t *master_wds;

  /* Note: Assuming beacon arrived on Master VAP only */

  /* Find addr in on_beacon list. If addr is in list, then
     exclude on_beacon entry from list, send it to serializer
     for further processing and delete. */

  master_wds = &core->slow_ctx->wds_mng;
  on_beacon = wds_on_beacon_exclude(master_wds, addr);
  if (on_beacon)
  {
    _mtlk_process_hw_task(core, SERIALIZABLE, wds_on_beacon_proc, HANDLE_T(master_wds), on_beacon, sizeof(wds_on_beacon_t));
    mtlk_osal_mem_free(on_beacon);
  }

  return MTLK_ERR_OK;
}

uint32 __MTLK_IFUNC
mtlk_core_wds_on_timer (mtlk_osal_timer_t *timer, mtlk_handle_t wds_peer_handle)
{
  /* Note: This function is called from timer context
           Do not modify WDS structure from here
  */
  wds_peer_t  *peer;
  wds_t       *wds;
  mtlk_core_t *core;

  peer = (wds_peer_t *)wds_peer_handle;
  wds = peer->wds;
  core = mtlk_vap_get_core(wds->vap_handle);

  mtlk_core_schedule_internal_task(core, HANDLE_T(wds), wds_peer_fsm_timer, &(peer->addr), sizeof(peer->addr));

  return 0;
}

uint32 __MTLK_IFUNC
mtlk_core_wds_on_host_disconnect_tmr (mtlk_osal_timer_t *timer,
                                      mtlk_handle_t host_handle)
{
  mtlk_core_t *core;

  core = mtlk_vap_get_core(mtlk_host_get_vap_handle(host_handle));
  mtlk_core_schedule_internal_task(core, host_handle, mtlk_host_wds_disconnect_tmr, NULL, 0);

  return 0; /* timer will be rescheduled in wds host timer handler */
}

uint32 __MTLK_IFUNC
mtlk_core_ta_on_timer (mtlk_osal_timer_t *timer, mtlk_handle_t ta_handle)
{
  /* Note: This function is called from timer context
           Do not modify TA structure from here
  */
  mtlk_core_t     *core;

  core = mtlk_vap_get_core(mtlk_ta_get_vap_handle(ta_handle));

  mtlk_core_schedule_internal_task(core, ta_handle, ta_timer, NULL, 0);

  return mtlk_ta_get_timer_resolution_ms(ta_handle);
}

int __MTLK_IFUNC
detect_replay(mtlk_core_t *core, mtlk_nbuf_t *nbuf, u8 *last_rc, BOOL is_mgmt)
{
  return _detect_replay(core, nbuf, last_rc, is_mgmt);
}

int __MTLK_IFUNC
get_rsc_buf(mtlk_core_t* core, mtlk_nbuf_t *nbuf, int off)
{
  return _get_rsc_buf(core, nbuf, off);
}

int __MTLK_IFUNC
mtlk_core_stop_lm (struct nic *core)
{
  return (_mtlk_core_stop_lm(HANDLE_T(core), NULL, 0));
}

int __MTLK_IFUNC
mtlk_core_set_tx_antennas (mtlk_core_t *core, uint8 num_tx_antennas)
{
  int res, i;
  uint8 val_array[MTLK_NUM_ANTENNAS_BUFSIZE];

  MTLK_ASSERT(NULL != core);

  memset (val_array, 0, sizeof (val_array));

  for (i = 0; i < num_tx_antennas; i++) {
    val_array[i] = i + 1;
  }

  res = MTLK_CORE_PDB_SET_BINARY(core,
                                 PARAM_DB_CORE_TX_ANTENNAS,
                                 val_array,
                                 MTLK_NUM_ANTENNAS_BUFSIZE);

  if (MTLK_ERR_OK != res) {
    ILOG2_V("Can not save TX antennas configuration in to the PDB");
  }

  return res;
}

int __MTLK_IFUNC
mtlk_core_set_uapsd_max_sp (mtlk_core_t *core, uint8 uapsd_max_sp)
{
  int res = MTLK_ERR_OK;

  MTLK_ASSERT(NULL != core);

  if (uapsd_max_sp < UAPSD_MAX_SP_USER_MIN || uapsd_max_sp > UAPSD_MAX_SP_USER_MAX) {
    WLOG_DD("Value should be in interval %d..%d", UAPSD_MAX_SP_USER_MIN, UAPSD_MAX_SP_USER_MAX);
    res = MTLK_ERR_PARAMS;
  } else {
    core->uapsd_max_sp = uapsd_max_sp;
    res = mtlk_set_mib_value_uint8(mtlk_vap_get_txmm(core->vap_handle), MIB_AP_MAX_PKTS_IN_SP, uapsd_max_sp);
  }

  return res;
}

static int
_core_set_umi_key (mtlk_core_t *nic, int key_idx)
{
  int result = MTLK_ERR_OK;
  mtlk_txmm_data_t *man_entry = NULL;
  mtlk_txmm_msg_t man_msg;
  UMI_SET_KEY *pSetKey;

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), &result);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send UMI_KEY to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  man_entry->id           = UM_MAN_SET_KEY_REQ;
  man_entry->payload_size = sizeof(*pSetKey);

  memset(man_entry->payload, 0, man_entry->payload_size);
  pSetKey = (UMI_SET_KEY*)(man_entry->payload);

  pSetKey->u16KeyType = HOST_TO_MAC16(UMI_RSN_GROUP_KEY);
  switch (nic->group_cipher) {
    case IW_ENCODE_ALG_NONE:
      pSetKey->u16CipherSuite = HOST_TO_MAC16(UMI_RSN_CIPHER_SUITE_NONE);
      break;
    case IW_ENCODE_ALG_WEP:
      pSetKey->u16CipherSuite = HOST_TO_MAC16(UMI_RSN_CIPHER_SUITE_WEP40);
      break;
    case IW_ENCODE_ALG_TKIP:
      pSetKey->u16CipherSuite = HOST_TO_MAC16(UMI_RSN_CIPHER_SUITE_TKIP);
      break;
    case IW_ENCODE_ALG_CCMP:
      pSetKey->u16CipherSuite = HOST_TO_MAC16(UMI_RSN_CIPHER_SUITE_CCMP);
      break;
  }

  memset(pSetKey->sStationID.au8Addr, 0xFF, sizeof(pSetKey->sStationID.au8Addr));
  pSetKey->u16DefaultKeyIndex = HOST_TO_MAC16(key_idx);

  memcpy(pSetKey->au8Tk1,
         nic->slow_ctx->wep_keys.sKey[key_idx].au8KeyData,
         nic->slow_ctx->wep_keys.sKey[key_idx].u8KeyLength);

  ILOG2_D("CID-%04x: Sending SET UMI Key", mtlk_vap_get_oid(nic->vap_handle));
  result = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't send UMI_KEY request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), result);
    goto FINISH;
  }

  if (MAC_TO_HOST16(pSetKey->u16Status) != UMI_OK) {
    ELOG_DD("CID-%04x: Error returned for UMI_KEY request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), MAC_TO_HOST16(pSetKey->u16Status));
    result = MTLK_ERR_UMI;
    goto FINISH;
  }

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return result;
}

static int
_core_set_umi_key_igtk (mtlk_core_t *nic, int key_idx)
{
  int result = MTLK_ERR_OK;
  mtlk_txmm_data_t *man_entry = NULL;
  mtlk_txmm_msg_t man_msg;
  UMI_SET_KEY *pSetKey;
  uint8 igtk_key_array_idx = (key_idx == UMI_RSN_IGTK_GM_KEY_INDEX) ? 0 : 1;

  if(nic->igtk_cipher != IW_ENCODE_ALG_AES_CMAC) {
    goto FINISH;
  }
  if(key_idx != UMI_RSN_IGTK_GM_KEY_INDEX &&
     key_idx != UMI_RSN_IGTK_GN_KEY_INDEX)
  {
    ELOG_DD("CID-%04x: Invalid IGTK key index %d", mtlk_vap_get_oid(nic->vap_handle), key_idx);
    goto FINISH;
  }

  man_entry = mtlk_txmm_msg_init_with_empty_data(&man_msg, mtlk_vap_get_txmm(nic->vap_handle), &result);
  if (man_entry == NULL) {
    ELOG_D("CID-%04x: Can't send UMI_KEY to MAC due to the lack of MAN_MSG", mtlk_vap_get_oid(nic->vap_handle));
    goto FINISH;
  }

  man_entry->id           = UM_MAN_SET_KEY_REQ;
  man_entry->payload_size = sizeof(*pSetKey);

  memset(man_entry->payload, 0, man_entry->payload_size);
  pSetKey = (UMI_SET_KEY*)(man_entry->payload);

  pSetKey->u16KeyType = HOST_TO_MAC16(UMI_RSN_GROUP_KEY);
  pSetKey->u16CipherSuite = HOST_TO_MAC16(UMI_RSN_CIPHER_SUITE_IGTK);
  memset(pSetKey->sStationID.au8Addr, 0xFF, sizeof(pSetKey->sStationID.au8Addr));
  /* NB: FW accepts key indices one less than real value */
  pSetKey->u16DefaultKeyIndex = HOST_TO_MAC16(key_idx - 1);

  memcpy(pSetKey->au8Tk1,
         nic->igtk_key[igtk_key_array_idx],
         nic->igtk_key_len);

  ILOG2_D("CID-%04x: Sending SET UMI Key (IGTK)", mtlk_vap_get_oid(nic->vap_handle));
  result = mtlk_txmm_msg_send_blocked(&man_msg, MTLK_MM_BLOCKED_SEND_TIMEOUT);
  if (result != MTLK_ERR_OK) {
    ELOG_DD("CID-%04x: Can't send UMI_KEY request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), result);
    goto FINISH;
  }

  if (MAC_TO_HOST16(pSetKey->u16Status) != UMI_OK) {
    ELOG_DD("CID-%04x: Error returned for UMI_KEY request to MAC (err=%d)", mtlk_vap_get_oid(nic->vap_handle), MAC_TO_HOST16(pSetKey->u16Status));
    result = MTLK_ERR_UMI;
    goto FINISH;
  }

FINISH:
  if (man_entry) {
    mtlk_txmm_msg_cleanup(&man_msg);
  }

  return result;
}

static int
_core_on_fast_rcvry_configure (mtlk_core_t *core)
{
  int res = MTLK_ERR_OK;

  res = mtlk_progmodel_load(mtlk_vap_get_txmm(core->vap_handle),
                            core,
                            mtlk_core_get_freq_band_cfg(core),
                            MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_PROG_MODEL_SPECTRUM_MODE));
  if (res != MTLK_ERR_OK) {
    goto FINISH;
  }

  ILOG0_D("CID-%04x: Recovery pre-activation parameters:", mtlk_vap_get_oid(core->vap_handle));
  mtlk_core_configuration_dump(core);

  /* Set channel & spectrum depended MIBs */
  res = _core_set_preactivation_mibs(core);
  if (res != MTLK_ERR_OK) {
    goto FINISH;
  }

//TODO failing functions
#if 0 
  res = _mtlk_core_send_current_rx_high_threshold(core);
  if (res != MTLK_ERR_OK) {
    goto FINISH;
  }

  res = _mtlk_core_send_current_cca_threshold(core);
  if (res != MTLK_ERR_OK) {
    goto FINISH;
  }
#endif 

  res = _mtlk_core_set_interfdet_do_params(core);
  if (res != MTLK_ERR_OK) {
    goto FINISH;
  }

  res = _mtlk_mbss_send_preactivate_req(core);
  if (res != MTLK_ERR_OK) {
    goto FINISH;
  }

  res = mtlk_set_power_limit(core);
  if (res != MTLK_ERR_OK) {
    goto FINISH;
  }

FINISH:
  return res;
}

static int
_core_on_full_rcvry_configure (mtlk_core_t *core)
{
  int res = MTLK_ERR_OK;

  res = _mtlk_mbss_preactivate(core, FALSE);
  return res;
}

int __MTLK_IFUNC
core_on_rcvry_configure (mtlk_core_t *core,
                         uint32 was_connected,
                         uint32 preactivate,
                         uint32 rcvry_type)
{
  int i;
  int res = MTLK_ERR_OK;
  mtlk_core_t *master_core = mtlk_core_get_master(core);

  /* Restore CORE's and its sub modules configuration.
     This function is intended for recovery configurable
     parameters only. Non configurable parameters and
     variables must be set in RESTORE function.
  */

  res = mtlk_mbss_send_vap_add(core);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  res = mtlk_core_set_net_state(core, NET_STATE_READY);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  res = _mtlk_core_process_antennas_configuration(core);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  /* Set all MIBs, beside of security & preactivation MIBs */
  res = mtlk_set_vap_mibs(core);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  res = mtlk_mib_set_security(core);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  for (i = 0; i < MIB_WEP_N_DEFAULT_KEYS; i++) {
    res = _core_set_umi_key(core, i);
    if (res != MTLK_ERR_OK) {
      return res;
    }
  }

  res = _core_set_umi_key_igtk(core, UMI_RSN_IGTK_GM_KEY_INDEX);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  res = _core_set_umi_key_igtk(core, UMI_RSN_IGTK_GN_KEY_INDEX);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  for (i = 0; i < UMI_WPS_IE_NUM; i++) {
    res = mtlk_core_set_gen_ie(core,
                               core->slow_ctx->gen_ie[i],
                               core->slow_ctx->gen_ie_len[i],
                               i);
    if (res != MTLK_ERR_OK) {
      return res;
    }
  }

  if (preactivate) {
    {
      /* GPIO configuration */
      mtlk_fw_led_gpio_cfg_t fw_led_gpio_cfg;
      _mtlk_core_get_fw_led_gpio_cfg(master_core, &fw_led_gpio_cfg);
      res = _mtlk_core_set_fw_led_gpio_cfg(master_core, &fw_led_gpio_cfg);
      if (res != MTLK_ERR_OK) {
        return res;
      }
    }
    {
      /* LED state configuration */
      mtlk_fw_led_state_t fw_led_state;
      _mtlk_core_get_fw_led_state(master_core, &fw_led_state);
      res = _mtlk_core_set_fw_led_state(master_core, &fw_led_state);
      if (res != MTLK_ERR_OK) {
        return res;
      }
    }
    {
      /* Enhanced 11B thresholds configuration */
      mtlk_enhanced11b_th_t thresholds;
      thresholds.Consecutive11bTH = MTLK_CORE_PDB_GET_INT(master_core, PARAM_DB_CONSECUTIVE_11B_TH);
      thresholds.Consecutive11bTH = MTLK_CORE_PDB_GET_INT(master_core, PARAM_DB_CONSECUTIVE_11N_TH);
      res = _mtlk_core_set_enhanced11b_th(master_core, &thresholds);
      if (res != MTLK_ERR_OK) {
        return res;
      }
    }
    {
      /* Antenna selection configuration */
      mtlk_11b_antsel_t antsel;
      antsel.rate = MTLK_CORE_PDB_GET_INT(master_core, PARAM_DB_11B_ANTSEL_RATE);
      antsel.rxAnt = MTLK_CORE_PDB_GET_INT(master_core, PARAM_DB_11B_ANTSEL_RXANT);
      antsel.txAnt = MTLK_CORE_PDB_GET_INT(master_core, PARAM_DB_11B_ANTSEL_TXANT);
      _mtlk_core_set_11b_antsel(master_core, &antsel);
    }

    res = mtlk_core_set_mc_ps_size(core, MTLK_CORE_PDB_GET_INT(core, PARAM_DB_FW_MC_PS_MAX_FSDUS));
    if (res != MTLK_ERR_OK) {
      return res;
    }

    switch (rcvry_type) {
      case RCVRY_TYPE_FAST:
        res = _core_on_fast_rcvry_configure(master_core);
        if (res != MTLK_ERR_OK) {
          ELOG_D("CID-%04x: FAST recovery configuration failed", mtlk_vap_get_oid(core->vap_handle));
          return res;
        }
        break;
      case RCVRY_TYPE_FULL:
        res = _core_on_full_rcvry_configure(master_core);
        if (res != MTLK_ERR_OK) {
          ELOG_D("CID-%04x: FULL recovery configuration failed", mtlk_vap_get_oid(core->vap_handle));
          return res;
        }
        break;
      default:
        break;
    }

    /* CoC configuration */
    res = mtlk_coc_on_rcvry_configure(master_core->slow_ctx->coc_mngmt);
    if (res != MTLK_ERR_OK) {
      return res;
    }

    /* PCoC configuration */
#ifdef MTCFG_PMCU_SUPPORT
    res = mtlk_pcoc_on_rcvry_configure(master_core->slow_ctx->pcoc_mngmt);
    if (res != MTLK_ERR_OK) {
      return res;
    }
#endif /* MTCFG_PMCU_SUPPORT */

    /* Coexistence configuration */
    if (mtlk_core_is_20_40_active(master_core)) {
      switch (rcvry_type) {
      case RCVRY_TYPE_FAST:
        res = mtlk_20_40_ap_on_fast_rcvry_configure(master_core->coex_20_40_sm);
        break;
      case RCVRY_TYPE_FULL:
        {
          eCSM_STATES mode_to_start = CSM_STATE_20;
          if (MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_SELECTED_SPECTRUM_MODE) == SPECTRUM_40MHZ) {
            mode_to_start = CSM_STATE_20_40;
          }
          res = mtlk_20_40_ap_on_full_rcvry_configure(master_core->coex_20_40_sm,
            mode_to_start,
            core->wss);
        }
        break;
      default:
        break;
      }
      if (res != MTLK_ERR_OK) {
        return res;
      }
    }
  }

  if (was_connected) {
    res = mtlk_mbss_send_vap_activate(core);
    if (res != MTLK_ERR_OK) {
      ELOG_D("CID-%04x: VAP activation failed", mtlk_vap_get_oid(core->vap_handle));
      return res;
    }

    if (BR_MODE_WDS == MTLK_CORE_HOT_PATH_PDB_GET_INT(core, CORE_DB_CORE_BRIDGE_MODE)) {
      wds_on_if_up(&core->slow_ctx->wds_mng);
    }

    /* restore STA limits */
    res = _mtlk_core_set_mbss_vap_limits(core,
                                         MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_STA_LIMIT_MIN),
                                         MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_STA_LIMIT_MAX));
    if (res != MTLK_ERR_OK) {
      return res;
    }

    mtlk_stadb_start(&core->slow_ctx->stadb);
  }

  /* Start TX data */
  mtlk_flctrl_start_data(master_core->hw_tx_flctrl, master_core->flctrl_id);

  return MTLK_ERR_OK;
}

void __MTLK_IFUNC
core_schedule_recovery_task (mtlk_vap_handle_t vap_handle, void *task, mtlk_handle_t rcvry_handle, int vap_num)
{
  /* Just wrapper to put recovery task to the serializer */
  mtlk_core_t *core = mtlk_vap_get_core(vap_handle);

  _mtlk_process_emergency_task(core, (mtlk_core_task_func_t)task, rcvry_handle, &vap_num, sizeof(vap_num));

  return;
}

BOOL __MTLK_IFUNC
core_get_is_mac_fatal_pending (mtlk_core_t *core)
{
  return (BOOL)mtlk_osal_atomic_get(&mtlk_core_get_master(core)->is_mac_fatal_pending);
}

void __MTLK_IFUNC
core_set_is_mac_fatal_pending (mtlk_core_t *core, BOOL pending)
{
  mtlk_osal_atomic_set(&mtlk_core_get_master(core)->is_mac_fatal_pending, (uint32)pending);
}

void __MTLK_IFUNC
mtlk_core_store_calibration_channel_bit_map (mtlk_core_t *core, uint32 *storedCalibrationChannelBitMap)
{
  memcpy(core->storedCalibrationChannelBitMap,
         storedCalibrationChannelBitMap,
         sizeof(core->storedCalibrationChannelBitMap));
}

void __MTLK_IFUNC
core_on_rcvry_error (mtlk_core_t *core)
{
  mtlk_core_set_net_state_halted(core);
}

void __MTLK_IFUNC
core_on_rcvry_isol_irbd_unregister (mtlk_core_t *core)
{
#ifdef MTCFG_RF_MANAGEMENT_MTLK
  mtlk_rf_mgmt_stop(core->rf_mgmt);
#endif

  mtlk_wssd_unregister_request_handler(mtlk_vap_get_irbd(core->vap_handle), core->slow_ctx->stat_irb_handle);
  core->slow_ctx->stat_irb_handle = NULL;
}

void __MTLK_IFUNC
core_on_rcvry_isol (mtlk_core_t *core, uint32 rcvry_type)
{
  if (mtlk_vap_is_master_ap(core->vap_handle)) {
    /* Stop TX data */
    mtlk_flctrl_stop_data(core->hw_tx_flctrl, core->flctrl_id);

    /* Disable MAC WatchDog */
    mtlk_osal_timer_cancel_sync(&core->slow_ctx->mac_watchdog_timer);

    /* CoC isolation */
    mtlk_coc_on_rcvry_isol(core->slow_ctx->coc_mngmt);

    /* PCoC isolation */
#ifdef MTCFG_PMCU_SUPPORT
    mtlk_pcoc_on_rcvry_isol(core->slow_ctx->pcoc_mngmt);
#endif /* MTCFG_PMCU_SUPPORT */

    /* AOCs isolation */
    /* Coexistence isolation */
    mtlk_aocs_on_rcvry_isol(core->slow_ctx->aocs);
    switch (rcvry_type) {
      case RCVRY_TYPE_FAST:
        mtlk_aocs_stop_watchdog(core->slow_ctx->aocs);
        mtlk_20_40_ap_on_fast_rcvry_isol(core->coex_20_40_sm);
        break;
      case RCVRY_TYPE_FULL:
        _mtlk_mbss_undo_preactivate(core);
        mtlk_20_40_ap_on_full_rcvry_isol(core->coex_20_40_sm);
        break;
      default:
        break;
    }

    mtlk_dot11h_on_rcvry_isol(mtlk_core_get_dfs(core));
  }

  clean_all_sta_on_disconnect(core);

  if (mtlk_vap_is_ap(core->vap_handle)) {
    wds_on_if_down(&core->slow_ctx->wds_mng);
  }

  mtlk_stadb_stop(&core->slow_ctx->stadb);
  if (rcvry_type == RCVRY_TYPE_FULL) {
    mtlk_cache_clear(&core->slow_ctx->cache);
    core->cb_scanned_bands = 0;
  }

  mtlk_core_set_net_state_halted(core);
}

int __MTLK_IFUNC
core_on_rcvry_restore (mtlk_core_t *core)
{
  /* Restore CORE's data & variables
     not related with current configuration
  */
  int res = MTLK_ERR_OK;

  res = mtlk_core_set_net_state(core, NET_STATE_IDLE);
  if (res != MTLK_ERR_OK) {
    return res;
  }

  mtlk_sq_peer_ctx_on_rcvry_restore(core->sq, &core->sq_broadcast_ctx, MTLK_SQ_TX_LIMIT_DEFAULT);

  /* Restore WATCHDOG timer */
  if (!mtlk_vap_is_slave_ap(core->vap_handle)) {
    mtlk_osal_timer_set(&core->slow_ctx->mac_watchdog_timer,
                        MTLK_CORE_PDB_GET_INT(core, PARAM_DB_CORE_MAC_WATCHDOG_TIMER_PERIOD_MS));
  }

#ifdef MTCFG_RF_MANAGEMENT_MTLK
  mtlk_rf_mgmt_start(core->rf_mgmt);
#endif

  /* register back irb handler */
  core->slow_ctx->stat_irb_handle =
    mtlk_wssd_register_request_handler(mtlk_vap_get_irbd(core->vap_handle),
                                       _mtlk_core_stat_handle_request,
                                       HANDLE_T(core->slow_ctx));
  if (core->slow_ctx->stat_irb_handle == NULL) {
    return res;
  }

  if (mtlk_vap_is_master(core->vap_handle)) {
    res = mtlk_aocs_on_rcvry_restore(core->slow_ctx->aocs);
    if (res != MTLK_ERR_OK) {
      return res;
    }

    mtlk_20_40_ap_on_rcvry_restore(core->coex_20_40_sm);

    core->slow_ctx->last_pm_spectrum = -1;
    core->slow_ctx->last_pm_freq = MTLK_HW_BAND_NONE;
  }

  return MTLK_ERR_OK;
}

mtlk_dot11h_t* __MTLK_IFUNC
mtlk_core_get_dfs(mtlk_core_t* core)
{
  if (mtlk_vap_is_slave_ap(core->vap_handle)) {
    return mtlk_core_get_master(core)->slow_ctx->dot11h;
  }

  return core->slow_ctx->dot11h;
}

void __MTLK_IFUNC
mtlk_core_api_cleanup (mtlk_core_api_t *core_api)
{
  mtlk_core_t* core = HANDLE_T_PTR(mtlk_core_t, core_api->core_handle);

  _mtlk_core_cleanup(core);
  mtlk_fast_mem_free(core);
}

static int
_mtlk_core_set_sm_required(mtlk_core_t *core, BOOL enable_sm_required)
{
  MTLK_ASSERT(mtlk_vap_is_master_ap(core->vap_handle));

  if (TRUE == enable_sm_required) {
    mtlk_aocs_enable_smrequired(core->slow_ctx->aocs);
  } else {
    mtlk_aocs_disable_smrequired(core->slow_ctx->aocs);
  }
  return MTLK_ERR_OK;
}

static __INLINE BOOL
_mtlk_core_has_connections(mtlk_core_t *core)
{
  return !mtlk_stadb_is_empty(&core->slow_ctx->stadb);
}

static __INLINE int
_mtlk_core_save_tx_power_limit(mtlk_core_t *core, mtlk_tx_power_limit_cfg_t *cfg)
{

  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_POWER_LIMIT_11B_BOOST,     cfg->field_01);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_POWER_LIMIT_BPSK_BOOST,    cfg->field_02);
  MTLK_CORE_PDB_SET_INT(core, PARAM_DB_CORE_POWER_LIMIT_AUTO_RESPONCE, cfg->field_03);

  /* send UM_MAN_CHANGE_TX_POWER_LIMIT_REQ only after "MIB_TPC_ANT_"
   * configuration, issue WAVE300_SW-2705 */
  if (mtlk_vap_manager_get_active_vaps_number(mtlk_vap_get_manager(core->vap_handle))) {
    return mtlk_set_power_limit(core);
  }

  return MTLK_ERR_OK;
}

BOOL __MTLK_IFUNC
mtlk_core_is_connected (mtlk_core_t *core)
{
  return mtlk_core_net_state_is_connected(mtlk_core_get_net_state(core));
}

BOOL __MTLK_IFUNC
mtlk_core_is_halted (mtlk_core_t *core)
{
  return (mtlk_core_get_net_state(core) & NET_STATE_HALTED);
}

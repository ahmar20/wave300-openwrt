/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
/*
 *
 * mtdump application main module.
 *
 */

#include "mtlkinc.h"

#include "mtlkwssa_drvinfo.h"
#include "mtidl_reader.h"
#include "mtlksighandler.h"
#include "mtlk_pathutils.h"
#include "argv_parser.h"

#define LOG_LOCAL_GID   GID_MTDUMP
#define LOG_LOCAL_FID   1

static void
_mtlk_print_error(const char* text, mtlk_error_t id)
{
  fprintf(stderr, "%s: %s\n", text, mtlk_get_error_text(id));
}

static void
_mtlk_dump_sta_list(uint32 sta_number, const IEEE_ADDR* sta_array)
{
  if (sta_number > 0)
  {
    uint32 i;
    fprintf(stdout, "\n\n%u peer(s) connected:\n\n", sta_number);
    for(i = 0; i < sta_number; i++)
    {
      fprintf(stdout, "\t" MAC_PRINTF_FMT "\n", MAC_PRINTF_ARG(&sta_array[i]));
    }
  }
  else
  {
    fprintf(stdout, "\n\nNo peers connected.\n\n");
  }

  fprintf(stdout, "\n");
}

static BOOL
_mtlk_parse_mac_addr(const char* addr_str, IEEE_ADDR* addr_bin)
{
#define _MAC_ARG_SCAN(x) &(x)[0],&(x)[1],&(x)[2],&(x)[3],&(x)[4],&(x)[5]
  uint32 mac_addr_bytes[IEEE_ADDR_LEN];

  if(sizeof(IEEE_ADDR) > sscanf(addr_str, MAC_PRINTF_FMT, _MAC_ARG_SCAN(mac_addr_bytes)))
  {
    return FALSE;
  }
  else
  {
    int i;
    for(i = 0; i < IEEE_ADDR_LEN; i++)
      addr_bin->au8Addr[i] = mac_addr_bytes[i];
    return TRUE;
  }
#undef _MAC_ARG_SCAN
}

static void
_mtlk_wss_on_event(mtlk_wss_data_source_t info_source,
                   uint32                 info_id,
                   void*                  buffer,
                   uint32                 buff_length,
                   mtlk_handle_t          ctx)
{
  char* mtidl_path = HANDLE_T_PTR(char, ctx);
  int res = mtlk_print_mtidl_item_by_id(mtidl_path, info_source, info_id, buffer, buff_length);

  if(MTLK_ERR_OK != res)
  {
    ELOG_DDD("Event parsing failed, id: %d, source %d, buffer length: %d",
             info_id, info_source, buff_length);
    _mtlk_print_error("Failed to parse the event arrived", res);
  }

  printf("\n");
}

MTLK_INIT_STEPS_LIST_BEGIN(mtdump)
  MTLK_INIT_STEPS_LIST_ENTRY(mtdump, OSDEP_LOG)
  MTLK_INIT_STEPS_LIST_ENTRY(mtdump, OSAL)
  MTLK_INIT_STEPS_LIST_ENTRY(mtdump, SHANDLER)
  MTLK_INIT_STEPS_LIST_ENTRY(mtdump, WSSA_API)
  MTLK_INIT_STEPS_LIST_ENTRY(mtdump, EXIT_EVT)
MTLK_INIT_INNER_STEPS_BEGIN(mtdump)
MTLK_INIT_STEPS_LIST_END(mtdump);

typedef struct
{
  char              app_location[1024];
  uint16            info_index;
  mtlk_handle_t     cleanup_ctx;
  mtlk_osal_event_t exit_evt;
  MTLK_DECLARE_INIT_STATUS;
} mtdump_app_t;

static mtdump_app_t gmtdump;

static const struct mtlk_argv_param_info_ex param_mtidl_path = {
  {
    "f",
    "mtidl-path",
    MTLK_ARGV_PINFO_FLAG_HAS_STR_DATA
  },
  "file or directory path to mtidlc file(s)",
  MTLK_ARGV_PTYPE_OPTIONAL
};

static const struct mtlk_argv_param_info_ex param_info_index = {
  {
    "i",
    "info-index",
    MTLK_ARGV_PINFO_FLAG_HAS_INT_DATA
  },
  "statistics index (used for aggregation)",
  MTLK_ARGV_PTYPE_OPTIONAL
};

static void __MTLK_IFUNC
mtdump_shandler (mtlk_handle_t usr_ctx, int signo)
{
  MTLK_UNREFERENCED_PARAM(usr_ctx);
  MTLK_UNREFERENCED_PARAM(signo);

  ILOG0_D("Signal arrived: %d", signo);

  mtlk_osal_event_set(&gmtdump.exit_evt);
}

static void
mtdump_cleanup ()
{
  MTLK_CLEANUP_BEGIN(mtdump, MTLK_OBJ_PTR(&gmtdump))
    MTLK_CLEANUP_STEP(mtdump, EXIT_EVT, MTLK_OBJ_PTR(&gmtdump),
                      mtlk_osal_event_cleanup, (&gmtdump.exit_evt));
    MTLK_CLEANUP_STEP(mtdump, WSSA_API, MTLK_OBJ_PTR(&gmtdump),
                      mtlk_wssa_api_cleanup, (gmtdump.cleanup_ctx));
    MTLK_CLEANUP_STEP(mtdump, SHANDLER, MTLK_OBJ_PTR(&gmtdump),
                      mtlk_shandler_cleanup, ());
    MTLK_CLEANUP_STEP(mtdump, OSAL, MTLK_OBJ_PTR(&gmtdump),
                      mtlk_osal_cleanup, ());
    MTLK_CLEANUP_STEP(mtdump, OSDEP_LOG, MTLK_OBJ_PTR(&gmtdump),
                      _mtlk_osdep_log_cleanup, ());
  MTLK_CLEANUP_END(mtdump, MTLK_OBJ_PTR(&gmtdump))
}

static int
mtdump_init ()
{
  MTLK_INIT_TRY(mtdump, MTLK_OBJ_PTR(&gmtdump))
    MTLK_INIT_STEP(mtdump, OSDEP_LOG, MTLK_OBJ_PTR(&gmtdump),
                   _mtlk_osdep_log_init, ("mtdump"));
    MTLK_INIT_STEP(mtdump, OSAL, MTLK_OBJ_PTR(&gmtdump),
                   mtlk_osal_init, ());
    MTLK_INIT_STEP(mtdump, SHANDLER, MTLK_OBJ_PTR(&gmtdump),
                   mtlk_shandler_init, (mtdump_shandler, HANDLE_T(0)));
    MTLK_INIT_STEP(mtdump, WSSA_API, MTLK_OBJ_PTR(&gmtdump),
                   mtlk_wssa_api_init, (NULL, HANDLE_T(0), &gmtdump.cleanup_ctx));
    MTLK_INIT_STEP(mtdump, EXIT_EVT, MTLK_OBJ_PTR(&gmtdump),
                   mtlk_osal_event_init, (&gmtdump.exit_evt));
  MTLK_INIT_FINALLY(mtdump, MTLK_OBJ_PTR(&gmtdump))
  MTLK_INIT_RETURN(mtdump, MTLK_OBJ_PTR(&gmtdump), mtdump_cleanup, ())
}

static int
_mtlk_process_wssa_based_commands(const char *cmd, const char *ifname, IEEE_ADDR *peer_id)
{
  if(!strcasecmp(cmd, "PeerList"))
  {
    int res = MTLK_ERR_UNKNOWN;
    mtlk_wss_provider_list_t* providers_list =
      mtlk_wss_get_providers_list(ifname, MTIDL_PROVIDER_PEER, &res);

    if(NULL != providers_list)
    {
       _mtlk_dump_sta_list(providers_list->providers_number,
                           (IEEE_ADDR*) providers_list->provider_id_array);
       mtlk_wss_free_providers_list(providers_list);
       return MTLK_ERR_OK;
    }
    else
    {
      _mtlk_print_error("Failed to retrieve information from wlan", res);
      return res;
    }
  }
  else if(!strcasecmp(cmd, "listen"))
  {
    mtlk_handle_t unregister_handle;
    int res = mtlk_wss_register_events_callback(ifname, MTIDL_PROVIDER_WLAN, NULL, _mtlk_wss_on_event,
                                                HANDLE_T(gmtdump.app_location), &unregister_handle, NULL, 0);
    if(MTLK_ERR_OK != res)
    {
      _mtlk_print_error("Failed to listen on wlan", res);
      return res;
    }
    else
    {
      fprintf(stderr, "Waiting for events...\n"
                      "Press Ctrl-C to interrupt.\n");
    }

    res = mtlk_osal_event_wait(&gmtdump.exit_evt, MTLK_OSAL_EVENT_INFINITE);
    if (res != MTLK_ERR_OK) {
      _mtlk_print_error("Failed to wait on wlan", res);
    }
    mtlk_wss_unregister_events_callback(unregister_handle);
    return MTLK_ERR_OK;
  }
  else
  {
    int res = mtlk_request_mtidl_item(gmtdump.app_location, ifname, cmd, peer_id, gmtdump.info_index);
    if(MTLK_ERR_OK != res)
    {
      _mtlk_print_error("Failed to retrieve information from wlan", res);
      return res;
    }
  }

  return MTLK_ERR_OK;
}

static int
parse_named_args(int *unnamed_args_num_p, int argc, char *argv[])
{
  /* Process all named arguments here.
     Correct number of unnamed arguments accordingly
     on each named argument found.
     Assumption: All named arguments must follow unnamed ones
  */

  int res;
  int argv_inited = FALSE;
  mtlk_argv_param_t *param = NULL;
  mtlk_argv_parser_t argv_parser;

  res = mtlk_argv_parser_init(&argv_parser, argc, argv);
  if (res != MTLK_ERR_OK) {
    ELOG_D("Can't init argv parser (err=%d)", res);
    goto end_of_parse_named_args;
  }
  argv_inited = TRUE;

  /* Get MTIDLs' file or directory name from input params */
  param = mtlk_argv_parser_param_get(&argv_parser, &param_mtidl_path.info);
  if (param) {
    const char *file_path;

    file_path = mtlk_argv_parser_param_get_str_val(param);
    if (file_path == NULL){
      mtlk_argv_parser_param_release(param);
      _mtlk_print_error("mtidlc path is not specified", MTLK_ERR_PARAMS);
      res = MTLK_ERR_VALUE;
      goto end_of_parse_named_args;
    }
    *unnamed_args_num_p -= 2;
    strncpy(gmtdump.app_location, file_path, ARRAY_SIZE(gmtdump.app_location));
    mtlk_argv_parser_param_release(param);
  }

  /* Get statistics index from input params */
  param = mtlk_argv_parser_param_get(&argv_parser, &param_info_index.info);
  if (param) {
    uint16 info_index;

    info_index = mtlk_argv_parser_param_get_uint_val(param, 0);
    *unnamed_args_num_p -= 2;
    gmtdump.info_index = info_index;
    mtlk_argv_parser_param_release(param);
  }

end_of_parse_named_args:

  if (argv_inited) {
    mtlk_argv_parser_cleanup(&argv_parser);
  }

  return res;
}

int
main(int argc, char *argv[])
{
  char* ifname;
  char* element_name;
  IEEE_ADDR mac_addr;
  BOOL has_mac_addr = FALSE;
  uint32 mtidl_items_number;
  int unnamed_args_num;

  int res = mtdump_init();
  if(MTLK_ERR_OK != res)
  {
    _mtlk_print_error("Application init failed", res);
    return -1;
  }

  gmtdump.app_location[0] = 0;
  gmtdump.info_index = 0;
  unnamed_args_num = argc;

  res = parse_named_args(&unnamed_args_num, argc, argv);

  if(MTLK_ERR_OK != res){
     mtdump_cleanup();
     return -7;
  }

  /* Get executable current path if MTIDL path was NOT specified explicitly */
  if (gmtdump.app_location[0] == 0){
    res = mtlk_get_current_executable_path(gmtdump.app_location, ARRAY_SIZE(gmtdump.app_location));
    if(MTLK_ERR_OK != res)
    {
      _mtlk_print_error("Failed to obtain full mtdump path", res);

      mtdump_cleanup();
      return -6;
    }
  }

  res = mtlk_count_mtidl_items(gmtdump.app_location, &mtidl_items_number);
  if(MTLK_ERR_OK != res)
  {
    _mtlk_print_error("Failed to access MTIDL database", res);

    mtdump_cleanup();
    return -5;
  }
  else if(0 == mtidl_items_number)
  {
    fprintf(stderr, "\nWARNING: No items found in MTIDL database\n");
  }
  else
  {
    ILOG0_D("%d items found in MTIDL database", mtidl_items_number);
  }

  if (unnamed_args_num < 3) {
    fprintf(stderr, "\n"
          "Lantiq status retrieval utility v. " MTLK_SOURCE_VERSION "\n"
          "Used to retrieve status of Lantiq wireless subsystem and dump it in human readable form\n\n"
          "Usage:\n\n"
          "  Information retrieval mode:\n"
          "  mtdump <interface> <info_name> [<MAC_addr>] [-f <mtidl_path>] [-i <info_index>]\n"
          "    - interface is network interface name (wlan0, wlan1 etc.)\n"
          "    - info_name is name of information element to be retrieved.\n"
          "    - MAC_addr is a wireless MAC address given in form XX:XX:XX:XX:XX:XX\n"
          "    - mtidl_path is a file name or directory with MTIDL item's \n"
          "      description. If a directory specified, all files with the .mtidlc\n"
          "      extension will be processed. Option must follows all others.\n"
          "    - info_index is a TS index used for aggregation statistics\n"
          "\n"
          "      Supported elements are:\n"
          "        PeerList                       - List of peers currently connected\n");
               mtlk_print_requestable_mtidl_items_list(gmtdump.app_location);
    fprintf(stderr, "\n"
          "  Events dumping mode:\n"
          "  mtdump <interface> listen\n"
          "    - interface is network interface name (wlan0, wlan1 etc.)\n\n");

    mtdump_cleanup();
    return -2;
  }

  ifname = argv[1];
  element_name = argv[2];

  if(unnamed_args_num > 3)
  {
    has_mac_addr = TRUE;
    if(!_mtlk_parse_mac_addr(argv[3], &mac_addr))
    {
      _mtlk_print_error("Invalid MAC address passed", MTLK_ERR_PARAMS);

      mtdump_cleanup();
      return -3;
    }
  }

  res = _mtlk_process_wssa_based_commands(element_name, ifname, has_mac_addr ? &mac_addr : NULL);
  if(MTLK_ERR_OK != res)
  {
    _mtlk_print_error("Command processing failed", res);

    mtdump_cleanup();
    return -4;
  }

  mtdump_cleanup();
  return 0;
}

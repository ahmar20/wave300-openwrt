/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __NL_H__
#define __NL_H__

#ifdef MTCFG_USE_GENL

#define MTLK_GENL_FAMILY_NAME     "MTLK_WLS"
#define MTLK_GENL_FAMILY_VERSION  1

/* attributes of the family */
enum {
        MTLK_GENL_ATTR_UNSPEC,
        MTLK_GENL_ATTR_EVENT,
        __MTLK_GENL_ATTR_MAX,
};
#define MTLK_GENL_ATTR_MAX (__MTLK_GENL_ATTR_MAX -1)

/* supported commands, genl commands, not mtlk commands (arp, wd, rmmod) */
enum {
        MTLK_GENL_CMD_UNSPEC,
        MTLK_GENL_CMD_EVENT,
        __MTLK_GENL_CMD_MAX,
};
#define MTLK_GENL_CMD_MAX (__MTLK_GENL_CMD_MAX -1)

#endif /* MTCFG_USE_GENL */

#define NL_DRV_CMD_MAN_FRAME      4
#define NL_DRV_IRBM_NOTIFY        5
#define NL_DRV_CMD_RCVRY_STARTED  6
#define NL_DRV_CMD_RCVRY_CONFIG   7

#if 0 //pc2005 from 5.3 MTCFG_LINUX_BACKPORT
#define NETLINK_MESSAGE_GROUP           (1L << 0)/* the group that drvhlpr, hostapd and wpa_supplicant join */
#else
#define NETLINK_MESSAGE_GROUP           (0L << 0)/* the group that drvhlpr, hostapd and wpa_supplicant join */
#endif
/* TODO: change NETLINK_SIMPLE_CONFIG_GROUP to group that WPS application will listen
 * it should not match any other active group 
 */
 
//pc2005 from 5.3
#if 0 
#define NETLINK_SIMPLE_CONFIG_GROUP     (1L << 0) /* the group that wsccmd application (Simple Config) joins */ 
#define NETLINK_IRBM_GROUP              (1L << 1) /* the group that IRB clients join */
#define NETLINK_LOGSERVER_GROUP         (1L << 2) /* the group that logserver joins */
#else
#define MTLK_NETLINK_SIMPLE_CONFIG_GROUP_NAME "mtlk_config"
#define MTLK_NETLINK_IRBM_GROUP_NAME          "mtlk_irb"
#define MTLK_NETLINK_LOGSERVER_GROUP_NAME     "mtlk_logserver"

#define NETLINK_SIMPLE_CONFIG_GROUP     (0L << 0) /* the group that wsccmd application (Simple Config) joins */
#define NETLINK_IRBM_GROUP              (1L << 0) /* the group that IRB clients join */
#define NETLINK_LOGSERVER_GROUP         (2L << 0) /* the group that logserver joins */
#endif

struct mtlk_nl_msghdr {
  char fingerprint[4]; // "mtlk"
  __u8  proto_ver;
  __u8  cmd_id;
  __u16 data_len;
} __attribute__ ((aligned(1), packed));

#endif

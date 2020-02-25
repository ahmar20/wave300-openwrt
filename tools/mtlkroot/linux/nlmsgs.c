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
 * Subsystem providing communication with userspace over
 * NETLINK_USERSOCK netlink protocol.
 *
 */
#include "mtlkinc.h"

#include <linux/module.h>
#include <linux/skbuff.h>
#include <linux/socket.h>
#include <net/sock.h>
#include <linux/netlink.h>

#ifdef MTCFG_USE_GENL
#include <net/genetlink.h>
#endif

#include "nl.h"

#include "bt_acs.h"

#define LOG_LOCAL_GID   GID_NLMSGS
#define LOG_LOCAL_FID   1


struct sock *bt_acs_nl_sock = NULL;
DEFINE_MUTEX(bt_acs_nl_mutex);
static void bt_acs_nl_input (struct sk_buff *inskb) {}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0)
//static void bt_acs_nl_bind (int group) {}
static int bt_acs_nl_bind (struct net *net, int group) { return 0;}
#endif

#ifdef MTCFG_USE_GENL

/* Module parameter */
extern int mtlk_genl_family_id;

static struct genl_multicast_group mtlk_mcgrps[] = {
  [NETLINK_SIMPLE_CONFIG_GROUP] = { .name = MTLK_NETLINK_SIMPLE_CONFIG_GROUP_NAME, },
  [NETLINK_IRBM_GROUP] = { .name = MTLK_NETLINK_IRBM_GROUP_NAME, },
  [NETLINK_LOGSERVER_GROUP] = { .name = MTLK_NETLINK_LOGSERVER_GROUP_NAME, },
};

/* family structure */
static struct genl_family mtlk_genl_family = {
//pc2005 GENL_ID_GENERATE removed
//        .id = GENL_ID_GENERATE,
        .id = 0,
        .name = MTLK_GENL_FAMILY_NAME,
        .version = MTLK_GENL_FAMILY_VERSION,
        .maxattr = MTLK_GENL_ATTR_MAX,

        .mcgrps = mtlk_mcgrps,
        .n_mcgrps = ARRAY_SIZE(mtlk_mcgrps),
};

#else /* MTCFG_USE_GENL */

struct sock *nl_sock = NULL;
#if LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,22)
DEFINE_MUTEX(nl_mutex);
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
static void nl_input (struct sock *sk, int len)
{
  struct sk_buff *skb;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
  while ((skb = skb_dequeue(&sk->receive_queue)) != NULL) {
#else
  while ((skb = skb_dequeue(&sk->sk_receive_queue)) != NULL) {
#endif 
    kfree(skb);
  }

}
#else /* kernel version >=  2.6.24 */
static void nl_input (struct sk_buff *inskb) {}
static void nl_bind (int group) {}
#endif

#endif /* MTCFG_USE_GENL */

int mtlk_nl_bt_acs_send_brd_msg(void *data, int length)
{
  struct sk_buff *skb = NULL;
  struct nlmsghdr *nlh;

  /* no socket - no messages */
  if (!bt_acs_nl_sock)
    return MTLK_ERR_UNKNOWN;

  skb = alloc_skb(NLMSG_SPACE(length), GFP_ATOMIC);
  if (skb == NULL)
    return MTLK_ERR_NO_MEM;

  nlh = (struct nlmsghdr*) skb->data;
  nlh->nlmsg_len = NLMSG_SPACE(length);
  nlh->nlmsg_pid = 0;
  nlh->nlmsg_flags = 0;

  /* fill the message header */
  skb_put(skb, NLMSG_SPACE(length));

  memcpy(NLMSG_DATA(nlh), data, length);

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
  NETLINK_CB(skb).pid = 0;  /* from kernel */
#else
  NETLINK_CB(skb).portid = 0;  /* from kernel */
#endif
  
  if (netlink_broadcast(bt_acs_nl_sock, skb, 0, NETLINK_BT_ACS_GROUP, GFP_ATOMIC))
    return MTLK_ERR_UNKNOWN;
  
  return MTLK_ERR_OK;
}

int mtlk_nl_bt_acs_init(void)
{
#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
  bt_acs_nl_sock = netlink_kernel_create(&init_net, NETLINK_BT_ACS, NETLINK_BT_ACS_GROUP_LAST,
                                          bt_acs_nl_input, &bt_acs_nl_mutex, THIS_MODULE);
#else
  struct netlink_kernel_cfg bt_acs_nl_cfg;

  memset(&bt_acs_nl_cfg, 0, sizeof(bt_acs_nl_cfg));	//pc2005 from 5.3
  bt_acs_nl_cfg.groups = NETLINK_BT_ACS_GROUP_LAST;
  bt_acs_nl_cfg.flags = 0;
  bt_acs_nl_cfg.input = &bt_acs_nl_input;
  bt_acs_nl_cfg.cb_mutex = &bt_acs_nl_mutex;
  bt_acs_nl_cfg.bind = &bt_acs_nl_bind;

  bt_acs_nl_sock = netlink_kernel_create(&init_net, NETLINK_BT_ACS, &bt_acs_nl_cfg);
#endif

  if (!bt_acs_nl_sock) {
    return MTLK_ERR_UNKNOWN;
  }
  return MTLK_ERR_OK;

}

void mtlk_nl_bt_acs_cleanup(void)
{

  if(bt_acs_nl_sock){
    sock_release(bt_acs_nl_sock->sk_socket);
  }

}

#ifndef MTCFG_USE_GENL

int mtlk_nl_send_brd_msg(void *data, int length, gfp_t flags, u32 dst_group, u8 cmd)
{
  struct sk_buff *skb = NULL;
  struct nlmsghdr *nlh;
  struct mtlk_nl_msghdr *mhdr;
  int full_len = length + sizeof(*mhdr);

  /* no socket - no messages */
  if (!nl_sock)
    return MTLK_ERR_UNKNOWN;

  skb = alloc_skb(NLMSG_SPACE(full_len), flags);
  if (skb == NULL)
    return MTLK_ERR_NO_MEM;

  nlh = (struct nlmsghdr*) skb->data;
  nlh->nlmsg_len = NLMSG_SPACE(full_len);
  nlh->nlmsg_pid = 0;
  nlh->nlmsg_flags = 0;

  /* fill the message header */

  skb_put(skb, NLMSG_SPACE(full_len));

  mhdr = (struct mtlk_nl_msghdr*) (NLMSG_DATA(nlh));
  memcpy(mhdr->fingerprint, "mtlk", 4);
  mhdr->proto_ver = 1;
  mhdr->cmd_id = cmd;
  mhdr->data_len = length;

  memcpy((char *)mhdr + sizeof(*mhdr), data, length);
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
  NETLINK_CB(skb).dst_groups = dst_group;
#else
  NETLINK_CB(skb).dst_group = dst_group;
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(3,7,0)
  NETLINK_CB(skb).pid = 0;      /* from kernel */
#else
  NETLINK_CB(skb).portid = 0;
#endif
  
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
  /* void return value in 2.4 kernels */
  netlink_broadcast(nl_sock, skb, 0, dst_group, flags);
#else
  if (netlink_broadcast(nl_sock, skb, 0, dst_group, flags))
    return MTLK_ERR_UNKNOWN;
#endif

  return MTLK_ERR_OK;
}


int mtlk_nl_init(void)
{
  /* netlink groups that are expected:
   * 1 - is joined by hostapd, wpa_supplicant and drvhlpr
   * 2 - is joined by wsccmd (Simple Config)
   * 3 - is joined by IRB media
   * so, total number of 3 groups must be told to kernel
   * when creating socket
   */

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,14)
  nl_sock = netlink_kernel_create(NETLINK_USERSOCK, nl_input);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,22)
  nl_sock = netlink_kernel_create(NETLINK_USERSOCK, 3, nl_input, THIS_MODULE);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,24)
  nl_sock = netlink_kernel_create(NETLINK_USERSOCK, 3, nl_input,
                                                       &nl_mutex, THIS_MODULE);
#elif LINUX_VERSION_CODE < KERNEL_VERSION(3,6,0)
  nl_sock = netlink_kernel_create(&init_net, NETLINK_USERSOCK, 3, nl_input,
                                                       &nl_mutex, THIS_MODULE);
#else
  {
    struct netlink_kernel_cfg nl_cfg;

    memset(&nl_cfg, 0, sizeof(nl_cfg));	//pc2005 from 5.3
    nl_cfg.groups = 3;
    nl_cfg.flags = 0;
    nl_cfg.input = &nl_input;
    nl_cfg.cb_mutex = &nl_mutex;
    nl_cfg.bind = &nl_bind;

    nl_sock = netlink_kernel_create(&init_net, NETLINK_USERSOCK, &nl_cfg);
  }
#endif
  if (!nl_sock) {
    return MTLK_ERR_UNKNOWN;
  }

  if (mtlk_nl_bt_acs_init() != MTLK_ERR_OK){
    return MTLK_ERR_UNKNOWN;
  }

  return MTLK_ERR_OK;
}

void mtlk_nl_cleanup(void)
{
  if(nl_sock)
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,0)
    sock_release(nl_sock->socket);
#else
    sock_release(nl_sock->sk_socket);
#endif

  mtlk_nl_bt_acs_cleanup();

}


#else /* MTCFG_USE_GENL */

int mtlk_nl_send_brd_msg(void *data, int length, gfp_t flags, u32 dst_group, u8 cmd)
{
  struct sk_buff *skb;
  struct nlattr *attr;
  void *msg_header;
  int size, genl_res, send_group;
  int res = MTLK_ERR_UNKNOWN;
  struct mtlk_nl_msghdr *mhdr;
  int full_len = length + sizeof(*mhdr);

  /* allocate memory */
  size = nla_total_size(full_len);

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
  skb = nlmsg_new(NLMSG_ALIGN (GENL_HDRLEN + size));
#elif LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
  skb = nlmsg_new(genlmsg_total_size(size), flags);
#else
  skb = genlmsg_new(size, flags);
#endif
  if (!skb)
    return MTLK_ERR_NO_MEM;

  /* add the genetlink message header */
#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,20)
  msg_header = genlmsg_put(skb, 0, 0, mtlk_genl_family.id, 0, 0,
                           MTLK_GENL_CMD_EVENT, mtlk_genl_family.version);
#else
  msg_header = genlmsg_put(skb, 0, 0, &mtlk_genl_family, 0, MTLK_GENL_CMD_EVENT);
#endif
  if (!msg_header)
    goto out_free_skb;

  /* fill the data */
  attr = nla_reserve(skb, MTLK_GENL_ATTR_EVENT, full_len);
  if (!attr)
    goto out_free_skb;

  mhdr = (struct mtlk_nl_msghdr*) (nla_data(attr));
  memcpy(mhdr->fingerprint, "mtlk", 4);
  mhdr->proto_ver = 1;
  mhdr->cmd_id = cmd;
  mhdr->data_len = length;

  memcpy((char *)mhdr + sizeof(*mhdr), data, length);

  /* send multicast genetlink message */
//TODO kernel changed int -> void
//  genl_res = 
  genlmsg_end(skb, msg_header);
//  if (genl_res < 0)
//    goto out_free_skb;
  
  /* the group to broadcast on is calculated on base of family id */
  send_group = mtlk_genl_family.id + dst_group - 1;

#if LINUX_VERSION_CODE < KERNEL_VERSION(2,6,19)
  genl_res = genlmsg_multicast(skb, 0, send_group);
#else
  send_group = dst_group;
  genl_res = genlmsg_multicast(&mtlk_genl_family, skb, 0, send_group, flags);
#endif

  if (genl_res)
    return MTLK_ERR_UNKNOWN;
  else
    return MTLK_ERR_OK;

out_free_skb:
  nlmsg_free(skb);
  return res;
}

int mtlk_nl_init(void)
{
  int result;

  if(mtlk_genl_family_id)
  {
    mtlk_genl_family.id = mtlk_genl_family_id;
  }

  result = genl_register_family(&mtlk_genl_family);
  if (result) {
    return MTLK_ERR_UNKNOWN;
  }

  /* Pass actual value back to module param
   * to be readable via sysfs and iwpriv */
  mtlk_genl_family_id = mtlk_genl_family.id;

  if (mtlk_nl_bt_acs_init() != MTLK_ERR_OK) {
    genl_unregister_family(&mtlk_genl_family);
    return MTLK_ERR_UNKNOWN;
  }

  return MTLK_ERR_OK;
}

void mtlk_nl_cleanup(void)
{
  genl_unregister_family(&mtlk_genl_family);

  mtlk_nl_bt_acs_cleanup();

}


#endif /* MTCFG_USE_GENL */

EXPORT_SYMBOL(mtlk_nl_send_brd_msg);
EXPORT_SYMBOL(mtlk_nl_bt_acs_send_brd_msg);

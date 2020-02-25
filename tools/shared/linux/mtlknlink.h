/******************************************************************************

                               Copyright (c) 2012
                            Lantiq Deutschland GmbH

  For licensing information, see the file 'LICENSE' in the root folder of
  this software module.

******************************************************************************/
#ifndef __MTLK_NLINK_H__
#define __MTLK_NLINK_H__

#define   MTLK_IDEFS_ON
#include "mtlkidefs.h"

typedef void (*mtlk_netlink_callback_t)(void* ctx, void* data);

#ifdef MTCFG_USE_GENL

#include <netlink/netlink.h>
#include <netlink/genl/genl.h>
#include <netlink/genl/ctrl.h>

typedef struct _mtlk_nlink_socket_t
{
  int family;
  struct nl_sock *sock;
  mtlk_netlink_callback_t receive_callback;
  void* receive_callback_ctx;
} mtlk_nlink_socket_t;

//TODO: Move this to .c file

#define MTLK_GENL_FAMILY_NAME    "MTLK_WLS"

#else /* MTCFG_USE_GENL */

#include <asm/types.h>
#include <sys/socket.h>
#include <linux/netlink.h>

typedef struct _mtlk_nlink_socket_t
{
  int sock_fd;
  struct sockaddr_nl src_addr;
  mtlk_netlink_callback_t receive_callback;
  void* receive_callback_ctx;
} mtlk_nlink_socket_t;

#endif /* MTCFG_USE_GENL */

#if 1	//pc2005 from 5.3 defined(MTCFG_USE_GENL) && defined(MTCFG_LINUX_BACKPORT)
int __MTLK_IFUNC
mtlk_nlink_create(mtlk_nlink_socket_t* nlink_socket, const char* group_name,
                  mtlk_netlink_callback_t receive_callback, void* callback_ctx);
#else
int __MTLK_IFUNC
mtlk_nlink_create(mtlk_nlink_socket_t* nlink_socket, int netlink_group,
                  mtlk_netlink_callback_t receive_callback, void* callback_ctx);
#endif

int __MTLK_IFUNC
mtlk_nlink_receive_loop(mtlk_nlink_socket_t* nlink_socket, int stop_fd);

void __MTLK_IFUNC
mtlk_nlink_cleanup(mtlk_nlink_socket_t* nlink_socket);

#define   MTLK_IDEFS_OFF
#include "mtlkidefs.h"

#endif /* __MTLK_NLINK_H__ */

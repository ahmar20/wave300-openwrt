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
 *
 * WPA security helper routines
 *
 */

#ifndef __WPA_H__
#define __WPA_H__

#define LOG_LOCAL_GID   GID_WPA
#define LOG_LOCAL_FID   0

#define WPA_KEY_MGMT_IEEE8021X  0x00
#define WPA_KEY_MGMT_PSK        0x01
#define WPA_KEY_MGMT_NONE       0x02
#define WPA_KEY_MGMT_IEEE8021X_NO_WPA 0x08
#define WPA_KEY_MGMT_WPA_NONE   0x10

#define WPA_PROTO_WPA 0x00
#define WPA_PROTO_RSN 0x01

#define GENERIC_INFO_ELEM 0xdd
#define RSN_INFO_ELEM 0x30

#define PMKID_LEN 16

struct wpa_ie_data {
        int proto;
        int pairwise_cipher;
        int group_cipher;
        int key_mgmt;
        int capabilities;
        int num_pmkid;
        const u8 *pmkid;
};

int wpa_parse_wpa_ie(const u8 *wpa_ie, size_t wpa_ie_len,
             struct wpa_ie_data *data);

#undef LOG_LOCAL_GID
#undef LOG_LOCAL_FID

#endif  // __WPA_H__

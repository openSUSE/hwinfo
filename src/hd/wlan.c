#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <sys/ioctl.h>
#include <linux/types.h>                /* for "caddr_t" et al          */
#include <linux/socket.h>               /* for "struct sockaddr" et al  */
#include <linux/if.h>                   /* for IFNAMSIZ and co... */
#include <linux/wireless.h>
#include <net/ethernet.h>

#include "hd.h"
#include "hd_int.h"
#include "wlan.h"

/**
 * @defgroup WLANint WLAN devices
 * @ingroup libhdDEVint
 * @brief WLAN device functions
 *
 * @{
 */

#ifndef LIBHD_TINY

#define AUTH_ALG_OPEN_SYSTEM    0x01
#define AUTH_ALG_SHARED_KEY     0x02
#define AUTH_ALG_LEAP           0x04

typedef enum { WPA_ALG_NONE, WPA_ALG_WEP, WPA_ALG_TKIP, WPA_ALG_CCMP } wpa_alg;
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;

struct wpa_driver_ops {
  int (*set_wpa)(const char *ifnmae, int enabled);
  int (*set_auth_alg)(const char *ifname, int auth_alg);
  int (*set_key)(const char *ifname, wpa_alg alg, unsigned char *addr,
		 int key_idx, int set_tx, u8 *seq, size_t seq_len,
		 u8 *key, size_t key_len);
};

struct wpa_driver_ops wpa_driver_hostap_ops;
struct wpa_driver_ops wpa_driver_prism54_ops;
struct wpa_driver_ops wpa_driver_hermes_ops;
struct wpa_driver_ops wpa_driver_madwifi_ops;
struct wpa_driver_ops wpa_driver_atmel_ops;
struct wpa_driver_ops wpa_driver_wext_ops;
struct wpa_driver_ops wpa_driver_ndiswrapper_ops;
struct wpa_driver_ops wpa_driver_ipw_ops;

/* the iw_ functions are copied from libiw, so we do not need to
   link against it */

int iw_sockets_open(void)
{
  static const int families[] = {
    AF_INET, AF_IPX, AF_AX25, AF_APPLETALK
  };
  unsigned int  i;
  int           sock;

  /*
   * Now pick any (exisiting) useful socket family for generic queries
   * Note : don't open all the socket, only returns when one matches,
   * all protocols might not be valid.
   * Workaround by Jim Kaba <jkaba@sarnoff.com>
   * Note : in 99% of the case, we will just open the inet_sock.
   * The remaining 1% case are not fully correct...
   */

  /* Try all families we support */
  for(i = 0; i < sizeof(families)/sizeof(int); ++i)
    {
      /* Try to open the socket, if success returns it */
      sock = socket(families[i], SOCK_DGRAM, 0);
      if(sock >= 0)
        return sock;
  }

  return -1;
}

static inline int
iw_get_ext(int                  skfd,           /* Socket to the kernel */
           const char *         ifname,         /* Device name */
           int                  request,        /* WE ID */
           struct iwreq *       pwrq)           /* Fixed part of the request */
{
  /* Set device name */
  strncpy(pwrq->ifr_name, ifname, IFNAMSIZ);
  /* Do the request */
  return(ioctl(skfd, request, pwrq));
}

int iw_get_range_info(int           skfd,
		      const char *  ifname,
		      struct iw_range *    range)
{
  struct iwreq          wrq;
  char                  buffer[sizeof(struct iw_range) * 2];    /* Large enough */
  struct iw_range *     range_raw;

  /* Cleanup */
  bzero(buffer, sizeof(buffer));

  wrq.u.data.pointer = (caddr_t) buffer;
  wrq.u.data.length = sizeof(buffer);
  wrq.u.data.flags = 0;
  if(iw_get_ext(skfd, ifname, SIOCGIWRANGE, &wrq) < 0)
    return(-1);

  /* Point to the buffer */
  range_raw = (struct iw_range *) buffer;

  /* For new versions, we can check the version directly, for old versions
   * we use magic. 300 bytes is a also magic number, don't touch... */
  if(wrq.u.data.length < 300) {
    /* That's v10 or earlier. Ouch ! Let's make a guess...*/
    range_raw->we_version_compiled = 9;
  }

  /* Check how it needs to be processed */
  if(range_raw->we_version_compiled > 15) {
    /* This is our native format, that's easy... */
    /* Copy stuff at the right place, ignore extra */
    memcpy((char *) range, buffer, sizeof(struct iw_range));
  }
  else {
    /* not supported */
    return(-1);
  }

  return(0);
}

double iw_freq2float(const struct iw_freq *    in)
{
  int           i;
  double        res = (double) in->m;
  for(i = 0; i < in->e; i++)
    res *= 10;
  return(res);
}

void hd_scan_wlan(hd_data_t *hd_data)
{
  hd_t *hd;
  hd_res_t *res;
  struct iw_range range;
  int k;
  int skfd;
  struct wpa_driver_ops *wpa_drv=NULL;

  if(!hd_probe_feature(hd_data, pr_wlan)) return;

  hd_data->module = mod_wlan;

  PROGRESS(1, 0, "detecting wlan features");

  if ((skfd = iw_sockets_open()) < 0) {
    ADD2LOG( "could not open socket, wlan feature query failed\n" );
    return;
  }

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_network &&
      hd->unix_dev_name ) {
      /* Get list of frequencies / channels */
      if(iw_get_range_info(skfd, hd->unix_dev_name, &range) < 0) {
	/* this failed, maybe device does not support wireless extensions */
	continue;
      }
      ADD2LOG("*** device %s is wireless ***\n", hd->unix_dev_name);
      hd->is.wlan = 1;
      res = new_mem(sizeof *res);
      res->any.type = res_wlan;

      if(range.num_frequency > 0) {
	char buff[20];
	for(k = 0; k < range.num_frequency; k++) {
	  snprintf(buff, 19, "%i", range.freq[k].i);
	  add_str_list(&res->wlan.channels, buff);
	  snprintf(buff, 19, "%g", (float)iw_freq2float(&(range.freq[k]))/1000000000);
	  add_str_list(&res->wlan.frequencies, buff);
	}
	for(k = 0; k < range.num_bitrates; k++) {
	  snprintf(buff, 19, "%g", (float)range.bitrate[k]/1000000);
	  add_str_list(&res->wlan.bitrates, buff);
	}
	for(k = 0; k < range.num_encoding_sizes; k++) {
	  snprintf(buff, 19, "WEP%i", range.encoding_size[k]*8);
	  add_str_list(&res->wlan.enc_modes, buff);
	}

	/* open mode is always supported */
	add_str_list(&res->wlan.auth_modes, "open");
	/* if WEP is supported, be assume shared key auth support */
	if(range.num_encoding_sizes) {
	  add_str_list(&res->wlan.auth_modes, "sharedkey");
	}

	/* detect WPA capabilities */
	if (!hd_data->flags.nowpa && hd->drivers) {
	  if (search_str_list(hd->drivers, "ath_pci"))
	    wpa_drv = &wpa_driver_madwifi_ops;
	  else if (strncmp(hd->drivers->str, "at76", 4)==0)
	    wpa_drv = &wpa_driver_atmel_ops;
	  else 
	    wpa_drv = &wpa_driver_wext_ops;
	}
	
	if (wpa_drv) {
	  if (wpa_drv->set_wpa(hd->unix_dev_name, 1) == 0) {
	    add_str_list(&res->wlan.auth_modes, "wpa-psk");
	    add_str_list(&res->wlan.auth_modes, "wpa-eap");
	    if (wpa_drv->set_auth_alg && 
		wpa_drv->set_auth_alg(hd->unix_dev_name, AUTH_ALG_LEAP)==0)
	      add_str_list(&res->wlan.auth_modes, "wpa-leap");
	    if (wpa_drv->set_key(hd->unix_dev_name, WPA_ALG_TKIP, "ff:ff:ff:ff:ff:ff",
				 0, 0, 0, 0,
				 "00000000000000000000000000000000", 32) ==0)
	      add_str_list(&res->wlan.enc_modes, "TKIP");
	    if (wpa_drv->set_key(hd->unix_dev_name, WPA_ALG_CCMP, "ff:ff:ff:ff:ff:ff",
				 0, 0, 0, 0, 
				 "0000000000000000", 16) ==0)
	      add_str_list(&res->wlan.enc_modes, "CCMP");
	    wpa_drv->set_wpa(hd->unix_dev_name, 0);
	  }
	}
      }
      add_res_entry(&hd->res, res);
    }
  }
}

/* following functions are copied from wpa_supplicant
   they are used to detect WPA capabilities */

/* begin hostap */

#define PRISM2_IOCTL_PRISM2_PARAM (SIOCIWFIRSTPRIV + 0)
#define PRISM2_IOCTL_HOSTAPD (SIOCDEVPRIVATE + 14)
#define HOSTAP_CRYPT_ALG_NAME_LEN 16
#define HOSTAP_CRYPT_FLAG_SET_TX_KEY (1 << (0))
#define PRISM2_HOSTAPD_GENERIC_ELEMENT_HDR_LEN \
 ((int) (&((struct prism2_hostapd_param *) 0)->u.generic_elem.data))

enum {
  PRISM2_SET_ENCRYPTION = 6,
  PRISM2_HOSTAPD_SET_GENERIC_ELEMENT = 12,
  PRISM2_PARAM_AP_AUTH_ALGS = 15,
  PRISM2_PARAM_HOST_ROAMING = 21,
  PRISM2_PARAM_WPA = 36,
  PRISM2_PARAM_PRIVACY_INVOKED = 37,
};

struct prism2_hostapd_param {
  u32 cmd;
  u8 sta_addr[ETH_ALEN];
  union {
    struct {
      u16 aid;
      u16 capability;
      u8 tx_supp_rates;
    } add_sta;
    struct {
      u32 inactive_sec;
    } get_info_sta;
    struct {
      u8 alg[HOSTAP_CRYPT_ALG_NAME_LEN];
      u32 flags;
      u32 err;
      u8 idx;
      u8 seq[8]; /* sequence counter (set: RX, get: TX) */
      u16 key_len;
      u8 key[0];
    } crypt;
    struct {
      u32 flags_and;
      u32 flags_or;
    } set_flags_sta;
    struct {
      u16 rid;
      u16 len;
      u8 data[0];
    } rid;
    struct {
      u8 len;
      u8 data[0];
    } generic_elem;
    struct {
      u16 cmd;
      u16 reason_code;
    } mlme;
    struct {
      u8 ssid_len;
      u8 ssid[32];
    } scan_req;
  } u;
};


int hostapd_ioctl(const char *dev, struct prism2_hostapd_param *param,
		  int len, int show_err)
{
  int s;
  int ret =0;
  struct iwreq iwr;

  s = socket(PF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    return -1;
  }

  memset(&iwr, 0, sizeof(iwr));
  strncpy(iwr.ifr_name, dev, IFNAMSIZ);
  iwr.u.data.pointer = (caddr_t) param;
  iwr.u.data.length = len;

  if (ioctl(s, PRISM2_IOCTL_HOSTAPD, &iwr) < 0) {
    ret=1;
  }
  close(s);

  return 0;
}

int prism2param(const char *ifname, int param, int value)
{
  struct iwreq iwr;
  int *i, s, ret = 0;

  s = socket(PF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    return -1;
  }

  memset(&iwr, 0, sizeof(iwr));
  strncpy(iwr.ifr_name, ifname, IFNAMSIZ);
  i = (int *) iwr.u.name;
  *i++ = param;
  *i++ = value;

  if (ioctl(s, PRISM2_IOCTL_PRISM2_PARAM, &iwr) < 0) {
    ret = -1;
  }
  close(s);
  return ret;
}

int wpa_driver_hostap_set_auth_alg(const char *ifname, int auth_alg)
{
  int algs = 0;

  if (auth_alg & AUTH_ALG_OPEN_SYSTEM)
    algs |= 1;
  if (auth_alg & AUTH_ALG_SHARED_KEY)
    algs |= 2;
  if (auth_alg & AUTH_ALG_LEAP)
    algs |= 4;
  if (algs == 0)
    algs = 1; /* at least one algorithm should be set */

  return prism2param(ifname, PRISM2_PARAM_AP_AUTH_ALGS, algs);
}

int wpa_driver_hostap_set_wpa(const char *ifname, int enabled)
{
  int ret = 0;

  if (prism2param(ifname, PRISM2_PARAM_HOST_ROAMING,
		  enabled ? 2 : 0) < 0)
    ret = -1;
  if (prism2param(ifname, PRISM2_PARAM_PRIVACY_INVOKED, enabled) < 0)
    ret = -1;
  if (prism2param(ifname, PRISM2_PARAM_WPA, enabled) < 0)
    ret = -1;

  return ret;
}

int wpa_driver_hostap_set_key(const char *ifname, wpa_alg alg,
			      unsigned char *addr, int key_idx,
			      int set_tx, u8 *seq, size_t seq_len,
			      u8 *key, size_t key_len)
{
  struct prism2_hostapd_param *param;
  u8 *buf;
  size_t blen;
  int ret = 0;
  char *alg_name;

  switch (alg) {
      case WPA_ALG_NONE:
	alg_name = "none";
	break;
      case WPA_ALG_WEP:
	alg_name = "WEP";
	break;
      case WPA_ALG_TKIP:
	alg_name = "TKIP";
	break;
      case WPA_ALG_CCMP:
	alg_name = "CCMP";
	break;
      default:
	return -1;
  }

  if (seq_len > 8)
    return -2;

  blen = sizeof(*param) + key_len;
  buf = malloc(blen);
  if (buf == NULL)
    return -1;
  memset(buf, 0, blen);

  param = (struct prism2_hostapd_param *) buf;
  param->cmd = PRISM2_SET_ENCRYPTION;
  memset(param->sta_addr, 0xff, ETH_ALEN);

  strncpy(param->u.crypt.alg, alg_name, HOSTAP_CRYPT_ALG_NAME_LEN);
  param->u.crypt.flags = set_tx ? HOSTAP_CRYPT_FLAG_SET_TX_KEY : 0;
  param->u.crypt.idx = key_idx;
  memcpy(param->u.crypt.seq, seq, seq_len);
  param->u.crypt.key_len = key_len;
  memcpy((u8 *) (param + 1), key, key_len);

  if (hostapd_ioctl(ifname, param, blen, 1)) {
    ret = -1;
  }
  free(buf);

  return ret;
}

struct wpa_driver_ops wpa_driver_hostap_ops = {
  .set_wpa = wpa_driver_hostap_set_wpa,
  .set_key = wpa_driver_hostap_set_key,
  .set_auth_alg = wpa_driver_hostap_set_auth_alg,
};

/* end hostap */

/* begin madwifi */

#define     IEEE80211_IOCTL_SETPARAM        (SIOCIWFIRSTPRIV+0)
#define     IEEE80211_IOCTL_SETKEY          (SIOCIWFIRSTPRIV+2)
#define     IEEE80211_CIPHER_WEP            0
#define     IEEE80211_CIPHER_TKIP           1
#define     IEEE80211_CIPHER_AES_CCM        3
#define     IEEE80211_ADDR_LEN      6
#define     IEEE80211_KEY_XMIT      0x01
#define     IEEE80211_KEY_RECV      0x02  
#define     IEEE80211_KEYBUF_SIZE   16
#define     IEEE80211_MICBUF_SIZE   16

enum {
  IEEE80211_PARAM_WPA             = 10,   /* WPA mode (0,1,2) */
  IEEE80211_PARAM_ROAMING         = 12,   /* roaming mode */
  IEEE80211_PARAM_PRIVACY         = 13,   /* privacy invoked */
};

struct ieee80211req_key {
  u_int8_t        ik_type;        /* key/cipher type */
  u_int8_t        ik_pad;
  u_int16_t       ik_keyix;       /* key index */
  u_int8_t        ik_keylen;      /* key length in bytes */
  u_int8_t        ik_flags;
#define IEEE80211_KEY_DEFAULT   0x80    /* default xmit key */
  u_int8_t        ik_macaddr[IEEE80211_ADDR_LEN];
  u_int64_t       ik_keyrsc;      /* key receive sequence counter */
  u_int64_t       ik_keytsc;      /* key transmit sequence counter */
  u_int8_t        ik_keydata[IEEE80211_KEYBUF_SIZE+IEEE80211_MICBUF_SIZE];
};

int
set80211param(const char *dev, int op, int arg)
{
  struct iwreq iwr;
  int s=-1;

  if (s < 0 ? (s = socket(AF_INET, SOCK_DGRAM, 0)) == -1 : 0) {
    return -1;
  }

  memset(&iwr, 0, sizeof(iwr));
  strncpy(iwr.ifr_name, dev, IFNAMSIZ);
  iwr.u.mode = op;
  memcpy(iwr.u.name+sizeof(__u32), &arg, sizeof(arg));

  if (ioctl(s, IEEE80211_IOCTL_SETPARAM, &iwr) < 0) {
    return -1;
  }
  return 0;
}

static int
wpa_driver_madwifi_set_wpa(const char *ifname, int enabled)
{
  int ret = 0;

  if (set80211param(ifname, IEEE80211_PARAM_ROAMING, enabled ? 2 : 0) < 0)
    ret = -1;
  if (set80211param(ifname, IEEE80211_PARAM_PRIVACY, enabled) < 0)
    ret = -1;
  if (set80211param(ifname, IEEE80211_PARAM_WPA, enabled ? 3 : 0) < 0)
    ret = -1;

  return ret;
}

static int
set80211priv(const char *dev, int op, void *data, int len)
{
  struct iwreq iwr;
  int s=-1;

  if (s < 0 ? (s = socket(AF_INET, SOCK_DGRAM, 0)) == -1 : 0) {
    return -1;
  }

  memset(&iwr, 0, sizeof(iwr));
  strncpy(iwr.ifr_name, dev, IFNAMSIZ);
  if (len < IFNAMSIZ) {
    /*
     * Argument data fits inline; put it there.
     */
    memcpy(iwr.u.name, data, len);
  } else {
    /*
     * Argument data too big for inline transfer; setup a
     * parameter block instead; the kernel will transfer
     * the data for the driver.
     */
    iwr.u.data.pointer = data;
    iwr.u.data.length = len;
  }

  if (ioctl(s, op, &iwr) < 0) {
    return -1;
  }
  return 0;
}

static int
wpa_driver_madwifi_set_key(const char *ifname, wpa_alg alg,
			   unsigned char *addr, int key_idx,
			   int set_tx, u8 *seq, size_t seq_len,
			   u8 *key, size_t key_len)
{
  struct ieee80211req_key wk;
  char *alg_name;
  u_int8_t cipher;

  if (alg == WPA_ALG_NONE)
    return 0;

  switch (alg) {
      case WPA_ALG_WEP:
	alg_name = "WEP";
	cipher = IEEE80211_CIPHER_WEP;
	break;
      case WPA_ALG_TKIP:
	alg_name = "TKIP";
	cipher = IEEE80211_CIPHER_TKIP;
	break;
      case WPA_ALG_CCMP:
	alg_name = "CCMP";
	cipher = IEEE80211_CIPHER_AES_CCM;
	break;
      default:
	return -1;
  }

  if (seq_len > sizeof(u_int64_t)) {
    return -2;
  }
  if (key_len > sizeof(wk.ik_keydata)) {
    return -3;
  }

  memset(&wk, 0, sizeof(wk));
  wk.ik_type = cipher;
  wk.ik_flags = IEEE80211_KEY_RECV;
  if (set_tx) {
    wk.ik_flags |= IEEE80211_KEY_XMIT | IEEE80211_KEY_DEFAULT;
    memcpy(wk.ik_macaddr, addr, IEEE80211_ADDR_LEN);
  } else
    memset(wk.ik_macaddr, 0, IEEE80211_ADDR_LEN);
  wk.ik_keyix = key_idx;
  wk.ik_keylen = key_len;
  memcpy(&wk.ik_keyrsc, seq, seq_len);
  memcpy(wk.ik_keydata, key, key_len);

  return set80211priv(ifname, IEEE80211_IOCTL_SETKEY, &wk, sizeof(wk));
}

struct wpa_driver_ops wpa_driver_madwifi_ops = {
  .set_wpa		= wpa_driver_madwifi_set_wpa,
  .set_key		= wpa_driver_madwifi_set_key,
};

/* end madwifi */

/* begin ipw */

#define IPW_IOCTL_WPA_SUPPLICANT		SIOCIWFIRSTPRIV+30
#define IPW_CMD_SET_WPA_PARAM			1
#define IPW_CMD_SET_ENCRYPTION			3
#define IPW_PARAM_WPA_ENABLED			1
#define IPW_PARAM_AUTH_ALGS			5
#define	IPW_CRYPT_ALG_NAME_LEN			16

struct ipw_param {
  u32 cmd;
  u8 sta_addr[ETH_ALEN];
  union {
    struct {
      u8 name;
      u32 value;
    } wpa_param;
    struct {
      u32 len;
      u8 *data;
    } wpa_ie;
    struct{
      int command;
      int reason_code;
    } mlme;
    struct {
      u8 alg[IPW_CRYPT_ALG_NAME_LEN];
      u8 set_tx;
      u32 err;
      u8 idx;
      u8 seq[8];
      u16 key_len;
      u8 key[0];
    } crypt;

  } u;
};

int ipw_ioctl(const char *dev, struct ipw_param *param, int len)
{
  struct iwreq iwr;
  int s;
  int ret = 0;

  s = socket(PF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    return -1;
  }

  memset(&iwr, 0, sizeof(iwr));
  strncpy(iwr.ifr_name, dev, IFNAMSIZ);
  iwr.u.data.pointer = (caddr_t) param;
  iwr.u.data.length = len;

  if (ioctl(s, IPW_IOCTL_WPA_SUPPLICANT, &iwr) < 0) {
    ret = -1;
  }

  close(s);
  return ret;
}

int wpa_driver_ipw_set_wpa(const char *ifname, int enabled)
{
  int ret = 0;
  struct ipw_param param;

  memset(&param, 0, sizeof(param));
  param.cmd = IPW_CMD_SET_WPA_PARAM;
  param.u.wpa_param.name = IPW_PARAM_WPA_ENABLED;
  param.u.wpa_param.value = enabled;

  if (ipw_ioctl(ifname, &param, sizeof(param)) < 0)
    ret = -1;

  return ret;
}

int wpa_driver_ipw_set_key(const char *ifname, wpa_alg alg,
			   unsigned char *addr, int key_idx, int set_tx,
			   u8 *seq, size_t seq_len,
			   u8 *key, size_t key_len)
{
  struct ipw_param *param;
  u8 *buf;
  size_t blen;
  int ret = 0;
  char *alg_name;

  switch (alg) {
      case WPA_ALG_NONE:
	alg_name = "none";
	break;
      case WPA_ALG_WEP:
	alg_name = "WEP";
	break;
      case WPA_ALG_TKIP:
	alg_name = "TKIP";
	break;
      case WPA_ALG_CCMP:
	alg_name = "CCMP";
	break;
      default:
	return -1;
  }

  if (seq_len > 8)
    return -2;

  blen = sizeof(*param) + key_len;
  buf = malloc(blen);
  if (buf == NULL)
    return -1;
  memset(buf, 0, blen);

  param = (struct ipw_param *) buf;
  param->cmd = IPW_CMD_SET_ENCRYPTION;
  memset(param->sta_addr, 0xff, ETH_ALEN);
  strncpy(param->u.crypt.alg, alg_name, IPW_CRYPT_ALG_NAME_LEN);
  param->u.crypt.set_tx = set_tx ? 1 : 0;
  param->u.crypt.idx = key_idx;
  memcpy(param->u.crypt.seq, seq, seq_len);
  param->u.crypt.key_len = key_len;
  memcpy((u8 *) (param + 1), key, key_len);

  if (ipw_ioctl(ifname, param, blen)) {
    ret = -1;
  }
  free(buf);

  return ret;
}

int wpa_driver_ipw_set_auth_alg(const char *ifname, int auth_alg)
{
  int algs = 0;
  struct ipw_param param;

  if (auth_alg & AUTH_ALG_OPEN_SYSTEM)
    algs |= 1;
  if (auth_alg & AUTH_ALG_SHARED_KEY)
    algs |= 2;
  if (auth_alg & AUTH_ALG_LEAP)
    algs |= 4;
  if (algs == 0)
    algs = 1; /* at least one algorithm should be set */

  memset(&param, 0, sizeof(param));
  param.cmd = IPW_CMD_SET_WPA_PARAM;
  param.u.wpa_param.name = IPW_PARAM_AUTH_ALGS;
  param.u.wpa_param.value = algs;

  return ipw_ioctl(ifname, &param, sizeof(param));
}

struct wpa_driver_ops wpa_driver_ipw_ops = {
  .set_wpa = wpa_driver_ipw_set_wpa,
  .set_key = wpa_driver_ipw_set_key,
  .set_auth_alg = wpa_driver_ipw_set_auth_alg
};

/* end ipw */

/* begin atmel */

#define ATMEL_WPA_IOCTL                (SIOCIWFIRSTPRIV + 2)
#define ATMEL_WPA_IOCTL_PARAM          (SIOCIWFIRSTPRIV + 3)
#define ATMEL_WPA_IOCTL_GET_PARAM      (SIOCIWFIRSTPRIV + 4)

#define MAX_KEY_LENGTH      40

/* ATMEL_WPA_IOCTL ioctl() cmd: */
enum {
    SET_WPA_ENCRYPTION  = 1,
    SET_CIPHER_SUITES   = 2,
};

/* ATMEL_WPA_IOCTL_PARAM ioctl() cmd: */
enum {
            ATMEL_PARAM_WPA = 1,
            ATMEL_PARAM_PRIVACY_INVOKED = 2,
            ATMEL_PARAM_WPA_TYPE = 3,
};

struct atmel_param{
    unsigned char sta_addr[6];
        int     cmd;
        u8      alg;
        u8      key_idx;
        u8      set_tx;
        u8      seq[8];
        u8      seq_len;
        u16     key_len;
        u8      key[MAX_KEY_LENGTH];
    struct{
        int     reason_code;
        u8      state;
    }mlme;
    u8          pairwise_suite;
    u8          group_suite;
    u8          key_mgmt_suite;
};

int atmel_ioctl(const char *dev, struct atmel_param *param, int len)
{
  int s;
  int ret=0;
  struct iwreq iwr;
  
  s = socket(PF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    return -1;
  }
  
  memset(&iwr, 0, sizeof(iwr));
  strncpy(iwr.ifr_name, dev, IFNAMSIZ);
  iwr.u.data.pointer = (caddr_t) param;
  iwr.u.data.length = len;
  
  if (ioctl(s, ATMEL_WPA_IOCTL, &iwr) < 0) {
    ret = -1;
  }
  close(s);
  
  return 0;
}

int atmel2param(const char *ifname, int param, int value)
{
  struct iwreq iwr;
  int *i, s, ret = 0;
  
  s = socket(PF_INET, SOCK_DGRAM, 0);
  if (s < 0) {
    return -1;
  }

  memset(&iwr, 0, sizeof(iwr));
  strncpy(iwr.ifr_name, ifname, IFNAMSIZ);
  i = (int *) iwr.u.name;
  *i++ = param;
  *i++ = value;
  
  if (ioctl(s, ATMEL_WPA_IOCTL_PARAM, &iwr) < 0) {
    ret = -1;
  }
  close(s);
  return ret;
}

int wpa_driver_atmel_set_wpa(const char *ifname, int enabled)
{
  int ret = 0;
  
  if (atmel2param(ifname, ATMEL_PARAM_PRIVACY_INVOKED, enabled) < 0)
    ret = -1;
  if (atmel2param(ifname, ATMEL_PARAM_WPA, enabled) < 0)
    ret = -1;
  
  return ret;
}

int wpa_driver_atmel_set_key(const char *ifname, wpa_alg alg,
			     unsigned char *addr, int key_idx,
			     int set_tx, u8 *seq, size_t seq_len,
			     u8 *key, size_t key_len)
{
	int ret = 0;
        struct atmel_param *param;
	u8 *buf;
        u8 alg_type;
        
	size_t blen;
	char *alg_name;

	switch (alg) {
	case WPA_ALG_NONE:
		alg_name = "none";
                alg_type = 0;
		break;
	case WPA_ALG_WEP:
		alg_name = "WEP";
		alg_type = 1;
                break;
	case WPA_ALG_TKIP:
		alg_name = "TKIP";
		alg_type = 2;
                break;
	case WPA_ALG_CCMP:
		alg_name = "CCMP";
		alg_type = 3;
                break;
	default:
		return -1;
	}

	if (seq_len > 8)
		return -2;

	blen = sizeof(*param) + key_len;
	buf = malloc(blen);
	if (buf == NULL)
		return -1;
	memset(buf, 0, blen);

	param = (struct atmel_param *) buf;
        
        param->cmd = SET_WPA_ENCRYPTION; 
        
        if (addr == NULL)
		memset(param->sta_addr, 0xff, ETH_ALEN);
	else
		memcpy(param->sta_addr, addr, ETH_ALEN);
        
        param->alg = alg_type;
        param->key_idx = key_idx;
        param->set_tx = set_tx;
        memcpy(param->seq, seq, seq_len);
        param->seq_len = seq_len;
        param->key_len = key_len;
	memcpy((u8 *)param->key, key, key_len);
	
        if (atmel_ioctl(ifname, param, blen)) {
		ret = -1;
	}
	free(buf);

	return ret;
}

struct wpa_driver_ops wpa_driver_atmel_ops = {
	.set_wpa = wpa_driver_atmel_set_wpa,
	.set_key = wpa_driver_atmel_set_key,
};

/* end atmel */

/* begin ndiswrapper */

#define WPA_SET_WPA 			SIOCIWFIRSTPRIV+1
#define WPA_SET_KEY 			SIOCIWFIRSTPRIV+2
#define WPA_SET_AUTH_ALG	 	SIOCIWFIRSTPRIV+8

struct wpa_key
{
	wpa_alg alg;
	u8 *addr;
	int key_index;
	int set_tx;
	u8 *seq;
	size_t seq_len;
	u8 *key;
	size_t key_len;
};

int wpa_ndiswrapper_set_ext(const char *ifname, int request, struct iwreq *pwrq)
{
  int s;
  int ret;

  s = socket( AF_INET, SOCK_DGRAM, 0);
  if (s < 0)
    return -1;

  strncpy(pwrq->ifr_name, ifname, IFNAMSIZ);
  ret = ioctl(s, request, pwrq);
  close(s);
  return ret;
}

int wpa_ndiswrapper_set_wpa(const char *ifname, int enabled)
{
  struct iwreq priv_req;
  int ret = 0;
  
  memset(&priv_req, 0, sizeof(priv_req));
  
  priv_req.u.data.flags = enabled;
  if (wpa_ndiswrapper_set_ext(ifname, WPA_SET_WPA, &priv_req) < 0)
    ret = -1;
  return ret;
}

int wpa_ndiswrapper_set_key(const char *ifname, wpa_alg alg, u8 *addr,
			    int key_idx, int set_tx, u8 *seq,
			    size_t seq_len, u8 *key, size_t key_len)
{
  struct wpa_key wpa_key;
  int ret = 0;
  struct iwreq priv_req;
  
  memset(&priv_req, 0, sizeof(priv_req));
  
  wpa_key.alg = alg;
  wpa_key.addr = addr;
  wpa_key.key_index = key_idx;
  wpa_key.set_tx = set_tx;
  wpa_key.seq = seq;
  wpa_key.seq_len = seq_len;
  wpa_key.key = key;
  wpa_key.key_len = key_len;
  
  priv_req.u.data.pointer = (void *)&wpa_key;
  
  if (wpa_ndiswrapper_set_ext(ifname, WPA_SET_KEY, &priv_req) < 0)
    ret = -1;
  return ret;
}

static int wpa_ndiswrapper_set_auth_alg(const char *ifname, int auth_alg)
{
  int ret = 0;
  struct iwreq priv_req;
  
  memset(&priv_req, 0, sizeof(priv_req));
  
  priv_req.u.param.value = auth_alg;
  if (wpa_ndiswrapper_set_ext(ifname, WPA_SET_AUTH_ALG, &priv_req) < 0)
    ret = -1;
  return ret;
}

struct wpa_driver_ops wpa_driver_ndiswrapper_ops = {
	.set_wpa = wpa_ndiswrapper_set_wpa,
	.set_key = wpa_ndiswrapper_set_key,
	.set_auth_alg = wpa_ndiswrapper_set_auth_alg,
};

/* end ndiswrapper */

/* begin wext */
#define IW_AUTH_WPA_ENABLED             7
#define IW_AUTH_INDEX           0x0FFF

struct wpa_driver_wext_data {
	void *ctx;
	int event_sock;
	int ioctl_sock;
	char ifname[IFNAMSIZ + 1];
};

static int wpa_driver_wext_set_wpa(const char *ifname, int enabled)
{
	struct iwreq iwr;
	int ret = 0;
	int s;
	
	memset(&iwr, 0,sizeof(iwr));
	strncpy(iwr.ifr_name, ifname, IFNAMSIZ);
	iwr.u.param.flags = IW_AUTH_WPA_ENABLED & IW_AUTH_INDEX;
	iwr.u.param.value = enabled;

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0) return -1;
	
	if ((ret=ioctl(s, SIOCSIWAUTH, &iwr)) < 0) {
		ret = -1;
	}
	close(s);
	return ret;
}

static int wpa_driver_wext_set_auth_alg(const char *ifname, int auth_alg)
{
	int algs = 0;
	struct iwreq iwr;
	int ret = 0;
	int s;
	
	if (auth_alg & AUTH_ALG_OPEN_SYSTEM)
		algs |= IW_AUTH_ALG_OPEN_SYSTEM;
	if (auth_alg & AUTH_ALG_SHARED_KEY)
		algs |= IW_AUTH_ALG_SHARED_KEY;
	if (auth_alg & AUTH_ALG_LEAP)
		algs |= IW_AUTH_ALG_LEAP;
	if (algs == 0) {
		 /* at least one algorithm should be set */
		algs = IW_AUTH_ALG_OPEN_SYSTEM;
	}
	
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, ifname, IFNAMSIZ);
	iwr.u.param.flags = IW_AUTH_80211_AUTH_ALG & IW_AUTH_INDEX;
	iwr.u.param.value = algs;

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0) return -1;

	if (ioctl(s, SIOCSIWAUTH, &iwr) < 0) {
		ret = -1;
	}
	close(s);
	return ret;
}

static int wpa_driver_wext_set_key(const char *ifname, wpa_alg alg, u8 *addr,
																	 int key_idx, int set_tx, u8 *seq, size_t seq_len,
                                   u8 *key, size_t key_len)
{
	struct iwreq iwr;
	struct iw_encode_ext *ext;
	int ret = 0;
	int s;

	if (seq_len > IW_ENCODE_SEQ_MAX_SIZE) {
		return -1;
	}

	ext = malloc(sizeof(*ext) + key_len);
	if (ext == NULL)
		return -1;
	memset(ext, 0, sizeof(*ext) + key_len);
	memset(&iwr, 0, sizeof(iwr));
	strncpy(iwr.ifr_name, ifname, IFNAMSIZ);
	iwr.u.encoding.flags = key_idx + 1;
	if (alg == WPA_ALG_NONE)
		iwr.u.encoding.flags |= IW_ENCODE_DISABLED;
	iwr.u.encoding.pointer = (caddr_t) ext;
	iwr.u.encoding.length = sizeof(*ext) + key_len;

	if (addr == NULL ||
			memcmp(addr, "\xff\xff\xff\xff\xff\xff", ETH_ALEN) == 0)
		ext->ext_flags |= IW_ENCODE_EXT_GROUP_KEY;
	if (set_tx)
		ext->ext_flags |= IW_ENCODE_EXT_SET_TX_KEY;
	
	 //ext->addr.sa_family = ARPHRD_ETHER;
	if (addr)
		memcpy(ext->addr.sa_data, addr, ETH_ALEN);
	else
		memset(ext->addr.sa_data, 0xff, ETH_ALEN);
	if (key && key_len) {
		memcpy(ext + 1, key, key_len);
		ext->key_len = key_len;
	}
	switch (alg) {
		case WPA_ALG_NONE:
			ext->alg = IW_ENCODE_ALG_NONE;
			break;
		case WPA_ALG_WEP:
			ext->alg = IW_ENCODE_ALG_WEP;
			break;
		case WPA_ALG_TKIP:
			ext->alg = IW_ENCODE_ALG_TKIP;
			break;
		case WPA_ALG_CCMP:
			ext->alg = IW_ENCODE_ALG_CCMP;
			break;
		default:
			free(ext);
			return -1;
	}

	if (seq && seq_len) {
		ext->ext_flags |= IW_ENCODE_EXT_RX_SEQ_VALID;
		memcpy(ext->rx_seq, seq, seq_len);
	}

	s = socket(PF_INET, SOCK_DGRAM, 0);
	if (s < 0) return -1;
	
	if (ioctl(s, SIOCSIWENCODEEXT, &iwr) < 0) {
		ret = -1;
	}
	close(s);
	free(ext);
	return ret;
}

struct wpa_driver_ops wpa_driver_wext_ops = {
  .set_wpa = wpa_driver_wext_set_wpa,
  .set_key = wpa_driver_wext_set_key,
  .set_auth_alg = wpa_driver_wext_set_auth_alg
};

/* end wext */

#endif		/* !defined(LIBHD_TINY) */

/** @} */


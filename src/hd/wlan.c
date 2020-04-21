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
  strncpy(pwrq->ifr_name, ifname, IFNAMSIZ - 1);
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

  if(!hd_probe_feature(hd_data, pr_wlan)) return;

  hd_data->module = mod_wlan;

  PROGRESS(1, 0, "detecting wlan features");

  if ((skfd = iw_sockets_open()) < 0) {
    ADD2LOG( "could not open socket, wlan feature query failed\n" );
    return;
  }

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      (
        hd_is_hw_class(hd, hw_network_ctrl) ||
        hd->base_class.id == bc_network
      ) &&
      hd->unix_dev_name
    ) {
      /* Get list of frequencies / channels */
      if(iw_get_range_info(skfd, hd->unix_dev_name, &range) < 0) {
        /* this failed, maybe device does not support wireless extensions */
        continue;
      }
      ADD2LOG("*** device %s is wireless ***\n", hd->unix_dev_name);
      hd->is.wlan = 1;

      hd->base_class.id = bc_network;
      hd->sub_class.id = 0x82;			/* wlan */

      res = new_mem(sizeof *res);
      res->any.type = res_wlan;

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
      /* if WEP is supported, we assume shared key auth support */
      if(range.num_encoding_sizes) {
        add_str_list(&res->wlan.auth_modes, "sharedkey");
      }

      if (range.enc_capa & (IW_ENC_CAPA_WPA | IW_ENC_CAPA_WPA2)) {
        add_str_list(&res->wlan.auth_modes, "wpa-psk");
        add_str_list(&res->wlan.auth_modes, "wpa-eap");
        if (range.enc_capa & IW_ENC_CAPA_CIPHER_TKIP)
          add_str_list(&res->wlan.enc_modes, "TKIP");
        if (range.enc_capa & IW_ENC_CAPA_CIPHER_CCMP)
          add_str_list(&res->wlan.enc_modes, "CCMP");
      }
      add_res_entry(&hd->res, res);
    }
  }
}
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "hd.h"
#include "hd_int.h"
#include "net.h"

static void read_net_ifs(hd_data_t *hd_data);
static void dump_net_data(hd_data_t *hd_data);

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * gather network interface info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


/*
 * This is independent of the other scans.
 */

void hd_scan_net(hd_data_t *hd_data)
{
  int found;
  unsigned u;
  hd_t *hd, *hd0;
  str_list_t *sl;

  if(!hd_probe_feature(hd_data, pr_net)) return;

  hd_data->module = mod_net;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->net = NULL;

  PROGRESS(1, 0, "get net-if data");

  read_net_ifs(hd_data);
  if((hd_data->debug & HD_DEB_NET)) dump_net_data(hd_data);

  PROGRESS(2, 0, "build list");

  for(sl = hd_data->net; sl; sl = sl->next) {
    found = 0;
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(
        hd->base_class == bc_network_interface &&
        hd->unix_dev_name &&
        !strcmp(hd->unix_dev_name, sl->str)
      ) {
        found = 1;
        break;
      }
    }

    if(!found) {
      hd = add_hd_entry(hd_data, __LINE__, 0);
      hd->base_class = bc_network_interface;
      hd->unix_dev_name = new_str(sl->str);

      if(!strcmp(sl->str, "lo")) {
        hd->sub_class = sc_nif_loopback;
      }
      else if(sscanf(sl->str, "eth%u", &u) == 1) {
        hd->sub_class = sc_nif_ethernet;
        hd->slot = u;
      }
      else if(sscanf(sl->str, "tr%u", &u) == 1) {
        hd->sub_class = sc_nif_tokenring;
        hd->slot = u;
      }
      else if(sscanf(sl->str, "fddi%u", &u) == 1) {
        hd->sub_class = sc_nif_fddi;
        hd->slot = u;
      }
      else if(sscanf(sl->str, "escon%u", &u) == 1) {
        hd->sub_class = sc_nif_escon;
        hd->slot = u;
      }
      /* ##### add more interface names here */
      else {
        hd->sub_class = sc_nif_other;
      }

      hd->bus = bus_none;

#if defined(__s390__)
// temporary hack for s390
      if(hd->sub_class != sc_nif_loopback) {
        hd0 = hd;
        hd = add_hd_entry(hd_data, __LINE__, 0);
        hd->base_class = bc_network;
        hd->unix_dev_name = new_str(hd0->unix_dev_name);
        hd->slot = hd0->slot;
        hd->vend = MAKE_ID(TAG_SPECIAL, 0x6001);	// IBM
        switch(hd0->sub_class) {
          case sc_nif_ethernet:
            hd->sub_class = 0;
            hd->dev = MAKE_ID(TAG_SPECIAL, 0x0000);
            str_printf(&hd->dev_name, 0, "Ethernet card %d", hd->slot);
            break;
          case sc_nif_tokenring:
            hd->sub_class = 1;
            hd->dev = MAKE_ID(TAG_SPECIAL, 0x0001);
            str_printf(&hd->dev_name, 0, "Token ring card %d", hd->slot);
            break;
          case sc_nif_escon:
            hd->sub_class = 0x70;
            hd->dev = MAKE_ID(TAG_SPECIAL, 0x0070);
            str_printf(&hd->dev_name, 0, "ESCON %d", hd->slot);
            break;
          default:
            hd->sub_class = 0x80;
            hd->dev = MAKE_ID(TAG_SPECIAL, 0x0080);
        }
      }
#endif

    }
  }
}


#if 0
  char buf1[256] = "eth0", *buf = buf1;
  struct stat sbuf;
  struct utsname ubuf;

  so = socket(PF_INET, SOCK_DGRAM, 0);

  if(so >= 0) {
    if((i = ioctl(so, SIOCGIFHWADDR, buf)) == 0) {
      len = 6;
      buf = buf1+18;
    }
    close(so);
  }
#endif



/*
 * Read the list of network interfaces. The info is taken from PROC_NET_IF_INFO.
 */
void read_net_ifs(hd_data_t *hd_data)
{
  char buf[16];
  str_list_t *sl, *sl0;

  if(!(sl0 = read_file(PROC_NET_IF_INFO, 2, 0))) return;

  for(sl = sl0; sl; sl = sl->next) {
    if(sscanf(sl->str, " %15[^:]:", buf) == 1) {
      add_str_list(&hd_data->net, buf);
    }
  }

  free_str_list(sl0);
}


/*
 * Add some network interface data to the global log.
 */
void dump_net_data(hd_data_t *hd_data)
{
  str_list_t *sl;

  ADD2LOG("-----  network interfaces -----\n");
  for(sl = hd_data->net; sl; sl = sl->next) {
    ADD2LOG("  %s\n", sl->str);
  }
  ADD2LOG("-----  network interfaces end -----\n");
}


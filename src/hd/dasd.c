#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <linux/hdreg.h>

#include "hd.h"
#include "hd_int.h"
#include "dasd.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * s390 disk info
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#if defined(__s390__) || defined(__s390x__)

void hd_scan_dasd(hd_data_t *hd_data)
{
  hd_t *hd;
  char c1,c2,c3;
  unsigned u0, u2, u3;
  str_list_t *sl, *sl0;
  hd_res_t *res;
  struct hd_geometry geo;
  int i, fd;
  char	string1[512];
  char  meldung[512];
  char  ro[32];
  char  dasdname[32];
  char  *s1;
  int	count1, count2;

  if(!hd_probe_feature(hd_data, pr_dasd)) return;

  hd_data->module = mod_dasd;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "read info");

  sl0 = read_file(PROC_DASD "/devices", 0, 0);

  i = 1;
  for(sl = sl0; sl; sl = sl->next) {
    /*
     *	The format of /proc/dasd/devices is changing.
     *  Old format:
     *   0150(ECKD) at ( 94:  0) is dasda:active at blocksize: 4096, 600840 blocks, 2347 MB
     *   01ab(ECKD) at ( 94:  4) is dasdb:active at blocksize: 4096, 600840 blocks, 2347 MB(ro)
     *   01ab(none) at ( 94:  8) is dasdc:unknown
     *   01ac(none) at ( 94: 12) is dasdd:n/f
     *   ...
     *   11aa(ECKD) at (<a>:<a>) is dasdba: active at blocksize: 4096, 600840 blocks, 2347 MB
     *   11ab(ECKD) at (<a>:<a>) is dasdbb: active at blocksize: 4096, 600840 blocks, 2347 MB(ro)
     *  New format:
     *   0150(ECKD) at ( 94:  0) is dasda      : active at blocksize: 4096, 600840 blocks, 2347 MB
     *   01ab(ECKD) at ( 94:  4) is dasdb  (ro): active at blocksize: 4096, 600840 blocks, 2347 MB
     *   01ab(none) at ( 94:  8) is dasdc  (ro): unknown
     *   01ac(none) at ( 94: 12) is dasdd      : n/f
     *   11aa(ECKD) at (<a>:<a>) is dasdba     : active at blocksize: 4096, 600840 blocks, 2347 MB
     *   11ab(ECKD) at (<a>:<a>) is dasdbb (ro): active at blocksize: 4096, 600840 blocks, 2347 MB
     *
     *	The problem is to recognise the (ro)-flag in the middle of the string. Strategy is to
     *	split the string at the second : and to parse it with two sscanfs.
     *	TODO - evaluate the result in ro
     */
    count1 = count2 = 0;
    strcpy(string1,sl->str);
    s1 = strchr(string1, ':');
    if (s1 == NULL) continue;
    s1++;
    s1 = strchr(s1, ':');
    if (s1 == NULL) continue;
    *s1 = '\0';
    s1++;
    if (*s1 == ' ') s1++; /* skip leading blank of new format */
    dasdname[1] = dasdname[2] = dasdname[3] = ro[0] = c1 = c2 = c3 = '\0';
    count1 = sscanf(string1, "%x%*s at (%*u:%*u) is dasd%c%c%c%s", &u0, &c1, &c2, &c3, ro);
    if (count1 < 2 && 5 < count1) continue;
    count2 = sscanf(s1, "active at blocksize: %u, %u blocks, %*s %s", &u2, &u3, ro);
    if (count2 != 3) continue;
    dasdname[0] = c1;
    if ( 'a' <= c2 && c2 <= 'z') dasdname[1] = c2;
    if ( 'a' <= c3 && c3 <= 'z') dasdname[2] = c3;
    sprintf(meldung, "dasd device %s, count1 %d, count2 %d\n",sl->str,count1,count2);
    ADD2LOG(meldung);
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_storage_device;
    hd->bus.id = bus_vio;
    hd->slot = (u0 >> 8) & 0xff;
    hd->func = u0 & 0xff;

    hd->sub_class.id = sc_sdev_disk;

    hd->device.name = new_str("S390 Disk");
    str_printf(&hd->unix_dev_name, 0, "/dev/dasd%s", dasdname);
    str_printf(&hd->rom_id, 0, "%04X", u0);

    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->size.type = res_size;
    res->size.unit = size_unit_sectors;
    res->size.val1 = u3;
    res->size.val2 = u2;

    fd = open(hd->unix_dev_name, O_RDONLY | O_NONBLOCK);
    if(fd >= 0) {
      PROGRESS(2, i++, "ioctl");
      if(!ioctl(fd, HDIO_GETGEO, &geo)) {
	ADD2LOG("dasd ioctl(geo) ok\n");
	res = add_res_entry(&hd->res, new_mem(sizeof *res));
	res->disk_geo.type = res_disk_geo;
	res->disk_geo.cyls = geo.cylinders;
	res->disk_geo.heads = geo.heads;
	res->disk_geo.sectors = geo.sectors;
	res->disk_geo.logical = 1;
      }
      close(fd);
    }
  }
  free_str_list(sl0);

  if(i > 1) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_storage;
    hd->sub_class.id = sc_sto_other;
    hd->vendor.name = new_str("IBM");
    hd->device.name = new_str("VIO DASD");
  }
}

#endif	/* defined(__s390__) || defined(__s390x__) */


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
  unsigned int channel, size;
  hd_res_t *res;
  struct hd_geometry geo;
  int i, fd;
  char	string1[512];
  char  dasdname[32];
  int old_entry_found=0;

  struct sysfs_class* block_dev_class;
  struct dlist* block_devs;
  struct sysfs_class_device* bd;
  struct sysfs_attribute* attr;

  if(!hd_probe_feature(hd_data, pr_dasd)) return;

  hd_data->module = mod_dasd;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "read info");

  block_dev_class=sysfs_open_class("block");
  if(!block_dev_class)
  {
    ADD2LOG("failed to get sysfs class block");
    return;
  }

  block_devs=sysfs_get_class_devices(block_dev_class);
  if(!block_devs)
  {
    ADD2LOG("failed to get devices of class block");
    return;
  }


  i = 1;
  dlist_for_each_data(block_devs,bd,struct sysfs_class_device)
  {
    /* check if there is a physical device behind this */
    strcpy(string1,bd->path);
    strcat(string1,"/device");
    if(readlink(string1,string1,512)==-1)
      continue;

    /* we are only interested in DASDs */
    if(!strstr(bd->name,"dasd"))
       continue;

    strcpy(dasdname,bd->name);
    channel=strtol(rindex(string1,'.')+1,NULL,16);
    //fprintf(stderr,"s1 %s channel %d\n",string1,channel);

    attr=sysfs_get_classdev_attr(bd,"size");
    if(attr)
      size=strtol(attr->value,NULL,10);
    else
      size=0;

    /* check if s390.c has already found the device */

    old_entry_found=0;
    for(hd=hd_data->hd;hd;hd=hd->next)
    {
	//fprintf(stderr,"bcid %d\n",hd->base_class.id);
    	if(hd->base_class.id == bc_storage_device)
    	{
    		res=hd->res;
    		for(res=hd->res;res;res=res->next)
    		{
    			if(res->io.type==res_io && res->io.base==channel)
    			{
    				old_entry_found=1;
    				goto out;
    			}
    		}
    	}
    }
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_storage_device;

/* a DASD is not necessarily virtual */
#if 0
    hd->bus.id = bus_vio;
#endif

out:
    
    if(!old_entry_found)
    {
	    res = add_res_entry(&hd->res, new_mem(sizeof *res));
	    res->io.type=res_io;
	    res->io.base=channel;
	    res->io.range=1;
	    res->io.enabled=1;
	    res->io.access=acc_rw;
    }

    hd->sub_class.id = sc_sdev_disk;

    hd->device.name = new_str("S390 Disk");
    str_printf(&hd->unix_dev_name, 0, "/dev/%s", dasdname);

    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->size.type = res_size;
    res->size.unit = size_unit_sectors;
    res->size.val1 = size;	// blocks
    res->size.val2 = 512;	// block size, not sure if this is always correct for sysfs

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

  sysfs_close_class(block_dev_class);

  if(i > 1) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_storage;
    hd->sub_class.id = sc_sto_other;
    hd->vendor.name = new_str("IBM");
    hd->device.name = new_str("DASD");
  }
}

#endif	/* defined(__s390__) || defined(__s390x__) */


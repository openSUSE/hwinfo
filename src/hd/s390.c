#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "s390.h"

#if defined(__s390__) || defined(__s390x__)

#include <sysfs/libsysfs.h>
#include <sysfs/dlist.h>

#define BUSNAME "ccw"

static void hd_scan_s390_ex(hd_data_t *hd_data, int disks_only)
{
  hd_t* hd;
  hd_res_t* res;
  struct sysfs_bus *bus;
  struct sysfs_device *curdev = NULL;
  struct dlist *attributes = NULL;
  struct sysfs_attribute *curattr = NULL;
  struct dlist *devlist = NULL;

  unsigned int devtype=0,devmod=0,cutype=0,cumod=0;

  /* list of each channel's cutype, used for finding multichannel devices */
  int cutypes[1<<16]={0};
  int i;

  hd_data->module=mod_s390;

  remove_hd_entries(hd_data);

  //sl0=sl=read_file(PROCSUBCHANNELS, 2, 0);
  bus=sysfs_open_bus(BUSNAME);

  if (!bus)
  {
    ADD2LOG("unable to open" BUSNAME "bus");
    return;
  }

  devlist=sysfs_get_bus_devices(bus);
  
  if(!devlist)
  {
  	ADD2LOG("unable to get devices on bus " BUSNAME);
  	return;
  }
  
  /* build cutypes list */
  dlist_for_each_data(devlist, curdev, struct sysfs_device)
  {
    int channel=strtol(rindex(curdev->bus_id,'.')+1,NULL,16);
    attributes = sysfs_get_device_attributes(curdev);
    dlist_for_each_data(attributes,curattr,struct sysfs_attribute)
    {
      if(strcmp("cutype",curattr->name)==0)
	cutype=strtol(curattr->value,NULL,16);
    }
    cutypes[channel]=cutype;
  }
  /* check for each channel if it must be skipped and identify virtual reader/punch */
  for(i=0;i<(1<<16);i++)
  {
    if(/* cutypes[i]==0x1731 || */ cutypes[i]==0x3088)	/* It seems that QDIO devices only appear once */
    {
      cutypes[i+1]=-1;	/* skip */
      if(cutypes[i]==0x1731)
	cutypes[i+2]=-1;
    }
    if(cutypes[i]==0x2540)
    {
      cutypes[i]=-2;	/* reader */
      cutypes[i+1]=-3;	/* punch */
    }
  }
  
  dlist_for_each_data(devlist, curdev, struct sysfs_device)
  {

    res=new_mem(sizeof *res);

    attributes = sysfs_get_device_attributes(curdev);
    dlist_for_each_data(attributes,curattr, struct sysfs_attribute)
    {
      if (strcmp("online",curattr->name)==0)
	res->io.enabled=atoi(curattr->value);
      else if (strcmp("cutype",curattr->name)==0)
      {
	cutype=strtol(curattr->value,NULL,16);
	cumod=strtol(index(curattr->value,'/')+1,NULL,16);
      } else if (strcmp("devtype",curattr->name)==0)
      {
	devtype=strtol(curattr->value,NULL,16);
	devmod=strtol(index(curattr->value,'/')+1,NULL,16);
      }
    }

    res->io.type=res_io;
    res->io.access=acc_rw;  /* fix-up RO/WO devices in IDs file */
    res->io.base=strtol(rindex(curdev->bus_id,'.')+1,NULL,16);

    /* Skip additional channels for multi-channel devices */
    if(cutypes[res->io.base] == -1)
      continue;

    if(disks_only && cutype!=0x3990 &&
       (cutype != 0x1731 || devtype != 0x1732 || cumod != 3))
      continue;

    res->io.range=1;
    switch (cutype)
    {
      /* three channels */
      case 0x1731:    /* QDIO (QETH, HSI, zFCP) */
	res->io.range++;
      /* two channels */
      case 0x3088:    /* CU3088 (CTC, LCS) */
	res->io.range++;
    }

    hd=add_hd_entry(hd_data,__LINE__,0);
    add_res_entry(&hd->res,res);
    hd->vendor.id=MAKE_ID(TAG_SPECIAL,0x6001); /* IBM */
    hd->device.id=MAKE_ID(TAG_SPECIAL,cutype);
    hd->sub_device.id=MAKE_ID(TAG_SPECIAL,devtype);
    
    if(cutypes[res->io.base]==-2)	/* virtual reader */
    {
      hd->base_class.id=bc_scanner;
    }
    if(cutypes[res->io.base]==-3)	/* virtual punch */
    {
      hd->base_class.id=bc_printer;
    }
    /* all other device data (names, classes etc.) comes from the s390 ID file */

    hd->detail=free_hd_detail(hd->detail);
    hd->detail=new_mem(sizeof *hd->detail);
    hd->detail->ccw.type=hd_detail_ccw;
    hd->detail->ccw.data=new_mem(sizeof(ccw_t));
    hd->detail->ccw.data->cu_model=cumod;
    hd->detail->ccw.data->dev_model=devmod;
    hd->detail->ccw.data->lcss=(strtol(curdev->bus_id,0,16) << 8) + strtol(curdev->bus_id+2,0,16);
    hddb_add_info(hd_data,hd);
  }

}

void hd_scan_s390(hd_data_t *hd_data)
{
  if (!hd_probe_feature(hd_data, pr_s390)) return;
  hd_scan_s390_ex(hd_data, 0);
}

void hd_scan_s390disks(hd_data_t *hd_data)
{
  if (!hd_probe_feature(hd_data, pr_s390disks)) return;
  hd_scan_s390_ex(hd_data, 1);
}

#endif


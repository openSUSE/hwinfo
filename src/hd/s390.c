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
#define BUSNAME_GROUP "ccwgroup"
#define BUSNAME_IUCV "iucv"

static void hd_scan_s390_ex(hd_data_t *hd_data, int disks_only)
{
  hd_t* hd;
  hd_res_t* res;
  struct sysfs_bus *bus;
  struct sysfs_bus *bus_group;
  struct sysfs_device *curdev = NULL;
  struct dlist *attributes = NULL;
  struct sysfs_attribute *curattr = NULL;
  struct dlist *devlist = NULL;
  struct dlist *devlist_group = NULL;
  int virtual_machine=0;

  unsigned int devtype=0,devmod=0,cutype=0,cumod=0;

  /* list of each channel's cutype, used for finding multichannel devices */
  int cutypes[1<<16]={0};
  int i;

  hd_data->module=mod_s390;

  remove_hd_entries(hd_data);

  bus=sysfs_open_bus(BUSNAME);
  bus_group=sysfs_open_bus(BUSNAME_GROUP);

  if (!bus)
  {
    ADD2LOG("unable to open" BUSNAME "bus");
    return;
  }

  devlist=sysfs_get_bus_devices(bus);
  if(bus_group) devlist_group=sysfs_get_bus_devices(bus_group);
  
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
    if(!attributes) continue;
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
    if(cutypes[i]==0x3088)	/* It seems that QDIO devices only appear once */
      cutypes[i+1]*=-1;	/* negative cutype -> skip */

    if(cutypes[i]==0x2540)
    {
      virtual_machine=1;	/* we are running in VM */
      cutypes[i]=-2;	/* reader */
      if(i < (1<<16)-1 && cutypes[i+1] == 0x2540)
        cutypes[i+1]=-3;	/* punch */
    }
  }
  
  /* identify grouped channels */
  if(devlist_group) dlist_for_each_data(devlist_group, curdev, struct sysfs_device)
  {
    struct sysfs_directory* d;
    struct dlist* dl;
    struct sysfs_link* cl;
    //printf("ccwg %s\n",curdev->path);
    d=sysfs_open_directory(curdev->path);
    dl=sysfs_get_dir_links(d);
    dlist_for_each_data(dl,cl,struct sysfs_link)	/* iterate over this channel group */
    {
        if(!rindex(cl->target,'.')) continue;
	int channel=strtol(rindex(cl->target,'.')+1,NULL,16);
    	//printf("channel %x name %s target %s\n",channel,cl->name,cl->target);
    	if(strncmp("cdev",cl->name,4)==0)
    	{
    		if(cl->name[4]=='0')	/* first channel in group gets an entry */
    		{
    			if(cutypes[channel]<0) cutypes[channel]*=-1;	/* make sure its positive */
    		}
    		else			/* other channels in group are skipped */
    			if(cutypes[channel]>0) cutypes[channel]*=-1;	/* make sure its negative */
    	}
    		
    }
  }
  
  dlist_for_each_data(devlist, curdev, struct sysfs_device)
  {
    int readonly=0;
    res=new_mem(sizeof *res);

    attributes = sysfs_get_device_attributes(curdev);
    if(!attributes) continue;
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
      } else if (strcmp("readonly",curattr->name)==0)
      {
        readonly=atoi(curattr->value);
      }
    }
    
    res->io.type=res_io;
    res->io.access=readonly?acc_ro:acc_rw;
    res->io.base=strtol(rindex(curdev->bus_id,'.')+1,NULL,16);

    /* Skip additional channels for multi-channel devices */
    if(cutypes[res->io.base] < -3)
      continue;

    if(disks_only && cutype!=0x3990 && cutype!=0x2105 && cutype!=0x3880 && cutype!=0x9343 && cutype!=0x6310 &&
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
    hd->bus.id=bus_ccw;
    hd->sysfs_device_link = new_str(hd_sysfs_id(curdev->path));
    hd->sysfs_id = new_str(hd->sysfs_device_link);
    hd->sysfs_bus_id = new_str(strrchr(curdev->path,'/')+1);
    
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

  if(virtual_machine)
  {
  	/* add an unactivated IUCV device */
  	hd=add_hd_entry(hd_data,__LINE__,0);
  	hd->vendor.id=MAKE_ID(TAG_SPECIAL,0x6001); /* IBM */
  	hd->device.id=MAKE_ID(TAG_SPECIAL,0x0005); /* IUCV */
  	hd->bus.id=bus_iucv;
  	hd->base_class.id=bc_network;
  	hd->status.active=status_no;
  	hd->status.available=status_yes;
  	hddb_add_info(hd_data,hd);
  	
  	/* add activated IUCV devices */
	bus=sysfs_open_bus(BUSNAME_IUCV);
	if(bus)
	{
	  devlist=sysfs_get_bus_devices(bus);
	  if(devlist)
	  {
	    dlist_for_each_data(devlist, curdev, struct sysfs_device)
	    {
	      hd=add_hd_entry(hd_data,__LINE__,0);
	      hd->vendor.id=MAKE_ID(TAG_SPECIAL,0x6001); /* IBM */
	      hd->device.id=MAKE_ID(TAG_SPECIAL,0x0005); /* IUCV */
	      hd->bus.id=bus_iucv;
	      hd->base_class.id=bc_network;
	      hd->status.active=status_yes;
	      hd->status.available=status_yes;
	      attributes = sysfs_get_device_attributes(curdev);
	      dlist_for_each_data(attributes,curattr,struct sysfs_attribute)
	      {
	      	if(strcmp("user",curattr->name)==0)
	      	  hd->rom_id=new_str(curattr->value);
  	      }
  	      hd->sysfs_device_link = new_str(hd_sysfs_id(curdev->path));
              hd->sysfs_id = new_str(hd->sysfs_device_link);
  	      hd->sysfs_bus_id = new_str(strrchr(curdev->path,'/')+1);
  	      hddb_add_info(hd_data,hd);
  	    }
  	  }
  	}
  	              
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


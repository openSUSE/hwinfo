#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "s390.h"

/**
 * @defgroup S390int S390 information
 * @ingroup libhdINFOint
 * @brief S390 devices and information functions
 *
 * @{
 */

#if defined(__s390__) || defined(__s390x__)

#include <dirent.h>
#include <unistd.h>

#define BUSNAME "ccw"
#define BUSNAME_GROUP "ccwgroup"
#define BUSNAME_IUCV "iucv"

static void hd_scan_s390_ex(hd_data_t *hd_data, int disks_only)
{
  hd_t* hd;
  hd_res_t* res;
  DIR *bus;
  DIR *bus_group;
  struct dirent *curdev = NULL;
  char attrname[128];
  char dirname[128];
  char linkname[128];
  int virtual_machine=0;

  unsigned int devtype=0,devmod=0,cutype=0,cumod=0;

  /* list of each channel's cutype, used for finding multichannel devices */
  /* FIXME: may fail with channel subsystems > 0.0 */
  int cutypes[1<<16]={0};
  int i;

  hd_data->module=mod_s390;

  remove_hd_entries(hd_data);

  bus = opendir("/sys/bus/" BUSNAME "/devices");
  bus_group = opendir("/sys/bus/" BUSNAME_GROUP "/devices");

  if (!bus)
  {
    ADD2LOG("unable to open" BUSNAME "bus");
    return;
  }

  /* build cutypes list */
  while((curdev = readdir(bus)))
  {
    if(curdev->d_type == DT_DIR) continue;	// skip "." and ".."
    int channel=strtol(rindex(curdev->d_name,'.')+1,NULL,16);
    cutypes[channel] = strtol(get_sysfs_attr(BUSNAME, curdev->d_name, "cutype"), NULL, 16);
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
  if(bus_group) while((curdev = readdir(bus_group)))
  {
    DIR* d;
    struct dlist* dl;
    struct dirent* cl;
    
    if(curdev->d_type == DT_DIR) continue;	// skip "." and ".."
    
    sprintf(dirname,"%s/%s","/sys/bus/" BUSNAME_GROUP "/devices/", curdev->d_name);
    d = opendir(dirname);
    
    while ((cl = readdir(d)))
    {
        if(cl->d_type != DT_LNK) continue;	// skip everything except symlinks
        
        sprintf(linkname, "%s/%s", dirname, cl->d_name);
        memset(attrname,0,128);
        if(readlink(linkname, attrname, 127) == -1) continue;
        
        if(!rindex(attrname,'.')) continue;	// no dot? should not happen...
        
	int channel=strtol(rindex(attrname,'.')+1,NULL,16);
    	if(strncmp("cdev",cl->d_name,4)==0)
    	{
    		if(cl->d_name[4]=='0')	/* first channel in group gets an entry */
    		{
    			if(cutypes[channel]<0) cutypes[channel]*=-1;	/* make sure its positive */
    		}
    		else			/* other channels in group are skipped */
    			if(cutypes[channel]>0) cutypes[channel]*=-1;	/* make sure its negative */
    	}
    		
    }
    closedir(d);
    
  }
  
  rewinddir(bus);
  while((curdev = readdir(bus)))
  {
    int readonly=0;
    
    if(curdev->d_type == DT_DIR) continue;	// skip "." and ".."
    
    res=new_mem(sizeof *res);

    res->io.enabled = atoi(get_sysfs_attr(BUSNAME, curdev->d_name, "online"));
    cutype = strtol(get_sysfs_attr(BUSNAME, curdev->d_name, "cutype"), NULL, 16);
    cumod = strtol(index(get_sysfs_attr(BUSNAME, curdev->d_name, "cutype"), '/') + 1, NULL, 16);
    devtype = strtol(get_sysfs_attr(BUSNAME, curdev->d_name, "devtype"), NULL, 16);
    devmod = strtol(index(get_sysfs_attr(BUSNAME, curdev->d_name, "devtype"), '/') + 1, NULL, 16);
    readonly = atoi(get_sysfs_attr(BUSNAME, curdev->d_name, "readonly")?:"0");
    
    sprintf(attrname, "/sys/bus/" BUSNAME "/devices/%s", curdev->d_name);
    
    res->io.type=res_io;
    res->io.access=readonly?acc_ro:acc_rw;
    res->io.base=strtol(rindex(curdev->d_name,'.')+1,NULL,16);

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
    
    /* whether resolving the symlink makes sense or not is debatable, but libsysfs did it, so it
       is consistent with earlier versions of this code */
    memset(linkname,0,128);
    if(readlink(attrname,linkname,127) == -1)
      hd->sysfs_device_link = new_str(hd_sysfs_id(attrname));
    else
      hd->sysfs_device_link = new_str(hd_sysfs_id(linkname+6));
    
    hd->sysfs_id = new_str(hd->sysfs_device_link);
    hd->sysfs_bus_id = new_str(strrchr(attrname,'/')+1);
    
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
    hd->detail->ccw.data->lcss=(strtol(curdev->d_name,0,16) << 8) + strtol(curdev->d_name+2,0,16);
    hddb_add_info(hd_data,hd);
  }
  closedir(bus);
  closedir(bus_group);

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
	bus = opendir("/sys/bus/" BUSNAME_IUCV "/devices");
	
	if(bus)
	{
          while((curdev = readdir(bus)))
          {
            if(curdev->d_type == DT_DIR) continue;	// skip "." and ".."
            hd=add_hd_entry(hd_data,__LINE__,0);
            hd->vendor.id=MAKE_ID(TAG_SPECIAL,0x6001); /* IBM */
            hd->device.id=MAKE_ID(TAG_SPECIAL,0x0005); /* IUCV */
            hd->bus.id=bus_iucv;
            hd->base_class.id=bc_network;
            hd->status.active=status_yes;
            hd->status.available=status_yes;
            hd->rom_id = new_str(get_sysfs_attr(BUSNAME_IUCV, curdev->d_name, "user"));

            sprintf(attrname, "/sys/bus/" BUSNAME_IUCV "/devices/%s", curdev->d_name);
            hd->sysfs_device_link = new_str(hd_sysfs_id(attrname));
            hd->sysfs_id = new_str(hd->sysfs_device_link);
            hd->sysfs_bus_id = new_str(strrchr(attrname,'/')+1);
            hddb_add_info(hd_data,hd);
          }
  	  closedir(bus);
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

/** @} */


#include <stdio.h>
#include <stdlib.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "s390.h"

#if defined(__s390__) || defined(__s390x__)

#define PROCSUBCHANNELS "/proc/subchannels"

void hd_scan_s390(hd_data_t *hd_data)
{
	hd_t* hd;
	hd_res_t* res;
	char line[256];
	str_list_t* sl,*sl0;
	int i;
	
	unsigned int chid,schid,devtype,devmod,cutype,cumod,inuse,pim,pam,pom,chpid1,chpid2;
	
	if(!hd_probe_feature(hd_data, pr_s390)) return;
	
	hd_data->module=mod_s390;
	
	remove_hd_entries(hd_data);
	
	sl0=sl=read_file("/proc/subchannels", 2, 0);

	if(!sl)
	{
	    ADD2LOG("unable to open" PROCSUBCHANNELS);
	    return;
	}

	for(;sl;sl=sl->next)
	{
		/* segment line */
		sl->str[4]=sl->str[11]=sl->str[17]=sl->str[20]=sl->str[27]=sl->str[30]=sl->str[35]=sl->str[42]=sl->str[46]=sl->str[50]=sl->str[60]=sl->str[69]=0;
		
		chid=strtol(sl->str,NULL,16);
		schid=strtol(sl->str+7,NULL,16);
		devtype=strtol(sl->str+13,NULL,16);
		devmod=strtol(sl->str+18,NULL,16);
		cutype=strtol(sl->str+23,NULL,16);
		cumod=strtol(sl->str+28,NULL,16);
		inuse=sl->str[32]=='y'?1:0;
		pim=strtol(sl->str+40,NULL,16);
		pam=strtol(sl->str+44,NULL,16);
		pom=strtol(sl->str+48,NULL,16);
		chpid1=strtol(sl->str+52,NULL,16);
		chpid2=strtol(sl->str+61,NULL,16);
		
		hd=add_hd_entry(hd_data,__LINE__,0);
		
		res=new_mem(sizeof *res);
		res->io.type=res_io;
		res->io.enabled=1;
		res->io.base=chid;
		res->io.range=1;
		res->io.access=acc_rw;
		switch(cutype)
		{
			case 0x1731:	/* QETH */
				res->io.range++;
			case 0x3088:	/* CTC */
			case 0x2540:	/* ??? */
				res->io.range++;
		}
		for(i=0; i<res->io.range-1;i++)
		    if(sl) sl=sl->next;

		add_res_entry(&hd->res,res);
		
		hd->vendor.id=MAKE_ID(TAG_SPECIAL, 0x6001); /* IBM */
		hd->device.id=MAKE_ID(TAG_SPECIAL,cutype);
		hd->sub_device.id=MAKE_ID(TAG_SPECIAL,devtype);
		hd->revision.id=MAKE_ID(TAG_SPECIAL,(devmod<<8)+cumod);
		sprintf(line,"0x%x 0x%x",chpid1,chpid2);
		hd->rom_id=new_str(line);
		hddb_add_info(hd_data,hd);
	}
	free_str_list(sl0);
}

#endif


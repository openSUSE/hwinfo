#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include "ihw.h"

#ifndef IHW_DATABASE
#define  IHW_DATABASE "/usr/lib/YaST/ihw_database"
#endif

#define debprintf(a...)

/* private data */

static int ihw_init=0;
static int ihw_max_handle;
static int max_para_rec;
static int para_list_cnt;
static int max_name_len;
static int max_para_name;
static int max_para_name_len;


struct cibas {
	ihw_card_info	ici;
	int		pcnt;
	int		pidx;
};

typedef struct cibas cibase;

static cibase	*isdncard_info;
static ihw_para_info *parameter_info;
static unsigned long *paralist;
static char	*namesdb;
static char     *para_name;

static int	*isdncard_txt;
static int	*isdncard_type;
static int	*isdncard_id;


typedef int (*fcmp) (const void *, const void *);

int compare_name(const int *c1, const int *c2) {
	return(strcasecmp(isdncard_info[*c1].ici.name, isdncard_info[*c2].ici.name));
}

int compare_type(const int *c1, const int *c2) {
	int x= isdncard_info[*c1].ici.type - isdncard_info[*c2].ici.type;

	if (!x)
		x=isdncard_info[*c1].ici.subtype - isdncard_info[*c2].ici.subtype;
	return(x);
}

int compare_id(const int *c1, const int *c2) {
	int x= isdncard_info[*c1].ici.vendor - isdncard_info[*c2].ici.vendor;

	if (!x)
		x=isdncard_info[*c1].ici.device - isdncard_info[*c2].ici.device;
	if (!x)
		x=isdncard_info[*c1].ici.subvendor - isdncard_info[*c2].ici.subvendor;
	if (!x)
		x=isdncard_info[*c1].ici.subdevice - isdncard_info[*c2].ici.subdevice;
	return(x);
}

int init_ihw() {
	int i,j;
	FILE *file;
	int rec_count, version, dummy;
	int v0,v1,v2,v3;
	char *np;
	unsigned long *pl;

	if (!(file=fopen(IHW_DATABASE, "rb")))
		return(1);
	v0 = fgetc(file);
	v1 = fgetc(file);
	v2 = fgetc(file);
	v3 = fgetc(file);
	version = v0 + 256*(v1 + 256*(v2 + 256*v3));
	if (version != IHW_VERSION) {
		fclose(file);
		return(2);
	} else {
		fscanf(file,"%08x", &rec_count);
		fscanf(file,"%08x", &max_name_len);
		fscanf(file,"%08x", &max_para_rec);
		fscanf(file,"%08x", &para_list_cnt);
		fscanf(file,"%08x", &max_para_name);
		fscanf(file,"%08x", &max_para_name_len);
	/* dummy entries for extentions total 16 int */
		fscanf(file,"%08x", &dummy);
		fscanf(file,"%08x", &dummy);
		fscanf(file,"%08x", &dummy);
		fscanf(file,"%08x", &dummy);
		fscanf(file,"%08x", &dummy);
		fscanf(file,"%08x", &dummy);
		fscanf(file,"%08x", &dummy);
		fscanf(file,"%08x", &dummy);
		fscanf(file,"%08x", &dummy);
		v0 = fgetc(file);
		if (v0 != '\n') {
			fclose(file);
			return(2);
		}
	}
	if (rec_count<1) {
		fclose(file);
		return(11);
	}
	if(!(isdncard_txt = (int *) malloc(rec_count*sizeof(int)))) {
		fclose(file);
		return(3);
	}
	if(!(isdncard_type = (int *) malloc(rec_count*sizeof(int)))) {
		fclose(file);
		return(4);
	}
	if(!(isdncard_id = (int *) malloc(rec_count*sizeof(int)))) {
		fclose(file);
		return(5);
	}
	if(!(isdncard_info = (cibase *) malloc(rec_count*sizeof(cibase)))) {
		fclose(file);
		return(6);
	}
	if (!(namesdb = malloc(max_name_len*rec_count))) {
		fclose(file);
		return(7);
	}
	if (!(paralist = (unsigned long *)malloc(para_list_cnt*sizeof(unsigned long)))) {
		fclose(file);
		return(8);
	}
	if (!(parameter_info = (ihw_para_info *)malloc(max_para_rec*sizeof(ihw_para_info)))) {
		fclose(file);
		return(9);
	}
	if (!(para_name = (char *)malloc(max_para_name_len*max_para_name))) {
		fclose(file);
		return(9);
	}
	fread(namesdb, max_name_len, rec_count, file);
	for (i=0; i< rec_count; i++) {
		fscanf(file,"%04x",&isdncard_info[i].pcnt);
		fscanf(file,"%08x",&isdncard_info[i].pidx);
		fscanf(file,"%04x",&isdncard_info[i].ici.handle);
		fscanf(file,"%04x",&isdncard_info[i].ici.type);
		fscanf(file,"%04x",&isdncard_info[i].ici.subtype);
		fscanf(file,"%04x",&isdncard_info[i].ici.Class);
		fscanf(file,"%08x",&isdncard_info[i].ici.vendor);
		fscanf(file,"%08x",&isdncard_info[i].ici.device);
		fscanf(file,"%08x",&isdncard_info[i].ici.subvendor);
		fscanf(file,"%08x",&isdncard_info[i].ici.subdevice);
		fscanf(file,"%08x",&isdncard_txt[i]);
		fscanf(file,"%08x",&isdncard_type[i]);
		fscanf(file,"%08x",&isdncard_id[i]);
		v0 = fgetc(file);
		if (v0 != '\n') {
			fclose(file);
			return(2);
		}
	}
	for (i=0; i< max_para_rec; i++) {
		fscanf(file,"%04x",&parameter_info[i].handle);
		fscanf(file,"%08x",&parameter_info[i].type);
		fscanf(file,"%08x",&parameter_info[i].flags);
		fscanf(file,"%016lx",&parameter_info[i].def_value);
		fscanf(file,"%016lx",&parameter_info[i].bytecnt);
		fscanf(file,"%08x", &v0);
		if (v0)
			parameter_info[i].list = (unsigned long *)1;
		else
			parameter_info[i].list = NULL;
		v0 = fgetc(file);
		if (v0 != '\n') {
			fclose(file);
			return(2);
		}
	}
	for (i=0; i< para_list_cnt; i++) {
		fscanf(file,"%016lx",&paralist[i]);
	}
	v0 = fgetc(file);
	if (v0 != '\n') {
		fclose(file);
		return(2);
	}
	fread(para_name, max_para_name_len, max_para_name, file);
	fclose(file);
	ihw_max_handle = rec_count;
	np = namesdb;
	pl = paralist;
	for (i=0; i<rec_count; i++) {
		isdncard_info[i].ici.name = np;
		np += max_name_len;
		for (j=0; j<isdncard_info[i].pcnt; j++) {
			parameter_info[isdncard_info[i].pidx + j].name =
				para_name + max_para_name_len*
				parameter_info[isdncard_info[i].pidx + j].type;
			if (parameter_info[isdncard_info[i].pidx + j].list) {
				parameter_info[isdncard_info[i].pidx + j].list = pl;
				pl += *pl;
				pl++;
			}
		}
	}
	ihw_max_handle--;
	ihw_init = 1;
	return(0);
}

/* interface */

ihw_card_info   *ihw_get_device(ihw_card_info   *ici) {
	
	if(!ihw_init)
		if (init_ihw())
			return(NULL);
	if(!ici)
		return(NULL);
	if(ici->handle < 0)
		ici->handle = 0;
	else if (ici->handle < ihw_max_handle)
		ici->handle++;
	if (ici->handle >= ihw_max_handle)
		return(NULL);
	memcpy(ici, &isdncard_info[isdncard_txt[ici->handle]].ici, sizeof(ihw_card_info)); 
	return(ici);
}

ihw_card_info   *ihw_get_device_from_type(ihw_card_info   *ici) {

	int key,*ret;
	
	debprintf("ihw_get_device_from_type ici %#x\n", (unsigned long)ici);
	
	if(!ihw_init)
		if (init_ihw())
			return(NULL);
	if(!ici)
		return(NULL);
	key = ihw_max_handle;
	isdncard_info[key].ici = *ici;
	if (!(ret=bsearch(&key, isdncard_type, ihw_max_handle, sizeof(int), (fcmp)compare_type))) {
		debprintf("ret NULL\n");
		return(NULL);
	}
	debprintf("ret idx %d\n", *ret);
	if (*ret <0)
		return(NULL);
	if (*ret >= ihw_max_handle)
		return(NULL);
	memcpy(ici,&isdncard_info[*ret].ici, sizeof(ihw_card_info)); 
	return(ici);
}

ihw_card_info   *ihw_get_device_from_id(ihw_card_info   *ici) {

	int key,*ret;
	
	debprintf("ihw_get_device_from_id ici %#x\n", (unsigned long)ici);
	
	if(!ihw_init)
		if (init_ihw())
			return(NULL);
	if(!ici)
		return(NULL);
	key = ihw_max_handle;
	isdncard_info[key].ici = *ici;
	if (!(ret=bsearch(&key, isdncard_id, ihw_max_handle, sizeof(int), (fcmp)compare_id))) {
		debprintf("bs1 ret NULL\n");
		key = ihw_max_handle;
		isdncard_info[key].ici.subvendor = PCI_ANY_ID;
		isdncard_info[key].ici.subdevice = PCI_ANY_ID;
		if (!(ret=bsearch(&key, isdncard_id, ihw_max_handle, sizeof(int), (fcmp)compare_id))) {
			debprintf("bs2 ret NULL\n");
			return(NULL);
		}
	}
	debprintf("ret idx %d\n", *ret);
	if (*ret <0)
		return(NULL);
	if (*ret >= ihw_max_handle)
		return(NULL);
	memcpy(ici,&isdncard_info[*ret].ici, sizeof(ihw_card_info)); 
	return(ici);
}

ihw_para_info	*ihw_get_parameter(int device_handle, ihw_para_info *ipi) {
	cibase *ib;
	ihw_para_info *pi;
	int handle;

	if(!ihw_init)
		if (init_ihw())
			return(NULL);
	if(!ipi)
		return(NULL);
	if ((device_handle<0) || (device_handle>=ihw_max_handle)) {
		return(NULL);
	}
	ib = isdncard_info + isdncard_txt[device_handle];
	if (ipi->handle<0)
		ipi->handle = -1;
	ipi->handle++;
	if (ipi->handle >= 5)
		return(NULL);
	if (ib->pcnt <= ipi->handle)
		return(NULL);
	pi = parameter_info + ib->pidx + ipi->handle;
	if (!pi->type)
		return(NULL);
	handle = ipi->handle;
	memcpy(ipi, pi, sizeof(ihw_para_info));
	ipi->handle = handle;
	return(ipi);
}

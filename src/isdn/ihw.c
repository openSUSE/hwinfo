#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "hd.h"

#define debprintf(a...)
// #define debprintf(a...) printf(## a)

/* private data */

#include "db/ihw_db.h"

#define IHW_card_cnt	((sizeof(isdncard_info) / sizeof(ihw_card_info)) -1)
#define IHW_driver_cnt	((sizeof(driver_info) / sizeof(ihw_driver_info))-1)
#define IHW_para_cnt	(sizeof(parameter_info) / sizeof(ihw_para_info))
typedef int (*fcmp) (const void *, const void *);

static int compare_type(const int *c1, const int *c2) {
	int x= driver_info[*c1].typ - driver_info[*c2].typ;

	if (!x)
		x=driver_info[*c1].subtyp - driver_info[*c2].subtyp;
	return(x);
}

static int compare_id(const int *c1, const int *c2) {
	int x= isdncard_info[*c1].vendor - isdncard_info[*c2].vendor;

	if (!x)
		x=isdncard_info[*c1].device - isdncard_info[*c2].device;
	if (!x)
		x=isdncard_info[*c1].subvendor - isdncard_info[*c2].subvendor;
	if (!x)
		x=isdncard_info[*c1].subdevice - isdncard_info[*c2].subdevice;
	return(x);
}

#if 0
/* interface */
static ihw_card_info      *ihw_get_card(int handle)
{
	if (handle<0)
		return(NULL);
	if (handle>=IHW_card_cnt)
		return(NULL);
	return(&isdncard_info[handle]);
}
#endif

static ihw_driver_info *ihw_get_driver_from_type(int typ, int subtyp)
{
	int key,*ret;
	
	key = IHW_driver_cnt;
	driver_info[key].typ = typ;
	driver_info[key].subtyp = subtyp;
	if (!(ret=bsearch(&key, isdndrv_type, IHW_driver_cnt, sizeof(int), (fcmp)compare_type))) {
		debprintf("ret NULL\n");
		return(NULL);
	}
	debprintf("ret idx %d\n", *ret);
	if (*ret <0)
		return(NULL);
	if (*ret >= IHW_driver_cnt)
		return(NULL);
	return(&driver_info[*ret]);
}

ihw_card_info *hd_ihw_get_card_from_type(int typ, int subtyp)
{
	ihw_driver_info	*idi;
	
	idi = ihw_get_driver_from_type(typ, subtyp);
	if (idi) {
		if (idi->card_ref >= 0)
			return(&isdncard_info[idi->card_ref]);
	}
	return(NULL);
}

ihw_card_info	*hd_ihw_get_card_from_id(	int vendor, int device,
					int subvendor, int subdevice)
{
	int key,*ret;	
	key = IHW_card_cnt;
	isdncard_info[key].vendor = vendor;
	isdncard_info[key].device = device;
	isdncard_info[key].subvendor = subvendor;
	isdncard_info[key].subdevice = subdevice;
	if (!(ret=bsearch(&key, isdncard_id, IHW_card_cnt, sizeof(int), (fcmp)compare_id))) {
		debprintf("bs1 ret NULL\n");
		key = IHW_card_cnt;
		isdncard_info[key].subvendor = PCI_ANY_ID;
		isdncard_info[key].subdevice = PCI_ANY_ID;
		if (!(ret=bsearch(&key, isdncard_id, IHW_card_cnt, sizeof(int), (fcmp)compare_id))) {
			debprintf("bs2 ret NULL\n");
			return(NULL);
		}
	}
	debprintf("ret idx %d\n", *ret);
	if (*ret <0)
		return(NULL);
	if (*ret >= IHW_card_cnt)
		return(NULL);
	return(&isdncard_info[*ret]);
}

ihw_para_info	*hd_ihw_get_parameter(int card_handle, int pnr)
{
	int	pi;
	
	if ((card_handle<0) || (card_handle >= IHW_card_cnt)) {
		return(NULL);
	}
	if (!isdncard_info[card_handle].paracnt)
		return(NULL);
	if (pnr < 1)
		return(NULL);
	if (isdncard_info[card_handle].paracnt<pnr)
		return(NULL);
	pi = isdncard_info[card_handle].para + pnr -1;
	if (pi < 0)
		return(NULL);
	if (pi >= IHW_para_cnt)
		return(NULL);
	if (!parameter_info[pi].type)
		return(NULL);
	return((ihw_para_info *) &parameter_info[pi]);
}

ihw_driver_info *hd_ihw_get_driver(int handle)
{
	if (handle<0)
		return(NULL);
	if (handle >= IHW_driver_cnt)
		return(NULL);
	return(&driver_info[handle]);
}


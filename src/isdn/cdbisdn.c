#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "hd.h"

#define debprintf(a...)
// #define debprintf(a...) printf(## a)

/* private data */

#include "cdb/isdn_cdb.h"

#define CDBISDN_vendor_cnt	(sizeof(cdb_isdnvendor_info) / sizeof(cdb_isdn_vendor))
#define CDBISDN_card_cnt	((sizeof(cdb_isdncard_info) / sizeof(cdb_isdn_card)) -1)
#define CDBISDN_vario_cnt	((sizeof(cdb_isdnvario_info) / sizeof(cdb_isdn_vario))-1)
typedef int (*fcmp) (const void *, const void *);

static int compare_type(cdb_isdn_vario *v1, cdb_isdn_vario *v2) {
	int x= v1->typ - v2->typ;

	if (!x)
		x=v1->subtyp - v2->subtyp;
	return(x);
}

static int compare_id(const int *c1, const int *c2) {
	int x= cdb_isdncard_info[*c1].vendor - cdb_isdncard_info[*c2].vendor;

	if (!x)
		x=cdb_isdncard_info[*c1].device - cdb_isdncard_info[*c2].device;
	if (!x)
		x=cdb_isdncard_info[*c1].subvendor - cdb_isdncard_info[*c2].subvendor;
	if (!x)
		x=cdb_isdncard_info[*c1].subdevice - cdb_isdncard_info[*c2].subdevice;
	return(x);
}

/* interface */

cdb_isdn_vendor	*hd_cdbisdn_get_vendor(int handle)
{
	if (handle<0)
		return(NULL);
	if ((unsigned)handle >= CDBISDN_vendor_cnt)
		return(NULL);
	return(&cdb_isdnvendor_info[handle]);
}

cdb_isdn_card	*hd_cdbisdn_get_card(int handle)
{
	if (handle<=0)
		return(NULL);
	if ((unsigned) handle>CDBISDN_card_cnt)
		return(NULL);
	return(&cdb_isdncard_info[handle]);
}

cdb_isdn_vario	*hd_cdbisdn_get_vario_from_type(int typ, int subtyp)
{
	cdb_isdn_vario key, *ret;
	
	key.typ = typ;
	key.subtyp = subtyp;
	if (!(ret=bsearch(&key, &cdb_isdnvario_info[1], CDBISDN_vario_cnt, sizeof(cdb_isdn_vario), (fcmp)compare_type))) {
		debprintf("ret NULL\n");
		return(NULL);
	}
	return(ret);
}

cdb_isdn_card	*hd_cdbisdn_get_card_from_type(int typ, int subtyp)
{
	cdb_isdn_vario	*civ;
	
	civ = hd_cdbisdn_get_vario_from_type(typ, subtyp);
	if (civ) {
		if (civ->card_ref > 0)
			return(&cdb_isdncard_info[civ->card_ref]);
	}
	return(NULL);
}

cdb_isdn_card	*hd_cdbisdn_get_card_from_id(int vendor, int device, int subvendor, int subdevice)
{
	int key, *ret;	
	key = 0;
	cdb_isdncard_info[key].vendor = vendor;
	cdb_isdncard_info[key].device = device;
	cdb_isdncard_info[key].subvendor = subvendor;
	cdb_isdncard_info[key].subdevice = subdevice;
	if (!(ret=bsearch(&key, cdb_isdncard_idsorted, CDBISDN_card_cnt, sizeof(int), (fcmp)compare_id))) {
		debprintf("bs1 ret NULL\n");
		key = 0;
		cdb_isdncard_info[key].subvendor = PCI_ANY_ID;
		cdb_isdncard_info[key].subdevice = PCI_ANY_ID;
		if (!(ret=bsearch(&key, cdb_isdncard_idsorted, CDBISDN_card_cnt, sizeof(int), (fcmp)compare_id))) {
			debprintf("bs2 ret NULL\n");
			return(NULL);
		}
	}
	debprintf("ret idx %d\n", *ret);
	if (*ret <= 0)
		return(NULL);
	if ((unsigned) *ret > CDBISDN_card_cnt)
		return(NULL);
	return(&cdb_isdncard_info[*ret]);
}

cdb_isdn_vario *hd_cdbisdn_get_vario(int handle)
{
	if (handle<=0)
		return(NULL);
	if ((unsigned) handle > CDBISDN_vario_cnt)
		return(NULL);
	return(&cdb_isdnvario_info[handle]);
}

int	hd_cdbisdn_get_version(void)
{
	return(CDBISDN_VERSION);
}

int	hd_cdbisdn_get_db_version(void)
{
	return(CDBISDN_DBVERSION);
}

char	*hd_cdbisdn_get_db_date(void)
{
	return((char *)CDBISDN_DATE);
}

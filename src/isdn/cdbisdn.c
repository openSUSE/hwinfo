#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <errno.h>
#include "hd.h"
#include "hd_int.h"

#define debprintf(a...)
// #define debprintf(a...) fprintf(stderr, ## a)

/* private data */

#include "cdb/isdn_cdb.h"
#include "cdb/cdb_hwdb.h"

static int		CDBISDN_vendor_cnt;
static int		CDBISDN_card_cnt;
static int		CDBISDN_vario_cnt;
static int		CDBISDN_name_size;
static char		*CDBISDN_names;

static cdb_isdn_vendor	*cdb_isdnvendor_info;
static cdb_isdn_card	*cdb_isdncard_info;
static int 		*cdb_isdncard_idsorted;
static cdb_isdn_vario	*cdb_isdnvario_info;
static int		cdb_dbversion;
static char		cdb_date[32];

static char		line[1024];

static int		CDBISDN_readhwdb;

static int
init_cdbisdn(void)
{
	FILE	*cdb;
	char	*s, *p = NULL;
	int	rectyp, l, cnt = 0, icnt = 0;

	cdb = fopen(CDBISDN_HWDB_FILE, "rb");
	if (!cdb) {
		debprintf("open failure %s\n", CDBISDN_HWDB_FILE);
		goto fallback;
	}
	while (!feof(cdb)) {
		s = fgets(line, 1024, cdb);
		if (!s)
			break;
		if (!s[0] || s[0] == '!' || s[0] == '#' || s[0] == '\n') 
			continue;
		if (s[0] != '$') {
			debprintf("got wrong line %s\n", s);
			continue;
		}
		sscanf(s, "$%d", &rectyp);
		switch(rectyp) {
			case IWHREC_TYPE_VERSION:
				sscanf(s + 4, "%d", &cdb_dbversion);
				break;
			case IWHREC_TYPE_DATE:
				l = strlen(s + 4);
				if (!l)
					continue;
				l--;
				if (l > 31)
					l = 31;
				strncpy(cdb_date, s + 4, l);
				cdb_date[l] = 0;
				break;
			case IWHREC_TYPE_NAME_SIZE:
				sscanf(s + 4, "%d", &CDBISDN_name_size);
				CDBISDN_names = calloc(CDBISDN_name_size + 1, 1);
				if (!CDBISDN_names) {
					debprintf("fail to allocate %d bytes for CDBISDN_names\n", CDBISDN_name_size);
					goto fallback_close;
				}
				p = CDBISDN_names;
				cnt = 0;
				icnt = 0;
				break;
			case IWHREC_TYPE_NAME_DATA:
				if (!p)
					goto fallback_close;
				l = strlen(s + 4);
				icnt += l;
				if (icnt > CDBISDN_name_size) {
					debprintf("name_size overflow %d/%d\n", icnt, CDBISDN_name_size);
					goto fallback_close;
				}
				strcpy(p, s + 4);
				p[l-1] = 0;
				p += l;
				cnt++;
				break;
			case IWHREC_TYPE_NAME_COUNT:
				sscanf(s + 4, "%d", &l);
				if (cnt != l)
					goto fallback_close;
				break;
			case IWHREC_TYPE_VENDOR_COUNT:
				sscanf(s + 4, "%d", &CDBISDN_vendor_cnt);
				cdb_isdnvendor_info = calloc(CDBISDN_vendor_cnt, sizeof(cdb_isdn_vendor));
				if (!cdb_isdnvendor_info) {
					debprintf("fail to allocate %d vendor structs\n", CDBISDN_vendor_cnt);
					goto fallback_close;
				}
				cnt = 0;
				break;
			case IWHREC_TYPE_VENDOR_RECORD:
				if (cnt >= CDBISDN_vendor_cnt) {
					debprintf("vendor overflow %d/%d\n", cnt, CDBISDN_vendor_cnt);
					goto fallback_close;
				}
				l = sscanf(s + 4, "%p %p %d %d",
					&cdb_isdnvendor_info[cnt].name,
					&cdb_isdnvendor_info[cnt].shortname,
					&cdb_isdnvendor_info[cnt].vnr,
					&cdb_isdnvendor_info[cnt].refcnt);
				if (l != 4) {
					debprintf("error reading vendor record %s\n", s);
					goto fallback_close;
				}
				cdb_isdnvendor_info[cnt].name = CDBISDN_names + (u_long)cdb_isdnvendor_info[cnt].name;
				cdb_isdnvendor_info[cnt].shortname = CDBISDN_names + (u_long)cdb_isdnvendor_info[cnt].shortname;
				cnt++;
				break;
			case IWHREC_TYPE_CARD_COUNT:
				sscanf(s + 4, "%d", &CDBISDN_card_cnt);
				cdb_isdncard_info = calloc(CDBISDN_card_cnt + 1, sizeof(cdb_isdn_card));
				cdb_isdncard_idsorted = calloc(CDBISDN_card_cnt, sizeof(int));
				if (!cdb_isdncard_info || !cdb_isdncard_idsorted) {
					debprintf("fail to allocate %d vendor structs\n", CDBISDN_card_cnt);
					goto fallback_close;
				}
				cnt = 0;
				icnt = 0;
				break;
			case IWHREC_TYPE_CARD_RECORD:
				if (cnt > CDBISDN_card_cnt) {
					debprintf("card overflow %d/%d\n", cnt, CDBISDN_card_cnt);
					goto fallback_close;
				}
				l = sscanf(s + 4, "%d %d %p %p %p %p %d %d %d %d %d %d %d %d %d",
					&cdb_isdncard_info[cnt].handle,		/* internal identifier idx in database */
					&cdb_isdncard_info[cnt].vhandle,	/* internal identifier to vendor database */
					&cdb_isdncard_info[cnt].name,		/* cardname */
					&cdb_isdncard_info[cnt].lname,		/* vendor short name + cardname */
					&cdb_isdncard_info[cnt].Class,		/* CLASS of the card */
					&cdb_isdncard_info[cnt].bus,		/* bus type */
					&cdb_isdncard_info[cnt].revision,	/* revision used with USB */
					&cdb_isdncard_info[cnt].vendor,		/* Vendor ID for ISAPNP and PCI cards */
					&cdb_isdncard_info[cnt].device,		/* Device ID for ISAPNP and PCI cards */
					&cdb_isdncard_info[cnt].subvendor,	/* Subvendor ID for PCI cards */
					&cdb_isdncard_info[cnt].subdevice,	/* Subdevice ID for PCI cards */
					&cdb_isdncard_info[cnt].features,	/* feature flags */
					&cdb_isdncard_info[cnt].line_cnt,	/* count of ISDN ports */
					&cdb_isdncard_info[cnt].vario_cnt,	/* count of driver varios */
					&cdb_isdncard_info[cnt].vario);		/* referenz to driver vario record */
				if (l != 15) {
					debprintf("error reading card record %s\n", s);
					goto fallback_close;
				}
				cdb_isdncard_info[cnt].name = CDBISDN_names + (u_long)cdb_isdncard_info[cnt].name;
				cdb_isdncard_info[cnt].lname = CDBISDN_names + (u_long)cdb_isdncard_info[cnt].lname;
				cdb_isdncard_info[cnt].Class = CDBISDN_names + (u_long)cdb_isdncard_info[cnt].Class;
				cdb_isdncard_info[cnt].bus = CDBISDN_names + (u_long)cdb_isdncard_info[cnt].bus;
				cnt++;
				break;
			case IWHREC_TYPE_CARD_IDSORTED:
				if (icnt >= CDBISDN_card_cnt) {
					debprintf("card overflow %d/%d\n", icnt, CDBISDN_card_cnt);
					goto fallback_close;
				}
				sscanf(s + 4, "%d", &cdb_isdncard_idsorted[icnt]);
				icnt++;
				break;
			case IWHREC_TYPE_VARIO_COUNT:
				sscanf(s + 4, "%d", &CDBISDN_vario_cnt);
				cdb_isdnvario_info = calloc(CDBISDN_vario_cnt+1, sizeof(cdb_isdn_vario));
				if (!cdb_isdnvario_info) {
					debprintf("fail to allocate %d vario structs\n", CDBISDN_vario_cnt);
					goto fallback_close;
				}
				cnt = 0;
				break;
			case IWHREC_TYPE_VARIO_RECORD:
				if (cnt > CDBISDN_vario_cnt) {
					debprintf("vario overflow %d/%d\n", cnt, CDBISDN_vario_cnt);
					goto fallback_close;
				}
				l = sscanf(s + 4, "%d %d %d %d %d %d %p %p %p %p %p %p %p %p %p %p %p %p %p %p %d %p",
					&cdb_isdnvario_info[cnt].handle,		/* idx in database */	
					&cdb_isdnvario_info[cnt].next_vario,	/* link to alternate vario */
					&cdb_isdnvario_info[cnt].drvid,		/* unique id of the driver vario */
					&cdb_isdnvario_info[cnt].typ,		/* Type to identify the driver */
					&cdb_isdnvario_info[cnt].subtyp,		/* Subtype of the driver type */
					&cdb_isdnvario_info[cnt].smp,		/* SMP supported ? */
					&cdb_isdnvario_info[cnt].mod_name,	/* name of the driver module */
					&cdb_isdnvario_info[cnt].para_str,	/* optional parameter string */
					&cdb_isdnvario_info[cnt].mod_preload,	/* optional modules to preload */
					&cdb_isdnvario_info[cnt].cfg_prog,	/* optional cfg prog */
					&cdb_isdnvario_info[cnt].firmware,	/* optional firmware to load */
					&cdb_isdnvario_info[cnt].description,	/* optional description */
					&cdb_isdnvario_info[cnt].need_pkg,	/* list of packages needed for function */
					&cdb_isdnvario_info[cnt].info,		/* optional additional info */
					&cdb_isdnvario_info[cnt].protocol,	/* supported D-channel protocols */
					&cdb_isdnvario_info[cnt].interface,	/* supported API interfaces */
					&cdb_isdnvario_info[cnt].io,		/* possible IO ports with legacy ISA cards */
					&cdb_isdnvario_info[cnt].irq,		/* possible interrupts with legacy ISA cards */
					&cdb_isdnvario_info[cnt].membase,	/* possible membase with legacy ISA cards */
					&cdb_isdnvario_info[cnt].features,	/* optional features*/
					&cdb_isdnvario_info[cnt].card_ref,	/* reference to a card */
					&cdb_isdnvario_info[cnt].name);		/* driver name */
				if (l != 22) {
					debprintf("error reading vario record %s\n", s);
					goto fallback_close;
				}
				cdb_isdnvario_info[cnt].mod_name = CDBISDN_names + (u_long)cdb_isdnvario_info[cnt].mod_name;
				cdb_isdnvario_info[cnt].para_str = CDBISDN_names + (u_long)cdb_isdnvario_info[cnt].para_str;
				cdb_isdnvario_info[cnt].mod_preload = CDBISDN_names + (u_long)cdb_isdnvario_info[cnt].mod_preload;
				cdb_isdnvario_info[cnt].cfg_prog = CDBISDN_names + (u_long)cdb_isdnvario_info[cnt].cfg_prog;
				cdb_isdnvario_info[cnt].firmware = CDBISDN_names + (u_long)cdb_isdnvario_info[cnt].firmware;
				cdb_isdnvario_info[cnt].description = CDBISDN_names + (u_long)cdb_isdnvario_info[cnt].description;
				cdb_isdnvario_info[cnt].need_pkg = CDBISDN_names + (u_long)cdb_isdnvario_info[cnt].need_pkg;
				cdb_isdnvario_info[cnt].info = CDBISDN_names + (u_long)cdb_isdnvario_info[cnt].info;
				cdb_isdnvario_info[cnt].protocol = CDBISDN_names + (u_long)cdb_isdnvario_info[cnt].protocol;
				cdb_isdnvario_info[cnt].interface = CDBISDN_names + (u_long)cdb_isdnvario_info[cnt].interface;
				cdb_isdnvario_info[cnt].io = CDBISDN_names + (u_long)cdb_isdnvario_info[cnt].io;
				cdb_isdnvario_info[cnt].irq = CDBISDN_names + (u_long)cdb_isdnvario_info[cnt].irq;
				cdb_isdnvario_info[cnt].membase = CDBISDN_names + (u_long)cdb_isdnvario_info[cnt].membase;
				cdb_isdnvario_info[cnt].features = CDBISDN_names + (u_long)cdb_isdnvario_info[cnt].features;
				cdb_isdnvario_info[cnt].name = CDBISDN_names + (u_long)cdb_isdnvario_info[cnt].name;
				cnt++;
				break;
			default:
				debprintf("got wrong RecType %d\n", rectyp);
				break;
		}
	}
	fclose(cdb);
	if (CDBISDN_name_size == 0 ||
		CDBISDN_vendor_cnt == 0 ||
		CDBISDN_card_cnt == 0 ||
		CDBISDN_vario_cnt == 0)
		goto fallback;
	debprintf("successfull reading %s\n", CDBISDN_HWDB_FILE);
	CDBISDN_readhwdb = 1;	
	return(0);
fallback_close:
	fclose(cdb);
fallback:
	debprintf("error reading %s\n", CDBISDN_HWDB_FILE);
	CDBISDN_vendor_cnt = (sizeof(cdb_isdnvendor_info_init) / sizeof(cdb_isdn_vendor));
	CDBISDN_card_cnt = ((sizeof(cdb_isdncard_info_init) / sizeof(cdb_isdn_card)) -1);
	CDBISDN_vario_cnt = ((sizeof(cdb_isdnvario_info_init) / sizeof(cdb_isdn_vario))-1);
	cdb_isdnvendor_info = cdb_isdnvendor_info_init;
	cdb_isdncard_info = cdb_isdncard_info_init;
	cdb_isdncard_idsorted = cdb_isdncard_idsorted_init;
	cdb_isdnvario_info = cdb_isdnvario_info_init;
	cdb_dbversion = CDBISDN_DBVERSION;
	strncpy(cdb_date, CDBISDN_DATE, 31);
	CDBISDN_readhwdb = 1;
	return(1);
}

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

API_SYM cdb_isdn_vendor	*hd_cdbisdn_get_vendor(int handle)
{
	if (!CDBISDN_readhwdb)
		init_cdbisdn();
	if (handle<0)
		return(NULL);
	if ((unsigned)handle >= CDBISDN_vendor_cnt)
		return(NULL);
	return(&cdb_isdnvendor_info[handle]);
}

API_SYM cdb_isdn_card	*hd_cdbisdn_get_card(int handle)
{
	if (!CDBISDN_readhwdb)
		init_cdbisdn();
	if (handle<=0)
		return(NULL);
	if ((unsigned) handle>CDBISDN_card_cnt)
		return(NULL);
	return(&cdb_isdncard_info[handle]);
}

API_SYM cdb_isdn_vario	*hd_cdbisdn_get_vario_from_type(int typ, int subtyp)
{
	cdb_isdn_vario key, *ret;
	
	if (!CDBISDN_readhwdb)
		CDBISDN_readhwdb = init_cdbisdn();
	key.typ = typ;
	key.subtyp = subtyp;
	if (!(ret=bsearch(&key, &cdb_isdnvario_info[1], CDBISDN_vario_cnt, sizeof(cdb_isdn_vario), (fcmp)compare_type))) {
		debprintf("ret NULL\n");
		return(NULL);
	}
	return(ret);
}

API_SYM cdb_isdn_card	*hd_cdbisdn_get_card_from_type(int typ, int subtyp)
{
	cdb_isdn_vario	*civ;
	
	if (!CDBISDN_readhwdb)
		init_cdbisdn();
	civ = hd_cdbisdn_get_vario_from_type(typ, subtyp);
	if (civ) {
		if (civ->card_ref > 0)
			return(&cdb_isdncard_info[civ->card_ref]);
	}
	return(NULL);
}

API_SYM cdb_isdn_card	*hd_cdbisdn_get_card_from_id(int vendor, int device, int subvendor, int subdevice)
{
	int key, *ret;	

	if (!CDBISDN_readhwdb)
		init_cdbisdn();
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

API_SYM cdb_isdn_vario *hd_cdbisdn_get_vario(int handle)
{
	if (!CDBISDN_readhwdb)
		init_cdbisdn();
	if (handle<=0)
		return(NULL);
	if ((unsigned) handle > CDBISDN_vario_cnt)
		return(NULL);
	return(&cdb_isdnvario_info[handle]);
}

API_SYM int	hd_cdbisdn_get_version(void)
{
	if (!CDBISDN_readhwdb)
		init_cdbisdn();
	return(CDBISDN_VERSION);
}

API_SYM int	hd_cdbisdn_get_db_version(void)
{
	if (!CDBISDN_readhwdb)
		init_cdbisdn();
	return(cdb_dbversion);
}

API_SYM char	*hd_cdbisdn_get_db_date(void)
{
	if (!CDBISDN_readhwdb)
		init_cdbisdn();
	return(cdb_date);
}

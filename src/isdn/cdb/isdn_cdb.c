#include <stdio.h>
#include <stdlib.h>
#include <malloc.h>
#include <string.h>
#include <time.h>
#include "lex.yy.c"
#include "hd.h"

int yywrap(void) {
	return(1);
}

#define MAXCARDS	300
#define MAXVARIO	(MAXCARDS*4)
#define MAXNAMECNT	(MAXCARDS*256)

static int		max_ce = MAXCARDS;
static int		ce_idx = 0;
static int		max_vario = MAXVARIO;
static int		vario_idx;
static int		name_str_cnt = 0;
static int		max_name_str_cnt = MAXNAMECNT;
static int		max_ivendor = MAXCARDS;
static int		ivendor_idx = 0;

static char		*name_str;
static cdb_isdn_card	*cards;
static cdb_isdn_vario	*varios;
static cdb_isdn_vendor	*vendors;

static int		*isdncard_id;

static int		drvid_cnt;
static int		drv_subtyp_cnt;
static int		drv_typ_cnt;
static int		supported_cnt;

static int		not_supported = 0;

struct {
	char	*lname;
	char    *sname;
} _vendorshortnames[] = {
	{"Billion","Billion"},
	{"AVM Computersysteme Vertriebs GmbH","AVM"},
	{"ITH","ITH"},
	{"HST","HST"},
	{"CCD","CCD"},
	{"Telekom AG","DTAG"},
	{"Sagem","Sagem"},
	{"Formula-n","Formula-n"},
	{"ICN","ICN"},
	{"TigerJet","TJET"},
	{"Scitel","Scitel"},
	{"Elsa AG","Elsa"},
	{"Teles AG","Teles"},
	{"Sedlbauer AG","Sedlbauer"},
	{"Eicon","Eicon"},
	{"Creatix Polymedia Gesellschaft für Kommunikationssysteme mbH","Creatix"},
	{"Siemens","Siemens"},
	{"AsKey","AsKey"},
	{"Gazel","Gazel"},
	{"ASUSCOM","Asus"},
	{"Digi","Digi"},
	{"Genius","Genius"},
	{"Berkom","Berkom"},
	{"U.S.Robotics","USR"},
	{"Zolltrix","Zolltrix"},
	{"Motorola","Motorola"},
	{"diverse","div."},
	{"Winbond","Winbond"},
	{"ITK","ITK"},
	{"Dynalink","Dynalink"},
	{"SGS Thomson Microelectronics","SGST"},
	{"Iscas","Iscas"},
	{"Tiger","Tiger"},
	{"Seyon","Seyon"},
	{"Acer","Acer"},
	{"TeleInt","TeleInt"},
	{"Pyramid","Pyramid"},
	{"COMPAQ","COMPAQ"},
	{"Abocom/Magitek","Abocom"},
	{NULL,NULL},
};

typedef int (*fcmp) (const void *, const void *);

static int compare_vendor(cdb_isdn_vendor *v1, cdb_isdn_vendor *v2) {
	return(strcasecmp(v1->name, v2->name));
}

static int compare_card(cdb_isdn_card *c1, cdb_isdn_card *c2) {
	return(strcasecmp(c1->name, c2->name));
}

int compare_name(const int *c1, const int *c2) {
	return(strcasecmp(cards[*c1].name,
		cards[*c2].name));
}

static int compare_type(cdb_isdn_vario *v1, cdb_isdn_vario *v2) {
	int x= v1->typ - v2->typ;

	if (!x)
		x=v1->subtyp - v2->subtyp;
	return(x);
}

static int compare_id(const int *c1, const int *c2) {
	int x = cards[*c1].vendor - cards[*c2].vendor;

	if (!x)
		x = cards[*c1].device -
			cards[*c2].device;
	if (!x)
		x = cards[*c1].subvendor -
			cards[*c2].subvendor;
	if (!x)
		x = cards[*c1].subdevice -
			cards[*c2].subdevice;
	return(x);
}

static char *add_name(const char *str, int merge) {
	char	*p;
	int	l;

	if (!str)
		return(NULL);
	if (merge) {
		p = name_str;
		while (*p) {
			if (!strcmp(p, str))
				break;
			p += strlen(p) +1;
			if (p >= (name_str + max_name_str_cnt))
				return(NULL);
		}
		if (*p)
			return(p);
	} else {
		p = name_str + name_str_cnt;
	}
	l = strlen(str) +1;
	if ((p + l) >= (name_str + max_name_str_cnt))
		return(NULL);
	strcpy(p, str);
	name_str_cnt += l;
	return(p);
}

static char stmp[4096],sstmp[4096];

static char *add_lname(int v, const char *str) {
	sprintf(stmp, "%s %s", vendors[v].shortname, str);
	return(add_name(stmp, 1));
}

static char *add_name_list(const char *str, const char *list) {
	if (!list || !list[0])
		return(add_name(str, 1));
	sprintf(stmp, "%s,%s", list, str);
	return(add_name(stmp, 1));
}

static char *add_sortedname_list(const char *str, const char *list, const char *fmt) {
	u_int	v,i,flg=0;
	char	*t,*p;
	if (!list || !list[0])
		return(add_name(str, 1));
	strncpy(stmp, list, 4096);
	sscanf(str, fmt, &v);
	p = sstmp;
	t = strtok(stmp, ",");
	while (t) {
		sscanf(t, fmt, &i);
		if (!flg && i>v) {
			flg++;
			p += sprintf(p, fmt, v);
			*p++ = ',';
		}
		p += sprintf(p, fmt, i);
		*p++ = ',';
		t = strtok(NULL, ",");
	}
	if (!flg)
		p += sprintf(p, fmt, v);
	else
		p--;
	*p = 0;
	return(add_name(sstmp, 1));
}

static int add_vendor(char *v, int card) {
	int	i,found = 0;
	
	for(i=0;i < ivendor_idx; i++) {
		if (!strcmp(v, vendors[i].name)) {
			vendors[i].refcnt++;
			return(i);
		}
	}
	if (ivendor_idx < max_ivendor) {
		i=0;
		while (_vendorshortnames[i].lname) {
			if (!strcmp(v, _vendorshortnames[i].lname)) {
				found++;
				break;
			} else if (!strcmp(v, _vendorshortnames[i].sname)) {
				found++;
				break;
			}
			i++;
		}
		if (found) {
			vendors[ivendor_idx].name = _vendorshortnames[i].lname;
			vendors[ivendor_idx].shortname = _vendorshortnames[i].sname;
		} else if (!(vendors[ivendor_idx].name = add_name(v, 1)))
			return(-1);
		vendors[ivendor_idx].vnr = ivendor_idx;
		vendors[ivendor_idx].refcnt++;
		ivendor_idx++;
		return(ivendor_idx-1);
	} else
		return(-1);
}

static int new_vario(char *v, int c) {

	vario_idx++;
	if (vario_idx>=max_vario)
		return(-1);
	drvid_cnt = 0;
	drv_subtyp_cnt = 0;
	drv_typ_cnt = 0;
	not_supported = 0;
	supported_cnt = 0;
	if (!(varios[vario_idx].name = add_name(v, 1)))
		return(-1);
	if (cards[c].vario>0) {
		varios[vario_idx-1].next_vario = vario_idx;
	} else
		cards[c].vario = vario_idx;
	varios[vario_idx].handle = vario_idx;
	varios[vario_idx].card_ref = c;
	cards[c].vario_cnt++;
	return(0);
}

void del_vario(void) {
	fprintf(stderr, "del_vario: %d %s\n", vario_idx, cards[varios[vario_idx].card_ref].name);
	cards[varios[vario_idx].card_ref].vario_cnt--;
	if (vario_idx>0) {
		if (varios[vario_idx-1].next_vario == vario_idx) {
			if (cards[varios[vario_idx].card_ref].vario_cnt == 1)
				cards[varios[vario_idx].card_ref].vario = vario_idx-1;
			varios[vario_idx-1].next_vario = 0;
		} else if (cards[varios[vario_idx].card_ref].vario == vario_idx) {
			cards[varios[vario_idx].card_ref].vario = 0;
		} else {
			fprintf(stderr, "del_vario:internal error\n");
			exit(98);
		}
	}
	memset(&varios[vario_idx], 0, sizeof(cdb_isdn_vario));
	vario_idx--;
}

int new_entry(void) {
	if (not_supported) {
		not_supported = 0;
		fprintf(stderr, "new_entry:not_supported %s\n", cards[ce_idx].name);
		if (cards[ce_idx].vario_cnt < 1) {
			vendors[cards[ce_idx].vhandle].refcnt--;
			memset(&cards[ce_idx], 0, sizeof(cdb_isdn_card));
			ce_idx--;
		}
	}
	ce_idx++;
	if (ce_idx >= max_ce)
		return(1);
	cards[ce_idx].handle = ce_idx;
	cards[ce_idx].vendor = PCI_ANY_ID;
	cards[ce_idx].device = PCI_ANY_ID;
	cards[ce_idx].subvendor = PCI_ANY_ID;
	cards[ce_idx].subdevice = PCI_ANY_ID;
	return(0);
}

void add_current_item(int item, char *val) {
	int i;

	if ((item != vario) && not_supported)
		return;
	switch (item) {
		case vendor:
			i = add_vendor(val, ce_idx);
			if (i<0) {
				fprintf(stderr, "error in add_vendor %s\n", val);
				exit(100);
			}
			cards[ce_idx].vhandle = i;
			break;
		case device:
			cards[ce_idx].name = add_name(val, 1);
			if (!cards[ce_idx].name) {
				fprintf(stderr, "error in add_name %s\n", val);
				exit(101);
			}
			cards[ce_idx].lname = add_lname(cards[ce_idx].vhandle, val);
			if (!cards[ce_idx].lname) {
				fprintf(stderr, "error in add_lname %s\n", val);
				exit(101);
			}
			break;
		case vendor_id:
			i = sscanf(val,"%x", &cards[ce_idx].vendor);
			if (i!=1) {
				fprintf(stderr, "error to hex %s\n", val);
				exit(102);
			}
			break;
		case device_id:
			i = sscanf(val,"%x", &cards[ce_idx].device);
			if (i!=1) {
				fprintf(stderr, "error to hex %s\n", val);
				exit(102);
			}
			break;
		case subvendor_id:
			i = sscanf(val,"%x", &cards[ce_idx].subvendor);
			if (i!=1) {
				fprintf(stderr, "error to hex %s\n", val);
				exit(102);
			}
			break;
		case subdevice_id:
			i = sscanf(val,"%x", &cards[ce_idx].subdevice);
			if (i!=1) {
				fprintf(stderr, "error to hex %s\n", val);
				exit(102);
			}
			break;
		case device_class:
			cards[ce_idx].Class = add_name(val, 1);
			if (!cards[ce_idx].name) {
				fprintf(stderr, "error in add_name %s\n", val);
				exit(101);
			}
			break;
		case bus_type:
			cards[ce_idx].bus = add_name(val, 1);
			if (!cards[ce_idx].name) {
				fprintf(stderr, "error in add_name %s\n", val);
				exit(101);
			}
			break;
		case vario:
			if (new_vario(val, ce_idx)) {
				fprintf(stderr, "error in new_vario(%s, %d)\n", val, ce_idx);
				exit(103);
			}
			break;
		case SMP:
			if (!strcasecmp(val, "no"))
				varios[vario_idx].smp = 0;
			else if (!strcasecmp(val, "yes"))
				varios[vario_idx].smp = 1;
			break;
		case drv_id:
			if (drvid_cnt) {
				fprintf(stderr, "more as one drvid_cnt (%s) card (%s)\n", val, cards[ce_idx].name);
			} else {
				i = sscanf(val,"%x", &varios[vario_idx].drvid);
				if (i!=1) {
					fprintf(stderr, "error to hex %s\n", val);
					exit(102);
				}
			}
			drvid_cnt++; 
			break;
		case drv_subtyp:
			if (drv_subtyp_cnt) {
				fprintf(stderr, "more as one drv_subtyp (%s) card (%s)\n", val, cards[ce_idx].name);
			} else {
				i = sscanf(val,"%d", &varios[vario_idx].subtyp);
				if (i!=1) {
					fprintf(stderr, "error to decimal %s\n", val);
					exit(104);
				}
			}
			drv_subtyp_cnt++; 
			break;
		case drv_typ:
			if (drv_typ_cnt) {
				fprintf(stderr, "more as one drv_typ (%s) card (%s)\n", val, cards[ce_idx].name);
			} else {
				i = sscanf(val,"%d", &varios[vario_idx].typ);
				if (i!=1) {
					fprintf(stderr, "error to decimal %s\n", val);
					exit(104);
				}
			}
			drv_typ_cnt++; 
			break;
		case interface:
			varios[vario_idx].interface = add_name_list(val, varios[vario_idx].interface);
			break;
		case line_cnt:
			i = sscanf(val,"%d", &cards[ce_idx].line_cnt);
			if (i!=1) {
				fprintf(stderr, "error to hex %s\n", val);
				exit(102);
			}
			break;
		case line_protocol:
			varios[vario_idx].protocol = add_name_list(val, varios[vario_idx].protocol);
			break;
		case module:
			varios[vario_idx].mod_name = add_name(val, 1);
			break;
		case need_packages:
			varios[vario_idx].need_pkg = add_name_list(val, varios[vario_idx].need_pkg);
			break;
		case supported:
			if (supported_cnt)
				fprintf(stderr, "more as one supported entry (%s) vendor(%s) card(%s)\n", val, 
					vendors[cards[ce_idx].vhandle].name, cards[ce_idx].name);
			if (!strcasecmp(val, "not")) {
				not_supported = 1;
				del_vario();
			}
			supported_cnt++;
			break;
		case feature:
			varios[vario_idx].features = add_name_list(val, varios[vario_idx].features);
			break;
		case info:
			varios[vario_idx].info = add_name(val, 1);
			break;
		case special:
			break;
		case firmware:
			varios[vario_idx].firmware = add_name(val, 1);
			break;
		case short_description:
			varios[vario_idx].description = add_name(val, 1);
			break;
		case IRQ:
			varios[vario_idx].irq = add_sortedname_list(val, varios[vario_idx].irq, "%d");
			break;
		case IO:
			varios[vario_idx].io = add_sortedname_list(val, varios[vario_idx].io, "0x%x");
			break;
		case MEMBASE:
			varios[vario_idx].membase = add_sortedname_list(val, varios[vario_idx].membase, "0x%x");
			break;
		case alternative_name:
			break;
		case revision:
			i = sscanf(val,"%x", &cards[ce_idx].revision);
			if (i!=1) {
				fprintf(stderr, "error to hex %s\n", val);
				exit(102);
			}
			if ((cards[ce_idx].subvendor == PCI_ANY_ID) &&
				(cards[ce_idx].subdevice == PCI_ANY_ID))
				cards[ce_idx].subvendor = cards[ce_idx].revision;
			break;
	}
}

static void SortVendors(void) {
	int	v,c;

	qsort(vendors, ivendor_idx, sizeof(cdb_isdn_vendor), (fcmp)compare_vendor);
	/* readjust card data */
	for (c = 1; c <= ce_idx; c++) {
		for (v = 0; v < ivendor_idx; v++) {
			if (cards[c].vhandle == vendors[v].vnr) {
				cards[c].vhandle = v;
				break;
			}
		}
	}
	/* now adjust own handle */
	for (v = 0; v < ivendor_idx; v++) {
		vendors[v].vnr = v;
	}
}

static void SortCards(void) {
	int	v,c;

	qsort(&cards[1], ce_idx, sizeof(cdb_isdn_card), (fcmp)compare_card);
	/* readjust vario data */
	for (v = 1; v <= vario_idx; v++) {
		for (c = 1; c <= ce_idx; c++) {
			if (cards[c].handle == varios[v].card_ref) {
				varios[v].card_ref = c;
				break;
			}
		}
	}
	/* now adjust own handle */
	for (c = 0; c <= ce_idx; c++) {
		cards[c].handle = c;
	}
	isdncard_id = malloc(ce_idx*sizeof(int));
	if (!isdncard_id) {
		fprintf(stderr, "no mem for isdncard_id (%d entries)\n", ce_idx);
		exit(97);
	}
	for (c = 0; c < ce_idx; c++)
		isdncard_id[c] = c + 1;
	qsort(isdncard_id, ce_idx, sizeof(int), (fcmp)compare_id);
}

static void SortVarios(void) {
	int	v,c,i;

	qsort(&varios[1], vario_idx, sizeof(cdb_isdn_vario), (fcmp)compare_type);
	/* readjust vario data */
	for (v = 1; v <= vario_idx; v++) {
		if (varios[v].next_vario) {
			for (i = 1; i <= vario_idx; i++) {
				if (varios[i].handle == varios[v].next_vario) {
					varios[v].next_vario = i;
					break;
				}
			}
		}
	}
	/* readjust card data */
	for (c = 1; c <= ce_idx; c++) {
		for (v = 1; v <= vario_idx; v++) {
			if (varios[v].handle == cards[c].vario) {
				cards[c].vario = v;
				break;
			}
		}
	}
	/* now adjust own handle */
	for (v = 1; v <= vario_idx; v++) {
		varios[v].handle = v;
	}
}

static int WriteVendors(FILE *f) {
	int i;

	fprintf(f, "/* vendor database */\n");
	fprintf(f,"static cdb_isdn_vendor cdb_isdnvendor_info[] = {\n");
	for (i=0; i < ivendor_idx; i++) {
		fprintf(f,"	{");
		if (vendors[i].name)
			fprintf(f,"\"%s\",", vendors[i].name);
		else
			fprintf(f,"\"\",");
		if (vendors[i].shortname)
			fprintf(f,"\"%s\",", vendors[i].shortname);
		else
			fprintf(f,"\"\",");
		fprintf(f,"%d,",vendors[i].vnr);
		fprintf(f,"%d",vendors[i].refcnt);
		fprintf(f,"},\n");
	}
	fprintf(f,"};\n");
	return(0);
}

static int WriteCards(FILE *f) {
	int i;

	fprintf(f, "/* card database */\n");
	fprintf(f,"static cdb_isdn_card cdb_isdncard_info[] = {\n");
	for (i=0; i <= ce_idx; i++) {
		fprintf(f,"	{");
		fprintf(f,"%d,",cards[i].handle);
		fprintf(f,"%d,",cards[i].vhandle);
		if (cards[i].name)
			fprintf(f,"\"%s\",", cards[i].name);
		else
			fprintf(f,"\"\",");
		if (cards[i].lname)
			fprintf(f,"\"%s\",", cards[i].lname);
		else
			fprintf(f,"\"\",");
		if (cards[i].Class)
			fprintf(f,"\"%s\",",cards[i].Class);
		else
			fprintf(f,"NULL,");
		if (cards[i].bus)
			fprintf(f,"\"%s\",",cards[i].bus);
		else
			fprintf(f,"NULL,");
		fprintf(f,"%d,",cards[i].revision);
		fprintf(f,"%d,",cards[i].vendor);
		fprintf(f,"%d,",cards[i].device);
		fprintf(f,"%d,",cards[i].subvendor);
		fprintf(f,"%d,",cards[i].subdevice);
		fprintf(f,"%d,",cards[i].features);
		fprintf(f,"%d,",cards[i].line_cnt);
		fprintf(f,"%d,",cards[i].vario_cnt);
		fprintf(f,"%d",cards[i].vario);
		fprintf(f,"},\n");
	}
	fprintf(f,"};\n");
	fprintf(f,"static int cdb_isdncard_idsorted[] = {");
	for (i=0; i < ce_idx; i++) {
		if (!(i%8))
			fprintf(f,"\n	");
		fprintf(f,"%d,",isdncard_id[i]);
	}
	fprintf(f,"\n};\n");
	return(0);
}

static int WriteVarios(FILE *f) {
	int i;

	fprintf(f, "/* driver database */\n");
	fprintf(f,"static cdb_isdn_vario cdb_isdnvario_info[] = {\n");
	for (i=0; i <= vario_idx; i++) {
		fprintf(f,"	{");
		fprintf(f,"%d,",varios[i].handle);
		fprintf(f,"%d,",varios[i].next_vario);
		fprintf(f,"%d,",varios[i].drvid);
		fprintf(f,"%d,",varios[i].typ);
		fprintf(f,"%d,",varios[i].subtyp);
		fprintf(f,"%d,",varios[i].smp);
		if (varios[i].mod_name)
			fprintf(f,"\"%s\",", varios[i].mod_name);
		else
			fprintf(f,"\"\",");
		if (varios[i].para_str)
			fprintf(f,"\"%s\",", varios[i].para_str);
		else
			fprintf(f,"\"\",");
		if (varios[i].mod_preload)
			fprintf(f,"\"%s\",", varios[i].mod_preload);
		else
			fprintf(f,"\"\",");
		if (varios[i].cfg_prog)
			fprintf(f,"\"%s\",", varios[i].cfg_prog);
		else
			fprintf(f,"\"\",");
		if (varios[i].firmware)
			fprintf(f,"\"%s\",", varios[i].firmware);
		else
			fprintf(f,"\"\",");
		if (varios[i].description)
			fprintf(f,"\"%s\",", varios[i].description);
		else
			fprintf(f,"\"\",");
		if (varios[i].need_pkg)
			fprintf(f,"\"%s\",", varios[i].need_pkg);
		else
			fprintf(f,"\"\",");
		if (varios[i].info)
			fprintf(f,"\"%s\",", varios[i].info);
		else
			fprintf(f,"\"\",");
		if (varios[i].protocol)
			fprintf(f,"\"%s\",", varios[i].protocol);
		else
			fprintf(f,"\"\",");
		if (varios[i].interface)
			fprintf(f,"\"%s\",", varios[i].interface);
		else
			fprintf(f,"\"\",");
		if (varios[i].io)
			fprintf(f,"\"%s\",", varios[i].io);
		else
			fprintf(f,"\"\",");
		if (varios[i].irq)
			fprintf(f,"\"%s\",", varios[i].irq);
		else
			fprintf(f,"\"\",");
		if (varios[i].membase)
			fprintf(f,"\"%s\",", varios[i].membase);
		else
			fprintf(f,"\"\",");
		if (varios[i].features)
			fprintf(f,"\"%s\",", varios[i].features);
		else
			fprintf(f,"\"\",");
		fprintf(f,"%d,",varios[i].card_ref);
		if (varios[i].name)
			fprintf(f,"\"%s\"", varios[i].name);
		else
			fprintf(f,"\"\"");
		fprintf(f,"},\n");
	}
	fprintf(f,"};\n");
	return(0);
}

int main(argc,argv)
int argc;
char **argv;
{
	char	line[256];
	int	l, version = 0x100;
	time_t	tim;
	if (argc<2) {
		fprintf(stderr, "Error no filename\n");
		exit(1);
	}
	if (!(stdin=freopen(argv[1],"rb", stdin))) {
		fprintf(stderr, "Cannot open %s as stdin\n", argv[1]);
		exit(2);
	}
	if (argc >2) {
		if (!(stdout=freopen(argv[2],"w", stdout))) {
			fprintf(stderr, "Cannot open %s as stdout\n", argv[2]);
			exit(3);
		}
	}
	cards = calloc(max_ce, sizeof(cdb_isdn_card));
	if (!cards) {
		fprintf(stderr, "cannot alloc card\n");
		fclose(stdin);
		exit(4);
	}
	varios = calloc(max_vario, sizeof(cdb_isdn_vario));
	if (!varios) {
		fprintf(stderr, "cannot alloc vario\n");
		fclose(stdin);
		free(cards);
		exit(5);
	}
	name_str = calloc(max_name_str_cnt, 1);
	if (!name_str) {
		fprintf(stderr, "cannot alloc name_str\n");
		fclose(stdin);
		free(cards);
		free(varios);
		exit(6);
	}
	vendors = calloc(max_ivendor, sizeof(cdb_isdn_vendor));
	if (!vendors) {
		fprintf(stderr, "cannot alloc vendors\n");
		fclose(stdin);
		free(cards);
		free(varios);
		free(name_str);
		exit(7);
	}
	BEGIN Main;
	yylex();

	SortVendors();
	SortCards();
	SortVarios();
	
	fprintf(stdout, "/* CDBISDN database */\n");
	fprintf(stdout,"const int  CDBISDN_DBVERSION = 0x%x;\n", version);
	time(&tim);
	strcpy(line,ctime(&tim));
	l = strlen(line);
	if (l)
		line[l-1] = 0;
	fprintf(stdout,"const char CDBISDN_DATE[]  = \"%s\";\n", line); 
	WriteVendors(stdout);
	WriteCards(stdout);
	WriteVarios(stdout);

	fclose(stdin);
	free(cards);
	free(name_str);
	free(vendors);
	free(varios);
	fprintf(stderr, "used cards(%d/%d)\n", ce_idx, max_ce);
	fprintf(stderr, "used varios(%d/%d)\n", vario_idx, max_vario);
	fprintf(stderr, "used vendors(%d/%d)\n", ivendor_idx, max_ivendor);
	fprintf(stderr, "used name_str(%d/%d)\n",name_str_cnt,  max_name_str_cnt);
	return(0);
}

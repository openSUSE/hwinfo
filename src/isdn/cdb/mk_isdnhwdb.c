#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lex.yy.c"
#include "hd.h"
#include "cdb_read.h"
#include "cdb_hwdb.h"


int yywrap(void) {
	return(1);
}

static int WriteNames(FILE *f) {
	char	*p, *e;
	int 	l, nc=0;

	fprintf(f, "! name data\n");
	fprintf(f, "$%02d %d\n", IWHREC_TYPE_NAME_SIZE, name_str_cnt);
	p = e = name_str;
	e += name_str_cnt;
	while (p < e) {
		l = strlen(p);
		fprintf(f, "$%02d %s\n", IWHREC_TYPE_NAME_DATA, p);
		p += l + 1;
		nc++;
	}
	fprintf(f, "$%02d %d\n", IWHREC_TYPE_NAME_COUNT, nc);
	return(0);
}

static int WriteVendors(FILE *f) {
	int i, nullidx, idx;

	fprintf(f, "! vendor database\n");
	fprintf(f, "$%02d %d\n", IWHREC_TYPE_VENDOR_COUNT, ivendor_idx);
	nullidx = strlen(name_str); /* first 0 in string array */
	for (i=0; i < ivendor_idx; i++) {
		fprintf(f,"$%02d ", IWHREC_TYPE_VENDOR_RECORD);
		if (vendors[i].name && vendors[i].name[0])
			idx = vendors[i].name - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (vendors[i].shortname && vendors[i].shortname[0])
			idx = vendors[i].shortname - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		fprintf(f,"%d ", vendors[i].vnr);
		fprintf(f,"%d\n",vendors[i].refcnt);
	}
	return(0);
}

static int WriteCards(FILE *f) {
	int i, nullidx, idx;

	fprintf(f, "! card database\n");
	fprintf(f, "$%02d %d\n", IWHREC_TYPE_CARD_COUNT, ce_idx);
	nullidx = strlen(name_str); /* first 0 in string array */

	for (i=0; i <= ce_idx; i++) {
		fprintf(f,"$%02d ", IWHREC_TYPE_CARD_RECORD);
		fprintf(f,"%d ",cards[i].handle);
		fprintf(f,"%d ",cards[i].vhandle);
		if (cards[i].name && cards[i].name[0])
			idx = cards[i].name - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (cards[i].lname && cards[i].lname[0])
			idx = cards[i].lname - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (cards[i].Class && cards[i].Class[0])
			idx = cards[i].Class - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (cards[i].bus && cards[i].bus[0])
			idx = cards[i].bus - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		fprintf(f,"%d ",cards[i].revision);
		fprintf(f,"%d ",cards[i].vendor);
		fprintf(f,"%d ",cards[i].device);
		fprintf(f,"%d ",cards[i].subvendor);
		fprintf(f,"%d ",cards[i].subdevice);
		fprintf(f,"%d ",cards[i].features);
		fprintf(f,"%d ",cards[i].line_cnt);
		fprintf(f,"%d ",cards[i].vario_cnt);
		fprintf(f,"%d\n",cards[i].vario);
	}
	for (i=0; i < ce_idx; i++) {
		fprintf(f,"$%02d %d\n", IWHREC_TYPE_CARD_IDSORTED, isdncard_id[i]);
	}
	return(0);
}

static int WriteVarios(FILE *f) {
	int i, nullidx, idx;

	fprintf(f, "! driver database\n");
	fprintf(f, "$%02d %d\n", IWHREC_TYPE_VARIO_COUNT, vario_idx);
	nullidx = strlen(name_str); /* first 0 in string array */

	for (i=0; i <= vario_idx; i++) {
		fprintf(f,"$%02d ", IWHREC_TYPE_VARIO_RECORD);
		fprintf(f,"%d ",varios[i].handle);
		fprintf(f,"%d ",varios[i].next_vario);
		fprintf(f,"%d ",varios[i].drvid);
		fprintf(f,"%d ",varios[i].typ);
		fprintf(f,"%d ",varios[i].subtyp);
		fprintf(f,"%d ",varios[i].smp);
		if (varios[i].mod_name && varios[i].mod_name[0])
			idx = varios[i].mod_name - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (varios[i].para_str && varios[i].para_str[0])
			idx = varios[i].para_str - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (varios[i].mod_preload && varios[i].mod_preload[0])
			idx = varios[i].mod_preload - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (varios[i].cfg_prog && varios[i].cfg_prog[0])
			idx = varios[i].cfg_prog - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (varios[i].firmware && varios[i].firmware[0])
			idx = varios[i].firmware - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (varios[i].description && varios[i].description[0])
			idx = varios[i].description - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (varios[i].need_pkg && varios[i].need_pkg[0])
			idx = varios[i].need_pkg - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (varios[i].info && varios[i].info[0])
			idx = varios[i].info - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (varios[i].protocol && varios[i].protocol[0])
			idx = varios[i].protocol - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (varios[i].interface && varios[i].interface[0])
			idx = varios[i].interface - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (varios[i].io && varios[i].io[0])
			idx = varios[i].io - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (varios[i].irq && varios[i].irq[0])
			idx = varios[i].irq - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (varios[i].membase && varios[i].membase[0])
			idx = varios[i].membase - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		if (varios[i].features && varios[i].features[0])
			idx = varios[i].features - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x ", idx);
		fprintf(f,"%d ",varios[i].card_ref);
		if (varios[i].name && varios[i].name[0])
			idx = varios[i].name - name_str;
		else
			idx = nullidx;
		fprintf(f,"%x\n", idx);
	}
	return(0);
}

int main(argc,argv)
int argc;
char **argv;
{
	char	line[256];
	int	l;
	time_t	tim;
	const char	*source_date_epoch;
	if (argc<2) {
		if (!freopen(CDBISDN_CDB_FILE,"rb", stdin)) {
			fprintf(stderr, "Cannot open %s as stdin\n", CDBISDN_CDB_FILE);
			exit(2);
		}
	} else {
		if (!freopen(argv[1],"rb", stdin)) {
			fprintf(stderr, "Cannot open %s as stdin\n", argv[1]);
			exit(2);
		}
	}
	if (argc >2) {
		if (strcmp(argv[2], "-")) { /* - := stdout */
			if (!freopen(argv[2],"w", stdout)) {
				fprintf(stderr, "Cannot open %s as stdout\n", argv[2]);
				exit(3);
			}
		}
	} else { /* default: CDBISDN_HWDB_FILE */
		if (!freopen(CDBISDN_HWDB_FILE,"w", stdout)) {
			fprintf(stderr, "Cannot open %s as stdout\n", CDBISDN_HWDB_FILE);
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
	
	fprintf(stdout, "! CDBISDN database version %x\n", CDB_DATAVERSION + 1);
	fprintf(stdout, "! file is build with mk_isdnhwdb\n");
	fprintf(stdout, "! Do not change this file !!!\n");
	fprintf(stdout,"$%02d %d\n", IWHREC_TYPE_VERSION, CDB_DATAVERSION + 1);
	if ((source_date_epoch = getenv("SOURCE_DATE_EPOCH")) == NULL || (tim = (time_t)strtol(source_date_epoch, NULL, 10)) <= 0)
		time(&tim);
	strcpy(line,asctime(gmtime(&tim)));
	l = strlen(line);
	if (l)
		line[l-1] = 0;
	fprintf(stdout,"$%02d %s\n", IWHREC_TYPE_DATE, line);
	WriteNames(stdout);
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

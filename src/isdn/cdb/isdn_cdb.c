#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "lex.yy.c"
#include "hd.h"
#include "cdb_read.h"

int yywrap(void) {
	return(1);
}

static int WriteVendors(FILE *f) {
	int i;

	fprintf(f, "/* vendor database */\n");
	fprintf(f,"static cdb_isdn_vendor cdb_isdnvendor_info_init[] = {\n");
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
	fprintf(f,"static cdb_isdn_card cdb_isdncard_info_init[] = {\n");
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
	fprintf(f,"static int cdb_isdncard_idsorted_init[] = {");
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
	fprintf(f,"static cdb_isdn_vario cdb_isdnvario_info_init[] = {\n");
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
	int	l;
	time_t	tim;
	const char	*source_date_epoch;
	if (argc<2) {
		fprintf(stderr, "Error no filename\n");
		exit(1);
	}
	if (!freopen(argv[1],"rb", stdin)) {
		fprintf(stderr, "Cannot open %s as stdin\n", argv[1]);
		exit(2);
	}
	if (argc >2) {
		if (!freopen(argv[2],"w", stdout)) {
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
	fprintf(stdout,"const int  CDBISDN_DBVERSION = 0x%x;\n", CDB_DATAVERSION);
	if ((source_date_epoch = getenv("SOURCE_DATE_EPOCH")) == NULL || (tim = (time_t)strtol(source_date_epoch, NULL, 10)) <= 0)
		time(&tim);
	strcpy(line,asctime(gmtime(&tim)));
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

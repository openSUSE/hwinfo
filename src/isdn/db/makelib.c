#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "hd.h"

#define MAX_LINE	32*1024
#define	PARA_NAMES	16
#define PARA_NAMES_LEN	32

const char *para_names[PARA_NAMES] = {"none", "IRQ", "MEMBASE", "IO", "IO0", "IO1",
				"IO2", "spare7", "BASE_ADRESS0", "BASE_ADRESS1",
				"BASE_ADRESS2", "BASE_ADRESS3", "BASE_ADRESS4",
				"BASE_ADRESS5","spare14", "spare15"};

char	p_names[PARA_NAMES*PARA_NAMES_LEN];
				

static ihw_card_info	*isdncard_info;
static ihw_driver_info	*driver_info;
static ihw_para_info	*parameter_info;
static unsigned long	*paralist;

static int		*isdncard_txt;
static int		*isdndrv_type;
static int		*isdncard_id;


static int	cname_str_cnt = 0;
static int	max_cname_str_cnt = 0;
static char	*cname_str = NULL;

static int	drv_str_cnt = 0;
static int	max_drv_str_cnt = 0;
static char	*drv_str = NULL;

static int	max_card_rec = 0;
static int	max_para_rec = 0;
static int	para_list_cnt = 0;
static int	max_driver_rec = 0;


typedef int (*fcmp) (const void *, const void *);

int compare_name(const int *c1, const int *c2) {
	return(strcasecmp(isdncard_info[*c1].name,
		isdncard_info[*c2].name));
}


int compare_type(const int *c1, const int *c2) {
	int x= driver_info[*c1].typ - driver_info[*c2].typ;

	if (!x)
		x=driver_info[*c1].subtyp - driver_info[*c2].subtyp;
	return(x);
}

int compare_id(const int *c1, const int *c2) {
	int x = isdncard_info[*c1].vendor - isdncard_info[*c2].vendor;

	if (!x)
		x = isdncard_info[*c1].device -
			isdncard_info[*c2].device;
	if (!x)
		x = isdncard_info[*c1].subvendor -
			isdncard_info[*c2].subvendor;
	if (!x)
		x = isdncard_info[*c1].subdevice -
			isdncard_info[*c2].subdevice;
	return(x);
}


char *extract_quotes(char *s)
{
	char	*p, *r;

	if (!s)
		return(NULL);
	r = NULL;
	p = s;
	while (*s) {
		if (*s == '"') {
			r = ++s;
			break;
		}
		s++;
	}
	while (*s) {
		if (*s == '"') {
			*s = 0;
			break;
		}
		s++;
	}
	if (r && *r)
		return(r);
	else
		return(NULL);
}

char *add_card_name(char *n)
{
	char	*s, *p;
	int	l;
	
	s = extract_quotes(n);
	if (!s)
		return(NULL);
	p = cname_str + cname_str_cnt;
	l = strlen(s) +1;
	if ((p + l) >= (cname_str + max_cname_str_cnt))
		return(NULL);
	strcpy(p, s);
	cname_str_cnt += l;
	return(p);
}

char *add_drv_str(char *s, int merge) {
	char	*p, *str;
	int	l;
	
	str = extract_quotes(s);
	if (!str)
		return(NULL);
	if (merge) {
		p = drv_str;
		while (*p) {
			if (!strcmp(p, str))
				break;
			p += strlen(p) +1;
			if (p >= (drv_str + max_drv_str_cnt))
				return(NULL);
		}
		if (*p)
			return(p);
	} else {
		p = drv_str + drv_str_cnt;
	}
	l = strlen(str) +1;
	if ((p + l) >= (drv_str + max_drv_str_cnt))
		return(NULL);
	strcpy(p, str);
	drv_str_cnt += l;
	return(p);
}

int chk_drvid(int id)
{
	int i=0;

	for (i=0;i<max_driver_rec; i++)
		if (driver_info[i].drvid == id)
			break;
	return(i);
}

int new_card(char *s)
{
	char	*t;
	
	max_card_rec++;
	isdncard_txt[max_card_rec] = max_card_rec;
	isdncard_id[max_card_rec] = max_card_rec;
	isdncard_info[max_card_rec].handle = max_card_rec;
	t = strtok(s, ";");
	isdncard_info[max_card_rec].name = add_card_name(t);
	fprintf(stderr, "add_card:%s",
		isdncard_info[max_card_rec].name);
	t = strtok(NULL, ";");
	sscanf(t, "%d", &isdncard_info[max_card_rec].Class);
	fprintf(stderr, " C%d", isdncard_info[max_card_rec].Class);
	t = strtok(NULL, ";");
	sscanf(t, "%x", &isdncard_info[max_card_rec].vendor);
	fprintf(stderr, " 0x%x", isdncard_info[max_card_rec].vendor);
	t = strtok(NULL, ";");
	sscanf(t, "%x", &isdncard_info[max_card_rec].device);
	fprintf(stderr, "/0x%x", isdncard_info[max_card_rec].device);
	t = strtok(NULL, ";");
	sscanf(t, "%x", &isdncard_info[max_card_rec].subvendor);
	fprintf(stderr, "/0x%x", isdncard_info[max_card_rec].subvendor);
	t = strtok(NULL, ";");
	sscanf(t, "%x", &isdncard_info[max_card_rec].subdevice);
	fprintf(stderr, "/0x%x", isdncard_info[max_card_rec].subdevice);
	t = strtok(NULL, ";");
	sscanf(t, "%x", &isdncard_info[max_card_rec].features);
	fprintf(stderr, "/0x%x", isdncard_info[max_card_rec].features);
	t = strtok(NULL, ";");
	sscanf(t, "%x", &isdncard_info[max_card_rec].line_cnt);
	fprintf(stderr, "/0x%x", isdncard_info[max_card_rec].line_cnt);
	isdncard_info[max_card_rec].driver = -1;
	fprintf(stderr, "\n");
	return(0);
}

int new_driver(char *s)
{
	char	*t;
	int	idx;
	
	max_driver_rec++;
	isdndrv_type[max_driver_rec] = max_driver_rec;
	driver_info[max_driver_rec].handle = max_driver_rec;
	driver_info[max_driver_rec].next_drv = -1;
	driver_info[max_driver_rec].card_ref = max_card_rec;
	if (isdncard_info[max_card_rec].driver == -1)
		isdncard_info[max_card_rec].driver = max_driver_rec;
	else
		driver_info[max_driver_rec-1].next_drv = max_driver_rec;
	t = strtok(s, ";");
	driver_info[max_driver_rec].name = add_drv_str(t, 1);
	fprintf(stderr, "add_drv:%s",
		driver_info[max_driver_rec].name);
	t = strtok(NULL, ";");
	driver_info[max_driver_rec].mod_name = add_drv_str(t, 1);
	fprintf(stderr, "add_drv:%s",
		driver_info[max_driver_rec].mod_name);
	t = strtok(NULL, ";");
	sscanf(t, "%d", &driver_info[max_driver_rec].typ);
	fprintf(stderr, " %d", driver_info[max_driver_rec].typ);
	t = strtok(NULL, ";");
	sscanf(t, "%d", &driver_info[max_driver_rec].subtyp);
	fprintf(stderr, "/%d", driver_info[max_driver_rec].subtyp);
	driver_info[max_driver_rec].drvid = driver_info[max_driver_rec].typ +
		(driver_info[max_driver_rec].subtyp * 0x10000);
	idx=chk_drvid(driver_info[max_driver_rec].drvid);
	if (idx != max_driver_rec) {
		fprintf(stderr, "duplicate drvid %d/%d at %d and %d\n",
			driver_info[max_driver_rec].typ,
			driver_info[max_driver_rec].subtyp,
			idx, max_driver_rec);
		exit(1);
	}
	t = strtok(NULL, ";");
	sscanf(t, "%x", &driver_info[max_driver_rec].arch);
	fprintf(stderr, " A%x", driver_info[max_driver_rec].arch);
	t = strtok(NULL, ";");
	sscanf(t, "%x", &driver_info[max_driver_rec].features);
	fprintf(stderr, " F%x", driver_info[max_driver_rec].features);
	t = strtok(NULL, ";");
	driver_info[max_driver_rec].para_str = add_drv_str(t, 0);
	fprintf(stderr, " P:%s",
		driver_info[max_driver_rec].para_str ? 
		driver_info[max_driver_rec].para_str : "\"\"");
	t = strtok(NULL, ";");
	driver_info[max_driver_rec].mod_preload = add_drv_str(t, 0);
	fprintf(stderr, " M:%s",
		driver_info[max_driver_rec].mod_preload ? 
		driver_info[max_driver_rec].mod_preload : "\"\"");
	t = strtok(NULL, ";");
	driver_info[max_driver_rec].cfg_prog = add_drv_str(t, 0);
	fprintf(stderr, " C:%s",
		driver_info[max_driver_rec].cfg_prog ? 
		driver_info[max_driver_rec].cfg_prog : "\"\"");
	t = strtok(NULL, ";");
	driver_info[max_driver_rec].firmware = add_drv_str(t, 0);
	fprintf(stderr, " F:%s",
		driver_info[max_driver_rec].firmware ? 
		driver_info[max_driver_rec].firmware : "\"\"");
	t = strtok(NULL, ";");
	driver_info[max_driver_rec].description = add_drv_str(t, 0);
	fprintf(stderr, " D:%s",
		driver_info[max_driver_rec].description ? 
		driver_info[max_driver_rec].description : "\"\"");
	t = strtok(NULL, ";");
	driver_info[max_driver_rec].need_pkg = add_drv_str(t, 0);
	fprintf(stderr, " N:%s",
		driver_info[max_driver_rec].need_pkg ?
		driver_info[max_driver_rec].need_pkg : "\"\"");
	fprintf(stderr, "\n");
	return(0);
}

int new_para(char *s)
{
	char	*t;
	int	cnt, i;
	
	max_para_rec++;
	if (isdncard_info[max_card_rec].paracnt) {
		isdncard_info[max_card_rec].paracnt++;
	} else {
		isdncard_info[max_card_rec].paracnt = 1;
		isdncard_info[max_card_rec].para = max_para_rec;
	}
	parameter_info[max_para_rec].list = NULL;
	t = strtok(s, ";");
	sscanf(t, "%d", &parameter_info[max_para_rec].type);
	parameter_info[max_para_rec].name =
		para_names[parameter_info[max_para_rec].type];
	fprintf(stderr, "add_para:%s", parameter_info[max_para_rec].name);
	t = strtok(NULL, ";");
	sscanf(t, "%lx", &parameter_info[max_para_rec].def_value);
	fprintf(stderr, " D%lx", parameter_info[max_para_rec].def_value);
	t = strtok(NULL, ";");
	sscanf(t, "%x", &parameter_info[max_para_rec].flags);
	fprintf(stderr, " F%x", parameter_info[max_para_rec].flags);
	t = strtok(NULL, ";");
	sscanf(t, "%d", &cnt);
	fprintf(stderr, " L%d", cnt);
	if (cnt) {
		parameter_info[max_para_rec].list = &paralist[para_list_cnt];
		paralist[para_list_cnt++] = cnt;
		for (i=0;i<cnt;i++) {
			t = strtok(NULL, ";");
			sscanf(t, "%lx", &paralist[para_list_cnt]);
			fprintf(stderr, " %lx", paralist[para_list_cnt]);
			para_list_cnt++;
		}
	} else
		parameter_info[max_para_rec].list = NULL;
	fprintf(stderr, "\n");
	return(0);
}

int main(argc,argv)
int argc;
char *argv[];
{
	FILE		*f;
	char		*s, c, line[MAX_LINE], ofn[1024];
	int		i,j;
	int		ver_major = 0;
	int		ver_minor = 0;
	int		version = 0;
	time_t		tim;

	if (argc<2) {
		fprintf(stderr, "Error no filename\n");
		exit(1);
	}
	if (!(f=fopen(argv[1],"rb"))) {
		printf("Cannot open %s\n", argv[1]);
		exit(1);
	}
	if (!(stderr = freopen("makelib.log", "w", stderr))) {
		printf("Cannot redirect stderr to %s\n",
			"makelib.log");
		exit(1);
	}
	while ((s = fgets(line, MAX_LINE, f))) {
		c = *s++;
		switch(c) {
			case 'C':
				fprintf(stderr, "c");
				max_card_rec++;
				break;
			case 'D':
				fprintf(stderr, "d");
				max_driver_rec++;
				break;
			case 'P':
				fprintf(stderr, "p");
				max_para_rec++;
				break;
			case 0:
				fprintf(stderr, "\nread nothing\n");
				break;
			case 'V':
				sscanf(s,"%d %d", &ver_major, &ver_minor);
				version = ver_major*0x10000 + ver_minor;
				break;
			case '#':
			case '\n':
				break;
			default:
				fprintf(stderr, "\nNot a key: %c\n", c);
				break;
		}
	}
	fclose(f);
	fprintf(stderr, "\n");
	fprintf(stderr, "Found %d cards %d drivers %d parameters\n",
		max_card_rec, max_driver_rec, max_para_rec);
	max_card_rec++;
	max_driver_rec++;
	max_cname_str_cnt = max_card_rec*128;
	max_drv_str_cnt = max_driver_rec*2048;
	para_list_cnt = max_para_rec * 64;
	
	if(!(isdncard_txt = (int *) malloc(max_card_rec*sizeof(int))))
		exit(1);
	if(!(isdndrv_type = (int *) malloc(max_driver_rec*sizeof(int))))
		exit(1);
	if(!(isdncard_id = (int *) malloc(max_card_rec*sizeof(int))))
		exit(1);
	if(!(isdncard_info = (ihw_card_info *) malloc(max_card_rec*
				sizeof(ihw_card_info))))
		exit(1);
	memset(isdncard_info, 0, max_card_rec*sizeof(ihw_card_info));
	if(!(driver_info = (ihw_driver_info *) malloc(max_driver_rec*sizeof(ihw_driver_info))))
		exit(1);
	memset(driver_info, 0, max_driver_rec*sizeof(ihw_driver_info));
	if(!(drv_str = (char *) malloc(max_drv_str_cnt)))
		exit(1);
	memset(drv_str, 0, max_drv_str_cnt);
	if(!(cname_str = (char *) malloc(max_cname_str_cnt)))
		exit(1);
	memset(cname_str, 0, max_cname_str_cnt);
	if(!(drv_str = (char *) malloc(max_drv_str_cnt)))
		exit(1);
	memset(drv_str, 0, max_drv_str_cnt);
	if (!(f=fopen(argv[1],"rb"))) {
		printf("Cannot open %s\n", argv[1]);
		exit(1);
	}
	if (!(paralist = (unsigned long *)malloc(para_list_cnt*sizeof(unsigned long)))) 
		exit(1);
	memset(paralist, 0, para_list_cnt*sizeof(unsigned long));
	if (!(parameter_info = (ihw_para_info *)malloc(max_para_rec*sizeof(ihw_para_info))))
		exit(1);
	memset(parameter_info, 0, max_para_rec*sizeof(ihw_para_info));
	max_card_rec = -1;
	max_driver_rec = -1;
	max_para_rec = -1;
	para_list_cnt = 0;
	while ((s = fgets(line, MAX_LINE, f))) {
		c = *s++;
		switch(c) {
			case 'C':
				new_card(s);
				break;
			case 'D':
				new_driver(s);
				break;
			case 'P':
				new_para(s);
				break;
			case '#':
			case 'V':
				break;
			case 0:
				fprintf(stderr, "read nothing\n");
				break;
			case '\n':
				break;
			default:
				fprintf(stderr, "Not a keyword: %s\n", s);
				break;
		}
	}
	fclose(f);
	fprintf(stderr, "got %d paralist entries\n", para_list_cnt);
	fprintf(stderr, "inserted %d bytes in drv_str and %d in cname_str\n",
		drv_str_cnt, cname_str_cnt);
	max_card_rec++;
	max_driver_rec++;
	max_para_rec++;
	qsort(isdncard_txt, max_card_rec, sizeof(int), (fcmp)compare_name);
	for (i=0; i<max_card_rec; i++) {
		fprintf(stderr, "%3d -> %3d\n",
			isdncard_info[isdncard_txt[i]].handle, i);
		isdncard_info[isdncard_txt[i]].handle = i;
	}
	for (i=0; i<max_driver_rec; i++) {
		fprintf(stderr, "%3d -> %3d\n",
			driver_info[i].card_ref,
			isdncard_info[driver_info[i].card_ref].handle);
		driver_info[i].card_ref =
			isdncard_info[driver_info[i].card_ref].handle;
	}
	qsort(isdncard_id, max_card_rec, sizeof(int), (fcmp)compare_id);
	
	qsort(isdndrv_type, max_driver_rec, sizeof(int), (fcmp)compare_type);
	for (i=0; i<max_driver_rec; i++) {
		fprintf(stderr, "%3d -> %3d\n",
			driver_info[isdndrv_type[i]].handle, i);
		driver_info[isdndrv_type[i]].handle = i;
	}
	for (i=0; i<max_driver_rec; i++) {
		if (driver_info[i].next_drv >= 0) {
			fprintf(stderr, "%3d -> %3d\n",
				driver_info[i].next_drv,
				driver_info[driver_info[i].next_drv].handle);
			driver_info[i].next_drv =
				driver_info[driver_info[i].next_drv].handle;
		} else
			fprintf(stderr, "%3d -> %3d\n",
				-1, -1);
	}
	for (i=0; i<max_card_rec; i++) {
		fprintf(stderr, "%3d -> %3d\n",
			isdncard_info[i].driver,
			driver_info[isdncard_info[i].driver].handle);
		isdncard_info[i].driver =
			driver_info[isdncard_info[i].driver].handle;
	}
	if (argc<3) {
		fprintf(stderr, "no out filename using ihw_db.h\n");
		strcpy(ofn, "ihw_db.h");
	} else
		strcpy(ofn, argv[2]);
	if (!(f=fopen(ofn,"w"))) {
		printf("Cannot open outfile %s\n", ofn);
		exit(1);
	}
	fprintf(f,"/* autogen by makeidb */\n");
	fprintf(f,"\n");
	fprintf(f,"const int  IHWDB_VERSION = %d;\n", version);
	time(&tim);
	strcpy(line,ctime(&tim));
	i = strlen(line);
	if (i)
		line[i-1] = 0;
	fprintf(f,"const char IHWDB_DATE[]  = \"%s\";\n", line); 
	fprintf(f,"static ihw_card_info isdncard_info[] = {\n");
	for (i=0; i< max_card_rec; i++) {
		fprintf(f,"{");
		fprintf(f,"%d,",isdncard_info[isdncard_txt[i]].handle);
		fprintf(f,"\"%s\",", isdncard_info[isdncard_txt[i]].name);
		fprintf(f,"%d,",isdncard_info[isdncard_txt[i]].Class);
		fprintf(f,"%d,",isdncard_info[isdncard_txt[i]].vendor);
		fprintf(f,"%d,",isdncard_info[isdncard_txt[i]].device);
		fprintf(f,"%d,",isdncard_info[isdncard_txt[i]].subvendor);
		fprintf(f,"%d,",isdncard_info[isdncard_txt[i]].subdevice);
		fprintf(f,"%d,",isdncard_info[isdncard_txt[i]].features);
		fprintf(f,"%d,",isdncard_info[isdncard_txt[i]].line_cnt);
		fprintf(f,"%d,",isdncard_info[isdncard_txt[i]].driver);
		fprintf(f,"%d,",isdncard_info[isdncard_txt[i]].paracnt);
		fprintf(f,"%d",isdncard_info[isdncard_txt[i]].para);
		fprintf(f,"},\n");
	}
	/* dummy entry */
	fprintf(f,"{");
	fprintf(f,"0,");
	fprintf(f,"\"   \",");
	fprintf(f,"0,");
	fprintf(f,"0,");
	fprintf(f,"0,");
	fprintf(f,"0,");
	fprintf(f,"0,");
	fprintf(f,"0,");
	fprintf(f,"0,");
	fprintf(f,"0");
	fprintf(f,"},\n");
	fprintf(f,"};\n");
	for (i=0; i< max_para_rec; i++) {
		if (parameter_info[i].list) {
			fprintf(f,"const long ihw_plist_%04x[] = ", i);
			fprintf(f,"{%ld", parameter_info[i].list[0]);
			for (j=1; (unsigned) j<=parameter_info[i].list[0]; j++) {
				fprintf(f,",%ld", parameter_info[i].list[j]);
			}
			fprintf(f,"};\n");
		}
	}
	fprintf(f,"\n");
	fprintf(f,"const ihw_para_info parameter_info[] = {\n");
	for (i=0; i< max_para_rec; i++) {
		fprintf(f,"{");
		fprintf(f,"\"%s\",", para_names[parameter_info[i].type]);
		fprintf(f,"%d,",parameter_info[i].type);
		fprintf(f,"%d,",parameter_info[i].flags);
		fprintf(f,"%ld,",parameter_info[i].def_value);
		fprintf(f,"%ld,",parameter_info[i].bytecnt);
		if (parameter_info[i].list)
			fprintf(f,"ihw_plist_%04x", i);
		else
			fprintf(f,"NULL");
		fprintf(f,"},\n");
	}
	fprintf(f,"};\n");
	fprintf(f,"static ihw_driver_info driver_info[] = {\n");
	for (i=0; i< max_driver_rec; i++) {
		fprintf(f,"{");
		fprintf(f,"%d,",driver_info[isdndrv_type[i]].handle);
		fprintf(f,"%d,",driver_info[isdndrv_type[i]].next_drv);
		fprintf(f,"%d,",driver_info[isdndrv_type[i]].drvid);
		fprintf(f,"%d,",driver_info[isdndrv_type[i]].typ);
		fprintf(f,"%d,",driver_info[isdndrv_type[i]].subtyp);
		if (driver_info[isdndrv_type[i]].mod_name)
			fprintf(f,"\"%s\",", driver_info[isdndrv_type[i]].mod_name);
		else
			fprintf(f,"\"\",");
		if (driver_info[isdndrv_type[i]].para_str)
			fprintf(f,"\"%s\",", driver_info[isdndrv_type[i]].para_str);
		else
			fprintf(f,"\"\",");
		if (driver_info[isdndrv_type[i]].mod_preload)
			fprintf(f,"\"%s\",", driver_info[isdndrv_type[i]].mod_preload);
		else
			fprintf(f,"\"\",");
		if (driver_info[isdndrv_type[i]].cfg_prog)
			fprintf(f,"\"%s\",", driver_info[isdndrv_type[i]].cfg_prog);
		else
			fprintf(f,"\"\",");
		if (driver_info[isdndrv_type[i]].firmware)
			fprintf(f,"\"%s\",", driver_info[isdndrv_type[i]].firmware);
		else
			fprintf(f,"\"\",");
		if (driver_info[isdndrv_type[i]].description)
			fprintf(f,"\"%s\",", driver_info[isdndrv_type[i]].description);
		else
			fprintf(f,"\"\",");
		if (driver_info[isdndrv_type[i]].need_pkg)
			fprintf(f,"\"%s\",", driver_info[isdndrv_type[i]].need_pkg);
		else
			fprintf(f,"\"\",");
		fprintf(f,"%d,",driver_info[isdndrv_type[i]].arch);
		fprintf(f,"%d,",driver_info[isdndrv_type[i]].features);
		fprintf(f,"%d,",driver_info[isdndrv_type[i]].card_ref);
		if (driver_info[isdndrv_type[i]].name)
			fprintf(f,"\"%s\"", driver_info[isdndrv_type[i]].name);
		else
			fprintf(f,"\"\"");
		fprintf(f,"},\n");
	}
	/* dummy entry */
	fprintf(f,"{");
	fprintf(f,"%d,",0);
	fprintf(f,"%d,",0);
	fprintf(f,"%d,",0);
	fprintf(f,"%d,",0);
	fprintf(f,"%d,",0);
	fprintf(f,"\"\",");
	fprintf(f,"\"\",");
	fprintf(f,"\"\",");
	fprintf(f,"\"\",");
	fprintf(f,"\"\",");
	fprintf(f,"\"\",");
	fprintf(f,"%d,",0);
	fprintf(f,"%d,",0);
	fprintf(f,"%d", 0);
	fprintf(f,"},\n");
	fprintf(f,"};\n");
	fprintf(f,"const int isdncard_id[] = {\n");
	for (i=0; i< max_card_rec; i++) {
		fprintf(f,"%d,\n", isdncard_info[isdncard_id[i]].handle);
	}
	fprintf(f,"};\n");
	fprintf(f,"const int isdndrv_type[] = {\n");
	for (i=0; i< max_driver_rec; i++) {
		fprintf(f,"%d,\n", driver_info[isdndrv_type[i]].handle);
	}
	fprintf(f,"};\n");
	fprintf(f,"\n");
	fclose(f);
	return(0);
}

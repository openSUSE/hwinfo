#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include "isdn_cdb_def.h"
#include "hd.h"

#define CDB_DATAVERSION	0x101
#define MAXCARDS	300
#define MAXVARIO	(MAXCARDS*4)
#define MAXNAMECNT	(MAXCARDS*256)

extern int		max_ce;
extern int		ce_idx;
extern int		max_vario;
extern int		vario_idx;
extern int		name_str_cnt;
extern int		max_name_str_cnt;
extern int		max_ivendor;
extern int		ivendor_idx;

extern char		*name_str;
extern cdb_isdn_card	*cards;
extern cdb_isdn_vario	*varios;
extern cdb_isdn_vendor	*vendors;

extern int		*isdncard_id;

extern int		drvid_cnt;
extern int		drv_subtyp_cnt;
extern int		drv_typ_cnt;
extern int		supported_cnt;

extern int		not_supported;

struct _vendorshortnames_t {
	char	*lname;
	char    *sname;
};

typedef int (*fcmp) (const void *, const void *);


extern void	del_vario(void);
extern int	new_entry(void);
extern void	add_current_item(int item, char *val);
extern void	SortVendors(void);
extern void	SortCards(void);
extern void	SortVarios(void);

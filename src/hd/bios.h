/*
 * smp info according to Intel smp spec (ia32)
 */
typedef struct {
  unsigned ok:1;		/* data are valid */
  unsigned rev;			/* MP spec revision */
  unsigned mpfp;		/* MP Floating Pointer struct */
  unsigned mpconfig_ok:1;	/* MP config table valid */
  unsigned mpconfig;		/* MP config table */
  unsigned mpconfig_size;	/* dto, size */
  unsigned char feature[5];	/* MP feature info */
  char oem_id[9];		/* oem id */
  char prod_id[13];		/* product id */
  unsigned cpus, cpus_en;	/* number of cpus & enabled cpus */
} smp_info_t;

/*
 * for memory areas
 */
typedef struct {
  unsigned start, size;         /* base address & size */
  unsigned char *data;          /* actual data */
} memory_range_t;


#define BIOS_ROM_START  0xc0000
#define BIOS_ROM_SIZE   0x40000

#define BIOS_RAM_START  0x400
#define BIOS_RAM_SIZE   0x100

void hd_scan_bios(hd_data_t *hd_data);

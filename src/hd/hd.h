#ifdef __cplusplus
extern "C" {
#endif


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 *                      libhd data structures
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

/*
 * debug flags
 */
#define HD_DEB_SHOW_LOG		(1 <<  0)
#define HD_DEB_PROGRESS		(1 <<  1)
#define HD_DEB_CREATION		(1 <<  2)
#define HD_DEB_DRIVER_INFO	(1 <<  3)
#define HD_DEB_PCI		(1 <<  4)
#define HD_DEB_ISAPNP		(1 <<  5)
#define HD_DEB_CDROM		(1 <<  6)
#define HD_DEB_NET		(1 <<  7)
#define HD_DEB_FLOPPY		(1 <<  8)
#define HD_DEB_MISC		(1 <<  9)
#define HD_DEB_SERIAL		(1 << 10)
#define HD_DEB_MONITOR		(1 << 11)
#define HD_DEB_CPU		(1 << 12)
#define HD_DEB_BIOS		(1 << 13)
#define HD_DEB_MOUSE		(1 << 14)
#define HD_DEB_IDE		(1 << 15)
#define HD_DEB_SCSI		(1 << 16)


/*
 * flags to control the probing.
 *
 * Note: only 32 features are supported at this time; if you want more,
 * change the definition of hd_data_t.probe
 */
enum probe_feature {
  pr_default = 1, pr_memory, pr_pci, pr_pci_range, pr_isapnp, pr_cdrom,
  pr_cdrom_info, pr_net, pr_floppy, pr_misc, pr_misc_serial, pr_misc_par,
  pr_misc_floppy, pr_serial, pr_cpu, pr_bios, pr_monitor, pr_mouse, pr_ide,
  pr_scsi,
  pr_all	/* pr_all must be the last */
};


/*
 * define a 64 bit unsigned int
 */
#include <limits.h>

#if ULONG_MAX > 0xfffffffful
#define HD_LL "l"
typedef unsigned long uint64;
#else
#define HD_LL "ll"
typedef unsigned long long uint64;
#endif


/*
 * device base classes and bus types
 *
 */

/* base class values (superset of PCI classes) */
enum base_classes {
  /* these *must* match standard PCI class numbers */
  bc_none, bc_storage, bc_network, bc_display, bc_multimedia,
  bc_memory, bc_bridge, bc_comm, bc_system, bc_input, bc_docking,
  bc_processor, bc_serial, bc_other = 0xff,

  // add our own classes here (starting at 0x100 as PCI values are 8 bit)
  bc_monitor = 0x100, bc_internal, bc_modem, bc_isdn, bc_ps2, bc_mouse,
  bc_storage_device, bc_network_interface
};

/* subclass values of bc_storage */
enum sc_storage {
  sc_sto_scsi, sc_sto_ide, sc_sto_floppy, sc_sto_ipi, sc_sto_raid,
  sc_sto_other = 0x80
};

/* subclass values of bc_display */
enum sc_display {
  sc_dis_vga, sc_dis_xga, sc_dis_other = 0x80
};

/* subclass values of bc_comm */
enum sc_comm {
  sc_com_ser, sc_com_par, sc_com_other = 0x80
};

/* subclass values of bc_system */
enum sc_system {
  sc_sys_pic, sc_sys_dma, sc_sys_timer, sc_sys_rtc, sc_sys_other = 0x80
};

/* subclass values of bc_input */
enum sc_input {
  sc_inp_keyb, sc_inp_digit, sc_inp_mouse, sc_inp_other = 0x80
};

/* internal sub class values */
enum sc_internal {
  sc_int_none, sc_int_isapnp_if, sc_int_main_mem, sc_int_cpu, sc_int_fpu, sc_int_bios
};

/* subclass values of bc_mouse */
enum sc_mouse {
  sc_mou_ps2, sc_mou_ser, sc_mou_bus
};

/* subclass values of bc_storage_device */
enum sc_std {
  sc_sdev_disk, sc_sdev_tape, sc_sdev_cdrom, sc_sdev_floppy,
  sc_sdev_other = 0x80
};

/* subclass values of bc_network_interface */
enum sc_net_if {
  sc_nif_loopback, sc_nif_ethernet, sc_nif_tokenring, sc_nif_fddi, sc_nif_other = 0x80
};

/* bus type values similar to PCI bridge subclasses */
enum bus_types {
  bus_none, bus_isa, bus_eisa, bus_mc, bus_pci, bus_pcmcia, bus_nubus,
  bus_cardbus, bus_other,

  /* outside the range of the PCI values */
  bus_ps2 = 0x80, bus_serial, bus_parallel, bus_floppy, bus_scsi, bus_ide
};

/*
 * Used whenever we create a list of strings (e.g. file read).
 */
typedef struct s_str_list_t {
  struct s_str_list_t *next;			/* linked list */
  char *str;  					/* some string  */
} str_list_t;

/*
 * structure holding the (raw) PCI data
 */
typedef struct s_pci_t {
  struct s_pci_t *next;				/* linked list */
  unsigned data_len;				/* the actual length of the data field */
  unsigned char data[256];			/* the PCI data */
  char *log;					/* log messages */
  unsigned flags,				/* various info, see enum pci_flags */
           cmd,					/* PCI_COMMAND */
           hdr_type;				/* PCI_HEADER_TYPE */
  unsigned bus,					/* PCI bus #, *nothing* to do with hw_t.bus */
           slot, func; 				/* slot & function */
  unsigned base_class, sub_class, prog_if;	/* PCI device classes */
  unsigned dev, vend, sub_dev, sub_vend, rev;	/* vendor & device ids */
  unsigned irq;					/* used irq, if any */
  unsigned base_addr[6];			/* I/O or memory base */
  unsigned base_len[6];				/* I/O or memory ranges */
  unsigned rom_base_addr;			/* memory base for card ROM */
  unsigned rom_base_len;			/* memory range for card ROM */
} pci_t;

/*
 * pci related flags cf. (pci_t).flags
 */
enum pci_flags {
  pci_flag_ok, pci_flag_pm, pci_flag_agp
};


/*
 *structures to hold the (raw) ISA-PnP data
 */
typedef struct {
  int len;
  int type;
  unsigned char *data;
} isapnp_res_t;

typedef struct {
  int csn;
  int log_devs;
  unsigned char *serial;
  unsigned char *card_regs;
  unsigned char (*ldev_regs)[0xd0];
  int res_len;
  unsigned broken:1;		/* mark a broken card */
  isapnp_res_t *res;
} isapnp_card_t;

typedef struct {
  int read_port;
  int cards;
  isapnp_card_t *card;
} isapnp_t;

typedef struct {
  isapnp_card_t *card;
  int dev;
  unsigned flags;				/* cf. enum isapnp_flags */
} isapnp_dev_t;

/*
 * ISA-PnP related flags; cf. (isapnp_dev_t).flags
 */
enum isapnp_flags {
  isapnp_flag_act
};


/*
 * special CDROM entry
 */
// ###### rename these! (cf. mouse_info_t!!!)
typedef struct {
  char *volume, *publisher, *preparer, *application, *creation_date;
} cdrom_info_t;

typedef struct {
  unsigned char block0[512];
} floppy_info_t;

/*
 * bios data
 */
typedef struct {
  unsigned apm_supported:1;
  unsigned apm_enabled:1;
  unsigned apm_ver, apm_subver;
  unsigned apm_bios_flags;

  unsigned vbe_ver;
  unsigned vbe_video_mem;

  unsigned ser_port0, ser_port1, ser_port2, ser_port3;
  unsigned par_port0, par_port1, par_port2;

  /* The id is still in big endian format! */
  unsigned is_pnp_bios:1;
  unsigned pnp_id;
} bios_info_t;


enum cpu_arch {
  arch_unknown = 0,
  arch_intel, arch_alpha, arch_sparc, arch_sparc64, arch_ppc, arch_68k
};

enum boot_arch {
  boot_unknown = 0,
  boot_lilo, boot_milo, boot_aboot, boot_silo, boot_ppc
};

/* special cpu entry */
typedef struct {
  enum cpu_arch architecture;
  unsigned family;		/* cpu variation on alpha  */
  unsigned model;		/* cpu revision on alpha  */
  unsigned stepping;
  unsigned cache;
  unsigned clock;
  char *vend_name;		/* system type on alpha  */
  char *model_name;		/* cpu model on alpha  */
} cpu_info_t;

/*
 * resource types
 */
enum resource_types {
  res_any, res_phys_mem, res_mem, res_io,
  res_irq, res_dma, res_monitor, res_size,
  res_disk_geo, res_cache, res_clock, res_baud,
  res_disk_log_geo, res_disk_phys_geo
};


/*
 * size units (cf. (res_size_t).unit)
 */
enum size_units {
  size_unit_cm, size_unit_cinch, size_unit_byte, size_unit_sectors,
  size_unit_kbyte, size_unit_mbyte, size_unit_gbyte
};

/*
 * access types for I/O and memory resources
 */
enum access_flags {
  acc_unknown, acc_ro, acc_wo, acc_rw		/* unknown, read only, write only, read/write */
};

enum yes_no_flag {
  flag_unknown, flag_no, flag_yes		/* unknown, no, yes */
};


/*
 * definitions for the various resource types
 */
typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
} res_any_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  uint64 base, range;
  unsigned
    enabled:1,				/* 0: disabled, 1 enabled */
    access:2,				/* enum access_flags */
    prefetch:2;				/* enum yes_no_flag */
} res_mem_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  uint64 range;
} res_phys_mem_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  uint64 base, range;
  unsigned
    enabled:1,				/* 0: disabled, 1 enabled */
    access:2;				/* enum access_flags */
} res_io_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  unsigned base;
  unsigned triggered;			/* # of interrupts */
  unsigned enabled:1;			/* 0: disabled, 1 enabled */
} res_irq_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  unsigned base;
  unsigned enabled:1;			/* 0: disabled, 1 enabled */
} res_dma_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  enum size_units unit;
  unsigned val1, val2;			/* to allow for 2D values */
} res_size_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  unsigned speed;
} res_baud_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  unsigned size;			/* in kbyte */
} res_cache_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  unsigned cyls, heads, sectors;
  unsigned logical:1;			/* logical/physical geometry */
} res_disk_geo_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  unsigned width, height;		/* in pixel */
  unsigned vfreq;			/* in Hz */
} res_monitor_t;

typedef union u_hd_res_t {
  union u_hd_res_t *next;  
  res_any_t any;
  res_io_t io;
  res_mem_t mem;
  res_phys_mem_t phys_mem;
  res_irq_t irq;
  res_dma_t dma;
  res_size_t size;
  res_cache_t cache;
  res_baud_t baud;
  res_disk_geo_t disk_geo;
  res_monitor_t monitor;
} hd_res_t;


/*
 * data gathered by the misc module; basically resources from /proc
 */
typedef struct {
  uint64 addr, size;
  char *dev;
  unsigned tag;
} misc_io_t;

typedef struct {
  unsigned channel;
  char *dev;
  unsigned tag;
} misc_dma_t;

typedef struct {
  unsigned irq, events;
  int devs;
  char **dev;
  unsigned tag;
} misc_irq_t;

typedef struct {
  unsigned io_len, dma_len, irq_len;
  misc_io_t *io;
  misc_dma_t *dma;
  misc_irq_t *irq;
  str_list_t *proc_io, *proc_dma, *proc_irq;
} misc_t;

typedef struct s_serial_t {
  struct s_serial_t *next;
  char *name;
  int line, port, irq, baud;
} serial_t;

typedef struct s_ser_mouse_t {
  struct s_ser_mouse_t *next;
  unsigned hd_idx;
  char *dev_name;
  int fd;
  unsigned is_mouse:1;
  unsigned char buf[256];
  int buf_len;
  int garbage, non_pnp, pnp;
  unsigned char pnp_id[8];
  unsigned pnp_rev;
  unsigned bits;
} ser_mouse_t;


/*
 * Some hardware doesn't fit into the hd_t scheme or there is info we
 * gathered during the scan process but that no-one really cares about. Such
 * stuff is stored in hd_detail_t.
 */
enum hd_detail_type {
  hd_detail_pci, hd_detail_isapnp, hd_detail_cdrom, hd_detail_floppy,
  hd_detail_bios, hd_detail_cpu
};

typedef struct {
  enum hd_detail_type type;
  pci_t *data;
} hd_detail_pci_t;

typedef struct {
  enum hd_detail_type type;
  isapnp_dev_t *data;
} hd_detail_isapnp_t;

typedef struct {
  enum hd_detail_type type;
  cdrom_info_t *data;
} hd_detail_cdrom_t;

typedef struct {
  enum hd_detail_type type;
  floppy_info_t *data;
} hd_detail_floppy_t;

typedef struct {
  enum hd_detail_type type;
  bios_info_t *data;
} hd_detail_bios_t;

typedef struct {
  enum hd_detail_type type;
  cpu_info_t *data;
} hd_detail_cpu_t;

typedef union {
  enum hd_detail_type type;
  hd_detail_pci_t pci;
  hd_detail_isapnp_t isapnp;
  hd_detail_cdrom_t cdrom;
  hd_detail_floppy_t floppy;
  hd_detail_bios_t bios;
  hd_detail_cpu_t cpu;
} hd_detail_t;


/*
 * Every hardware component gets an hd_t entry. The root of the chained
 * hardware list is in hd_data_t.
 */
typedef struct s_hd_t {
  struct s_hd_t *next;		/* pointer to next hd_t entry */
  unsigned idx;			/* unique index, starting at 1 */
  unsigned broken:1;		/* hardware appears to be broken in some way */
  unsigned bus, slot, func;
  unsigned base_class, sub_class, prog_if;
  unsigned dev, vend, sub_dev, sub_vend, rev;
  unsigned compat_dev, compat_vend;

  char *dev_name, *vend_name, *sub_dev_name, *sub_vend_name,
       *rev_name, *serial;

  unsigned attached_to;		/* idx field of 'parent' entry */
  char *unix_dev_name;		/* name of special device file, if any */

  unsigned module, line, count;	/* place where the entry was created */
  hd_res_t *res;
  hd_detail_t *detail;

  struct {			/* this struct is for internal purposes only */
    unsigned remove:1;		/* schedule for removal */
  } tag;

} hd_t;

typedef struct {
  enum probe_feature probe;	/* bitmask of probing features */
  hd_t *hd;			/* the hardware list */

  /* a callback to indicate that we are still doing something... */
  void (*progress)(char *pos, char *msg);
  
  char *log;			/* log messages */
  unsigned debug;		/* debug flags */

  /*
   * The following entries should *not* be accessed outside of libhd!!!
   */
  unsigned last_idx;		/* index of the last hd entry generated */
  unsigned module;		/* the current probing module we are in */
  enum boot_arch boot;		/* boot method */
  hd_t *old_hd;			/* old (outdated) entries (if you scan more than once) */
  pci_t *pci;			/* raw PCI data */
  isapnp_t *isapnp;		/* raw ISA-PnP data */
  str_list_t *cdrom;		/* list of CDROM devs from PROC_CDROM_INFO */
  str_list_t *net;		/* list of network interfaces */
  str_list_t *floppy;		/* contents of PROC_NVRAM, used by the floppy module */
  misc_t *misc;			/* data gathered in the misc module */
  serial_t *serial;		/* /proc's serial info */
  str_list_t *scsi;		/* /proc/scsi/scsi */
  ser_mouse_t *ser_mouse;	/* info about serial mice */
  str_list_t *cpu;		/* /proc/cpuinfo */
} hd_data_t;


// driver info types (module, mouse info, ...)
enum driver_info_type {
  di_none, di_module, di_mouse, di_x11, di_display
};

// module info
typedef struct {
  enum driver_info_type type;	// driver info type
  unsigned is_active:1;		// if module is already active
  unsigned autoload:1;		// if it is automatically loaded (via conf.modules)
  char *name;			// the actual module name
  char *load_cmd;		// the command line to run ("insmod xyz")
  char *conf;			// the conf.modules entry (e.g. for sb.o)
} module_info_t;

// mouse protocol info
typedef struct {
  enum driver_info_type type;	// driver info type
  char *xf86;			// the XF86 protocol name
  char *gpm;			// dto, gpm
} mouse_info_t;

// X server info
typedef struct {
  enum driver_info_type type;	// driver info type
  char *server;			// the XF86 server name
  char *x3d;			// 3D info
  struct {
    unsigned all:5;		// the next 5 entries combined
    unsigned c8:1, c15:1, c16:1, c24:1, c32:1;
  } colors;			// supported color depths
  unsigned dacspeed;		// max. ramdac clock
} x11_info_t;

// display info
typedef struct {
  enum driver_info_type type;		// driver info type
  unsigned width, height;		// max. useful display geometry
  unsigned min_vsync, max_vsync;	// vsync range
  unsigned min_hsync, max_hsync;	// hsync range
  unsigned bandwidth;			// max. pixel clock
} display_info_t;

// describes the device drivers; either:
// - the actions needed to load a module
// - or the mouse protocol
typedef union {
  enum driver_info_type type;	// driver info type
  module_info_t module;
  mouse_info_t mouse;
  x11_info_t x11;
  display_info_t display;
} driver_info_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 *                      libhd interface functions
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

/* implemented in hd.c */

/* the actual hardware scan */
void hd_scan(hd_data_t *hd_data);

hd_data_t *hd_free_hd_data(hd_data_t *hd_data);
hd_t *hd_free_hd_list(hd_t *hd);
driver_info_t *hd_free_driver_info(driver_info_t *di);

void hd_set_probe_feature(hd_data_t *hd_data, enum probe_feature feature);
void hd_clear_probe_feature(hd_data_t *hd_data, enum probe_feature feature);
int hd_probe_feature(hd_data_t *hd_data, enum probe_feature feature);

enum probe_feature hd_probe_feature_by_name(char *name);
char *hd_probe_feature_by_value(enum probe_feature feature);

driver_info_t *hd_driver_info(hd_t *hd);

hd_t *hd_cd_list(hd_data_t *hd_data, int rescan);
hd_t *hd_disk_list(hd_data_t *hd_data, int rescan);
hd_t *hd_net_list(hd_data_t *hd_data, int rescan);

int hd_has_special_eide(hd_data_t *hd_data);
int hd_has_pcmcia(hd_data_t *hd_data);
enum boot_arch hd_boot_arch(hd_data_t *hd_data);

/* implemented in hdx.c */

char *hd_bus_name(unsigned bus);
char *hd_base_class_name(unsigned base_class);
char *hd_sub_class_name(unsigned base_class, unsigned sub_class);
char *hd_vendor_name(unsigned vendor_id);
char *hd_device_name(unsigned vendor_id, unsigned device_id);
char *hd_sub_device_name(unsigned vendor_id, unsigned device_id, unsigned subvendor_id, unsigned subdevice_id);

int hd_bus_number(char *bus_name);
int hd_base_class_number(char *base_class_name);

char *hd_device_drv_name(unsigned vendor_id, unsigned device_id);
char *hd_sub_device_drv_name(unsigned vendor_id, unsigned device_id, unsigned subvendor_id, unsigned subdevice_id);


/* implemented in hdp.c */

void hd_dump_entry(hd_data_t *hd_data, hd_t *hd, FILE *f);


/* implemented in cdrom.c */

cdrom_info_t *hd_read_cdrom_info(hd_t *hd);

#ifdef __cplusplus
}
#endif


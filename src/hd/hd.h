#ifndef _HD_H
#define _HD_H

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
#define HD_DEB_USB		(1 << 17)
#define HD_DEB_ADB		(1 << 18)
#define HD_DEB_MODEM		(1 << 19)
#define HD_DEB_PARALLEL		(1 << 20)
#define HD_DEB_ISA		(1 << 21)
#define HD_DEB_BOOT		(1 << 22)

#include <inttypes.h>
#include <termios.h>

/*
 * macros to handle device & vendor ids
 *
 * example: to check if an id is a pci id and get its value,
 * do something like this:
 *
 * if(ID_TAG(hd->dev) == TAG_PCI) {
 *   pci_id = ID_VALUE(hd->dev)
 * }
 */
#define TAG_PCI		1	/* pci ids */
#define TAG_EISA	2	/* eisa ids (incl. monitors) */
#define TAG_USB		3	/* usb ids */
#define TAG_SPECIAL	4	/* internally used ids */
#define TAG_BUS		5	/* purely internal, you should never see this tag */
#define TAG_CLASS	6	/* dto */

#define ID_VALUE(id)		((id) & 0xffff)
#define ID_TAG(id)		(((id) >> 16) & 0xf)
#define MAKE_ID(tag, id_val)	((tag << 16) | (id_val))

/*
 * flags to control the probing.
 */
typedef enum probe_feature {
  pr_memory = 1, pr_pci, pr_pci_range, pr_pci_ext, pr_isapnp, pr_cdrom,
  pr_cdrom_info, pr_net, pr_floppy, pr_misc, pr_misc_serial, pr_misc_par,
  pr_misc_floppy, pr_serial, pr_cpu, pr_bios, pr_monitor, pr_mouse, pr_ide,
  pr_scsi, pr_scsi_geo, pr_usb, pr_usb_mods, pr_adb, pr_modem, pr_modem_usb,
  pr_parallel, pr_parallel_lp, pr_parallel_zip, pr_isa, pr_isa_isdn,
  pr_dac960, pr_smart, pr_isdn, pr_kbd, pr_prom, pr_sbus, pr_int,
  pr_braille, pr_braille_alva, pr_braille_fhp, pr_braille_ht, pr_ignx11,
  pr_sys,
  pr_max, pr_lxrc, pr_default, pr_all		/* pr_all must be the last */
} hd_probe_feature_t;

/*
 * list types for hd_list()
 */
typedef enum hw_item {
  hw_cdrom = 1, hw_floppy, hw_disk, hw_network, hw_display, hw_monitor,
  hw_mouse, hw_keyboard, hw_sound, hw_isdn, hw_modem, hw_storage_ctrl,
  hw_network_ctrl, hw_printer, hw_tv, hw_scanner, hw_braille, hw_sys,
  hw_cpu
} hd_hw_item_t;

/*
 * device base classes and bus types
 *
 */

/* base class values (superset of PCI classes) */
typedef enum base_classes {
  /* these *must* match standard PCI class numbers */
  bc_none, bc_storage, bc_network, bc_display, bc_multimedia,
  bc_memory, bc_bridge, bc_comm, bc_system, bc_input, bc_docking,
  bc_processor, bc_serial, bc_other = 0xff,

  // add our own classes here (starting at 0x100 as PCI values are 8 bit)
  bc_monitor = 0x100, bc_internal, bc_modem, bc_isdn, bc_ps2, bc_mouse,
  bc_storage_device, bc_network_interface, bc_keyboard, bc_printer,
  bc_hub, bc_braille
} hd_base_classes_t;

/* subclass values of bc_storage */
typedef enum sc_storage {
  sc_sto_scsi, sc_sto_ide, sc_sto_floppy, sc_sto_ipi, sc_sto_raid,
  sc_sto_other = 0x80
} hd_sc_storage_t;

/* subclass values of bc_display */
typedef enum sc_display {
  sc_dis_vga, sc_dis_xga, sc_dis_other = 0x80
} hd_sc_display_t;

/* subclass values of bc_bridge */
typedef enum sc_bridge { 
  sc_bridge_host, sc_bridge_isa, sc_bridge_eisa, sc_bridge_mc,
  sc_bridge_pci, sc_bridge_pcmcia, sc_bridge_nubus, sc_bridge_cardbus,
  sc_bridge_other = 0x80
} hd_sc_bridge_t;

/* subclass values of bc_comm */
typedef enum sc_comm { 
  sc_com_ser, sc_com_par, sc_com_other = 0x80
} hd_sc_comm_t;

/* subclass values of bc_system */
typedef enum sc_system {
  sc_sys_pic, sc_sys_dma, sc_sys_timer, sc_sys_rtc, sc_sys_other = 0x80
} hd_sc_system_t;

/* subclass values of bc_input */
typedef enum sc_input {
  sc_inp_keyb, sc_inp_digit, sc_inp_mouse, sc_inp_other = 0x80
} hd_sc_input_t;

/* subclass values of bc_serial */
typedef enum sc_serial {
  sc_ser_fire, sc_ser_access, sc_ser_ssa, sc_ser_usb, sc_ser_fiber,
  sc_ser_smbus, sc_ser_other = 0x80
} hd_sc_serial_t;

/* internal sub class values (bc_internal) */
typedef enum sc_internal {
  sc_int_none, sc_int_isapnp_if, sc_int_main_mem, sc_int_cpu, sc_int_fpu,
  sc_int_bios, sc_int_prom, sc_int_sys
} hd_sc_internal_t;

/* subclass values of bc_mouse */
typedef enum sc_mouse {
  sc_mou_ps2, sc_mou_ser, sc_mou_bus, sc_mou_usb, sc_mou_sun
} hd_sc_mouse_t;

/* subclass values of bc_storage_device */
typedef enum sc_std {
  sc_sdev_disk, sc_sdev_tape, sc_sdev_cdrom, sc_sdev_floppy,
  sc_sdev_other = 0x80
} hd_sc_std_t;

/* subclass values of bc_network_interface */
typedef enum sc_net_if {
  sc_nif_loopback, sc_nif_ethernet, sc_nif_tokenring, sc_nif_fddi, sc_nif_other = 0x80
} hd_sc_net_if_t;

/* subclass values of bc_multimedia */
typedef enum sc_multimedia {
  sc_multi_video, sc_multi_audio, sc_multi_other
} hd_sc_multimedia_t;

/* subclass values of bc_keyboard */
typedef enum sc_keyboard {
  sc_keyboard_kbd, sc_keyboard_console
} hd_sc_keyboard_t;

/* subclass values of bc_hub */
typedef enum sc_hub {
  sc_hub_other, sc_hub_usb
} hd_sc_hub_t;

/* prog_if's of sc_ser_usb */
typedef enum pif_usb_e {
  pif_usb_uhci = 0, pif_usb_ohci = 0x10, pif_usb_other = 0x80, pif_usb_device = 0xfe
} hd_pif_usb_t;

/* bus type values similar to PCI bridge subclasses */
typedef enum bus_types {
  bus_none, bus_isa, bus_eisa, bus_mc, bus_pci, bus_pcmcia, bus_nubus,
  bus_cardbus, bus_other,

  /* outside the range of the PCI values */
  bus_ps2 = 0x80, bus_serial, bus_parallel, bus_floppy, bus_scsi, bus_ide, bus_usb,
  bus_adb, bus_raid, bus_sbus
} hd_bus_types_t;

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
  unsigned data_ext_len;			/* max. accessed config byte; see code */
  unsigned char data[256];			/* the PCI data */
  char *log;					/* log messages */
  unsigned flags,				/* various info, see enum pci_flags */
           cmd,					/* PCI_COMMAND */
           hdr_type,				/* PCI_HEADER_TYPE */
           secondary_bus;			/* > 0 for PCI & CB bridges */
  unsigned bus,					/* PCI bus #, *nothing* to do with hw_t.bus */
           slot, func; 				/* slot & function */
  unsigned base_class, sub_class, prog_if;	/* PCI device classes */
  unsigned dev, vend, sub_dev, sub_vend, rev;	/* vendor & device ids */
  unsigned irq;					/* used irq, if any */
  uint64_t base_addr[6];			/* I/O or memory base */
  uint64_t base_len[6];				/* I/O or memory ranges */
  uint64_t rom_base_addr;			/* memory base for card ROM */
  uint64_t rom_base_len;			/* memory range for card ROM */
} pci_t;

/*
 * pci related flags cf. (pci_t).flags
 */
typedef enum pci_flags {
  pci_flag_ok, pci_flag_pm, pci_flag_agp
} hd_pci_flags_t;


/*
 * raw USB data
 */
typedef struct usb_s {
  struct usb_s *next;
  unsigned hd_idx;
  str_list_t *b, *c, *ci, *d, *e, *i, *p, *s, *t;
  /*
   * see Linux USB docu for the meaning of the above;
   * c: active config, ci: other configs
   */
  int bus, dev_nr, lev, parent, port, count, conns, used_conns;
  unsigned speed;
  unsigned vendor, device, rev;
  char *manufact, *product, *serial;
  char *driver;
  int d_cls, d_sub, d_prot;
  int i_cls, i_sub, i_prot;
} usb_t;

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
typedef enum isapnp_flags {
  isapnp_flag_act
} hd_isapnp_flags_t;


/*
 * raw SCSI data
 */
typedef struct scsi_s {
  struct scsi_s *next;
  unsigned deleted:1;
  unsigned generic:1;
  unsigned fake:1;
  char *dev_name;
  int generic_dev;
  unsigned host, channel, id, lun;
  char *vendor, *model, *rev, *type_str, *serial;
  int type;
  unsigned inode_low;
  char *proc_dir, *driver;
  unsigned unique;
  char *info;
  unsigned lgeo_c, lgeo_h, lgeo_s;
  unsigned pgeo_c, pgeo_h, pgeo_s;
  uint64_t size;
  unsigned sec_size;
} scsi_t;


/*
 * PROM tree on PPC
 */
typedef struct devtree_s {
  struct devtree_s *next;
  struct devtree_s *parent;
  unsigned idx;
  char *path, *filename;
  unsigned pci:1;
  char *name, *model, *device_type, *compatible;
  int class_code;                       /* class : sub_class : prog-if */
  int vendor_id, device_id, subvendor_id, subdevice_id;
  int revision_id, interrupt;
  unsigned char *edid;                  /* 128 bytes */
} devtree_t;


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
 * bios data (ix86)
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


/*
 * prom data (ppc, sparc)
 */
typedef struct {
  unsigned has_color:1;
  unsigned color;
} prom_info_t;


/*
 * general system data
 */
typedef struct {
  char *system_type;
  char *generation;
  char *vendor;
  char *model;
  char *serial;
  char *lang;
} sys_info_t;


/*
 * monitor (DDC) data
 */
typedef struct {
  unsigned manu_year;
  unsigned min_vsync, max_vsync;	/* vsync range */
  unsigned min_hsync, max_hsync;	/* hsync range */
  char *vendor;
  char *name;
  char *serial;
} monitor_info_t;


typedef enum cpu_arch {
  arch_unknown = 0,
  arch_intel, arch_alpha, arch_sparc, arch_sparc64, arch_ppc, arch_68k, arch_ia64
} hd_cpu_arch_t;

// ###### drop boot_arch at all?
typedef enum boot_arch {
  boot_unknown = 0,
  boot_lilo, boot_milo, boot_aboot, boot_silo, boot_ppc, boot_ia64
} hd_boot_arch_t;

/* special cpu entry */
typedef struct {
  enum cpu_arch architecture;
  unsigned family;		/* axp: cpu variation */
  unsigned model;		/* axp: cpu revision */
  unsigned stepping;
  unsigned cache;
  unsigned clock;
  char *vend_name;		/* axp: system type */
  char *model_name;		/* axp: cpu model */
  char *platform;		/* x86: NULL */
  str_list_t *features;		/* x86: flags */
} cpu_info_t;


/*
 * database info
 */
typedef struct {
  unsigned data_len, data_max;
  unsigned *data;
  unsigned names_len, names_max;
  char *names;
} hddb_data_t;


/*
 * resource types
 */
typedef enum resource_types {
  res_any, res_phys_mem, res_mem, res_io, res_irq, res_dma, res_monitor,
  res_size, res_disk_geo, res_cache, res_baud
} hd_resource_types_t;


/*
 * size units (cf. (res_size_t).unit)
 */
typedef enum size_units {
  size_unit_cm, size_unit_cinch, size_unit_byte, size_unit_sectors,
  size_unit_kbyte, size_unit_mbyte, size_unit_gbyte
} hd_size_units_t;

/*
 * access types for I/O and memory resources
 */
typedef enum access_flags {
  acc_unknown, acc_ro, acc_wo, acc_rw		/* unknown, read only, write only, read/write */
} hd_access_flags_t;

typedef enum yes_no_flag {
  flag_unknown, flag_no, flag_yes		/* unknown, no, yes */
} hd_yes_no_flag_t;


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
  uint64_t base, range;
  unsigned
    enabled:1,				/* 0: disabled, 1 enabled */
    access:2,				/* enum access_flags */
    prefetch:2;				/* enum yes_no_flag */
} res_mem_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  uint64_t range;
} res_phys_mem_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  uint64_t base, range;
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
  unsigned bits, stopbits;
  char parity;				/* n, e, o, s, m */
  char handshake;			/* -, h, s */
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
  uint64_t addr, size;
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
  struct termios tio;
  unsigned is_mouse:1;
  unsigned char buf[256];
  int buf_len;
  int garbage, non_pnp, pnp;
  unsigned char pnp_id[8];
  unsigned pnp_rev;
  unsigned bits;
} ser_mouse_t;

typedef struct s_ser_modem_t {
  struct s_ser_modem_t *next;
  unsigned hd_idx;
  char *dev_name;
  str_list_t *at_resp;
  int fd;
  struct termios tio;
  unsigned max_baud, cur_baud;
  unsigned is_modem:1;
  unsigned do_io:1;
  unsigned char buf[0x1000];
  int buf_len;
  int garbage, pnp;
  unsigned char pnp_id[8];
  char *serial, *class_name, *dev_id, *user_name, *vend;
  unsigned pnp_rev;
  unsigned bits;
} ser_modem_t;

/*
 * Notes on isdn_parm_t:
 *   - def_value is only relevant of alt_values != 0
 *   - def_value should be a value out of alt_value[]
 *   - see libihw docu for the meaning of name,type,flags,def_value
 */
typedef struct isdn_parm_s {
  struct isdn_parm_s *next;
  char *name;				/* parameter name */
  unsigned valid:1;			/* 1: entry is valid, 0: some inconsistencies */
  unsigned conflict:1;			/* 1: ressource conflict (eg. no free irq) */
  uint64_t value;			/* value of the parameter */
  unsigned type;			/* libihw type (P_...) */
  unsigned flags;			/* libihw flags (P_...) */
  unsigned def_value;			/* default value */
  int alt_values;			/* length of alt_value[] */
  unsigned *alt_value;			/* possible values */
} isdn_parm_t;

/* device driver info types */
typedef enum driver_info_type {
  di_any, di_display, di_module, di_mouse, di_x11, di_isdn, di_kbd
} hd_driver_info_t;

/* unspecific info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/* driver info type */
  str_list_t *hddb0, *hddb1;		/* the actual driver database entries */
} driver_info_any_t;

/* display (monitor) info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/* driver info type */
  str_list_t *hddb0, *hddb1;		/* the actual driver database entries */
  unsigned width, height;		/* max. useful display geometry */
  unsigned min_vsync, max_vsync;	/* vsync range */
  unsigned min_hsync, max_hsync;	/* hsync range */
  unsigned bandwidth;			/* max. pixel clock */
} driver_info_display_t;

/* module info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/* driver info type */
  str_list_t *hddb0, *hddb1;		/* the actual driver database entries */
  unsigned active:1;			/* if module is currently active */
  unsigned modprobe:1;			/* modprobe or insmod  */
  char *name;				/* module name */
  char *mod_args;			/* additional module args */
  char *conf;				/* conf.modules entry, if any (e.g. for sb.o) */
} driver_info_module_t;

/* mouse protocol info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/* driver info type */
  str_list_t *hddb0, *hddb1;		/* the actual driver database entries */
  char *xf86;				/* the XF86 protocol name */
  char *gpm;				/* dto, gpm */
} driver_info_mouse_t;

/* X11 server info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/* driver info type */
  str_list_t *hddb0, *hddb1;		/* the actual driver database entries */
  char *server;				/* the server/module name */
  char *xf86_ver;			/* XFree86 version (3 or 4) */
  unsigned x3d:1;			/* has 3D support */
  struct {
    unsigned all:5;			/* the next 5 entries combined */
    unsigned c8:1, c15:1, c16:1, c24:1, c32:1;
  } colors;				/* supported color depths */
  unsigned dacspeed;			/* max. ramdac clock */
  str_list_t *packages;			/* extra packages to install */
  str_list_t *extensions;		/* additional X extensions to load ('Module' section) */
  str_list_t *options;			/* special server options */
  str_list_t *raw;			/* extra info to add to XF86Config */
} driver_info_x11_t;

/* isdn info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/* driver info type */
  str_list_t *hddb0, *hddb1;		/* the actual driver database entries */
  int i4l_type, i4l_subtype;		/* I4L types */
  char *i4l_name;			/* I4L card name */
  isdn_parm_t *params;			/* isdn parameters */
} driver_info_isdn_t;

/* keyboard info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/* driver info type */
  str_list_t *hddb0, *hddb1;		/* the actual driver database entries */
  char *XkbRules;			/* XF86Config entries */
  char *XkbModel;
  char *XkbLayout;
  char *keymap;				/* console keymap */
} driver_info_kbd_t;

/*
 * holds device driver info
 */
typedef union driver_info_u {
  union driver_info_u *next;
  driver_info_any_t any;
  driver_info_module_t module;
  driver_info_mouse_t mouse;
  driver_info_x11_t x11;
  driver_info_display_t display;
  driver_info_isdn_t isdn;
  driver_info_kbd_t kbd;
} driver_info_t;


/*
 * Some hardware doesn't fit into the hd_t scheme or there is info we
 * gathered during the scan process but that no-one really cares about. Such
 * stuff is stored in hd_detail_t.
 */
typedef enum hd_detail_type {
  hd_detail_pci, hd_detail_usb, hd_detail_isapnp, hd_detail_cdrom,
  hd_detail_floppy, hd_detail_bios, hd_detail_cpu, hd_detail_prom,
  hd_detail_monitor, hd_detail_sys, hd_detail_scsi, hd_detail_devtree
} hd_detail_type_t;

typedef struct {
  enum hd_detail_type type;
  pci_t *data;
} hd_detail_pci_t;

typedef struct {
  enum hd_detail_type type;
  usb_t *data;
} hd_detail_usb_t;

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

typedef struct {
  enum hd_detail_type type;
  prom_info_t *data;
} hd_detail_prom_t;

typedef struct {
  enum hd_detail_type type;
  monitor_info_t *data;
} hd_detail_monitor_t;

typedef struct {
  enum hd_detail_type type;
  sys_info_t *data;
} hd_detail_sys_t;

typedef struct {
  enum hd_detail_type type;
  scsi_t *data;
} hd_detail_scsi_t;

typedef struct {
  enum hd_detail_type type;
  devtree_t *data;
} hd_detail_devtree_t;

typedef union {
  enum hd_detail_type type;
  hd_detail_pci_t pci;
  hd_detail_usb_t usb;
  hd_detail_isapnp_t isapnp;
  hd_detail_cdrom_t cdrom;
  hd_detail_floppy_t floppy;
  hd_detail_bios_t bios;
  hd_detail_cpu_t cpu;
  hd_detail_prom_t prom;
  hd_detail_monitor_t monitor;
  hd_detail_sys_t sys;
  hd_detail_scsi_t scsi;
  hd_detail_devtree_t devtree;
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
  char *rom_id;			/* BIOS/PROM device name (if any) */

  unsigned module, line, count;	/* place where the entry was created */
  hd_res_t *res;
  hd_detail_t *detail;

  struct {			/* this struct is for internal purposes only */
    unsigned remove:1;		/* schedule for removal */
  } tag;

} hd_t;

typedef struct {
  hd_t *hd;			/* the hardware list */

  /* a callback to indicate that we are still doing something... */
  void (*progress)(char *pos, char *msg);
  
  char *log;			/* log messages */
  unsigned debug;		/* debug flags */

  /*
   * The following entries should *not* be accessed outside of libhd!!!
   */
  unsigned char probe[(pr_all + 7) / 8];	/* bitmask of probing features */
  unsigned char probe_set[(pr_all + 7) / 8];	/* bitmask of probing features taht will always be set */
  unsigned char probe_clr[(pr_all + 7) / 8];	/* bitmask of probing features that will always be reset */
  unsigned last_idx;		/* index of the last hd entry generated */
  unsigned module;		/* the current probing module we are in */
  struct {
    unsigned internal:1;	/* hd_scan was called internally */
  } flags;			/* special flags */
  enum boot_arch boot;		/* boot method */
  hd_t *old_hd;			/* old (outdated) entries (if you scan more than once) */
  pci_t *pci;			/* raw PCI data */
  isapnp_t *isapnp;		/* raw ISA-PnP data */
  str_list_t *cdrom;		/* list of CDROM devs from PROC_CDROM_INFO */
  str_list_t *net;		/* list of network interfaces */
  str_list_t *floppy;		/* contents of PROC_NVRAM, used by the floppy module */
  misc_t *misc;			/* data gathered in the misc module */
  serial_t *serial;		/* /proc's serial info */
  scsi_t *scsi;			/* raw SCSI data */
  ser_mouse_t *ser_mouse;	/* info about serial mice */
  ser_modem_t *ser_modem;	/* info about serial modems */
  str_list_t *cpu;		/* /proc/cpuinfo */
  str_list_t *klog;		/* kernel log */
  str_list_t *proc_usb;		/* proc usb info */
  usb_t *usb;			/* usb info */
  hddb_data_t *hddb_dev;	/* device name database */
  hddb_data_t *hddb_drv;	/* driver info database */
  str_list_t *kmods;		/* list of active kernel modules */
  str_list_t *cd_list;		/* used by hd_cd_list() */
  str_list_t *disk_list;	/* dto, hd_disk_list() */
  str_list_t *net_list;		/* dto, hd_net_list() */
  str_list_t *mouse_list;	/* dto, hd_mouse_list() */
  str_list_t *floppy_list;	/* dto, hd_floppy_list() */
  str_list_t *keyboard_list;	/* dto, hd_keyboard_list() */
  str_list_t *display_list;	/* dto, hd_display_list() */
  uint64_t used_irqs;		/* irq usage */
  uint64_t assigned_irqs;	/* irqs automatically assigned by libhd (for driver info) */
  unsigned char *bios_rom;	/* BIOS 0xc0000 - 0xfffff */
  unsigned char *bios_ram;	/* BIOS   0x400 -   0x4ff */
  unsigned display;		/* hd_idx of the active (vga) display */
  unsigned color_code;		/* color, if any */
  char *cmd_line;		/* kernel command line */
  str_list_t *xtra_hd;		/* fake hd entries (for testing) */
  devtree_t *devtree;		/* prom device tree on ppc */
} hd_data_t;


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

driver_info_t *hd_driver_info(hd_data_t *hd_data, hd_t *hd);
int hd_module_is_active(hd_data_t *hd_data, char *mod);

hd_t *hd_base_class_list(hd_data_t *hd_data, unsigned base_class);
hd_t *hd_sub_class_list(hd_data_t *hd_data, unsigned base_class, unsigned sub_class);
hd_t *hd_bus_list(hd_data_t *hd_data, unsigned bus);
hd_t *hd_list(hd_data_t *hd_data, enum hw_item items, int rescan, hd_t *hd_old);

int hd_has_special_eide(hd_data_t *hd_data);
int hd_has_pcmcia(hd_data_t *hd_data);
int hd_apm_enabled(hd_data_t *hd_data);
int hd_usb_support(hd_data_t *hd_data);
int hd_smp_support(hd_data_t *hd_data);
int hd_mac_color(hd_data_t *hd_data);
int hd_color(hd_data_t *hd_data);
unsigned hd_display_adapter(hd_data_t *hd_data);
unsigned hd_boot_disk(hd_data_t *hd_data, int *matches);
enum cpu_arch hd_cpu_arch(hd_data_t *hd_data);
enum boot_arch hd_boot_arch(hd_data_t *hd_data);

hd_t *hd_get_device_by_idx(hd_data_t *hd_data, int idx);

/* implemented in hddb.c */

char *hd_bus_name(hd_data_t *hd_data, unsigned bus);
char *hd_class_name(hd_data_t *hd_data, int level, unsigned base_class, unsigned sub_class, unsigned prog_if);
char *hd_vendor_name(hd_data_t *hd_data, unsigned vendor);
char *hd_device_name(hd_data_t *hd_data, unsigned vendor, unsigned device);
char *hd_sub_device_name(hd_data_t *hd_data, unsigned vendor, unsigned device, unsigned subvendor, unsigned subdevice);

int hd_find_device_by_name(hd_data_t *hd_data, unsigned base_class, char *vendor, char *device, unsigned *vendor_id, unsigned *device_id);

/* implemented in hdp.c */

void hd_dump_entry(hd_data_t *hd_data, hd_t *hd, FILE *f);


/* implemented in cdrom.c */

cdrom_info_t *hd_read_cdrom_info(hd_t *hd);

#ifdef __cplusplus
}
#endif

#endif	/* _HD_H */

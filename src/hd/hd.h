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

#define HD_VERSION	6

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
#define HD_DEB_HDDB		(1 << 23)

#include <stdio.h>
#include <inttypes.h>
#include <termios.h>

/**
 * \defgroup idmacros Id macros
 * Macros to handle device and vendor ids.
 *
 * Example: to check if an id is a pci id and get its value,
 * do something like this:
 * \code
 * if(ID_TAG(hd->dev) == TAG_PCI) {
 *   pci_id = ID_VALUE(hd->dev)
 * }
 * \endcode
 *@{
 */

#define TAG_PCI		1	/**< PCI ids. */
#define TAG_EISA	2	/**< EISA ids (monitors, ISA-PnP, modems, mice etc). */
#define TAG_USB		3	/**< USB ids. */
#define TAG_SPECIAL	4	/**< Internally used ids. */
#define TAG_BUS		5	/**< (obsolete) purely internal, you should never see this tag. */
#define TAG_CLASS	6	/**< (obsolete) dto. */

/**
 * Get the real id value.
 */
#define ID_VALUE(id)		((id) & 0xffff)

/**
 * Get the tag value.
 */
#define ID_TAG(id)		(((id) >> 16) & 0xf)

/**
 * Combine tag and id value.
 */
#define MAKE_ID(tag, id_val)	((tag << 16) | (id_val))

/*@}*/

/*
 * flags to control the probing.
 */
typedef enum probe_feature {
  pr_memory = 1, pr_pci, pr_pci_range, pr_pci_ext, pr_isapnp, pr_cdrom,
  pr_cdrom_info, pr_net, pr_floppy, pr_misc, pr_misc_serial, pr_misc_par,
  pr_misc_floppy, pr_serial, pr_cpu, pr_bios, pr_monitor, pr_mouse, pr_ide,
  pr_scsi, pr_scsi_geo, pr_scsi_cache, pr_usb, pr_usb_mods, pr_adb,
  pr_modem, pr_modem_usb, pr_parallel, pr_parallel_lp, pr_parallel_zip,
  pr_isa, pr_isa_isdn, pr_dac960, pr_smart, pr_isdn, pr_kbd, pr_prom,
  pr_sbus, pr_int, pr_braille, pr_braille_alva, pr_braille_fhp,
  pr_braille_ht, pr_ignx11, pr_sys, pr_dasd, pr_i2o, pr_cciss, pr_bios_vbe,
  pr_isapnp_old, pr_isapnp_new, pr_isapnp_mod, pr_braille_baum, pr_manual,
  pr_fb, pr_bios_vbe2, pr_veth, pr_partition, pr_disk, pr_ataraid, pr_pppoe,
  /* pr_bios_32, */
  pr_max, pr_lxrc, pr_default, pr_all		/* pr_all must be last */
} hd_probe_feature_t;

/*
 * list types for hd_list()
 *
 * if you want to modify this: cf. manual.c::hw_items[]
 *
 * Note: hw_tv _must_ be < hw_display!
 */
typedef enum hw_item {
  hw_sys = 1, hw_cpu, hw_keyboard, hw_braille, hw_mouse, hw_joystick,
  hw_printer, hw_scanner, hw_chipcard, hw_monitor, hw_tv, hw_display,
  hw_framebuffer, hw_camera, hw_sound, hw_storage_ctrl, hw_network_ctrl,
  hw_isdn, hw_modem, hw_network, hw_disk, hw_partition, hw_cdrom, hw_floppy,
  hw_manual, hw_usb_ctrl, hw_usb, hw_bios, hw_pci, hw_isapnp, hw_bridge,
  hw_hub, hw_scsi, hw_ide, hw_memory, hw_dvb, hw_pcmcia, hw_pcmcia_ctrl,
  hw_ieee1394, hw_ieee1394_ctrl, hw_hotplug, hw_hotplug_ctrl, hw_zip, hw_pppoe,
  hw_unknown, hw_all					/* hw_all must be last */
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
  bc_processor, bc_serial, bc_wireless, bc_i2o, bc_other = 0xff,

  // add our own classes here (starting at 0x100 as PCI values are 8 bit)
  bc_monitor = 0x100, bc_internal, bc_modem, bc_isdn, bc_ps2, bc_mouse,
  bc_storage_device, bc_network_interface, bc_keyboard, bc_printer,
  bc_hub, bc_braille, bc_scanner, bc_joystick, bc_chipcard, bc_camera,
  bc_framebuffer, bc_dvb, bc_tv
} hd_base_classes_t;

/* subclass values of bc_monitor */
typedef enum sc_monitor {
  sc_mon_other, sc_mon_crt, sc_mon_lcd
} hd_sc_monitor_t;

/* subclass values of bc_storage */
typedef enum sc_storage {
  sc_sto_scsi, sc_sto_ide, sc_sto_floppy, sc_sto_ipi, sc_sto_raid,
  sc_sto_other = 0x80
} hd_sc_storage_t;

/* subclass values of bc_display */
typedef enum sc_display {
  sc_dis_vga, sc_dis_xga, sc_dis_other = 0x80
} hd_sc_display_t;

/* subclass values of bc_framebuffer */
typedef enum sc_framebuffer {
  sc_fb_vesa = 1
} hd_sc_framebuffer_t;

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
  sc_mou_ps2, sc_mou_ser, sc_mou_bus, sc_mou_usb, sc_mou_sun,
  sc_mou_other = 0x80
} hd_sc_mouse_t;

/* subclass values of bc_storage_device */
typedef enum sc_std {
  sc_sdev_disk, sc_sdev_tape, sc_sdev_cdrom, sc_sdev_floppy, sc_sdev_scanner,
  sc_sdev_other = 0x80
} hd_sc_std_t;

/* subclass values of bc_network_interface */
typedef enum sc_net_if {
  sc_nif_loopback, sc_nif_ethernet, sc_nif_tokenring, sc_nif_fddi,
  sc_nif_ctc, sc_nif_iucv, sc_nif_hsi, sc_nif_qeth,
  sc_nif_escon, sc_nif_myrinet, sc_nif_other = 0x80, sc_nif_sit
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

/* subclass values of bc_camera */
typedef enum sc_camera {
  sc_camera_webcam, sc_camera_digital
} hd_sc_camera_t;

/* subclass values of bc_modem */
typedef enum sc_modem {
  sc_mod_at, sc_mod_win1, sc_mod_win2
} hd_sc_modem_t;

/* prog_if's of sc_ser_usb */
typedef enum pif_usb_e {
  pif_usb_uhci = 0, pif_usb_ohci = 0x10, pif_usb_ehci = 0x20,
  pif_usb_other = 0x80, pif_usb_device = 0xfe
} hd_pif_usb_t;

/* CD-ROM  prog_if values */
typedef enum pif_cdrom {
  pif_cdrom, pif_cdr, pif_cdrw, pif_dvd, pif_dvdr, pif_dvdram
} hd_pif_cdrom_t ;

/* bus type values similar to PCI bridge subclasses */
typedef enum bus_types {
  bus_none, bus_isa, bus_eisa, bus_mc, bus_pci, bus_pcmcia, bus_nubus,
  bus_cardbus, bus_other,

  /* outside the range of the PCI values */
  bus_ps2 = 0x80, bus_serial, bus_parallel, bus_floppy, bus_scsi, bus_ide, bus_usb,
  bus_adb, bus_raid, bus_sbus, bus_i2o, bus_vio
} hd_bus_types_t;

/**
 * Hardware status.
 * The status is stored in /var/lib/hardware/unique-keys/ and used
 * to detect if the hardware is new and has to be configured by some
 * hardware %config tool.
 */
typedef struct {
  /**
   * Status fields are invalid.
   */
  unsigned invalid:1;
  /**
   * Hardware should be reconfigured.
   * Either \ref hd_status_t::status_yes or \ref hd_status_t::status_no.
   * A hardware must be reconfigured if it is in state
   * \ref hd_status_t::available == \ref hd_status_t::status_no and
   * \ref hd_status_t::needed == \ref hd_status_t::status_yes.
   * In other words, if a hardware that was
   * needed to run the system is gone.
   */
  unsigned reconfig:3;

  /**
   * Hardware %config status.
   * Set to \ref hd_status_t::status_yes if the hardware has been configured, otherwise
   * \ref hd_status_t::status_no.
   */
  unsigned configured:3;

  /**
   * Hardware availability.
   * Set to \ref hd_status_t::status_yes if the hardware has been detected or
   * \ref hd_status_t::status_no if the hardware has not been found. You can set
   * it to \ref hd_status_t::status_unknown to indicate that this hardware cannot
   * be automatically detected (say, ISA cards).
   * \note You can simulate all kinds of hardware on your system by
   * creating entries in /var/lib/hardware/unique-keys/ that have
   * \ref hd_status_t::available set to \ref hd_status_t::status_unknown.
   */
  unsigned available:3;

  /**
   * Hardware is needed.
   * Set to \ref hd_status_t::status_yes if this hardware is really necessary to run
   * your computer. The effect will be that some hardware %config dialog
   * is run if the hardware item is not found.
   * Typical examples are graphics cards and mice.
   */
  unsigned needed:3;

  /**
   * (Internal) original value of \ref available;
   * This is used to keep track of the original value of the \ref hd_status_t::available
   * state as it was stored in /var/lib/hardware/unique-keys/. (\ref hd_status_t::available
   * is automatically updated during the detection process.)
   */
  unsigned available_orig:3;
} hd_status_t;

/* hardware config status values */
typedef enum {
  status_no = 1, status_yes, status_unknown, status_new
} hd_status_value_t;

/**
 * Various types of hotplug devices.
 */
typedef enum {
  hp_none,	/**< Not a hotpluggable %device. */
  hp_pcmcia,	/**< PCMCIA %device. */
  hp_cardbus,	/**< Cardbus %device. */
  hp_pci,	/**< PCI hotplug %device. */
  hp_usb,	/**< USB %device. */
  hp_ieee1394	/**< IEEE 1394 (FireWire) %device */
} hd_hotplug_t;


/**
 * Holds id/name pairs.
 * Used for bus, class, vendor, %device and such.
 */
typedef struct {
  unsigned id;		/**< Numeric id. */
  char *name;		/**< Name (if any) that corresponds to \ref hd_id_t::id. */
} hd_id_t;

/**
 * String list type.
 * Used whenever we create a list of strings (e.g. file read).
 */
typedef struct s_str_list_t {
  struct s_str_list_t *next;	/**< Link to next member. */
  char *str;  			/**< Some string data. */
} str_list_t;


/*
 * for memory areas
 */
typedef struct {
  unsigned start, size;		/* base address & size */
  unsigned char *data;		/* actual data */
} memory_range_t;


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
  unsigned cpus, cpus_en;	/* number of cpus & ennabled cpus */
} smp_info_t;


/*
 * vesa bios extensions info
 */
typedef struct vbe_mode_info_s {
  unsigned number;		/* mode number */
  unsigned attributes;		/* mode attributes */
  unsigned width, height;	/* mode size */
  unsigned bytes_p_line;	/* line length */
  unsigned pixel_size;		/* bits per pixel */
  unsigned fb_start;		/* frame buffer start address (if any) */
  unsigned win_A_start;		/* window A start address */
  unsigned win_A_attr;		/* window A attributes */
  unsigned win_B_start;		/* window B start address */
  unsigned win_B_attr;		/* window B attributes */
  unsigned win_size;		/* window size in bytes */
  unsigned win_gran;		/* window granularity in bytes */
  unsigned pixel_clock;		/* maximum pixel clock */
} vbe_mode_info_t;


typedef struct {
  unsigned ok:1;		/* data are valid */
  unsigned version;		/* vbe version */
  unsigned oem_version;		/* oem version info */
  unsigned memory;		/* in bytes */
  unsigned fb_start;		/* != 0 if framebuffer is supported */
  char *oem_name;		/* oem name */
  char *vendor_name;		/* vendor name */
  char *product_name;		/* product name */
  char *product_revision;	/* product revision */
  unsigned modes;		/* number of supported video modes */
  vbe_mode_info_t *mode;	/* video mode list */
  unsigned current_mode;	/* current video mode */
  unsigned char ddc[0x80];	/* ddc monitor info */
} vbe_info_t;


/*
 * Compaq Controller Order EV (CQHORD) definition
 */
typedef struct {
    unsigned id;
    unsigned char slot;
    unsigned char bus;
    unsigned char devfn;
    unsigned char misc;
} cpq_ctlorder_t; 


typedef struct {
  unsigned ok:1;		/* data are valid */
  unsigned entry;		/* entry point */
  unsigned compaq:1;		/* is compaq system */
  cpq_ctlorder_t cpq_ctrl[32];	/* 32 == MAX_CONTROLLERS */
} bios32_info_t;


/*
 * smbios entries
 */
typedef enum {
  sm_biosinfo, sm_sysinfo, sm_boardinfo, sm_chassis,
  sm_processor, sm_memctrl, sm_memmodule, sm_cache,
  sm_connect, sm_slot, sm_onboard, sm_oem,
  sm_config, sm_lang, sm_group, sm_eventlog,
  sm_memarray, sm_memdevice, sm_memerror, sm_memarraymap,
  sm_memdevicemap, sm_mouse, sm_battery, sm_reset,
  sm_secure, sm_power, sm_voltage, sm_cool,
  sm_temperature, sm_current, sm_outofband, sm_bis,
  sm_boot, sm_memerror64, sm_mandev, sm_mandevcomp,
  sm_mdtd, sm_inactive = 126, sm_end = 127
} hd_smbios_type_t;

typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
} smbios_any_t;

typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *vendor;
  char *version;
  char *date;
  uint64_t features;
  unsigned xfeatures;
} smbios_biosinfo_t;

typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *manuf;
  char *product;
  char *version;
  char *serial;
  unsigned wake_up;
} smbios_sysinfo_t;

typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *manuf;
  char *product;
  char *version;
  char *serial;
} smbios_boardinfo_t;

typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *manuf;
  unsigned ch_type;
} smbios_chassis_t;

typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *socket;
  char *manuf;
  char *version;
  unsigned voltage;		/* in 0.1 V */
  unsigned ext_clock;		/* MHz */
  unsigned max_speed;		/* MHz */
  unsigned current_speed;	/* MHz */
  unsigned status;
  unsigned upgrade;
} smbios_processor_t;

typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *descr[8];
  unsigned dtype[8];
} smbios_onboard_t;

typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *current;
} smbios_lang_t;

typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  unsigned ecc;
  unsigned max_size;		/* kB */
} smbios_memarray_t;

typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *location;
  char *bank;
  unsigned width;
  unsigned eccbits;
  unsigned size;		/* kB */
  unsigned form;
  unsigned type1;
  unsigned type2;
  unsigned speed;
} smbios_memdevice_t;

typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  unsigned mtype;
  unsigned interface;
  unsigned buttons;
} smbios_mouse_t;

typedef union u_hd_smbios_t {
  union u_hd_smbios_t *next;  
  smbios_any_t any;
  smbios_biosinfo_t biosinfo;
  smbios_sysinfo_t sysinfo;
  smbios_boardinfo_t boardinfo;
  smbios_chassis_t chassis;
  smbios_processor_t processor;
  smbios_lang_t lang;
  smbios_memarray_t memarray;
  smbios_memdevice_t memdevice;
  smbios_mouse_t mouse;
  smbios_onboard_t onboard;
} hd_smbios_t;


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
  struct usb_s *cloned;
  int bus, dev_nr, lev, parent, port, count, conns, used_conns, ifdescr;
  unsigned speed;
  unsigned vendor, device, rev;
  char *manufact, *product, *serial;
  char *driver;
  int d_cls, d_sub, d_prot;
  int i_cls, i_sub, i_prot;
  unsigned country;
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
  unsigned ref:1;				/* internally used flag */
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
  char *guessed_dev_name;
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
  unsigned cache;
  str_list_t *host_info;
  char *usb_guid;
  unsigned pci_info;
  unsigned pci_bus;
  unsigned pci_slot;
  unsigned pci_func;
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
typedef struct cdrom_info_s {
  struct cdrom_info_s *next;
  char *name;
  unsigned speed;
  unsigned cdr:1, cdrw:1, dvd:1, dvdr:1, dvdram:1;
  unsigned cdrom:1;		/* cdrom in drive */
  struct {
    unsigned ok:1;
    char *volume, *publisher, *preparer, *application, *creation_date;
  } iso9660;
  struct {
    unsigned ok:1;
    unsigned platform;
    char *id_string;
    unsigned bootable:1;
    unsigned media_type;	/* boot emulation type */
    unsigned load_address;
    unsigned load_count;	/* sectors to load */
    unsigned start;		/* start sector */
    unsigned catalog;		/* boot catalog start */
    struct {
      unsigned c, h, s;
      unsigned size;
    } geo;
    char *label;
  } el_torito;

} cdrom_info_t;

// note: obsolete, will be removed
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
  unsigned lba_support:1;

  unsigned low_mem_size;
  smp_info_t smp;
  vbe_info_t vbe;

  unsigned smbios_ver;

  struct {
    unsigned width;
    unsigned height;
    char *vendor;
    char *name;
  } lcd;

  struct {
    char *vendor;
    char *type;
    unsigned bus;
    unsigned compat_vend;
    unsigned compat_dev;
  } mouse;

  struct {
    unsigned ok:1;
    unsigned scroll_lock:1;
    unsigned num_lock:1;
    unsigned caps_lock:1;
  } led;

  bios32_info_t bios32;

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
  arch_intel,
  arch_alpha,
  arch_sparc, arch_sparc64,
  arch_ppc, arch_ppc64,
  arch_68k,
  arch_ia64,
  arch_s390, arch_s390x,
  arch_arm,
  arch_mips,
  arch_x86_64
} hd_cpu_arch_t;

// ###### drop boot_arch at all?
typedef enum boot_arch {
  boot_unknown = 0,
  boot_lilo, boot_milo, boot_aboot, boot_silo, boot_ppc, boot_elilo, boot_s390,
  boot_mips, boot_grub
} hd_boot_arch_t;

/* special cpu entry */
typedef struct {
  enum cpu_arch architecture;
  unsigned family;		/* axp: cpu variation */
  unsigned model;		/* axp: cpu revision */
  unsigned stepping;
  unsigned cache;
  unsigned clock;
  unsigned units;		/* >1 "hyperthreading" */
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

typedef uint32_t hddb_entry_mask_t;

typedef struct hddb_list_s {   
  hddb_entry_mask_t key_mask;
  hddb_entry_mask_t value_mask;
  unsigned key;
  unsigned value;
} hddb_list_t;

typedef struct {
  unsigned list_len, list_max;
  hddb_list_t *list;
  unsigned ids_len, ids_max;
  unsigned *ids;
  unsigned strings_len, strings_max;
  char *strings;
} hddb2_data_t;

/*
 * pci module info
 */
typedef struct {
  char *module;
  unsigned vendor;
  unsigned device;
  unsigned subvendor;
  unsigned subdevice;
  unsigned pciclass;
  unsigned classmask;
} hddb_pci_t;


/*
 * resource types
 */
typedef enum resource_types {
  res_any, res_phys_mem, res_mem, res_io, res_irq, res_dma, res_monitor,
  res_size, res_disk_geo, res_cache, res_baud, res_init_strings, res_pppd_option,
  res_framebuffer
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
  unsigned interlaced:1;		/* 0/1 */
} res_monitor_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  char *init1;
  char *init2;
} res_init_strings_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  char *option;
} res_pppd_option_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  unsigned width, height;		/* in pixel */
  unsigned bytes_p_line;		/* line length in bytes (do not confuse with 'width') */
  unsigned colorbits;			/* 4, 8, 15, 16, 24, 32 */
  unsigned mode;			/* mode number for kernel */
} res_framebuffer_t;

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
  res_init_strings_t init_strings;
  res_pppd_option_t pppd_option;
  res_framebuffer_t framebuffer;
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
  char *device;
  unsigned line, port, irq, baud;
} serial_t;

typedef struct s_ser_device_t {
  struct s_ser_device_t *next;
  unsigned hd_idx;
  char *dev_name;
  str_list_t *at_resp;
  int fd;
  struct termios tio;
  unsigned max_baud, cur_baud;
  unsigned is_mouse:1;
  unsigned is_modem:1;
  unsigned do_io:1;
  unsigned char buf[0x1000];
  int buf_len;
  int garbage, non_pnp, pnp;
  unsigned char pnp_id[8];
  char *serial, *class_name, *dev_id, *user_name, *vend, *init_string1, *init_string2, *pppd_option;
  unsigned pnp_rev;
  unsigned bits;
} ser_device_t;

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
  str_list_t *names;			/* (ordered) list of module names */
  str_list_t *mod_args;			/* list of module args (corresponds to the module name list) */
  char *conf;				/* conf.modules entry, if any (e.g. for sb.o) */
} driver_info_module_t;

/* mouse protocol info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/* driver info type */
  str_list_t *hddb0, *hddb1;		/* the actual driver database entries */
  char *xf86;				/* the XF86 protocol name */
  char *gpm;				/* dto, gpm */
  int buttons;				/* number of buttons, -1 --> unknown */
  int wheels;				/* dto, wheels */
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
  str_list_t *extensions;		/* additional X extensions to load ('Module' section) */
  str_list_t *options;			/* special server options */
  str_list_t *raw;			/* extra info to add to XF86Config */
  char *script;				/* 3d script to run */
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


/* info about manually configured hardware (in /var/lib/hardware/) */
typedef struct hd_manual_s {
  struct hd_manual_s *next;

  char *unique_id;
  char *parent_id;
  unsigned hw_class;
  char *model;

  hd_status_t status;
  char *config_string;

  /* More or less free-form key, value pairs.
   * key should not contain '=', however.
   */
  str_list_t *key;
  str_list_t *value;
} hd_manual_t;


/**
 * Individual hardware item.
 * Every hardware component gets an \ref hd_t entry. A list of all hardware
 * items is in \ref hd_data_t::hd.
 */
typedef struct s_hd_t {
  struct s_hd_t *next;		/**< Link to next hardware item. */
  /**
   * Unique index, starting at 1.
   * Use \ref hd_get_device_by_idx() to look up an hardware entry by index. And don't
   * free the result!
   */
  unsigned idx;

  /**
   * Hardware appears to be broken in some way.
   * This was used to indicate broken framebuffer support of some graphics cards.
   * Currently unused.
   */
  unsigned broken:1;

  /**
   * Bus type (id and name).
   */ 
  hd_id_t bus;

  /**
   * Slot and bus number.
   * Bits 0-7: slot number, 8-31 bus number.
   */
  unsigned slot;

  /**
   * (PCI) function.
   */
  unsigned func;

  /**
   * Base class (id and name).
   */
  hd_id_t base_class;

  /**
   * Sub class (id and name).
   */
  hd_id_t sub_class;

  /**
   * (PCI) programming interface (id and name).
   */
  hd_id_t prog_if;

  /**
   * Vendor id and name.
   * Id is actually a combination of some tag to differentiate the
   * various id types and the real id. Use the \ref ID_VALUE macro to
   * get e.g. the real PCI id value for a PCI %device.
   */
  hd_id_t vendor;

  /**
   * Device id and name.
   * Id is actually a combination of some tag to differentiate the
   * various id types and the real id. Use the \ref ID_VALUE macro to
   * get e.g. the real PCI id value for a PCI %device.
   * \note If you're looking or something printable, you might want to use \ref hd_t::model
   * instead.
   */
  hd_id_t device;

  /**
   * Subvendor id and name.
   * Id is actually a combination of some tag to differentiate the
   * various id types and the real id. Use the \ref ID_VALUE macro to
   * get e.g. the real PCI id value for a PCI %device.
   */
  hd_id_t sub_vendor;

  /**
   * Subdevice id and name.
   * Id is actually a combination of some tag to differentiate the
   * various id types and the real id. Use the \ref ID_VALUE macro to
   * get e.g. the real PCI id value for a PCI %device.
   */
  hd_id_t sub_device;

  /**
   * Revision id or string.
   * If revision is numerical (e.g. PCI) \ref hd_id_t::id is used.
   * If revision is some char data (e.g. disk drives) it is stored in \ref hd_id_t::name.
   */
  hd_id_t revision;

  /**
   * Serial id.
   */
  char *serial;

  /**
   * Vendor id and name of some compatible hardware.
   * Used mainly for ISA-PnP devices.
   */
  hd_id_t compat_vendor;

  /**
   * Device id and name of some compatible hardware.
   * Used mainly for ISA-PnP devices.
   */
  hd_id_t compat_device;

  /**
   * Hardware class.
   * Not to confuse with \ref base_class!
   */
  hd_hw_item_t hw_class;

  /**
   * Alternative hardware class.
   * A hardware item my belong to more than one class (e.g. mouse vs. usb).
   * Still not to confuse with \ref base_class!
   */
  hd_hw_item_t hw_class2;

  /**
   * Second alternative hardware class.
   * A hardware item my belong to more than one class (e.g. mouse vs. usb).
   * Still not to confuse with \ref base_class!
   */
  hd_hw_item_t hw_class3;

  /**
   * Model name.
   * This is a combination of vendor and %device names. Some heuristics is used
   * to make it more presentable. Use this instead of \ref hd_t::vendor and
   * \ref hd_t::device.
   */
  char *model;

  /**
   * Device this hardware is attached to.
   * Link to some 'parent' %device. Use \ref hd_get_device_by_idx() to get
   * the corresponding hardware entry.
   */
  unsigned attached_to;

  /**
   * Special %device file.
   * Device file name to acces this hardware. Normally something below /dev.
   * For network interfaces this is the interface name.
   */
  char *unix_dev_name;

  /**
   * BIOS/PROM id.
   * Where appropriate, this is a special BIOS/PROM id (e.g. "0x80" for
   * the first harddisk on Intel-PCs).
   * CHPID for s390.
   */
  char *rom_id;

  /**
   * Unique id for this hardware.
   * A unique string identifying this hardware. The string consists
   * of two parts separated by a dot ("."). The part before the dot
   * describes the location (where the hardware is attached in the system).
   * The part after the dot identifies the hardware itself. The string
   * must not contain slashes ("/") because we're going to create files
   * with this id as name. Apart from this there are no restrictions on
   * the form of this string.
   */
  char *unique_id;

  /**
   * (Internal) Probing module that created this entry.
   */
  unsigned module;

  /**
   * (Internal) Source code line where this entry was created.
   */
  unsigned line;

  /**
   * (Internal) Counter, used in combination with \ref hd_t::module and \ref hd_t::line.
   */
  unsigned count;

  /**
   * Device resources.
   */
  hd_res_t *res;

  /**
   * Special info associated with this hardware.
   * \note This is going to change!
   */
  hd_detail_t *detail;

  /**
   * (Internal) Unspecific text info.
   * It is used to track IDE interfaces and assign them to the correct
   * IDE controllers.
   */
  str_list_t *extra_info;

  /**
   * Hardware status (if available).
   * The status is stored in files below /var/lib/hardware/unique-keys/. Every
   * hardware item gets a file there with its unique id as file name.
   */
  hd_status_t status;

  /**
   * Some %config info.
   * Every hardware item may get some string assigned. This string is stored
   * in files below /var/lib/hardware/unique-keys/. There is no meaning
   * associated with this string.
   */
  char *config_string;

  /**
   * Hotplug controller for this %device.
   * It indicates what kind of hotplug %device (if any) this is.
   */
  hd_hotplug_t hotplug;

  struct is_s {
    unsigned agp:1;		/* AGP device */
    unsigned isapnp:1;		/* ISA-PnP device */
    unsigned cardbus:1;		/* cardbus card *obsolete*! */
    unsigned pcmcia:1;		/* pcmcia card *obsolete*! */
    unsigned notready:1;	/* block devices: no medium, other: device not configured */
    unsigned manual:1;		/* undetectable, manually configured hardware */
    unsigned softraiddisk:1;	/* disk belongs to some soft raid array */
    unsigned zip:1;		/* zip floppy */
    unsigned cdr:1;		/* CD-R */
    unsigned cdrw:1;		/* CD-RW */
    unsigned dvd:1;		/* DVD */
    unsigned dvdr:1;		/* DVD-R */
    unsigned dvdram:1;		/* DVD-RAM */
    unsigned pppoe:1;		/* PPPOE modem connected */
  } is;

  struct tag_s {		/* this struct is for internal purposes only */
    unsigned remove:1;		/* schedule for removal */
    unsigned freeit:1;		/* for internal memory management */
    unsigned fixed:1;		/* fixed, do no longer modify this entry */
    unsigned ser_skip:1;	/* if serial line, don't scan for devices */
    unsigned ser_device:2;	/* if != 0: info about attached serial device; see serial.c */
  } tag;

  /**
   * (Internal) First 512 bytes of block devices.
   * To check accessibility of block devices we read the first block. The data
   * is used to identify the boot %device.
   */
  unsigned char *block0;

  /**
   * Currently active driver.
   */
  char *driver;

  /**
   * Old \ref unique_id for compatibility.
   * The calculation of unique ids has changed in libhd v3.17. Basically
   * we no longer use the vendor/%device names if there are vendor/%device
   * ids. (Otherwise a simple %device name database update would change the id,
   * which is really not what you want.)
   */
  char *old_unique_id;

  /**
   * \ref unique_id of parent (\ref attached_to).
   * \note Please do not use it for now.
   * 
   */
  char *parent_id;

  /**
   * (Internal) location independent \ref unique_id part.
   * The speed up some internal searches, we store it here separately.
   */
  char *unique_id1;

  /**
   * USB Global Unique Identifier.
   * Available for USB devices. This may even be set if \ref hd_t::bus is not
   * \ref bus_usb (e.g. USB storage devices will have \ref hd_t::bus set to
   * \ref bus_scsi due to SCSI emulation).
   */
  char *usb_guid;

  driver_info_t *driver_info;	/* device driver info */

  str_list_t *requires;		/* packages/programs required for this hardware */

  /*
   * These are used internally for memory management.
   * Do not even _think_ of modifying these!
   */
  unsigned ref_cnt;		/**< (Internal) memory reference count. */
  struct s_hd_t *ref;		/**< (Internal) if set, this is only a reference. */
} hd_t;


/**
 * Holds all data accumulated during hardware probing.
 */
typedef struct {
  /**
   * Current hardware list.
   * The list of all currently probed hardware. This is not identical with
   * the result of \ref hd_list(). (But a superset of it.)
   */
  hd_t *hd;

  /**
   * A progress indicator.
   * If this callback function is not NULL, it is called at various points and can
   * be used to give some user feedback what we are actually doing.
   * If the debug flag HD_DEB_PROGRESS is set, progress messages are logged.
   * \param pos Indicates where we are.
   * \param msg Indicates what we are going to do.
   */
  void (*progress)(char *pos, char *msg);
  
  /** Log messages.
   * All messages logged during hardware probing accumulate here.
   */
  char *log;

  /** Debug flags.
   * Although there exist some debug flag defines this scheme is currently
   * not followed consistently. It is guaranteed however that -1 will give
   * the most log messages and 0 the least.
   */
  unsigned debug;

  /**
   * Special flags.
   * Influence hardware probing in some strange ways with these. You normally
   * do not want to use them.
   */
  struct flag_struct {
    unsigned internal:1;	/**< \ref hd_scan() has been called internally. */
    unsigned dformat:2;		/**< Alternative output format. */
    unsigned no_parport:1;	/**< Don't do parport probing: parport modules (used to) crash pmacs. */
    unsigned iseries:1;		/**< Set if we are on an iSeries machine. */
    unsigned list_all:1;	/**< Return even devices with status 'not available'. */
    unsigned fast:1;		/**< Don't check tricky hardware. */
    unsigned list_md:1;		/**< Report md & lvm devices from /proc/partitions */
  } flags;


  /*
   * The following entries should *not* be accessed outside of libhd!
   */

  unsigned char probe[(pr_all + 7) / 8];	/**< (Internal) bitmask of probing features. */
  unsigned char probe_set[(pr_all + 7) / 8];	/**< (Iternal) bitmask of probing features that will always be set. */
  unsigned char probe_clr[(pr_all + 7) / 8];	/**< (Internal) bitmask of probing features that will always be reset. */
  unsigned last_idx;		/**< (Internal) index of the last hd entry generated */
  unsigned module;		/**< (Internal) the current probing module we are in */
  enum boot_arch boot;		/**< (Internal) boot method */
  hd_t *old_hd;			/**< (Internal) old (outdated) entries (if you scan more than once) */
  pci_t *pci;			/**< (Internal) raw PCI data */
  isapnp_t *isapnp;		/**< (Internal) raw ISA-PnP data */
  cdrom_info_t *cdrom;		/**< (Internal) CDROM devs from PROC_CDROM_INFO */
  str_list_t *net;		/**< (Internal) list of network interfaces */
  str_list_t *floppy;		/**< (Internal) contents of PROC_NVRAM, used by the floppy module */
  misc_t *misc;			/**< (Internal) data gathered in the misc module */
  serial_t *serial;		/**< (Internal) /proc's serial info */
  scsi_t *scsi;			/**< (Internal) raw SCSI data */
  ser_device_t *ser_mouse;	/**< (Internal) info about serial mice */
  ser_device_t *ser_modem;	/**< (Internal) info about serial modems */
  str_list_t *cpu;		/**< (Internal) /proc/cpuinfo */
  str_list_t *klog;		/**< (Internal) kernel log */
  str_list_t *proc_usb;		/**< (Internal) /proc/bus/usb info */
  usb_t *usb;			/**< (Internal) usb info */
  hddb_pci_t *hddb_pci;		/**< (Internal) pci module info */
  hddb2_data_t *hddb2[2];	/**< (Internal) hardware database */
  str_list_t *kmods;		/**< (Internal) list of active kernel modules */
  uint64_t used_irqs;		/**< (Internal) irq usage */
  uint64_t assigned_irqs;	/**< (Internal) irqs automatically assigned by libhd (for driver info) */
  memory_range_t bios_rom;	/**< (Internal) BIOS 0xc0000 - 0xfffff */
  memory_range_t bios_ram;	/**< (Internal) BIOS 0x00400 - 0x004ff */
  memory_range_t bios_ebda;	/**< (Internal) EBDA */
  unsigned display;		/**< (Internal) hd_idx of the active (vga) display */
  unsigned color_code;		/**< (Internal) color, if any */
  char *cmd_line;		/**< (Internal) kernel command line */
  str_list_t *xtra_hd;		/**< (Internal) fake hd entries (for testing) */
  devtree_t *devtree;		/**< (Internal) prom device tree on ppc */
  unsigned kernel_version;	/**< (Internal) kernel version */
  int in_vmware;		/**< (Internal) running in vmware */
  hd_manual_t *manual;		/**< (Internal) hardware config info */
  str_list_t *disks;		/**< (Internal) disks according to /proc/partitions */
  str_list_t *partitions;	/**< (Internal) dto, partitions */
  hd_smbios_t *smbios;		/**< (Internal) smbios data */
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

//! Free all data.
hd_data_t *hd_free_hd_data(hd_data_t *hd_data);

//! Free hardware items returned by e.g. \ref hd_list().
hd_t *hd_free_hd_list(hd_t *hd);

void hd_set_probe_feature(hd_data_t *hd_data, enum probe_feature feature);
void hd_clear_probe_feature(hd_data_t *hd_data, enum probe_feature feature);
int hd_probe_feature(hd_data_t *hd_data, enum probe_feature feature);
void hd_set_probe_feature_hw(hd_data_t *hd_data, hd_hw_item_t item);

enum probe_feature hd_probe_feature_by_name(char *name);
char *hd_probe_feature_by_value(enum probe_feature feature);

int hd_module_is_active(hd_data_t *hd_data, char *mod);

hd_t *hd_base_class_list(hd_data_t *hd_data, unsigned base_class);
hd_t *hd_sub_class_list(hd_data_t *hd_data, unsigned base_class, unsigned sub_class);
hd_t *hd_bus_list(hd_data_t *hd_data, unsigned bus);
hd_t *hd_list(hd_data_t *hd_data, hd_hw_item_t item, int rescan, hd_t *hd_old);
hd_t *hd_list_with_status(hd_data_t *hd_data, hd_hw_item_t item, hd_status_t status);

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

hd_t *hd_get_device_by_idx(hd_data_t *hd_data, unsigned idx);

/* implemented in hddb.c */

str_list_t *get_hddb_packages(hd_data_t *hd_data);

void hddb_dump_raw(hddb2_data_t *hddb, FILE *f);
void hddb_dump(hddb2_data_t *hddb, FILE *f);

/* implemented in hdp.c */

void hd_dump_entry(hd_data_t *hd_data, hd_t *hd, FILE *f);


/* implemented in cdrom.c */

cdrom_info_t *hd_read_cdrom_info(hd_data_t *hd_data, hd_t *hd);

/* implemented in manual.c */
hd_manual_t *hd_manual_read_entry(hd_data_t *hd_data, const char *id);
int hd_manual_write_entry(hd_data_t *hd_data, hd_manual_t *entry);
hd_manual_t *hd_free_manual(hd_manual_t *manual);
hd_t *hd_read_config(hd_data_t *hd_data, const char *id);
int hd_write_config(hd_data_t *hd_data, hd_t *hd);
char *hd_hw_item_name(hd_hw_item_t item);
char *hd_status_value_name(hd_status_value_t status);
int hd_change_status(const char *id, hd_status_t status, const char *config_string);


/*
 * - - - - - ihw interface - - - - -
 */


/* (C) kkeil@suse.de */

#define IHW_VERSION	0x0204
#define	CLASS_PCI	1
#define CLASS_ISAPNP	2
#define CLASS_ISALEGAL	3
#define CLASS_PCMCIA	4
#define CLASS_PC104	5
#define CLASS_PARALLEL	6
#define CLASS_SERIAL	7
#define CLASS_USB	8
#define CLASS_ONBOARD	9

/* parameter types */
#define P_NONE		0x0
#define P_IRQ		0x1
#define P_MEM		0x2
#define P_IO		0x3
#define P_IO0		0x4
#define P_IO1		0x5
#define P_IO2		0x6
#define P_BASE0		0x8
#define P_BASE1		0x9
#define P_BASE2		0xa
#define P_BASE3		0xb
#define P_BASE4		0xc
#define P_BASE5		0xd

#define P_TYPE_MASK	0xff

#define P_DEFINE	0x0100
#define P_SOFTSET	0x0200
#define P_HARDSET	0x0400
#define P_READABLE	0x0800
#define P_ISAPNP	0x1000
#define P_PCI		0x2000

#define P_PROPERTY_MASK	0xffff00

#ifndef PCI_ANY_ID
#define PCI_ANY_ID	0xffff
#endif

/* card features */
#define IHW_CARD_BRI		0x00000001
#define IHW_CARD_PRI		0x00000002
#define IHW_CARD_DSL		0x00000004
#define IHW_CARD_HANDSET	0x00000100
#define IHW_CARD_MODEM		0x00000200
 
/* supported architectures */
#define SUPPORT_ARCH_SMP	0x00010000
#define SUPPORT_ARCH_I386	0x00000001
#define SUPPORT_ARCH_AXP	0x00000002
#define SUPPORT_ARCH_PPC	0x00000004

/* driver features */
#define IHW_DRV_PROT_1TR6	0x00000001
#define IHW_DRV_PROT_DSS1	0x00000002
#define IHW_DRV_PROT_NI1	0x00000004
#define IHW_DRV_PROT_LEASED	0x00000008
#define IHW_DRV_FCLASS1		0x00010000
#define IHW_DRV_FCLASS2		0x00020000
#define IHW_DRV_CAPI20		0x00100000
#define IHW_DRV_CAPIFAX		0x00200000

/* card info */

typedef struct	{
	int	handle;		/* internal identifier idx in daatabase */
	const char *name;	/* Ascii cardname */
	int	Class;		/* CLASS of the card */
	int	vendor;		/* Vendor ID for ISAPNP and PCI cards */
	int     device;		/* Device ID for ISAPNP and PCI cards */
	int	subvendor;	/* Subvendor ID for PCI cards */
				/* A value of 0xffff is ANY_ID */
	int     subdevice;	/* Subdevice ID for PCI cards */
				/* A value of 0xffff is ANY_ID */
	unsigned int features;	/* feature flags */
	int	line_cnt;	/* count of ISDN ports */
	int	driver;		/* referenz to driver record */
	int	paracnt;	/* count of parameters */
	int	para;		/* index to first parameter */
} ihw_card_info;

typedef struct  {
	int	handle;		/* idx in database */	
	int     next_drv;	/* link to alternate driver */
	int     drvid;		/* unique id of the driver */
	int	typ;		/* Type to identify the driver */
	int	subtyp;		/* Subtype of the driver type */
	const char *mod_name;	/* name of the driver module */
	const char *para_str;	/* optional parameter string */
	const char *mod_preload;/* optional modules to preload */
	const char *cfg_prog;	/* optional cfg prog */
	const char *firmware;	/* optional firmware to load */
	const char *description;/* optional description */
	const char *need_pkg;	/* packages needed for function */
	unsigned int arch;	/* supported architectures */
	unsigned int features;	/* optional features*/
	int	card_ref;	/* reference to a card */
	const char *name;	/* driver name */
} ihw_driver_info;

/* parameter info */
typedef struct  {
	const char	*name;		/* Name of the parameter */
	unsigned int	type;		/* type of parameter (P_... */
	unsigned int	flags;		/* additional information about the */
					/* parameter */
	unsigned long	def_value;	/* default value */
	unsigned long   bytecnt;	/* byte count of ressource not used */
       	const unsigned long *list;	/* possible values of the parameter */
       					/* The first element gives the count */
       					/* of values */
} ihw_para_info;

/* get card informations in alphabetically order handle = 0,1,... */
/* if handle is out of bounds NULL is returned */

extern ihw_card_info	*hd_ihw_get_card(int handle);

/* get card informations  for the card with typ and */
/* subtyp returns NULL if no card match */

extern ihw_card_info	*hd_ihw_get_card_from_type(int typ, int subtyp);

/* get informations  for the card with VENDOR,DEVICE,SUBVENDOR, */
/* SUBDEVICE for  ISAPNP and PCI cards SUBVENDOR and SUBDEVICE should be */
/* set to 0xffff for ISAPNP cards; returns NULL if no card found */

extern ihw_card_info	*hd_ihw_get_card_from_id(int vendor, int device,
					int subvendor, int subdevice);

/* Get a parameter information for a card identified with "card_handle" */
/* the pnr starts with 1 for the first parameter 2 for the second ... */
/* returns NULL, if here are no parameter with that pnr */

extern ihw_para_info	*hd_ihw_get_parameter(int card_handle, int pnr);

/* get driver informations for the driver with handle */
/* returns NULL if no driver has this handle */

extern ihw_driver_info	*hd_ihw_get_driver(int handle);

/* get driver informations  for the driver with typ and subtype */
/* returns NULL if no driver has this typ and subtype */

extern ihw_driver_info	*hd_ihw_get_driver_from_type(int typ, int subtyp);

/* get ihw struct version */
extern int		hd_ihw_get_version(void);

/* get ihw database version */
extern int		hd_ihw_get_db_version(void);

/* get ihw database creation date */
extern char		*hd_ihw_get_db_date(void);

/*
 * - - - - - ihw interface end  - - - - -
 */


#ifdef __cplusplus
}
#endif

#endif	/* _HD_H */

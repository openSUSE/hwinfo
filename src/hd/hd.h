#ifndef _HD_H
#define _HD_H

/**
 * @defgroup libhdPublic Public interface
 * @{
 */

#ifdef __cplusplus
extern "C" {
#endif


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 *                      libhd data structures
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

/** Interface version */
#define HD_VERSION		0	/* will be set during install */
#define HD_MINOR_VERSION	0	/* will be set during install */
#define HD_FULL_VERSION		(HD_VERSION * 1000 + HD_MINOR_VERSION)

/**
 * @defgroup DEBUGpub Debug flags
 * @ingroup libhdPublic
 * hd_data_t debug flags
 * @see hd_data_t::debug
 * @{
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
/** @} */

#include <stdio.h>
#include <inttypes.h>
#include <termios.h>
#include <sys/types.h>

//typedef struct vm_s vm_t;

/**
 * libhd's directory
 */
#define HARDWARE_DIR		"/var/lib/hardware"

/**
 * \defgroup idmacros ID macros
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
#define TAG_PCMCIA	5	/**< PCMCIA ids. */
#define TAG_SDIO	6	/**< SDIO ids. */

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

/**
 * flags to control the probing.
 */
typedef enum probe_feature {
  pr_memory = 1, pr_pci, pr_isapnp, pr_net, pr_floppy, pr_misc,
  pr_misc_serial, pr_misc_par, pr_misc_floppy, pr_serial, pr_cpu, pr_bios,
  pr_monitor, pr_mouse, pr_scsi, pr_usb, pr_usb_mods, pr_adb, pr_modem,
  pr_modem_usb, pr_parallel, pr_parallel_lp, pr_parallel_zip, pr_isa,
  pr_isa_isdn, pr_isdn, pr_kbd, pr_prom, pr_sbus, pr_int, pr_braille,
  pr_braille_alva, pr_braille_fhp, pr_braille_ht, pr_ignx11, pr_sys,
  pr_bios_vbe, pr_isapnp_old, pr_isapnp_new, pr_isapnp_mod, pr_braille_baum,
  pr_manual, pr_fb, pr_veth, pr_pppoe, pr_scan, pr_pcmcia, pr_fork,
  pr_parallel_imm, pr_s390, pr_cpuemu, pr_sysfs, pr_s390disks, pr_udev,
  pr_block, pr_block_cdrom, pr_block_part, pr_edd, pr_edd_mod, pr_bios_ddc,
  pr_bios_fb, pr_bios_mode, pr_input, pr_block_mods, pr_bios_vesa,
  pr_cpuemu_debug, pr_scsi_noserial, pr_wlan, pr_bios_crc, pr_hal,
  pr_bios_vram, pr_bios_acpi, pr_bios_ddc_ports, pr_modules_pata,
  pr_net_eeprom, pr_x86emu,
  pr_max, pr_lxrc, pr_default, 
  pr_all		/**< pr_all must be last */
} hd_probe_feature_t;

/**
 * list types for hd_list()
 *
 * if you want to modify this: cf. manual.c::hw_items[]
 *
 * Note: hw_tv _must_ be < hw_display!
 * Sync with check_hd and convert_hd!
 */
typedef enum hw_item {
  hw_none = 0, hw_sys, hw_cpu, hw_keyboard, hw_braille, hw_mouse,
  hw_joystick, hw_printer, hw_scanner, hw_chipcard, hw_monitor, hw_tv,
  hw_display, hw_framebuffer, hw_camera, hw_sound, hw_storage_ctrl,
  hw_network_ctrl, hw_isdn, hw_modem, hw_network, hw_disk, hw_partition,
  hw_cdrom, hw_floppy, hw_manual, hw_usb_ctrl, hw_usb, hw_bios, hw_pci,
  hw_isapnp, hw_bridge, hw_hub, hw_scsi, hw_ide, hw_memory, hw_dvb,
  hw_pcmcia, hw_pcmcia_ctrl, hw_ieee1394, hw_ieee1394_ctrl, hw_hotplug,
  hw_hotplug_ctrl, hw_zip, hw_pppoe, hw_wlan, hw_redasd, hw_dsl, hw_block,
  hw_tape, hw_vbe, hw_bluetooth, hw_fingerprint, hw_mmc_ctrl,
  /** append new entries here */
  hw_unknown, hw_all				/**< hw_all must be last */
} hd_hw_item_t;

/**
 * @defgroup DEVCLASSpub Device class enums
 * Device base classes and bus types
 * @{
 */

/** base class values (superset of PCI classes) */
typedef enum base_classes {
  // these *must* match standard PCI class numbers
  bc_none, bc_storage, bc_network, bc_display, bc_multimedia,
  bc_memory, bc_bridge, bc_comm, bc_system, bc_input, bc_docking,
  bc_processor, bc_serial, bc_wireless, bc_i2o, bc_other = 0xff,

  // add our own classes here (starting at 0x100 as PCI values are 8 bit)
  bc_monitor = 0x100, bc_internal, bc_modem, bc_isdn, bc_ps2, bc_mouse,
  bc_storage_device, bc_network_interface, bc_keyboard, bc_printer,
  bc_hub, bc_braille, bc_scanner, bc_joystick, bc_chipcard, bc_camera,
  bc_framebuffer, bc_dvb, bc_tv, bc_partition, bc_dsl, bc_bluetooth, bc_fingerprint,
  bc_mmc_ctrl
} hd_base_classes_t;

/** subclass values of bc_monitor */
typedef enum sc_monitor {
  sc_mon_other, sc_mon_crt, sc_mon_lcd
} hd_sc_monitor_t;

/** subclass values of bc_storage */
typedef enum sc_storage {
  sc_sto_scsi, sc_sto_ide, sc_sto_floppy, sc_sto_ipi, sc_sto_raid,
  sc_sto_other = 0x80
} hd_sc_storage_t;

/** subclass values of bc_display */
typedef enum sc_display {
  sc_dis_vga, sc_dis_xga, sc_dis_other = 0x80
} hd_sc_display_t;

/** subclass values of bc_framebuffer */
typedef enum sc_framebuffer {
  sc_fb_vesa = 1
} hd_sc_framebuffer_t;

/** subclass values of bc_bridge */
typedef enum sc_bridge { 
  sc_bridge_host, sc_bridge_isa, sc_bridge_eisa, sc_bridge_mc,
  sc_bridge_pci, sc_bridge_pcmcia, sc_bridge_nubus, sc_bridge_cardbus,
  sc_bridge_other = 0x80
} hd_sc_bridge_t;

/** subclass values of bc_comm */
typedef enum sc_comm { 
  sc_com_ser, sc_com_par, sc_com_multi, sc_com_modem, sc_com_other = 0x80
} hd_sc_comm_t;

/** subclass values of bc_system */
typedef enum sc_system {
  sc_sys_pic, sc_sys_dma, sc_sys_timer, sc_sys_rtc, sc_sys_other = 0x80
} hd_sc_system_t;

/** subclass values of bc_input */
typedef enum sc_input {
  sc_inp_keyb, sc_inp_digit, sc_inp_mouse, sc_inp_other = 0x80
} hd_sc_input_t;

/** subclass values of bc_serial */
typedef enum sc_serial {
  sc_ser_fire, sc_ser_access, sc_ser_ssa, sc_ser_usb, sc_ser_fiber,
  sc_ser_smbus, sc_ser_infiniband, sc_ser_other = 0x80
} hd_sc_serial_t;

/** internal sub class values (bc_internal) */
typedef enum sc_internal {
  sc_int_none, sc_int_isapnp_if, sc_int_main_mem, sc_int_cpu, sc_int_fpu,
  sc_int_bios, sc_int_prom, sc_int_sys
} hd_sc_internal_t;

/** subclass values of bc_mouse */
typedef enum sc_mouse {
  sc_mou_ps2, sc_mou_ser, sc_mou_bus, sc_mou_usb, sc_mou_sun,
  sc_mou_other = 0x80
} hd_sc_mouse_t;

/** subclass values of bc_storage_device */
typedef enum sc_std {
  sc_sdev_disk, sc_sdev_tape, sc_sdev_cdrom, sc_sdev_floppy, sc_sdev_scanner,
  sc_sdev_other = 0x80
} hd_sc_std_t;

/** subclass values of bc_network_interface */
typedef enum sc_net_if {
  sc_nif_loopback, sc_nif_ethernet, sc_nif_tokenring, sc_nif_fddi,
  sc_nif_ctc, sc_nif_iucv, sc_nif_hsi, sc_nif_qeth,
  sc_nif_escon, sc_nif_myrinet, sc_nif_wlan, sc_nif_xp,
  sc_nif_usb, sc_nif_other = 0x80, sc_nif_sit
} hd_sc_net_if_t;

/** subclass values of bc_multimedia */
typedef enum sc_multimedia {
  sc_multi_video, sc_multi_audio, sc_multi_other
} hd_sc_multimedia_t;

/** subclass values of bc_keyboard */
typedef enum sc_keyboard {
  sc_keyboard_kbd, sc_keyboard_console
} hd_sc_keyboard_t;

/** subclass values of bc_hub */
typedef enum sc_hub {
  sc_hub_other, sc_hub_usb
} hd_sc_hub_t;

/** subclass values of bc_camera */
typedef enum sc_camera {
  sc_camera_webcam, sc_camera_digital
} hd_sc_camera_t;

/** subclass values of bc_modem */
typedef enum sc_modem {
  sc_mod_at, sc_mod_win1, sc_mod_win2, sc_mod_win3, sc_mod_win4
} hd_sc_modem_t;

/** subclass values of bc_dsl */
typedef enum sc_dsl {
  sc_dsl_unknown, sc_dsl_pppoe, sc_dsl_capi, sc_dsl_capiisdn
} hd_sc_dsl_t;

/** prog_if's of sc_ser_usb */
typedef enum pif_usb_e {
  pif_usb_uhci = 0, pif_usb_ohci = 0x10, pif_usb_ehci = 0x20, pif_usb_xhci = 0x30,
  pif_usb_other = 0x80, pif_usb_device = 0xfe
} hd_pif_usb_t;

/** CD-ROM  prog_if values */
typedef enum pif_cdrom {
  pif_cdrom, pif_cdr, pif_cdrw, pif_dvd, pif_dvdr, pif_dvdram
} hd_pif_cdrom_t ;

/** S/390 disk prog_if values */
typedef enum pif_s390disk {
  pif_scsi, pif_dasd, pif_dasd_fba
} hd_pif_s390disk_t;

/** bus type values similar to PCI bridge subclasses */
typedef enum bus_types {
  bus_none, bus_isa, bus_eisa, bus_mc, bus_pci, bus_pcmcia, bus_nubus,
  bus_cardbus, bus_other,

  /** outside the range of the PCI values */
  bus_ps2 = 0x80, bus_serial, bus_parallel, bus_floppy, bus_scsi, bus_ide, bus_usb,
  bus_adb, bus_raid, bus_sbus, bus_i2o, bus_vio, bus_ccw, bus_iucv, bus_ps3_system_bus,
  bus_virtio, bus_ibmebus, bus_gameport, bus_uisvirtpci, bus_mmc, bus_sdio, bus_nd
} hd_bus_types_t;

/** @} */

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

  /**
   * Hardware is active.
   */
  unsigned active:3;
} hd_status_t;

/** hardware config status values */
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
 * @defgroup HDDATATYPEpub General data types
 * @brief General types used all over the library
 * @{
 */

/**
 * Holds ID + name pairs.
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


/**
 * Bitmap data type
 */
typedef struct {
  unsigned char bitmap[16];	/**< large enough for all uses */
  unsigned bits;		/**< real bitmap length in bits */
  unsigned not_empty:1;		/**< at least 1 bit is set */
  str_list_t *str;		/**< interpreted bitmask */
} hd_bitmap_t;

/** @} */


/**
 * @defgroup DEVINFOpub Device information structs
 * @brief Standard device structs, compared to @ref HWDETAILpub
 */

/**
 * @addtogroup DEVINFOpub
 * @{
 */

/**
 * for memory areas
 */
typedef struct {
  unsigned start, size;		/**< base address & size */
  unsigned char *data;		/**< actual data */
} memory_range_t;


/**
 * smp info according to Intel smp spec (ia32)
 */
typedef struct {
  unsigned ok:1;		/**< data are valid */
  unsigned rev;			/**< MP spec revision */
  unsigned mpfp;		/**< MP Floating Pointer struct */
  unsigned mpconfig_ok:1;	/**< MP config table valid */
  unsigned mpconfig;		/**< MP config table */
  unsigned mpconfig_size;	/**< dto, size */
  unsigned char feature[5];	/**< MP feature info */
  char oem_id[9];		/**< oem id */
  char prod_id[13];		/**< product id */
  unsigned cpus, cpus_en;	/**< number of cpus & ennabled cpus */
} smp_info_t;


/**
 * VESA BIOS mode information item
 */
typedef struct vbe_mode_info_s {
  unsigned number;		/**< mode number */
  unsigned attributes;		/**< mode attributes */
  unsigned width, height;	/**< mode size */
  unsigned bytes_p_line;	/**< line length */
  unsigned pixel_size;		/**< bits per pixel */
  unsigned fb_start;		/**< frame buffer start address (if any) */
  unsigned win_A_start;		/**< window A start address */
  unsigned win_A_attr;		/**< window A attributes */
  unsigned win_B_start;		/**< window B start address */
  unsigned win_B_attr;		/**< window B attributes */
  unsigned win_size;		/**< window size in bytes */
  unsigned win_gran;		/**< window granularity in bytes */
  unsigned pixel_clock;		/**< maximum pixel clock */
} vbe_mode_info_t;

/**
 * @brief VESA BIOS extensions information
 * Also includes a VESA mode list
 * @see vbe_mode_info_t
 */
typedef struct {
  unsigned ok:1;		/**< data are valid */
  unsigned version;		/**< vbe version */
  unsigned oem_version;		/**< oem version info */
  unsigned memory;		/**< in bytes */
  unsigned fb_start;		/**< != 0 if framebuffer is supported */
  char *oem_name;		/**< oem name */
  char *vendor_name;		/**< vendor name */
  char *product_name;		/**< product name */
  char *product_revision;	/**< product revision */
  unsigned modes;		/**< number of supported video modes */
  vbe_mode_info_t *mode;	/**< video mode list */
  unsigned current_mode;	/**< current video mode */
  unsigned ddc_ports;		/**< max ports to probe */
  unsigned char ddc_port[4][0x80];	/**< ddc monitor info per port */
} vbe_info_t;


/**
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
  unsigned ok:1;		/**< data are valid */
  unsigned entry;		/**< entry point */
  unsigned compaq:1;		/**< is compaq system */
  cpq_ctlorder_t cpq_ctrl[32];	/**< 32 == MAX_CONTROLLERS */
} bios32_info_t;

/** @} */

/**
 * @defgroup SMBIOSpub SMBIOS structures
 * Structures holding decoded SMBIOS information
 * @{
 */

/** smbios entries */
typedef enum {
  sm_biosinfo, sm_sysinfo, sm_boardinfo, sm_chassis,
  sm_processor, sm_memctrl, sm_memmodule, sm_cache,
  sm_connect, sm_slot, sm_onboard, sm_oem,
  sm_config, sm_lang, sm_group, sm_eventlog,
  sm_memarray, sm_memdevice, sm_memerror, sm_memarraymap,
  sm_memdevicemap, sm_mouse, sm_battery, sm_reset,
  sm_secure, sm_power, sm_voltage, sm_cool,
  sm_temperature, sm_current, sm_outofband, sm_bis,
  sm_boot, sm_mem64error, sm_mandev, sm_mandevcomp,
  sm_mdtd, sm_inactive = 126, sm_end = 127
} hd_smbios_type_t;


/** common part of all smbios_* types */
typedef struct {
  union u_hd_smbios_t *next;	/**< link to next entry */
  hd_smbios_type_t type;	/**< BIOS info type */
  int data_len;			/**< formatted section length */
  unsigned char *data;		/**< formatted section */
  str_list_t *strings;		/**< strings taken from the unformed section */
  int handle;			/**< handle, unique 16 bit number */
} smbios_any_t;


/** BIOS related information */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *vendor;			/**< vendor name */
  char *version;		/**< version (free form) */
  char *date;			/**< date mm/dd/yyyy (old: yy) */
  hd_bitmap_t feature;		/**< BIOS characteristics */
  unsigned start;		/**< start address */
  unsigned rom_size;		/**< ROM size (in bytes) */
} smbios_biosinfo_t;


/** overall system related information */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *manuf;			/**< manufacturer */
  char *product;		/**< product name */
  char *version;		/**< version */
  char *serial;			/**< serial number */
  unsigned char uuid[16];	/**< universal unique id; all 0x00: undef, all 0xff: undef but settable */
  hd_id_t wake_up;		/**< wake-up type */
} smbios_sysinfo_t;


/** motherboard related information */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *manuf;			/**< manufacturer */
  char *product;		/**< product name */
  char *version;		/**< version */
  char *serial;			/**< serial number */
  char *asset;			/**< asset tag */
  hd_id_t board_type;		/**< board type */
  hd_bitmap_t feature;		/**< board features */
  char *location;		/**< location in chassis */
  int chassis;			/**< handle of chassis */
  int objects_len;		/**< number of contained objects */
  int *objects;			/**< array of object handles */
} smbios_boardinfo_t;


/** chassis information */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *manuf;			/**< manufacturer */
  char *version;		/**< version */
  char *serial;			/**< serial number */
  char *asset;			/**< asset tag */
  hd_id_t ch_type;		/**< chassis type */
  unsigned lock;		/**< 1: lock present, 0: not present or unknown */
  hd_id_t bootup;		/**< bootup state */
  hd_id_t power;		/**< power supply state (at last boot) */
  hd_id_t thermal;		/**< thermal state (at last boot) */
  hd_id_t security;		/**< security state (at last boot) */
  unsigned oem;			/**< OEM-specific information */
} smbios_chassis_t;


/** processor information */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *socket;			/**< socket */
  hd_id_t upgrade;		/**< socket type */
  char *manuf;			/**< manufacturer */
  char *version;		/**< version */
  char *serial;			/**< serial number */
  char *asset;			/**< asset tag */
  char *part;			/**< part number */
  hd_id_t pr_type;		/**< processor type */
  hd_id_t family;		/**< processor family */
  uint64_t cpu_id;		/**< processor id */
  unsigned voltage;		/**< in 0.1 V */
  unsigned ext_clock;		/**< MHz */
  unsigned max_speed;		/**< MHz */
  unsigned current_speed;	/**< MHz */
  unsigned sock_status;		/**< socket status (1: populated, 0: empty */
  hd_id_t cpu_status;		/**< cpu status */
  int l1_cache;			/**< handle of L1 cache */
  int l2_cache;			/**< handle of L2 cache */
  int l3_cache;			/**< handle of L3 cache */
} smbios_processor_t;


/** cache information */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *socket;			/**< socket designation */
  unsigned max_size;		/**< max cache size in kbytes */
  unsigned current_size;	/**< current size in kbytes */
  unsigned speed;		/**< cache speed in nanoseconds */
  hd_id_t mode;			/**< operational mode */
  unsigned state;		/**< 0/1: disabled/enabled */
  hd_id_t location;		/**< cache location */
  unsigned socketed;		/**< 0/1: not socketed/socketed */
  unsigned level;		/**< cache level (0 = L1, 1 = L2, ...) */
  hd_id_t ecc;			/**< error correction type */
  hd_id_t cache_type;		/**< logical cache type */
  hd_id_t assoc;		/**< cache associativity */
  hd_bitmap_t supp_sram;	/**< supported SRAM types */
  hd_bitmap_t sram;		/**< current SRAM type */
} smbios_cache_t;


/** port connector information */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  hd_id_t port_type;		/**< port type */
  char *i_des;			/**< internal reference designator */
  hd_id_t i_type;		/**< internal connector type */
  char *x_des;			/**< external reference designator */
  hd_id_t x_type;		/**< external connector type */
} smbios_connect_t;


/** system slot information */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *desig;			/**< slot designation */
  hd_id_t slot_type;		/**< slot type */
  hd_id_t bus_width;		/**< data bus width */
  hd_id_t usage;		/**< current usage */
  hd_id_t length;		/**< slot length */
  unsigned id;			/**< slot id */
  hd_bitmap_t feature;		/**< slot characteristics */
} smbios_slot_t;


/** on board devices information */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  unsigned dev_len;		/**< device list length */
  struct {
    char *name;			/**< device name */
    hd_id_t type;		/**< device type */
    unsigned status;		/**< 0: disabled, 1: enabled */
  } *dev;			/**< device list  */
} smbios_onboard_t;


/** OEM information */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  str_list_t *oem_strings;	/**< OEM strings */
} smbios_oem_t;


/** system config options */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  str_list_t *options;		/**< system config options */
} smbios_config_t;


/** language information */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;		/**< list of languages */
  int handle;
  char *current;		/**< current language */
} smbios_lang_t;


/** group associations */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *name;			/**< group name */
  int items_len;		/**< number of items in this group */
  int *item_handles;		/**< array of item handles */
} smbios_group_t;


/** physical memory array (consists of several memory devices) */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  hd_id_t location;		/**< memory device location */
  hd_id_t use;			/**< memory usage */
  hd_id_t ecc;			/**< ECC types */
  unsigned max_size;		/**< maximum memory size in kB */
  int error_handle;		/**< points to error info record; 0xfffe: not supported, 0xffff: no error */
  unsigned slots;		/**< slots or sockets for this device */
} smbios_memarray_t;


/** memory device */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  char *location;		/**< device location */
  char *bank;			/**< bank location */
  char *manuf;			/**< manufacturer */
  char *serial;			/**< serial number */
  char *asset;			/**< asset tag */
  char *part;			/**< part number */
  int array_handle;		/**< memory array this device belongs to */
  int error_handle;		/**< points to error info record; 0xfffe: not supported, 0xffff: no error */
  unsigned width;		/**< data width in bits */
  unsigned eccbits;		/**< ecc bits */
  unsigned size;		/**< kB */
  hd_id_t form;			/**< form factor */
  unsigned set;			/**< 0: does not belong to a set; 1-0xfe: set number; 0xff: unknown */
  hd_id_t mem_type;		/**< memory type */
  hd_bitmap_t type_detail;	/**< memory type details */
  unsigned speed;		/**< in MHz */
} smbios_memdevice_t;


/** 32-bit memory error information  */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  hd_id_t err_type;		/**< error type memory */
  hd_id_t granularity;		/**< memory array or memory partition */
  hd_id_t operation;		/**< mem operation causing the error */
  unsigned syndrome;		/**< vendor-specific ECC syndrome; 0: unknown */
  unsigned array_addr;		/**< fault address rel. to mem array; 0x80000000: unknown */
  unsigned device_addr;		/**< fault address rel to mem device; 0x80000000: unknown */
  unsigned range;		/**< range, within which the error can be determined; 0x80000000: unknown */
} smbios_memerror_t;


/** memory array mapped address */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  int array_handle;		/**< memory array this mapping belongs to */
  uint64_t start_addr;		/**< memory range start address */
  uint64_t end_addr;		/**< end address */
  unsigned part_width;		/**< number of memory devices */
} smbios_memarraymap_t;


/** memory device mapped address */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  int memdevice_handle;		/**< memory device handle */
  int arraymap_handle;		/**< memory array mapping handle */
  uint64_t start_addr;		/**< memory range start address */
  uint64_t end_addr;		/**< end address */
  unsigned row_pos;		/**< position of the referenced memory device in a row of the address partition */
  unsigned interleave_pos;	/**< dto, in an interleave */
  unsigned interleave_depth;	/**< number of consecutive rows */
} smbios_memdevicemap_t;


/** pointing device (aka 'mouse') information */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  hd_id_t mtype;		/**< mouse type */
  hd_id_t interface;		/**< interface type */
  unsigned buttons;		/**< number of buttons */
} smbios_mouse_t;


/** hardware security */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  hd_id_t power;		/**< power-on password status */
  hd_id_t keyboard;		/**< keyboard password status */
  hd_id_t admin;		/**< admin password status */
  hd_id_t reset;		/**< front panel reset status */
} smbios_secure_t;


/** system power controls */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  unsigned month;		/**< next scheduled power-on month */
  unsigned day;			/**< dto, day */
  unsigned hour;		/**< dto, hour */
  unsigned minute;		/**< dto, minute */
  unsigned second;		/**< dto, second */
} smbios_power_t;


/** 64-bit memory error information  */
typedef struct {
  union u_hd_smbios_t *next;
  hd_smbios_type_t type;
  int data_len;
  unsigned char *data;
  str_list_t *strings;
  int handle;
  hd_id_t err_type;		/**< error type memory */
  hd_id_t granularity;		/**< memory array or memory partition */
  hd_id_t operation;		/**< mem operation causing the error */
  unsigned syndrome;		/**< vendor-specific ECC syndrome; 0: unknown */
  uint64_t array_addr;		/**< fault address rel. to mem array; 0x80000000: unknown */
  uint64_t device_addr;		/**< fault address rel to mem device; 0x80000000: unknown */
  unsigned range;		/**< range, within which the error can be determined; 0x80000000: unknown */
} smbios_mem64error_t;


/** SMBIOS list item */
typedef union u_hd_smbios_t {
  union u_hd_smbios_t *next;  
  smbios_any_t any;
  smbios_biosinfo_t biosinfo;
  smbios_sysinfo_t sysinfo;
  smbios_boardinfo_t boardinfo;
  smbios_chassis_t chassis;
  smbios_processor_t processor;
  smbios_cache_t cache;
  smbios_connect_t connect;
  smbios_slot_t slot;
  smbios_onboard_t onboard;
  smbios_oem_t oem;
  smbios_config_t config;
  smbios_lang_t lang;
  smbios_group_t group;
  smbios_memarray_t memarray;
  smbios_memdevice_t memdevice;
  smbios_memerror_t memerror;
  smbios_memarraymap_t memarraymap;
  smbios_memdevicemap_t memdevicemap;
  smbios_mouse_t mouse;
  smbios_secure_t secure;
  smbios_power_t power;
  smbios_mem64error_t mem64error;
} hd_smbios_t;

/** @} */



/**
 * udev database info
 */
typedef struct s_udevinfo_t {
  struct s_udevinfo_t *next;
  char *sysfs;
  char *name;
  str_list_t *links;
} hd_udevinfo_t;


/**
 * sysfs driver info
 */
typedef struct s_sysfsdrv_t {
  struct s_sysfsdrv_t *next;
  char *driver;
  char *device;
  char *module;
} hd_sysfsdrv_t;


/**
 * device number; type is either 0 or 'b' or 'c'.
 *
 * range: number of nodes
 */
typedef struct {
  int type;
  unsigned major, minor, range;
} hd_dev_num_t;


/**
 * @defgroup HWDETAILpub Hardware information
 *
 * Some hardware doesn't fit into the hd_t scheme or there is info we
 * gathered during the scan process but that no-one really cares about. Such
 * stuff is stored in hd_detail_t.
 *
 * @{
 */


/**
 * @brief structure holding the (raw) PCI data
 */
typedef struct s_pci_t {
  struct s_pci_t *next;				/**< linked list */
  unsigned data_len;				/**< the actual length of the data field */
  unsigned data_ext_len;			/**< max. accessed config byte; see code */
  unsigned char data[256];			/**< the PCI data */
  char *log;					/**< log messages */
  unsigned flags,				/**< various info, see enum pci_flags */
           cmd,					/**< PCI_COMMAND */
           hdr_type,				/**< PCI_HEADER_TYPE */
           secondary_bus;			/**< > 0 for PCI & CB bridges */
  unsigned bus,					/**< PCI bus #, *nothing* to do with hw_t.bus */
           slot, func; 				/**< slot & function */
  unsigned base_class, sub_class, prog_if;	/**< PCI device classes */
  unsigned dev, vend, sub_dev, sub_vend, rev;	/**< vendor & device ids */
  unsigned irq;					/**< used irq, if any */
  uint64_t base_addr[7];			/**< I/O or memory base */
  uint64_t base_len[7];				/**< I/O or memory ranges */
  unsigned addr_flags[7];			/**< I/O or memory address flags */
  uint64_t rom_base_addr;			/**< memory base for card ROM */
  uint64_t rom_base_len;			/**< memory range for card ROM */
  char *sysfs_id;				/**< sysfs path */
  char *sysfs_bus_id;				/**< sysfs bus id */
  char *modalias;				/**< module alias */
  char *label;					/**< Consistant Device Name (CDN), pci firmware spec 3.1, chapter 4.6.7 */
  unsigned edid_len[6];				/**< edid record length */
  unsigned char edid_data[6][0x80];		/**< edid record */
} pci_t;

/**
 * @brief pci related flags 
 * cf. (pci_t).flags
 */
typedef enum pci_flags {
  pci_flag_ok, pci_flag_pm, pci_flag_agp
} hd_pci_flags_t;


/**
 * @brief raw USB data
 * @see Linux USB docs
 */
typedef struct usb_s {
  struct usb_s *next;
  unsigned hd_idx;
  unsigned hd_base_idx;
  str_list_t *c, *d, *e, *i, *p, *s, *t;
  struct usb_s *cloned;
  int bus, dev_nr, lev, parent, port, count, conns, used_conns, ifdescr;
  unsigned speed;
  unsigned vendor, device, rev;
  char *manufact, *product, *serial;
  char *driver;
  memory_range_t raw_descr;
  int d_cls, d_sub, d_prot;
  int i_alt, i_cls, i_sub, i_prot;
  unsigned country;
} usb_t;


/**
 * @brief ISA-PnP resource
 */
typedef struct {
  int len;
  int type;
  unsigned char *data;
} isapnp_res_t;

/**
 * @brief ISA-PnP card information (raw)
 */
typedef struct {
  int csn;
  int log_devs;
  unsigned char *serial;
  unsigned char *card_regs;
  unsigned char (*ldev_regs)[0xd0];
  int res_len;
  unsigned broken:1;		/**< mark a broken card */
  isapnp_res_t *res;
} isapnp_card_t;

/**
 * ISA-PnP collected card information struct
 */
typedef struct {
  int read_port;
  int cards;
  isapnp_card_t *card;
} isapnp_t;

/**
 * @brief ISA-PnP device information struct
 */
typedef struct {
  isapnp_card_t *card;
  int dev;
  unsigned flags;		/**< cf. enum isapnp_flags */
  unsigned ref:1;		/**< internally used flag */
} isapnp_dev_t;

/**
 * @brief ISA-PnP related flags
 * cf. (isapnp_dev_t).flags
 */
typedef enum isapnp_flags {
  isapnp_flag_act
} hd_isapnp_flags_t;


/**
 * @brief raw SCSI data
 */
typedef struct scsi_s {
  struct scsi_s *next;
  unsigned deleted:1;
  unsigned generic:1;
  unsigned fake:1;
  unsigned wwpn_ok:1;
  unsigned fcp_lun_ok:1;
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
  uint64_t wwpn;
  uint64_t fcp_lun;
  char *controller_id;
} scsi_t;


/**
 * @brief PROM tree on PPC
 */
typedef struct devtree_s {
  struct devtree_s *next;
  struct devtree_s *parent;
  unsigned idx;
  char *path, *filename;
  unsigned pci:1;
  char *name, *model, *device_type, *compatible;
  char *ccin, *fru_number, *loc_code, *serial_number, *part_number;
  char *description;
  int class_code;                       /**< class : sub_class : prog-if */
  int vendor_id, device_id, subvendor_id, subdevice_id;
  int revision_id, interrupt;
  unsigned char *edid;                  /**< 128 bytes */
} devtree_t;

enum pmac_model {
	AAPL_3400,
	AAPL_3500,
	AAPL_7200,
	AAPL_7300,
	AAPL_7500,
	AAPL_8500,
	AAPL_9500,
	AAPL_Gossamer,
	AAPL_PowerBook1998,
	AAPL_PowerMac_G3,
	AAPL_ShinerESB,
	AAPL_e407,
	AAPL_e411,
	PowerBook1_1,
	PowerBook2_1,
	PowerBook2_2,
	PowerBook3_1,
	PowerBook3_2,
	PowerBook3_3,
	PowerBook3_4,
	PowerBook3_5,
	PowerBook4_1,
	PowerBook4_2,
	PowerBook4_3,
	PowerBook5_1,
	PowerBook5_2,
	PowerBook5_3,
	PowerBook5_4,
	PowerBook5_5,
	PowerBook5_6,
	PowerBook5_7,
	PowerBook5_8,
	PowerBook5_9,
	PowerBook6_1,
	PowerBook6_2,
	PowerBook6_3,
	PowerBook6_4,
	PowerBook6_5,
	PowerBook6_7,
	PowerBook6_8,
	PowerMac1_1,
	PowerMac1_2,
	PowerMac10_1,
	PowerMac11_2,
	PowerMac12_1,
	PowerMac2_1,
	PowerMac2_2,
	PowerMac3_1,
	PowerMac3_2,
	PowerMac3_3,
	PowerMac3_4,
	PowerMac3_5,
	PowerMac3_6,
	PowerMac4_1,
	PowerMac4_2,
	PowerMac4_4,
	PowerMac5_1,
	PowerMac6_1,
	PowerMac6_3,
	PowerMac6_4,
	PowerMac7_2,
	PowerMac7_3,
	PowerMac8_1,
	PowerMac9_1,
	RackMac1_1,
	RackMac1_2,
	RackMac3_1,
	iMac_1,
};
/**
 * @brief PowerMac model matching
 */
struct pmac_mb_def {
  enum pmac_model model;
  const char *string;
};

/**
 * @brief Device/CU model numbers for S/390
 */
typedef struct ccw_s {
  unsigned char lcss;
  unsigned char cu_model;
  unsigned char dev_model;
} ccw_t;

/**
 * @brief Joystick details
 */
typedef struct joystick_s {
  unsigned char buttons;
  unsigned char axes;
} joystick_t;

/**
 * @brief special CDROM entry
 */
typedef struct cdrom_info_s {
  struct cdrom_info_s *next;
  char *name;
  unsigned speed;
  unsigned cdr:1, cdrw:1, dvd:1, dvdr:1, dvdram:1;
  unsigned cdrom:1;		/**< cdrom in drive */
  struct {
    unsigned ok:1;
    char *volume, *publisher, *preparer, *application, *creation_date;
  } iso9660;
  struct {
    unsigned ok:1;
    unsigned platform;
    char *id_string;
    unsigned bootable:1;
    unsigned media_type;	/**< boot emulation type */
    unsigned load_address;
    unsigned load_count;	/**< sectors to load */
    unsigned start;		/**< start sector */
    unsigned catalog;		/**< boot catalog start */
    struct {
      unsigned c, h, s;
      unsigned size;
    } geo;
    char *label;
  } el_torito;

} cdrom_info_t;


/**
 * @brief Floppy information 
 * note: obsolete, will be removed 
 * @deprecated
 */
typedef struct {
  unsigned char block0[512];
} floppy_info_t;


/**
 * @brief bios data (ix86)
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

  /** The id is still in big endian format! */
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
    unsigned xsize;
    unsigned ysize;
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


/**
 * @brief prom data (ppc, sparc)
 */
typedef struct {
  unsigned has_color:1;
  unsigned color;
} prom_info_t;


/**
 * @brief general system data
 */
typedef struct {
  char *system_type;
  char *generation;
  char *vendor;
  char *model;
  char *serial;
  char *lang;
  char *formfactor;
} sys_info_t;


/**
 * @brief monitor (DDC) data
 */
typedef struct {
  unsigned manu_year;
  unsigned manu_week;
  unsigned min_vsync, max_vsync;	/**< vsync range */
  unsigned min_hsync, max_hsync;	/**< hsync range */
  unsigned clock;			/**< pixel clock in kHz */
  unsigned width, height;		/**< display size */
  unsigned width_mm, height_mm;		/**< dto, in mm */
  unsigned hdisp, hsyncstart, hsyncend, htotal; /**< h_timings */
  unsigned vdisp, vsyncstart, vsyncend, vtotal; /**< v_timings */
  char hflag,vflag; /**< h/v flags */
  char *vendor;
  char *name;
  char *serial;
} monitor_info_t;

/** @} */


/**
 * @brief CPU architecture
 */
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
  arch_x86_64,
  arch_aarch64,
  arch_riscv
} hd_cpu_arch_t;

/**
 * @todo drop boot_arch at all? 
 */
typedef enum boot_arch {
  boot_unknown = 0,
  boot_lilo, boot_milo, boot_aboot, boot_silo, boot_ppc, boot_elilo, boot_s390,
  boot_mips, boot_grub, boot_uboot,
} hd_boot_arch_t;


/**
 * @addtogroup HWDETAILpub
 * @{
 */

/**
 * @brief special cpu entry
 */
typedef struct {
  enum cpu_arch architecture;
  unsigned family;		/**< axp: cpu variation */
  unsigned model;		/**< axp: cpu revision */
  unsigned stepping;
  unsigned cache;
  unsigned clock;
  unsigned units;		/**< >1 "hyperthreading" */
  char *vend_name;		/**< axp: system type */
  char *model_name;		/**< axp: cpu model */
  char *platform;		/**< x86: NULL */
  str_list_t *features;		/**< x86: flags */
  double bogo;			/**< bogo mips */
} cpu_info_t;


/**
 * @brief enhanced disk data 
 * (cf. edd.c)
 */
typedef struct {
  uint64_t sectors;
  struct {
    unsigned cyls, heads, sectors;
  } edd;
  struct {
    unsigned cyls, heads, sectors;
  } legacy;
  unsigned ext_fixed_disk:1;
  unsigned ext_lock_eject:1;
  unsigned ext_edd:1;
  unsigned ext_64bit:1;
  unsigned assigned:1;
  unsigned valid:1;
  unsigned ext_fibre:1;
  unsigned ext_net:1;
  char *sysfs_id;
  unsigned hd_idx;
  unsigned signature;
} edd_info_t;

/** @} */

/**
 * Hardware DB (v1) data
 */
typedef struct {
  unsigned data_len, data_max;
  unsigned *data;
  unsigned names_len, names_max;
  char *names;
} hddb_data_t;

/**
 * Hardware DB item entry mask
 */
typedef uint32_t hddb_entry_mask_t;

/**
 * Hardware DB list item
 */
typedef struct hddb_list_s {   
  hddb_entry_mask_t key_mask;
  hddb_entry_mask_t value_mask;
  unsigned key;
  unsigned value;
} hddb_list_t;

/**
 * Hardware DB (v2) data
 */
typedef struct {
  unsigned list_len, list_max;
  hddb_list_t *list;
  unsigned ids_len, ids_max;
  unsigned *ids;
  unsigned strings_len, strings_max;
  char *strings;
} hddb2_data_t;


/**
 * module information type
 */
typedef enum modinfo_type_e { mi_none = 0, mi_pci, mi_other } modinfo_type_t;

/**
 * module.alias information
 */
typedef struct {
  char *module;
  char *alias;
  modinfo_type_t type;
  union {
    struct {
      struct {
        unsigned vendor:1;
        unsigned device:1;
        unsigned sub_vendor:1;
        unsigned sub_device:1;
        unsigned base_class:1;
        unsigned sub_class:1;
        unsigned prog_if:1;
      } has;
      unsigned vendor;
      unsigned device;
      unsigned sub_vendor;
      unsigned sub_device;
      unsigned base_class;
      unsigned sub_class;
      unsigned prog_if;
    } pci;

    struct {
      struct {
        unsigned vendor:1;
        unsigned product:1;
        unsigned device_class:1;
        unsigned device_subclass:1;
      } has;
      unsigned vendor;
      unsigned product;
      unsigned device_class;
      unsigned device_subclass;
    } usb;
  };
} modinfo_t;


/**
 * HAL device property types
 */
typedef enum {
  p_invalid, p_string, p_int32, p_uint64, p_double, p_bool, p_list
} hal_prop_type_t;


/**
 * HAL device properties
 */
typedef struct hal_prop_s {
  struct hal_prop_s *next;
  hal_prop_type_t type;
  char *key;
  union {
    char *str;
    int32_t int32;
    uint64_t uint64;
    double d;     
    int b;
    str_list_t *list;
  } val;  
} hal_prop_t;


/**
 * HAL device
 */
typedef struct hal_device_s {
  struct hal_device_s *next, *parent;
  char *udi;
  unsigned used:1;
  hal_prop_t *prop;
} hal_device_t;

/**
 * resource types: see @ref RESOURCEpub
 */
typedef enum resource_types {
  res_any, res_phys_mem, res_mem, res_io, res_irq, res_dma, res_monitor,
  res_size, res_disk_geo, res_cache, res_baud, res_init_strings, res_pppd_option,
  res_framebuffer, res_hwaddr, res_link, res_wlan, res_fc, res_phwaddr
} hd_resource_types_t;


/**
 * size units (cf. (res_size_t).unit)
 */
typedef enum size_units {
  size_unit_cm, size_unit_cinch, size_unit_byte, size_unit_sectors,
  size_unit_kbyte, size_unit_mbyte, size_unit_gbyte, size_unit_mm
} hd_size_units_t;

/**
 * access types for I/O and memory resources
 */
typedef enum access_flags {
  acc_unknown, 		/**< unknown */
  acc_ro, 		/**< read only */
  acc_wo, 		/**< write only */
  acc_rw		/**< read/write */
} hd_access_flags_t;


typedef enum yes_no_flag {
  flag_unknown, 	/**< unknown */
  flag_no, 		/**< no */
  flag_yes		/**< yes */
} hd_yes_no_flag_t;


typedef enum geo_types {
  geo_physical = 0, 
  geo_logical, 
  geo_bios_edd, 
  geo_bios_legacy
} hd_geo_types_t;


/**
 * @defgroup RESOURCEpub Resource structures
 * definitions for the various resource types
 * @{
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
    enabled:1,			/**< 0: disabled, 1 enabled */
    access:2,			/**< enum access_flags */
    prefetch:2;			/**< enum yes_no_flag */
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
    enabled:1,				/**< 0: disabled, 1 enabled */
    access:2;				/**< enum access_flags */
} res_io_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  unsigned base;
  unsigned triggered;			/**< # of interrupts */
  unsigned enabled:1;			/**< 0: disabled, 1 enabled */
} res_irq_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  unsigned base;
  unsigned enabled:1;			/**< 0: disabled, 1 enabled */
} res_dma_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  enum size_units unit;
  uint64_t val1, val2;			/**< to allow for 2D values */
} res_size_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  unsigned speed;
  unsigned bits, stopbits;
  char parity;				/**< n, e, o, s, m */
  char handshake;			/**< -, h, s */
} res_baud_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  unsigned size;			/**< in kbyte */
} res_cache_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  unsigned cyls, heads, sectors;
  uint64_t size;
  enum geo_types geotype;		/**< 0-3: physical/logical/bios edd/bios legacy */
} res_disk_geo_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  unsigned width, height;		/**< in pixel */
  unsigned vfreq;			/**< in Hz */
  unsigned interlaced:1;		/**< 0/1 */
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
  unsigned width, height;		/**< in pixel */
  unsigned bytes_p_line;		/**< line length in bytes (do not confuse with 'width') */
  unsigned colorbits;			/**< 4, 8, 15, 16, 24, 32 */
  unsigned mode;			/**< mode number for kernel */
} res_framebuffer_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  char *addr;
} res_hwaddr_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  unsigned state:1;			/**< network link state: 0 - not connected, 1 - connected */
} res_link_t;

/** wlan capabilities */
typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  str_list_t *channels;
  str_list_t *frequencies; /**< in GHz units */
  str_list_t *bitrates;    /**< in Mbps units */
  str_list_t *auth_modes;  /**< open, sharedkey, wpa-psk, wpa-eap, wpa-leap */
  str_list_t *enc_modes;   /**< WEP40, WEP104, WEP128, WEP232, TKIP, CCMP */
} res_wlan_t;

typedef struct {
  union u_hd_res_t *next;
  enum resource_types type;
  unsigned wwpn_ok:1;
  unsigned fcp_lun_ok:1;
  unsigned port_id_ok:1;
  uint64_t wwpn;
  uint64_t fcp_lun;
  unsigned port_id;
  char *controller_id;
} res_fc_t;

/** libhd resource union */
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
  res_hwaddr_t hwaddr;
  res_link_t link;
  res_wlan_t wlan;
  res_fc_t fc;
} hd_res_t;

/** @} */

/**
 * @defgroup MISCpub Misc resource data
 * data gathered by the misc module; basically resources from /proc
 * @{
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

/** @} */

/**
 * Serial device resource and hardware information
 */
typedef struct s_serial_t {
  struct s_serial_t *next;
  char *name;
  char *device;
  unsigned line, port, irq, baud;
} serial_t;

/**
 * Serial device configuration information
 */
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

/**
 * @defgroup DRVINFO Driver information
 * @brief Driver information structures and union
 * @{
 */

/**
 * @brief ISDN configuration parameter
 * Notes on isdn_parm_t:
 *   - def_value is only relevant of alt_values != 0
 *   - def_value should be a value out of alt_value[]
 *   - see libihw docu for the meaning of name,type,flags,def_value
 */
typedef struct isdn_parm_s {
  struct isdn_parm_s *next;
  char *name;				/**< parameter name */
  unsigned valid:1;			/**< 1: entry is valid, 0: some inconsistencies */
  unsigned conflict:1;			/**< 1: ressource conflict (eg. no free irq) */
  uint64_t value;			/**< value of the parameter */
  unsigned type;			/**< CDBISDN type (P_...) */
  unsigned flags;			/**< CDBISDN flags (P_...) */
  unsigned def_value;			/**< default value */
  int alt_values;			/**< length of alt_value[] */
  unsigned *alt_value;			/**< possible values */
} isdn_parm_t;

/** device driver info types */
typedef enum driver_info_type {
  di_any, di_display, di_module, di_mouse, di_x11, di_isdn, di_kbd, di_dsl
} hd_driver_info_t;

/** unspecific info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/**< driver info type */
  str_list_t *hddb0, *hddb1;		/**< the actual driver database entries */
} driver_info_any_t;

/** display (monitor) info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/**< driver info type */
  str_list_t *hddb0, *hddb1;		/**< the actual driver database entries */
  unsigned width, height;		/**< max. useful display geometry */
  unsigned min_vsync, max_vsync;	/**< vsync range */
  unsigned min_hsync, max_hsync;	/**< hsync range */
  unsigned bandwidth;			/** max. pixel clock */
  unsigned hdisp, hsyncstart, hsyncend, htotal; /** h_timings */
  unsigned vdisp, vsyncstart, vsyncend, vtotal; /** v_timings */
  char hflag,vflag; /** h/v flags */
} driver_info_display_t;

/** module info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/**< driver info type */
  str_list_t *hddb0, *hddb1;		/**< the actual driver database entries */
  unsigned active:1;			/**< if module is currently active */
  unsigned modprobe:1;			/**< modprobe or insmod  */
  str_list_t *names;			/**< (ordered) list of module names */
  str_list_t *mod_args;			/**< list of module args (corresponds to the module name list) */
  char *conf;				/**< conf.modules entry, if any (e.g. for sb.o) */
} driver_info_module_t;

/** mouse protocol info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/**< driver info type */
  str_list_t *hddb0, *hddb1;		/**< the actual driver database entries */
  char *xf86;				/**< the XF86 protocol name */
  char *gpm;				/**< dto, gpm */
  int buttons;				/**< number of buttons, -1 --> unknown */
  int wheels;				/**< dto, wheels */
} driver_info_mouse_t;

/** X11 server info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/**< driver info type */
  str_list_t *hddb0, *hddb1;		/**< the actual driver database entries */
  char *server;				/**< the server/module name */
  char *xf86_ver;			/**< XFree86 version (3 or 4) */
  unsigned x3d:1;			/**< has 3D support */
  struct {
    unsigned all:5;			/**< the next 5 entries combined */
    unsigned c8:1, c15:1, c16:1, c24:1, c32:1;
  } colors;				/**< supported color depths */
  unsigned dacspeed;			/**< max. ramdac clock */
  str_list_t *extensions;		/**< additional X extensions to load ('Module' section) */
  str_list_t *options;			/**< special server options */
  str_list_t *raw;			/**< extra info to add to XF86Config */
  char *script;				/**< 3d script to run */
} driver_info_x11_t;

/** isdn info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/**< driver info type */
  str_list_t *hddb0, *hddb1;		/**< the actual driver database entries */
  int i4l_type, i4l_subtype;		/**< I4L types */
  char *i4l_name;			/**< I4L card name */
  isdn_parm_t *params;			/**< isdn parameters */
} driver_info_isdn_t;

/** dsl info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/**< driver info type */
  str_list_t *hddb0, *hddb1;		/**< the actual driver database entries */
  char *mode;				/**< DSL driver types */
  char *name;				/**< DSL driver name */
} driver_info_dsl_t;

/** keyboard info */
typedef struct {
  union driver_info_u *next;
  enum driver_info_type type;		/**< driver info type */
  str_list_t *hddb0, *hddb1;		/**< the actual driver database entries */
  char *XkbRules;			/**< XF86Config entries */
  char *XkbModel;
  char *XkbLayout;
  char *keymap;				/**< console keymap */
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
  driver_info_dsl_t dsl;
  driver_info_kbd_t kbd;
} driver_info_t;

/** @} */

/**
 * @ingroup HWDETAILpub
 * @{
 */

/**
 * Hardware detail information type
 */
typedef enum hd_detail_type {
  hd_detail_pci, hd_detail_usb, hd_detail_isapnp, hd_detail_cdrom,
  hd_detail_floppy, hd_detail_bios, hd_detail_cpu, hd_detail_prom,
  hd_detail_monitor, hd_detail_sys, hd_detail_scsi, hd_detail_devtree,
  hd_detail_ccw, hd_detail_joystick
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

typedef struct hd_detail_monitor_s {
  enum hd_detail_type type;
  monitor_info_t *data;
  struct hd_detail_monitor_s *next;
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

typedef struct {
  enum hd_detail_type type;
  ccw_t *data;
} hd_detail_ccw_t;

typedef struct {
  enum hd_detail_type type;
  joystick_t *data;
} hd_detail_joystick_t;

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
  hd_detail_ccw_t ccw;
  hd_detail_joystick_t joystick;
} hd_detail_t;

/** @} */

/** 
 * @defgroup MANUALpub Manual hardware configuration
 * @brief Handle manually configured hardware (in /var/lib/hardware/)
 * @ingroup libhdPublic
 */

/**
 * @brief Manually configured hardware information
 * @ingroup MANUALpub
 */
typedef struct hd_manual_s {
  struct hd_manual_s *next;

  char *unique_id;
  char *parent_id;
  char *child_ids;
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
   * Hardware class list.
   * A device may belong to more than one hardware class.
   */
  unsigned char hw_class_list[(hw_all + 7) / 8];	/**< (Internal) bitmask of hw classes. */

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
   * sysfs entry for this hardware, if any.
   */
  char *sysfs_id;

  /**
   * sysfs bus id for this hardware, if any.
   */
  char *sysfs_bus_id;

  /**
   * sysfs device link.
   */
  char *sysfs_device_link;

  /**
   * Special %device file.
   * Device file name to access this hardware. Normally something below /dev.
   * For network interfaces this is the interface name.
   */
  char *unix_dev_name;

  /**
   * Device type & number according to sysfs.
   */
  hd_dev_num_t unix_dev_num;

  /**
   * List of %device names.
   * Device file names to access this hardware. Normally something below /dev.
   * They should be all equivalent. The preferred name however is
   * \ref hd_t::unix_dev_name.
   */
  str_list_t *unix_dev_names;

  /**
   * Special %device file.
   * Device file name to access this hardware. Most hardware only has one
   * %device name stored in \ref hd_t::unix_dev_name. But in some cases
   * there's an alternative name.
   */
  char *unix_dev_name2;

  /**
   * Device type & number according to sysfs.
   */
  hd_dev_num_t unix_dev_num2;

  /**
   * BIOS/PROM id.
   * Where appropriate, this is a special BIOS/PROM id (e.g. "0x80" for
   * the first harddisk on Intel-PCs).
   * CHPID for s390.
   */
  char *rom_id;

  /**
   * HAL udi.
   */
  char *udi;

  /**
   * \ref udi of parent (\ref attached_to).
   */
  char *parent_udi;

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

  /** List of ids. */
  str_list_t *unique_ids;

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

   /**
    * Slot the hotplug device is connected to (e.g. PCMCIA socket).
    * \note \ref hotplug_slot counts 1-based (0: no information available).
    */
  unsigned hotplug_slot;

  struct is_s {
    unsigned agp:1;		/**< AGP device */
    unsigned isapnp:1;		/**< ISA-PnP device */
    unsigned notready:1;	/**< block devices: no medium, other: device not configured */
    unsigned manual:1;		/**< undetectable, manually configured hardware */
    unsigned softraiddisk:1;	/**< disk belongs to some soft raid array */
    unsigned zip:1;		/**< zip floppy */
    unsigned cdr:1;		/**< CD-R */
    unsigned cdrw:1;		/**< CD-RW */
    unsigned dvd:1;		/**< DVD */
    unsigned dvdr:1;		/**< DVD-R */
    unsigned dvdrw:1;		/**< DVD-RW */
    unsigned dvdrdl:1;		/**< DVD-R DL */
    unsigned dvdpr:1;		/**< DVD+R */
    unsigned dvdprw:1;		/**< DVD+RW */
    unsigned dvdprdl:1;		/**< DVD+R DL */
    unsigned dvdprwdl:1;	/**< DVD+RW DL */
    unsigned bd:1;		/**< BD */
    unsigned bdr:1;		/**< BD-R */
    unsigned bdre:1;		/**< BD-RE */
    unsigned hd:1;		/**< HD */
    unsigned hdr:1;		/**< HD-R */
    unsigned hdrw:1;		/**< HD-RW */
    unsigned dvdram:1;		/**< DVDRAM */
    unsigned mo:1;		/**< MO */
    unsigned mrw:1;		/**< MRW */
    unsigned mrww:1;		/**< MRW-W */
    unsigned pppoe:1;		/**< PPPOE modem connected */
    unsigned wlan:1;		/**< WLAN card */
    unsigned with_acpi:1;	/**< acpi works fine */
    unsigned hotpluggable:1;	/**< hotpluggable storage device */
    unsigned dualport:1;	/**< OSA Express device with two ports (S/390) */
    unsigned fcoe:1;		/**< fcoe device */
    unsigned fcoe_offload:2;	/**< fcoe offload capable device */
    unsigned iscsi_offload:2;	/**< iscsi offload capable  device */
    unsigned storage_only:2;	/**< storage only network interface */
  } is;

  struct tag_s {		/**< this struct is for internal purposes only */
    unsigned remove:1;		/**< schedule for removal */
    unsigned freeit:1;		/**< for internal memory management */
    unsigned fixed:1;		/**< fixed, do no longer modify this entry */
    unsigned skip_mouse:1;	/**< if serial line, don't scan for mice */
    unsigned skip_modem:1;	/**< if serial line, don't scan for modems */
    unsigned skip_braille:1;	/**< if serial line, don't scan for braille devices */
    unsigned ser_device:2;	/**< if != 0: info about attached serial device; see serial.c */
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
   * Currently active driver module (if any).
   */
  char *driver_module;

  /**
   * List of currently active drivers.
   */
  str_list_t *drivers;

  /**
   * List of currently active driver modules.
   */
  str_list_t *driver_modules;

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
   * \ref unique_ids of children (\ref parent_id).
   * \note Please do not use it for now.
   * 
   */
  str_list_t *child_ids;

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

  driver_info_t *driver_info;	/**< device driver info */

  str_list_t *requires;		/**< packages/programs required for this hardware */

  hal_prop_t *hal_prop;		/**< hal property list */

  hal_prop_t *persistent_prop;	/**< persistent property list */

  char *modalias;		/**< module alias */
  char *label;			/**< Consistent Device Name (CDN), pci firmware spec 3.1, chapter 4.6.7 */

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
   * @brief Current hardware list.
   * The list of all currently probed hardware. This is not identical with
   * the result of \ref hd_list(). (But a superset of it.)
   */
  hd_t *hd;

  /**
   * @brief A progress indicator.
   * If this callback function is not NULL, it is called at various points and can
   * be used to give some user feedback what we are actually doing.
   * If the debug flag HD_DEB_PROGRESS is set, progress messages are logged.
   * \param pos Indicates where we are.
   * \param msg Indicates what we are going to do.
   */
  void (*progress)(char *pos, char *msg);
  
  /** 
   * @brief Log messages.
   * All messages logged during hardware probing accumulate here.
   */
  char *log;

  /** 
   * @brief Debug flags.
   * Although there exist some debug flag defines this scheme is currently
   * not followed consistently. It is guaranteed however that -1 will give
   * the most log messages and 0 the least.
   * @see DEBUGpub
   */
  unsigned debug;

  /**
   * @brief Special flags.
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
    unsigned nofork:1;		/**< don't run potentially hanging code in a subprocess */
    unsigned nosysfs:1;		/**< don't ask sysfs */
    unsigned forked:1;		/**< we're running in a subprocess */
    unsigned cpuemu:1;		/**< use CPU emulation to run BIOS code (i386 only) */
    unsigned udev:1;		/**< return first udev symlink as device name */
    unsigned edd_used:1;	/**< internal: edd info has been used  */
    unsigned keep_kmods:2;	/**< internal: don't reread kmods */
    unsigned nobioscrc:1;	/**< internal: don't check VBIOS crc */
    unsigned biosvram:1;	/**< internal: map Video BIOS RAM (128k at 0xa0000) */
    unsigned nowpa:1;           /**< no longer used */
    unsigned pata:1;            /**< use new libata modules instead of classical ide modules */
    unsigned vbox:1;		/**< running in virtual box  */
    unsigned vmware:1;		/**< running in vmware  */
    unsigned vmware_mouse:1;	/**< has vmware mouse */
  } flags;


  /** 
   * @brief Concentrate on these devices.
   * List of sysfs ids for devices to look for.
   */
  str_list_t *only;

  /*
   * The following entries should *not* be accessed outside of libhd!
   */
  unsigned char probe[(pr_all + 7) / 8];	/**< (Internal) bitmask of probing features. */
  unsigned char probe_set[(pr_all + 7) / 8];	/**< (Iternal) bitmask of probing features that will always be set. */
  unsigned char probe_clr[(pr_all + 7) / 8];	/**< (Internal) bitmask of probing features that will always be reset. */
  hal_prop_t *probe_val;	/**< (Internal) probing features with arbitrary values */
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
  modinfo_t *modinfo_ext;	/**< (Internal) external module info */
  modinfo_t *modinfo;		/**< (Internal) module info */
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
  hd_t *manual;			/**< (Internal) hardware config info */
  str_list_t *disks;		/**< (Internal) disks according to /proc/partitions */
  str_list_t *partitions;	/**< (Internal) dto, partitions */
  str_list_t *cdroms;		/**< (Internal) cdroms according to PROC_CDROM_INFO */
  hd_smbios_t *smbios;		/**< (Internal) smbios data */
  struct {
    unsigned ok:1;
    unsigned size;
    unsigned used;
    void *data;
    int id;
    int updated;
  } shm;			/**< (Internal) our shm segment */
  unsigned pci_config_type;	/**< (Internal) PCI config type (1 or 2), 0: unknown */
  hd_udevinfo_t *udevinfo;	/**< (Internal) udev info */
  hd_sysfsdrv_t *sysfsdrv;	/**< (Internal) sysfs driver info */
  uint64_t sysfsdrv_id;		/**< (Internal) sysfs driver info id */
  str_list_t *scanner_db;	/**< (Internal) list of scanner modules */
  edd_info_t edd[0x80];		/**< (Internal) enhanced disk drive data */
  hal_device_t *hal;		/**< (Internal) HAL data (if any) */
  str_list_t *lsscsi;		/**< (Internal) lsscsi result (if any) */
  struct vm_s *vm;		/**< (Internal) x86emu vm */
  size_t log_size;		/**< (Internal) current log size (including final 0) */
  size_t log_max;		/**< (Internal) log buffer size */
  str_list_t *klog_raw;		/**< (Internal) unmodified kernel log */
} hd_data_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 *                      libhd interface functions
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

/* implemented in hd.c */

/** the actual hardware scan */
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
const char* hd_busid_to_hwcfg(int busid);
hd_t *hd_list(hd_data_t *hd_data, hd_hw_item_t item, int rescan, hd_t *hd_old);
hd_t *hd_list_with_status(hd_data_t *hd_data, hd_hw_item_t item, hd_status_t status);
hd_t *hd_list2(hd_data_t *hd_data, hd_hw_item_t *items, int rescan);
hd_t *hd_list_with_status2(hd_data_t *hd_data, hd_hw_item_t *items, hd_status_t status);

void hd_add_driver_data(hd_data_t *hd_data, hd_t *hd);

int hd_has_pcmcia(hd_data_t *hd_data);
#if 0
/**
 * will be gone soon
 * @deprecated
 */
int hd_apm_enabled(hd_data_t *hd_data);
#endif
int hd_usb_support(hd_data_t *hd_data);
int hd_smp_support(hd_data_t *hd_data);
int hd_mac_color(hd_data_t *hd_data);
int hd_color(hd_data_t *hd_data);
int hd_is_uml(hd_data_t *hd_data);
int hd_is_xen(hd_data_t *hd_data);
unsigned hd_display_adapter(hd_data_t *hd_data);
unsigned hd_boot_disk(hd_data_t *hd_data, int *matches);
enum cpu_arch hd_cpu_arch(hd_data_t *hd_data);
enum boot_arch hd_boot_arch(hd_data_t *hd_data);

hd_t *hd_get_device_by_idx(hd_data_t *hd_data, unsigned idx);

void hd_set_hw_class(hd_t *hd, hd_hw_item_t hw_class);
int hd_is_hw_class(hd_t *hd, hd_hw_item_t hw_class);

int hd_is_sgi_altix(hd_data_t *hd_data);

char *hd_version(void);

hal_prop_t *hd_free_hal_properties(hal_prop_t *prop);
hal_prop_t *hd_read_properties(const char *udi);
int hd_write_properties(const char *udi, hal_prop_t *prop);

int hd_change_status(const char *id, hd_status_t status, const char *config_string);
int hd_change_config_status(hd_data_t *hd_data, const char *id, hd_status_t status, const char *config_string);
int hd_read_mmap(hd_data_t *hd_data, char *name, unsigned char *buf, off_t start, unsigned size);

/* implemented in hddb.c */

/**
 * @todo implement in hddb.c
 */
str_list_t *hddb_get_packages(hd_data_t *hd_data);
void hddb_add_info(hd_data_t *hd_data, hd_t *hd);

void hddb_dump_raw(hddb2_data_t *hddb, FILE *f);
void hddb_dump(hddb2_data_t *hddb, FILE *f);


/* implemented in hdp.c */
void hd_dump_entry(hd_data_t *hd_data, hd_t *hd, FILE *f);

/* implemented in cdrom.c */
cdrom_info_t *hd_read_cdrom_info(hd_data_t *hd_data, hd_t *hd);

/**
 * @ingroup MANUALpub
 * @brief Manually configured devices
 * implemented in manual.c
 * @{
 */

hd_manual_t *hd_manual_read_entry(hd_data_t *hd_data, const char *id);
int hd_manual_write_entry(hd_data_t *hd_data, hd_manual_t *entry);
hd_manual_t *hd_free_manual(hd_manual_t *manual);
hd_t *hd_read_config(hd_data_t *hd_data, const char *id);
int hd_write_config(hd_data_t *hd_data, hd_t *hd);
char *hd_hw_item_name(hd_hw_item_t item);
hd_hw_item_t hd_hw_item_type(char *name);
char *hd_status_value_name(hd_status_value_t status);

/** @} */


/**
 * @defgroup CDB ISDN interface
 * @author (C) 2003 kkeil@suse.de
 * @brief Handle ISDN devices
 * @{
 */

#define CDBISDN_VERSION	0x0101

#ifndef PCI_ANY_ID
#define PCI_ANY_ID	0xffff
#endif

#define CDBISDN_P_NONE	0x0
#define CDBISDN_P_IRQ	0x1
#define CDBISDN_P_MEM	0x2
#define CDBISDN_P_IO	0x3

/** vendor info */
typedef struct {
	char	*name;
	char	*shortname;
	int	vnr;
	int	refcnt;
} cdb_isdn_vendor;

typedef struct	{
	int	handle;		/**< internal identifier idx in database */
	int	vhandle;	/**< internal identifier to vendor database */
	char	*name;		/**< cardname */
	char	*lname;		/**< vendor short name + cardname */
	char	*Class;		/**< CLASS of the card */
	char	*bus;		/**< bus type */
	int	revision;	/**< revision used with USB */
	int	vendor;		/**< Vendor ID for ISAPNP and PCI cards */
	int     device;		/**< Device ID for ISAPNP and PCI cards */
	int	subvendor;	/**< Subvendor ID for PCI cards */
				/**< A value of 0xffff is ANY_ID */
	int     subdevice;	/**< Subdevice ID for PCI cards */
				/**< A value of 0xffff is ANY_ID */
	unsigned int features;	/**< feature flags */
	int	line_cnt;	/**< count of ISDN ports */
	int	vario_cnt;	/**< count of driver varios */
	int	vario;		/**< referenz to driver vario record */
} cdb_isdn_card;

typedef struct  {
	int	handle;		/**< idx in database */	
	int     next_vario;	/**< link to alternate vario */
	int     drvid;		/**< unique id of the driver vario */
	int	typ;		/**< Type to identify the driver */
	int	subtyp;		/**< Subtype of the driver type */
	int	smp;		/**< SMP supported ? */
	char 	*mod_name;	/**< name of the driver module */
	char 	*para_str;	/**< optional parameter string */
	char 	*mod_preload;	/**< optional modules to preload */
	char 	*cfg_prog;	/**< optional cfg prog */
	char 	*firmware;	/**< optional firmware to load */
	char 	*description;	/**< optional description */
	char 	*need_pkg;	/**< list of packages needed for function */
	char 	*info;		/**< optional additional info */
	char 	*protocol;	/**< supported D-channel protocols */
	char 	*interface;	/**< supported API interfaces */
	char 	*io;		/**< possible IO ports with legacy ISA cards */
	char 	*irq;		/**< possible interrupts with legacy ISA cards */
	char 	*membase;	/**< possible membase with legacy ISA cards */
	char 	*features;	/**< optional features*/
	int 	card_ref;	/**< reference to a card */
	char 	*name;		/**< driver name */
} cdb_isdn_vario;


extern cdb_isdn_vendor	*hd_cdbisdn_get_vendor(int);
extern cdb_isdn_card	*hd_cdbisdn_get_card(int);
extern cdb_isdn_vario	*hd_cdbisdn_get_vario_from_type(int, int);
extern cdb_isdn_card	*hd_cdbisdn_get_card_from_type(int, int);
extern cdb_isdn_card	*hd_cdbisdn_get_card_from_id(int, int, int, int);
extern cdb_isdn_vario	*hd_cdbisdn_get_vario(int);
extern int		hd_cdbisdn_get_version(void);
extern int		hd_cdbisdn_get_db_version(void);
extern char		*hd_cdbisdn_get_db_date(void);

/** 
 * CDB ISDN interface end 
 * @}
 */

#ifdef __cplusplus
}
#endif

/** @} */

#endif	/* _HD_H */

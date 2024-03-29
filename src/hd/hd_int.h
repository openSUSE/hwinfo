#define PROC_CMDLINE		"/proc/cmdline"
#define PROC_PCI_DEVICES	"/proc/bus/pci/devices"
#define PROC_PCI_BUS		"/proc/bus/pci"
#define PROC_CPUINFO		"/proc/cpuinfo"
#define PROC_IOPORTS		"/proc/ioports"
#define PROC_DMA		"/proc/dma"
#define PROC_INTERRUPTS		"/proc/interrupts"
#define PROC_NVRAM_22		"/proc/driver/nvram"
#define PROC_NVRAM_24		"/proc/nvram"
#define PROC_IDE		"/proc/ide"
#define PROC_SCSI		"/proc/scsi"
#define PROC_CDROM_INFO		"/proc/sys/dev/cdrom/info"
#define PROC_NET_IF_INFO	"/proc/net/dev"
#define PROC_MODULES		"/proc/modules"
#define PROC_DRIVER_SERIAL	"/proc/tty/driver/serial"
#define PROC_DRIVER_MACSERIAL	"/proc/tty/driver/macserial"
#define PROC_PARPORT_22		"/proc/parport/"			/* Final '/' is essential! */
#define PROC_PARPORT_24		"/proc/sys/dev/parport/parport"
#define PROC_KCORE		"/proc/kcore"
// #define PROC_USB_DEVICES	"/proc/bus/usb/devices"
#define PROC_USB_DEVICES	"/proc/bus/usb/devices_please-use-sysfs-instead"
#define PROC_PROM		"/proc/device-tree"
#define PROC_MEMINFO		"/proc/meminfo"
#define PROC_VERSION		"/proc/version"
#define PROC_ISAPNP		"/proc/isapnp"
#define PROC_PARTITIONS		"/proc/partitions"
#define PROC_APM		"/proc/apm"
#define PROC_XEN_BALLOON	"/proc/xen/balloon"

#define DEV_NVRAM		"/dev/nvram"
#define DEV_PSAUX		"/dev/psaux"
#define DEV_ADBMOUSE		"/dev/adbmouse"
#define DEV_MEM			"/dev/mem"
#define DEV_KBD			"/dev/kbd"
#define DEV_CONSOLE		"/dev/console"
#define DEV_OPENPROM		"/dev/openprom"
#define DEV_SUNMOUSE		"/dev/sunmouse"
#define DEV_MICE		"/dev/input/mice"
#define DEV_FB			"/dev/fb"
#define DEV_FB0			"/dev/fb0"

#define PROG_MODPROBE		"/sbin/modprobe"
#define PROG_RMMOD		"/sbin/rmmod"
#define PROG_CARDCTL		"/sbin/cardctl"
#define PROG_UDEVINFO		"/usr/bin/udevinfo"
#define PROG_UDEVADM		"/usr/bin/udevadm"

#define KLOG_BOOT		"/var/log/boot.msg"
#define ISAPNP_CONF		"/etc/isapnp.conf"

#define KERNEL_22		0x020200
#define KERNEL_24		0x020400
#define KERNEL_26		0x020600

#if defined(__s390__) || defined(__s390x__) || defined(__alpha__) || defined(LIBHD_TINY)
#define WITH_ISDN	0
#else
#define WITH_ISDN	1
#endif

// maximum attribute size in sysfs we expect
// (this is to avoid accidentally reading unlimited data)
#define MAX_ATTR_SIZE		0x10000

#define PROGRESS(a, b, c) progress(hd_data, a, b, c)
#define ADD2LOG(a...) hd_log_printf(hd_data, a)

/*
 * define to make (hd_t).unique_id a hex string, otherwise it is a
 * base64-like string
 */
#undef NUMERIC_UNIQUE_ID

/*
 * exported symbol - all others are not exported by the library
 */
#define API_SYM			__attribute__((visibility("default")))

/*
 * All API symbols start with 'hd_' or 'hddb_'.
 *
 * Add aliases for some symbols that are widespread throughout libhd sources
 * to avoid massive code adjustments.
 */
#define read_dir		hd_read_dir
#define read_file		hd_read_file
#define name2eisa_id		hd_name2eisa_id
#define search_str_list		hd_search_str_list
#define add_str_list		hd_add_str_list
#define free_str_list		hd_free_str_list
#define reverse_str_list	hd_reverse_str_list
#define add_hd_entry		hd_add_hd_entry

/*
 * Internal probing module numbers. Use mod_name_by_idx() outside of libhd.
 */
enum mod_idx {
  mod_none, mod_memory, mod_pci, mod_isapnp, mod_pnpdump, mod_net,
  mod_floppy, mod_misc, mod_bios, mod_cpu, mod_monitor, mod_mouse, mod_scsi,
  mod_serial, mod_usb, mod_adb, mod_modem, mod_parallel, mod_isa, mod_isdn,
  mod_kbd, mod_prom, mod_sbus, mod_int, mod_braille, mod_xtra, mod_sys,
  mod_manual, mod_fb, mod_veth, mod_pppoe, mod_pcmcia, mod_s390,
  mod_sysfs, mod_dsl, mod_block, mod_edd, mod_input, mod_wlan, mod_hal
};

void *new_mem(size_t size);
void *resize_mem(void *, size_t);
void *add_mem(void *, size_t, size_t);
char *new_str(const char *);
void *free_mem(void *);
int have_common_res(hd_res_t *res1, hd_res_t *res2);
void join_res_io(hd_res_t **res1, hd_res_t *res2);
void join_res_irq(hd_res_t **res1, hd_res_t *res2);
void join_res_dma(hd_res_t **res1, hd_res_t *res2);
hd_res_t *free_res_list(hd_res_t *res);
hd_res_t *add_res_entry(hd_res_t **res, hd_res_t *new_res);
hd_t *add_hd_entry(hd_data_t *hd_data, unsigned line, unsigned count);
misc_t *free_misc(misc_t *m);
scsi_t *free_scsi(scsi_t *scsi, int free_all);
hd_detail_t *free_hd_detail(hd_detail_t *d);
devtree_t *free_devtree(hd_data_t *hd_data);
void hd_add_id(hd_data_t *hd_data, hd_t *hd);

char *isa_id2str(unsigned);
char *eisa_vendor_str(unsigned);
char *canon_str(char *, int);

int hex(char *string, int digits);

void hd_log(hd_data_t *hd_data, char *buf, ssize_t len);
void hd_log_printf(hd_data_t *hd_data, char *format, ...) __attribute__ ((format (printf, 2, 3)));            
void hd_log_hex(hd_data_t *hd_data, int with_ascii, unsigned data_len, unsigned char *data);

void str_printf(char **buf, int offset, char *format, ...) __attribute__ ((format (printf, 3, 4)));
void hexdump(char **buf, int with_ascii, unsigned data_len, unsigned char *data);
str_list_t *read_dir_canonical(char *dir_name, int type);
str_list_t *subcomponent_list(str_list_t *list, char *comp, int max);
int has_subcomponent(str_list_t *list, char *comp);
void progress(hd_data_t *hd_data, unsigned pos, unsigned count, char *msg);

void remove_hd_entries(hd_data_t *hd_data);
void remove_tagged_hd_entries(hd_data_t *hd_data);

driver_info_t *free_driver_info(driver_info_t *di);

int str2float(char *s, int n);
char *float2str(int i, int n);

/* return the file name of a module */
char *mod_name_by_idx(unsigned idx);

int hd_timeout(void(*func)(void *), void *arg, int timeout);

str_list_t *read_kmods(hd_data_t *hd_data);
char *get_cmd_param(hd_data_t *hd_data, int field);

#ifdef __i386__
/* smp/smp.c */
int detectSMP(void);
#endif

void update_irq_usage(hd_data_t *hd_data);
int run_cmd(hd_data_t *hd_data, char *cmd);
int load_module_with_params(hd_data_t *hd_data, char *module, char *params);
int load_module(hd_data_t *hd_data, char *module);
int unload_module(hd_data_t *hd_data, char *module);
int probe_module(hd_data_t *hd_data, char *module);

int cmp_hd(hd_t *hd1, hd_t *hd2);
unsigned has_something_attached(hd_data_t *hd_data, hd_t *hd);

str_list_t *get_cmdline(hd_data_t *hd_data, char *key);

int detect_smp_bios(hd_data_t *hd_data);
int detect_smp_prom(hd_data_t *hd_data);

unsigned char *read_block0(hd_data_t *hd_data, char *dev, int *timeout);

void hd_copy(hd_t *dst, hd_t *src);

/* parameter for gather_resources(,,, which) */
#define W_IO    (1 << 0)
#define W_DMA   (1 << 1)
#define W_IRQ   (1 << 2)

void gather_resources(misc_t *m, hd_res_t **r, char *name, unsigned which);

char *vend_id2str(unsigned vend);

int hd_getdisksize(hd_data_t *hd_data, char *dev, int fd, hd_res_t **geo, hd_res_t **size);

int is_pnpinfo(ser_device_t *mi, int ofs);

int is_pcmcia_ctrl(hd_data_t *hd_data, hd_t *hd);

void hd_fork(hd_data_t *hd_data, int timeout, int total_timeout);
void hd_fork_done(hd_data_t *hd_data);
void hd_shm_init(hd_data_t *hd_data);
void hd_shm_clean(hd_data_t *hd_data);
void hd_shm_done(hd_data_t *hd_data);
void *hd_shm_add(hd_data_t *hd_data, void *ptr, unsigned len);
int hd_is_shm_ptr(hd_data_t *hd_data, void *ptr);
void hd_move_to_shm(hd_data_t *hd_data);

void read_udevinfo(hd_data_t *hd_data);

hd_t *hd_find_sysfs_id(hd_data_t *hd_data, char *id);
hd_t *hd_find_sysfs_id_devname(hd_data_t *hd_data, char *id, char *devname);
int hd_attr_uint(char* attr, uint64_t* u, int base);
str_list_t *hd_attr_list(char *str);
char *hd_sysfs_id(char *path);
char *hd_sysfs_name2_dev(char *str);
char *hd_sysfs_dev2_name(char *str);
void hd_sysfs_driver_list(hd_data_t *hd_data);
char *hd_sysfs_find_driver(hd_data_t *hd_data, char *sysfs_id, int exact);
int hd_report_this(hd_data_t *hd_data, hd_t *hd);
str_list_t *hd_module_list(hd_data_t *hd_data, unsigned id);

char* get_sysfs_attr(const char* bus, const char* device, const char* attr);
char *get_sysfs_attr_by_path(const char *path, const char *attr);
char *get_sysfs_attr_by_path2(const char *path, const char *attr, unsigned *len);

void hd_pci_complete_data(hd_t *hd);
void hd_pci_read_data(hd_data_t *hd_data);

hal_device_t *hd_free_hal_devices(hal_device_t *dev);
char *hd_hal_print_prop(hal_prop_t *prop);

void hal_invalidate(hal_prop_t *prop);
void hal_invalidate_all(hal_prop_t *prop, const char *key);
hal_prop_t *hal_get_any(hal_prop_t *prop, const char *key);
hal_prop_t *hal_get_bool(hal_prop_t *prop, const char *key);
hal_prop_t *hal_get_int32(hal_prop_t *prop, const char *key);
hal_prop_t *hal_get_str(hal_prop_t *prop, const char *key);
hal_prop_t *hal_get_list(hal_prop_t *prop, const char *key);
char *hal_get_useful_str(hal_prop_t *prop, const char *key);       

hal_device_t *hal_find_device(hd_data_t *hd_data, char *udi);
hal_prop_t *hal_add_new(hal_prop_t **prop);

char *hd_get_hddb_dir(void);
char *hd_get_hddb_path(char *sub);

int hd_mod_cmp(char *str1, char *str2);

int get_probe_val_int(hd_data_t *hd_data, enum probe_feature feature);
char *get_probe_val_str(hd_data_t *hd_data, enum probe_feature feature);
str_list_t *get_probe_val_list(hd_data_t *hd_data, enum probe_feature feature);

str_list_t *sort_str_list(str_list_t *sl0, int (*cmp_func)(const void *, const void *));


#ifdef __cplusplus
}
#endif


#define PROC_CMDLINE		"/proc/cmdline"
#define PROC_PCI_DEVICES	"/proc/bus/pci/devices"
#define PROC_PCI_BUS		"/proc/bus/pci"
#define PROC_CPUINFO		"/proc/cpuinfo"
#define PROC_IOPORTS		"/proc/ioports"
#define PROC_DMA		"/proc/dma"
#define PROC_INTERRUPTS		"/proc/interrupts"
#define PROC_NVRAM		"/proc/nvram"
#define PROC_IDE		"/proc/ide"
#define PROC_SCSI		"/proc/scsi"
#define PROC_SCSI_SCSI		"/proc/scsi/scsi"
#define PROC_CDROM_INFO		"/proc/sys/dev/cdrom/info"
#define PROC_NET_IF_INFO	"/proc/net/dev"
#define PROC_MODULES		"/proc/modules"
#define PROC_DRIVER_SERIAL	"/proc/tty/driver/serial"
#define PROC_PARPORT		"/proc/parport"
#define PROC_KCORE		"/proc/kcore"
#define PROC_USB_DEVICES	"/proc/bus/usb/devices"
#define PROC_DAC960		"/proc/rd"
#define PROC_SMART		"/proc/array"
#define PROC_PROM		"/proc/device-tree"

#define DEV_USB_DEVICES		"/dev/usb/devices"
#define DEV_NVRAM		"/dev/nvram"
#define DEV_PSAUX		"/dev/psaux"
#define DEV_ADBMOUSE		"/dev/adbmouse"
#define DEV_MEM			"/dev/mem"
#define DEV_DAC960		"/dev/rd"
#define DEV_SMART		"/dev/ida"
#define DEV_KBD			"/dev/kbd"
#define DEV_CONSOLE		"/dev/console"
#define DEV_OPENPROM		"/dev/openprom"
#define DEV_SUNMOUSE		"/dev/sunmouse"

#define KLOG_BOOT		"/var/log/boot.msg"
#define ISAPNP_CONF		"/etc/isapnp.conf"

#define NAME_LIST		"hd.names"
#define DRIVER_LIST		"hd.drivers"

#define PROGRESS(a, b, c) progress(hd_data, a, b, c)
#define ADD2LOG(a...) str_printf(&hd_data->log, -2, a)

/*
 * Internal probing module numbers. Use mod_name_by_idx() outside of libhd.
 */
enum mod_idx {
  mod_none, mod_memory, mod_pci, mod_isapnp, mod_pnpdump, mod_cdrom,
  mod_net, mod_floppy, mod_misc, mod_bios, mod_cpu, mod_monitor, mod_mouse,
  mod_ide, mod_scsi, mod_serial, mod_usb, mod_adb, mod_modem, mod_parallel,
  mod_isa, mod_dac960, mod_smart, mod_isdn, mod_kbd, mod_prom, mod_sbus
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

char *isa_id2str(unsigned);
char *eisa_vendor_str(unsigned);
unsigned name2eisa_id(char *);
char *canon_str(char *, int);

int hex(char *string, int digits);

void str_printf(char **buf, int offset, char *format, ...) __attribute__ ((format (printf, 3, 4)));
void hexdump(char **buf, int with_ascii, unsigned data_len, unsigned char *data);
str_list_t *search_str_list(str_list_t *sl, char *str);
str_list_t *add_str_list(str_list_t **sl, char *str);
str_list_t *free_str_list(str_list_t *list);
str_list_t *read_file(char *file_name, unsigned start_line, unsigned lines);
void progress(hd_data_t *hd_data, unsigned pos, unsigned count, char *msg);

void remove_hd_entries(hd_data_t *hd_data);
void remove_tagged_hd_entries(hd_data_t *hd_data);

int str2float(char *s, int n);
char *float2str(int i, int n);

/* return the file name of a module */
char *mod_name_by_idx(unsigned idx);

int timeout(void(*func)(void *), void *arg, int timeout);

str_list_t *read_kmods(hd_data_t *hd_data);
char *get_cmd_param(int field);

/* smp/smp.c */
int detectSMP(void);

void update_irq_usage(hd_data_t *hd_data);
int run_cmd(hd_data_t *hd_data, char *cmd);
int load_module(hd_data_t *hd_data, char *module);

#ifdef __cplusplus
}
#endif


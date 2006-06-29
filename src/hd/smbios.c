#include <stdio.h>
#include <string.h>

#include "hd.h"
#include "hd_int.h"
#include "smbios.h"

/**
 * @defgroup SMBIOSint System Management BIOS (SMBIOS)
 * @ingroup libhdINFOint
 * @brief System Management BIOS functions
 *
 * @{
 */

enum sm_map_type { sm_map_str, sm_map_num2str };

typedef struct { unsigned num; char *str; } sm_num2str_t;

typedef struct {
  enum sm_map_type type;
  unsigned len;
  union {
    char **str;
    sm_num2str_t *num2str;
  } list;
} sm_str_map_t;


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static char *get_string(str_list_t *sl, int index);
static void smbios_bitmap_print(FILE *f, hd_bitmap_t *hbm, char *label, int style);
static void smbios_id_print(FILE *f, hd_id_t *hid, char *label);
static void smbios_str_print(FILE *f, char *str, char *label);
static void smbios_id2str(hd_id_t *hid, sm_str_map_t *map, unsigned def);
static void smbios_bitmap2str(hd_bitmap_t *hbm, sm_str_map_t *map);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define SMBIOS_PRINT_ID(a, b) smbios_id_print(f, &sm->a, b)
#define SMBIOS_PRINT_STR(a, b) smbios_str_print(f, sm->a, b)
#define SMBIOS_PRINT_BITMAP_SHORT(a, b) smbios_bitmap_print(f, &sm->a, b, 0)
#define SMBIOS_PRINT_BITMAP_LONG(a, b) smbios_bitmap_print(f, &sm->a, b, 1)

#define SMBIOS_DEF_MAP(a) \
  static sm_str_map_t a  = { \
    sizeof *a ## _ == sizeof (sm_num2str_t) ? sm_map_num2str : sm_map_str, \
    sizeof a ## _ / sizeof *a ## _, \
    { (void *) a ## _ } \
  };

/* ptr is (unsigned char *) */
#define READ_MEM16(ptr) ((ptr)[0] + ((ptr)[1] << 8))
#define READ_MEM32(ptr) ((ptr)[0] + ((ptr)[1] << 8) + ((ptr)[2] << 16) + ((ptr)[3] << 24))
#define READ_MEM64(ptr) (READ_MEM32(ptr) + ((uint64_t) READ_MEM32(ptr + 4) << 32))


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

static sm_num2str_t smbios_bios_feature_[] = {
  {  4, "ISA supported" },
  {  5, "MCA supported" },
  {  6, "EISA supported" },
  {  7, "PCI supported" },
  {  8, "PCMCIA supported" },
  {  9, "PnP supported" },
  { 10, "APM supported" },
  { 11, "BIOS flashable" },
  { 12, "BIOS shadowing allowed" },
  { 13, "VL-VESA supported" },
  { 14, "ESCD supported" },
  { 15, "CD boot supported" },
  { 16, "Selectable boot supported" },
  { 17, "BIOS ROM socketed" },
  { 18, "PCMCIA boot supported" },
  { 19, "EDD spec supported" },
  { 20, "1.2MB NEC 9800 Japanese Floppy supported" },
  { 21, "1.2MB Toshiba Japanese Floppy supported" },
  { 22, "360kB Floppy supported" },
  { 23, "1.2MB Floppy supported" },
  { 24, "720kB Floppy supported" },
  { 25, "2.88MB Floppy supported" },
  { 26, "Print Screen supported" },
  { 27, "8042 Keyboard Services supported" },
  { 28, "Serial Services supported" },
  { 29, "Printer Services supported" },
  { 30, "CGA/Mono Video supported" },
  { 31, "NEC PC-98" },
  { 64 + 0, "ACPI supported" },
  { 64 + 1, "USB Legacy supported" },
  { 64 + 2, "AGP supported" },
  { 64 + 3, "I2O boot supported" },
  { 64 + 4, "LS-120 boot supported" },
  { 64 + 5, "ATAPI ZIP boot supported" },
  { 64 + 6, "IEEE1394 boot supported" },
  { 64 + 7, "Smart Battery supported" },
  { 64 + 8, "BIOS Boot Spec supported" },
  { 64 + 9, "F12 Network boot supported" }
};
SMBIOS_DEF_MAP(smbios_bios_feature);


static char *smbios_system_wakeups_[] = {
  "Reserved", "Other", "Unknown", "APM Timer",
  "Modem Ring", "LAN Remote", "Power Switch", "PCI PME#",
  "AC Power Restored"
};
SMBIOS_DEF_MAP(smbios_system_wakeups);


static char *smbios_board_feature_[] = {
  "Hosting Board", "Needs One Daughter Board", "Removable", "Replaceable",
  "Hot Swappable"
};
SMBIOS_DEF_MAP(smbios_board_feature);


static char *smbios_board_types_[] = {
  NULL, "Other", "Unknown", "Server Blade",
  "Connectivity Switch", "System Management Module", "Processor Module", "I/O Module",
  "Memory Module", "Daughter Board", "Motherboard", "Processor/Memory Module",
  "Processor/IO Module", "Interconnect Board"
};
SMBIOS_DEF_MAP(smbios_board_types);


static char *smbios_chassis_types_[] = {
  NULL, "Other", "Unknown", "Desktop",
  "Low Profile Desktop", "Pizza Box", "Mini Tower", "Tower",
  "Portable", "LapTop", "Notebook", "Hand Held",
  "Docking Station", "All in One", "Sub Notebook", "Space Saving",
  "Lunch Box", "Main Server Chassis", "Expansion Chassis", "SubChassis",
  "Bus Expansion Chassis", "Peripheral Chassis", "RAID Chassis", "Rack Mount Chassis",
  "Sealed-case PC", "Multi-System Chassis"
};
SMBIOS_DEF_MAP(smbios_chassis_types);


static char *smbios_chassis_states_[] = {
  NULL, "Other", "Unknown", "Safe",
  "Warning", "Critical", "Non-recoverable"
};
SMBIOS_DEF_MAP(smbios_chassis_states);


static char *smbios_chassis_sec_states_[] = {
  NULL, "Other", "Unknown", "None",
  "External interface locked out", "External interface enabled"
};
SMBIOS_DEF_MAP(smbios_chassis_sec_states);


static char *smbios_proc_upgrades_[] = {
  NULL, "Other", "Unknown", "Daughter Board",
  "ZIF Socket", "Replaceable Piggy Back", "None", "LIF Socket",
  "Slot 1", "Slot 2", "370-Pin Socket", "Slot A",
  "Slot M", "Socket 423", "Socket A (Socket 462)", "Socket 478",
  "Socket 754", "Socket 940"
};
SMBIOS_DEF_MAP(smbios_proc_upgrades);


static char *smbios_proc_cpu_status_[8] = {
  "Unknown", "Enabled", "Disabled by User", "Disabled by BIOS",
  "Idle", "Reserved", "Reserved", "Other"
};
SMBIOS_DEF_MAP(smbios_proc_cpu_status);


static char *smbios_proc_types_[] = {
  NULL, "Other", "Unknown", "CPU",
  "Math", "DSP", "Video"
};
SMBIOS_DEF_MAP(smbios_proc_types);


static sm_num2str_t smbios_proc_families_[] = {
  { 0x00, NULL },
  { 0x01, "Other" },
  { 0x02, "Unknown" },
  { 0x03, "8086" },
  { 0x04, "80286" },
  { 0x05, "Intel386" },
  { 0x06, "Intel486" },
  { 0x07, "8087" },
  { 0x08, "80287" },
  { 0x09, "80387" },
  { 0x0a, "80487" },
  { 0x0b, "Pentium" },
  { 0x0c, "Pentium Pro" },
  { 0x0d, "Pentium II" },
  { 0x0e, "Pentium MMX" },
  { 0x0f, "Celeron" },
  { 0x10, "Pentium II Xeon" },
  { 0x11, "Pentium III" },
  { 0x12, "M1" },
  { 0x13, "M2" },
  { 0x18, "Duron" },
  { 0x19, "K5" },
  { 0x1a, "K6" },
  { 0x1b, "K6-2" },
  { 0x1c, "K6-3" },
  { 0x1d, "Athlon" },
  { 0x1e, "AMD2900" },
  { 0x1f, "K6-2+" },
  { 0x78, "Crusoe TM5000" },
  { 0x79, "Crusoe TM3000" },
  { 0x82, "Itanium" },
  { 0x83, "Athlon 64" },
  { 0x84, "Opteron Processor" },
  { 0xb0, "Pentium III Xeon" },
  { 0xb1, "Pentium III with SpeedStep" },
  { 0xb2, "Pentium 4" },
  { 0xb3, "Xeon" },
  { 0xb4, "AS400" },
  { 0xb5, "Xeon MP" },
  { 0xb6, "Athlon XP" },
  { 0xb7, "Athlon MP" },
  { 0xb8, "Itanium 2" }
};
SMBIOS_DEF_MAP(smbios_proc_families);


static char *smbios_cache_mode_[] = {
  "Write Through", "Write Back", "Varies with Memory Address", "Unknown"
};
SMBIOS_DEF_MAP(smbios_cache_mode);


static char *smbios_cache_location_[] = {
  "Internal", "External", "Reserved", "Unknown"
};
SMBIOS_DEF_MAP(smbios_cache_location);


static char *smbios_cache_ecc_[] = {
  NULL, "Other", "Unknown", "None",
  "Parity", "Single-bit", "Multi-bit", "CRC"
};
SMBIOS_DEF_MAP(smbios_cache_ecc);
#define smbios_memarray_ecc smbios_cache_ecc


static char *smbios_cache_type_[] = {
  NULL, "Other", "Unknown", "Instruction",
  "Data", "Unified"
};
SMBIOS_DEF_MAP(smbios_cache_type);


static char *smbios_cache_assoc_[] = {
  NULL, "Other", "Unknown", "Direct Mapped",
  "2-way Set-Associative", "4-way Set-Associative", "Fully Associative", "8-way Set-Associative",
  "16-way Set-Associative"
};
SMBIOS_DEF_MAP(smbios_cache_assoc);


static char *smbios_cache_sram_[] = {
  "Other", "Unknown", "Non-Burst", "Burst",
  "Pipeline Burst", "Synchronous", "Asynchronous"
};
SMBIOS_DEF_MAP(smbios_cache_sram);


static sm_num2str_t smbios_connect_conn_type_[] = {
  { 0x00, NULL },
  { 0x01, "Centronics" },
  { 0x02, "Mini Centronics" },
  { 0x03, "Proprietary" },
  { 0x04, "DB-25 pin male" },
  { 0x05, "DB-25 pin female" },
  { 0x06, "DB-15 pin male" },
  { 0x07, "DB-15 pin female" },
  { 0x08, "DB-9 pin male" },
  { 0x09, "DB-9 pin female" },
  { 0x0a, "RJ-11" },
  { 0x0b, "RJ-45" },
  { 0x0c, "50 Pin MiniSCSI" },
  { 0x0d, "Mini-DIN" },
  { 0x0e, "Micro-DIN" },
  { 0x0f, "PS/2" },
  { 0x10, "Infrared" },
  { 0x11, "HP-HIL" },
  { 0x12, "Access Bus [USB]" },
  { 0x13, "SSA SCSI" },
  { 0x14, "Circular DIN-8 male" },
  { 0x15, "Circular DIN-8 female" },
  { 0x16, "On Board IDE" },
  { 0x17, "On Board Floppy" },
  { 0x18, "9 Pin Dual Inline [pin 10 cut]" },
  { 0x19, "25 Pin Dual Inline [pin 26 cut]" },
  { 0x1a, "50 Pin Dual Inline" },
  { 0x1b, "68 Pin Dual Inline" },
  { 0x1c, "On Board Sound Input from CD-ROM" },
  { 0x1d, "Mini-Centronics Type-14" },
  { 0x1e, "Mini-Centronics Type-26" },
  { 0x1f, "Mini-jack [headphones]" },
  { 0x20, "BNC" },
  { 0x21, "1394" },
  { 0xa0, "PC-98" },
  { 0xa1, "PC-98Hireso" },
  { 0xa2, "PC-H98" },
  { 0xa3, "PC-98Note" },
  { 0xa4, "PC-98Full" },
  { 0xff, "Other" }
};
SMBIOS_DEF_MAP(smbios_connect_conn_type);


static sm_num2str_t smbios_connect_port_type_[] = {
  { 0x00, NULL },
  { 0x01, "Parallel Port XT/AT Compatible" },
  { 0x02, "Parallel Port PS/2" },
  { 0x03, "Parallel Port ECP" },
  { 0x04, "Parallel Port EPP" },
  { 0x05, "Parallel Port ECP/EPP" },
  { 0x06, "Serial Port XT/AT Compatible" },
  { 0x07, "Serial Port 16450 Compatible" },
  { 0x08, "Serial Port 16550 Compatible" },
  { 0x09, "Serial Port 16550A Compatible" },
  { 0x0a, "SCSI Port" },
  { 0x0b, "MIDI Port" },
  { 0x0c, "Joy Stick Port" },
  { 0x0d, "Keyboard Port" },
  { 0x0e, "Mouse Port" },
  { 0x0f, "SSA SCSI" },
  { 0x10, "USB" },
  { 0x11, "FireWire [IEEE P1394]" },
  { 0x12, "PCMCIA Type I" },
  { 0x13, "PCMCIA Type II" },
  { 0x14, "PCMCIA Type III" },
  { 0x15, "Cardbus" },
  { 0x16, "Access Bus Port" },
  { 0x17, "SCSI II" },
  { 0x18, "SCSI Wide" },
  { 0x19, "PC-98" },
  { 0x1a, "PC-98-Hireso" },
  { 0x1b, "PC-H98" },
  { 0x1c, "Video Port" },
  { 0x1d, "Audio Port" },
  { 0x1e, "Modem Port" },
  { 0x1f, "Network Port" },
  { 0xa0, "8251 Compatible" },
  { 0xa1, "8251 FIFO Compatible" },
  { 0xff, "Other" }
};
SMBIOS_DEF_MAP(smbios_connect_port_type);


static sm_num2str_t smbios_slot_type_[] = {
  { 0x00, NULL },
  { 0x01, "Other" },
  { 0x02, "Unknown" },
  { 0x03, "ISA" },
  { 0x04, "MCA" },
  { 0x05, "EISA" },
  { 0x06, "PCI" },
  { 0x07, "PC Card [PCMCIA]" },
  { 0x08, "VL-VESA" },
  { 0x09, "Proprietary" },
  { 0x0a, "Processor Card" },
  { 0x0b, "Proprietary Memory Card" },
  { 0x0c, "I/O Riser Card" },
  { 0x0d, "NuBus" },
  { 0x0e, "PCI - 66MHz Capable" },
  { 0x0f, "AGP" },
  { 0x10, "AGP 2X" },
  { 0x11, "AGP 4X" },
  { 0x12, "PCI-X" },
  { 0x13, "AGP 8X" },
  { 0xa0, "PC-98/C20" },
  { 0xa1, "PC-98/C24" },
  { 0xa2, "PC-98/E" },
  { 0xa3, "PC-98/Local Bus" },
  { 0xa4, "PC-98/Card" }
};
SMBIOS_DEF_MAP(smbios_slot_type);


static char *smbios_slot_bus_width_[] = {
  NULL, "Other", "Unknown", "8 bit",
  "16 bit", "32 bit", "64 bit", "128 bit"
};
SMBIOS_DEF_MAP(smbios_slot_bus_width);


static char *smbios_slot_usage_[] = {
  NULL, "Other", "Unknown", "Available",
  "In Use"
};
SMBIOS_DEF_MAP(smbios_slot_usage);


static char *smbios_slot_length_[] = {
  NULL, "Other", "Unknown", "Short",
  "Long"
};
SMBIOS_DEF_MAP(smbios_slot_length);


static char *smbios_slot_feature_[] = {
  "Unknown", "5.0 V", "3.3 V", "Shared",
  "PC Card-16", "CardBus", "Zoom Video", "Modem Ring Resume",
  "PME#", "Hot-Plug"
};
SMBIOS_DEF_MAP(smbios_slot_feature);


static char *smbios_onboard_type_[] = {
  "Other", "Other", "Unknown", "Video",
  "SCSI Controller", "Ethernet", "Token Ring", "Sound"
};
SMBIOS_DEF_MAP(smbios_onboard_type);


static sm_num2str_t smbios_memarray_location_[] = {
  { 0x00, NULL },
  { 0x01, "Other" },
  { 0x02, "Unknown" },
  { 0x03, "Motherboard" },
  { 0x04, "ISA add-on card" },
  { 0x05, "EISA add-on card" },
  { 0x06, "PCI add-on card" },
  { 0x07, "MCA add-on card" },
  { 0x08, "PCMCIA add-on card" },
  { 0x09, "Proprietary add-on card" },
  { 0x0a, "NuBus" },
  { 0xa0, "PC-98/C20 add-on card" },
  { 0xa1, "PC-98/C24 add-on card" },
  { 0xa2, "PC-98/E add-on card" },
  { 0xa3, "PC-98/Local bus add-on card" }
};
SMBIOS_DEF_MAP(smbios_memarray_location);


static char *smbios_memarray_use_[] = {
  NULL, "Other", "Unknown", "System memory",
  "Video memory", "Flash memory", "Non-volatile RAM", "Cache memory"
};
SMBIOS_DEF_MAP(smbios_memarray_use);


static char *smbios_mouse_type_[] = {
  NULL, "Other", "Unknown", "Mouse",
  "Track Ball", "Track Point", "Glide Point", "Touch Pad",
  "Touch Screen", "Optical Sensor"
};
SMBIOS_DEF_MAP(smbios_mouse_type);


static sm_num2str_t smbios_mouse_interface_[] = {
  { 0x00, NULL },
  { 0x01, "Other" },
  { 0x02, "Unknown" },
  { 0x03, "Serial" },
  { 0x04, "PS/2" },
  { 0x05, "Infrared" },
  { 0x06, "HP-HIL" },
  { 0x07, "Bus Mouse" },
  { 0x08, "ADB" },
  { 0xa0, "Bus mouse DB-9" },
  { 0xa1, "Bus mouse micro-DIN" },
  { 0xa2, "USB" }
};
SMBIOS_DEF_MAP(smbios_mouse_interface);


static char *smbios_memdevice_form_[] = {
  NULL, "Other", "Unknown", "SIMM",
  "SIP", "Chip", "DIP", "ZIP",
  "Proprietary Card", "DIMM", "TSOP", "Row of Chips",
  "RIMM", "SODIMM", "SRIMM"
};
SMBIOS_DEF_MAP(smbios_memdevice_form);


static char *smbios_memdevice_type_[] = {
  NULL, "Other", "Unknown", "DRAM",
  "EDRAM", "VRAM", "SRAM", "RAM",
  "ROM", "FLASH", "EEPROM", "FEPROM",
  "EPROM", "CDRAM", "3DRAM", "SDRAM",
  "SGRAM", "RDRAM", "DDR"
};
SMBIOS_DEF_MAP(smbios_memdevice_type);


static char *smbios_memdevice_detail_[] = {
  NULL, "Other", "Unknown", "Fast-paged",
  "Static column", "Pseudo-static", "RAMBUS", "Synchronous",
  "CMOS", "EDO", "Window DRAM", "Cache DRAM",
  "Non-volatile"
};
SMBIOS_DEF_MAP(smbios_memdevice_detail);


static char *smbios_memerror_type_[] = {
  NULL, "Other", "Unknown", "OK",
  "Bad read", "Parity error", "Single-bit error", "Double-bit error",
  "Multi-bit error", "Nibble error", "Checksum error", "CRC error",
  "Corrected single-bit error", "Corrected error", "Uncorrectable error"
};
SMBIOS_DEF_MAP(smbios_memerror_type);


static char *smbios_memerror_granularity_[] = {
  NULL, "Other", "Unknown", "Device level",
  "Memory partition level"
};
SMBIOS_DEF_MAP(smbios_memerror_granularity);


static char *smbios_memerror_operation_[] = {
  NULL, "Other", "Unknown", "Read",
  "Write", "Partial write"
};
SMBIOS_DEF_MAP(smbios_memerror_operation);


static char *smbios_secure_state_[] = {
  "Disabled", "Enabled", "Not Implemented", "Unknown"
};
SMBIOS_DEF_MAP(smbios_secure_state);


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

/*
 * return the index'th string from sl
 */
char *get_string(str_list_t *sl, int index)
{
  if(!sl || !index) return NULL;

  for(; sl; sl = sl->next, index--) {
    if(index == 1) return new_str(sl->str && *sl->str ? sl->str : NULL);
  }

  return NULL;
}


/*
 * Print a bitmap.
 * Style: 0: short, 1: long.
 */
void smbios_bitmap_print(FILE *f, hd_bitmap_t *hbm, char *label, int style)
{
  unsigned u;
  str_list_t *sl;

  if(hbm->not_empty) {
    fprintf(f, "    %s: 0x", label);
    for(u = (hbm->bits + 7) >> 3; u; u--) {
      fprintf(f, "%02x", hbm->bitmap[u - 1]);
    }
    fprintf(f, style ? "\n" : " (");
    for(sl = hbm->str; sl; sl = sl->next) {
      if(style) {
        fprintf(f, "      %s\n", sl->str);
      }
      else {
        fprintf(f, "%s%s", sl->str, sl->next ? ", " : "");
      }
    }
    if(!style) fprintf(f, ")\n");
  }
}


/*
 * Print id/name pair.
 */
void smbios_id_print(FILE *f, hd_id_t *hid, char *label)
{
  if(hid->name) fprintf(f, "    %s: 0x%02x (%s)\n", label, hid->id, hid->name);
}


/*
 * Print a string.
 */
void smbios_str_print(FILE *f, char *str, char *label)
{
  if(str) fprintf(f, "    %s: \"%s\"\n", label, str);
}


/*
 * Look up the name that corresponds to the id (if any).
 */
void smbios_id2str(hd_id_t *hid, sm_str_map_t *map, unsigned def)
{
  unsigned u;
  char *str, *def_str;

  if(map->type == sm_map_str) {
    str = map->list.str[hid->id < map->len ? hid->id : def];
    if(hid->id && !str) str = map->list.str[def];
  }
  else {
    for(str = def_str = NULL, u = 0; u < map->len; u++) {
      if(map->list.num2str[u].num == hid->id) str = map->list.num2str[u].str;
      if(str) break;
      if(map->list.num2str[u].num == def) def_str = map->list.num2str[u].str;
    }
    if(hid->id && !str) str = def_str;
  }
  hid->name = new_str(str);
}


/*
 * Convert a bitmap into a list of strings. (That is, interpret the
 * bitmap.)
 */
void smbios_bitmap2str(hd_bitmap_t *hbm, sm_str_map_t *map)
{
  unsigned u, bit;
  char *str;

  for(u = 0; u < (hbm->bits + 7) >> 3; u++) {
    if(hbm->bitmap[u]) {
      hbm->not_empty = 1;
      break;
    }
  }

  if(map->type == sm_map_str) {
    for(u = 0; u < map->len; u++) {
      if(u < hbm->bits && hbm->bitmap[u >> 3] & (1 << (u & 7))) {
        str = map->list.str[u];
        if(str) add_str_list(&hbm->str, str);
      }
    }
  }
  else {
    for(u = 0; u < map->len; u++) {
      bit = map->list.num2str[u].num;
      if(bit < hbm->bits && hbm->bitmap[bit >> 3] & (1 << (bit & 7))) {
        str = map->list.num2str[u].str;
        if(str) add_str_list(&hbm->str, str);
      }
    }
  }
}


/*
 * Interpret raw smbios data.
 */
void smbios_parse(hd_data_t *hd_data)
{
  hd_smbios_t *sm;
  str_list_t *sl_any, *sl;
  int cnt, data_len;
  unsigned char *sm_data;
  unsigned u, v;

  if(!hd_data->smbios) return;

  for(cnt = 0, sm = hd_data->smbios; sm; sm = sm->next, cnt++) {
    sm_data = sm->any.data;
    data_len = sm->any.data_len;
    sl_any = sm->any.strings;
    switch(sm->any.type) {
      case sm_biosinfo:
        if(data_len >= 0x12) {
          sm->biosinfo.start = READ_MEM16(sm_data + 6) << 4;
          sm->biosinfo.rom_size = (sm_data[9] + 1) << 16;
          sm->biosinfo.vendor = get_string(sl_any, sm_data[4]);
          sm->biosinfo.version = get_string(sl_any, sm_data[5]);
          sm->biosinfo.date = get_string(sl_any, sm_data[8]);
          memcpy(sm->biosinfo.feature.bitmap, sm_data + 0xa, 8);
        }
        if(data_len >= 0x13) {
          sm->biosinfo.feature.bitmap[8] = sm_data[0x12];
        }
        if(data_len >= 0x14) {
          sm->biosinfo.feature.bitmap[9] = sm_data[0x13];
        }
        sm->biosinfo.feature.bits = 80;
        smbios_bitmap2str(&sm->biosinfo.feature, &smbios_bios_feature);
        break;

      case sm_sysinfo:
        if(data_len >= 8) {
          sm->sysinfo.manuf = get_string(sl_any, sm_data[4]);
          sm->sysinfo.product = get_string(sl_any, sm_data[5]);
          sm->sysinfo.version = get_string(sl_any, sm_data[6]);
          sm->sysinfo.serial = get_string(sl_any, sm_data[7]);
        }
        if(data_len >= 0x19) {
          memcpy(sm->sysinfo.uuid, sm_data + 8, 16);
          sm->sysinfo.wake_up.id = sm_data[0x18];
          smbios_id2str(&sm->sysinfo.wake_up, &smbios_system_wakeups, 1);
        }
        break;

      case sm_boardinfo:
        if(data_len >= 8) {
          sm->boardinfo.manuf = get_string(sl_any, sm_data[4]);
          sm->boardinfo.product = get_string(sl_any, sm_data[5]);
          sm->boardinfo.version = get_string(sl_any, sm_data[6]);
          sm->boardinfo.serial = get_string(sl_any, sm_data[7]);
        }
        if(data_len >= 9) {
          sm->boardinfo.asset = get_string(sl_any, sm_data[8]);
        }
        if(data_len >= 0x0e) {
          sm->boardinfo.feature.bitmap[0] = sm_data[9];
          sm->boardinfo.feature.bits = 8;
          smbios_bitmap2str(&sm->boardinfo.feature, &smbios_board_feature);
          sm->boardinfo.location = get_string(sl_any, sm_data[0x0a]);
          sm->boardinfo.chassis = READ_MEM16(sm_data + 0x0b);
          sm->boardinfo.board_type.id = sm_data[0x0d];
          smbios_id2str(&sm->boardinfo.board_type, &smbios_board_types, 1);
        }
        if(data_len >= 0x0f) {
          u = sm_data[0x0e];
          if(u && data_len >= 0x0f + 2 * u) {
            sm->boardinfo.objects_len = u;
            sm->boardinfo.objects = new_mem(u * sizeof *sm->boardinfo.objects);
            for(u = 0; u < sm->boardinfo.objects_len; u++) {
              sm->boardinfo.objects[u] = READ_MEM16(sm_data + 0x0f + 2 * u);
            }
          }
        }
        break;

      case sm_chassis:
        if(data_len >= 6) {
          sm->chassis.manuf = get_string(sl_any, sm_data[4]);
          sm->chassis.lock = sm_data[5] >> 7;
          sm->chassis.ch_type.id = sm_data[5] & 0x7f;
          smbios_id2str(&sm->chassis.ch_type, &smbios_chassis_types, 1);
        }
        if(data_len >= 9) {
          sm->chassis.version = get_string(sl_any, sm_data[6]);
          sm->chassis.serial = get_string(sl_any, sm_data[7]);
          sm->chassis.asset = get_string(sl_any, sm_data[8]);
        }
        if(data_len >= 0x0d) {
          sm->chassis.bootup.id = sm_data[9];
          sm->chassis.power.id = sm_data[0x0a];
          sm->chassis.thermal.id = sm_data[0x0b];
          sm->chassis.security.id = sm_data[0x0c];
          smbios_id2str(&sm->chassis.bootup, &smbios_chassis_states, 1);
          smbios_id2str(&sm->chassis.power, &smbios_chassis_states, 1);
          smbios_id2str(&sm->chassis.thermal, &smbios_chassis_states, 1);
          smbios_id2str(&sm->chassis.security, &smbios_chassis_sec_states, 1);
        }
        if(data_len >= 0x11) {
          sm->chassis.oem = READ_MEM32(sm_data + 0x0d);
        }
        break;

      case sm_processor:
        if(data_len >= 0x1a) {
          sm->processor.socket = get_string(sl_any, sm_data[4]);
          sm->processor.manuf = get_string(sl_any, sm_data[7]);
          sm->processor.version = get_string(sl_any, sm_data[0x10]);
          sm->processor.voltage = sm_data[0x11];
          if(sm->processor.voltage & 0x80) {
            sm->processor.voltage &= 0x7f;
          }
          else {
            switch(sm->processor.voltage) {
              case 0x01:
                sm->processor.voltage = 50;
                break;
              case 0x02:
                sm->processor.voltage = 33;
                break;
              case 0x04:
                sm->processor.voltage = 29;
                break;
              default:
                sm->processor.voltage = 0;
            }
          }
          sm->processor.pr_type.id = sm_data[5];
          sm->processor.family.id = sm_data[6];
          sm->processor.cpu_id = READ_MEM64(sm_data + 8);
          sm->processor.ext_clock = READ_MEM16(sm_data + 0x12);
          sm->processor.max_speed = READ_MEM16(sm_data + 0x14);
          sm->processor.current_speed = READ_MEM16(sm_data + 0x16);
          sm->processor.sock_status = (sm_data[0x18] >> 6) & 1;
          sm->processor.cpu_status.id = sm_data[0x18] & 7;
          sm->processor.upgrade.id = sm_data[0x19];
          smbios_id2str(&sm->processor.pr_type, &smbios_proc_types, 1);
          smbios_id2str(&sm->processor.family, &smbios_proc_families, 1);
          smbios_id2str(&sm->processor.cpu_status, &smbios_proc_cpu_status, 0);
          smbios_id2str(&sm->processor.upgrade, &smbios_proc_upgrades, 1);
        }
        if(data_len >= 0x20) {
          sm->processor.l1_cache = READ_MEM16(sm_data + 0x1a);
          sm->processor.l2_cache = READ_MEM16(sm_data + 0x1c);
          sm->processor.l3_cache = READ_MEM16(sm_data + 0x1e);
          if(sm->processor.l1_cache == 0xffff) sm->processor.l1_cache = 0;
          if(sm->processor.l2_cache == 0xffff) sm->processor.l2_cache = 0;
          if(sm->processor.l3_cache == 0xffff) sm->processor.l3_cache = 0;
        }
        if(data_len >= 0x21) {
          sm->processor.serial = get_string(sl_any, sm_data[0x20]);
        }
        if(data_len >= 0x22) {
          sm->processor.asset = get_string(sl_any, sm_data[0x21]);
          sm->processor.part = get_string(sl_any, sm_data[0x22]);
        }
        break;

      case sm_cache:
        if(data_len >= 0x0f) {
          sm->cache.socket = get_string(sl_any, sm_data[4]);
          u = READ_MEM16(sm_data + 7);
          if((u & 0x8000)) u = (u & 0x7fff) << 6;
          sm->cache.max_size = u;
          u = READ_MEM16(sm_data + 9);
          if((u & 0x8000)) u = (u & 0x7fff) << 6;
          sm->cache.current_size = u;
          u = READ_MEM16(sm_data + 5);
          sm->cache.mode.id = (u >> 8) & 3;
          sm->cache.state = (u >> 7) & 1;
          sm->cache.location.id = (u >> 5) & 3;
          sm->cache.socketed = (u >> 3) & 1;
          sm->cache.level = u & 7;
          smbios_id2str(&sm->cache.mode, &smbios_cache_mode, 0);
          smbios_id2str(&sm->cache.location, &smbios_cache_location, 0);
          sm->cache.supp_sram.bitmap[0] = sm_data[0x0b];
          sm->cache.supp_sram.bitmap[1] = sm_data[0x0c];
          sm->cache.supp_sram.bits = 16;
          sm->cache.sram.bitmap[0] = sm_data[0x0d];
          sm->cache.sram.bitmap[1] = sm_data[0x0e];
          sm->cache.sram.bits = 16;
          smbios_bitmap2str(&sm->cache.supp_sram, &smbios_cache_sram);
          smbios_bitmap2str(&sm->cache.sram, &smbios_cache_sram);
        }
        if(data_len >= 0x13) {
          sm->cache.speed = sm_data[0x0f];
          sm->cache.ecc.id = sm_data[0x10];
          sm->cache.cache_type.id = sm_data[0x11];
          sm->cache.assoc.id = sm_data[0x12];
          smbios_id2str(&sm->cache.ecc, &smbios_cache_ecc, 1);
          smbios_id2str(&sm->cache.cache_type, &smbios_cache_type, 1);
          smbios_id2str(&sm->cache.assoc, &smbios_cache_assoc, 1);
        }
        break;

      case sm_connect:
        if(data_len >= 9) {
          sm->connect.i_des = get_string(sl_any, sm_data[4]);
          sm->connect.x_des = get_string(sl_any, sm_data[6]);
          sm->connect.i_type.id = sm_data[5];
          sm->connect.x_type.id = sm_data[7];
          sm->connect.port_type.id = sm_data[8];
          smbios_id2str(&sm->connect.i_type, &smbios_connect_conn_type, 0xff);
          smbios_id2str(&sm->connect.x_type, &smbios_connect_conn_type, 0xff);
          smbios_id2str(&sm->connect.port_type, &smbios_connect_port_type, 0xff);
        }
        break;

      case sm_slot:
        if(data_len >= 0x0c) {
          sm->slot.desig = get_string(sl_any, sm_data[4]);
          sm->slot.slot_type.id = sm_data[5];
          sm->slot.bus_width.id = sm_data[6];
          sm->slot.usage.id = sm_data[7];
          sm->slot.length.id = sm_data[8];
          sm->slot.id = READ_MEM16(sm_data + 9);
          sm->slot.feature.bitmap[0] = sm_data[0x0b];
        }
        if(data_len >= 0x0d) {
          sm->slot.feature.bitmap[1] = sm_data[0x0c];
        }
        sm->slot.feature.bits = 16;
        smbios_id2str(&sm->slot.slot_type, &smbios_slot_type, 1);
        smbios_id2str(&sm->slot.bus_width, &smbios_slot_bus_width, 1);
        smbios_id2str(&sm->slot.usage, &smbios_slot_usage, 1);
        smbios_id2str(&sm->slot.length, &smbios_slot_length, 1);
        smbios_bitmap2str(&sm->slot.feature, &smbios_slot_feature);
        break;

      case sm_onboard:
        if(data_len >= 4) {
          u = data_len - 4;
          if(!(u & 1)) {
            u >>= 1;
            if(u) {
              sm->onboard.dev_len = u;
              sm->onboard.dev = new_mem(u * sizeof *sm->onboard.dev);
            }
            for(u = 0; u < sm->onboard.dev_len; u++) {
              sm->onboard.dev[u].name = get_string(sl_any, sm_data[4 + (u << 1) + 1]);
              v = sm_data[4 + (u << 1)];
              sm->onboard.dev[u].status = v >> 7;
              sm->onboard.dev[u].type.id = v & 0x7f;
              smbios_id2str(&sm->onboard.dev[u].type, &smbios_onboard_type, 1);
            }
          }
        }
        break;

      case sm_oem:
        for(sl = sl_any; sl; sl = sl->next) {
          if(sl->str && *sl->str) add_str_list(&sm->oem.oem_strings, sl->str);
        }
        break;

      case sm_config:
        for(sl = sl_any; sl; sl = sl->next) {
          if(sl->str && *sl->str) add_str_list(&sm->config.options, sl->str);
        }
        break;

      case sm_lang:
        if(data_len >= 0x16) {
          sm->lang.current = get_string(sl_any, sm_data[0x15]);
        }
        break;

      case sm_group:
        if(data_len >= 5) {
          sm->group.name = get_string(sl_any, sm_data[4]);
          u = (data_len - 5) / 3;
          if(u) {
            sm->group.items_len = u;
            sm->group.item_handles = new_mem(u * sizeof *sm->group.item_handles);
            for(u = 0; u < sm->group.items_len; u++) {
              sm->group.item_handles[u] = READ_MEM16(sm_data + 6 + 3 * u);
            }
          }
        }
        break;

      case sm_memarray:
        if(data_len >= 0x0f) {
          sm->memarray.location.id = sm_data[4];
          sm->memarray.use.id = sm_data[5];
          sm->memarray.ecc.id = sm_data[6];
          sm->memarray.max_size = READ_MEM32(sm_data + 7);
          if(sm->memarray.max_size == 0x80000000) sm->memarray.max_size = 0;
          sm->memarray.error_handle = READ_MEM16(sm_data + 0x0b);
          sm->memarray.slots = READ_MEM16(sm_data + 0x0d);
          smbios_id2str(&sm->memarray.location, &smbios_memarray_location, 1);
          smbios_id2str(&sm->memarray.use, &smbios_memarray_use, 1);
          smbios_id2str(&sm->memarray.ecc, &smbios_memarray_ecc, 1);
        }
        break;

      case sm_memdevice:
        if(data_len >= 0x15) {
          sm->memdevice.array_handle = READ_MEM16(sm_data + 0x04);
          sm->memdevice.error_handle = READ_MEM16(sm_data + 0x06);
          sm->memdevice.eccbits = READ_MEM16(sm_data + 8);
          sm->memdevice.width = READ_MEM16(sm_data + 0xa);
          if(sm->memdevice.width == 0xffff) sm->memdevice.width = 0;
          if(sm->memdevice.eccbits == 0xffff) sm->memdevice.eccbits = 0;
          if(sm->memdevice.eccbits >= sm->memdevice.width) {
            sm->memdevice.eccbits -= sm->memdevice.width;
          }
          else {
            sm->memdevice.eccbits = 0;
          }
          sm->memdevice.size = READ_MEM16(sm_data + 0xc);
          if(sm->memdevice.size == 0xffff) sm->memdevice.size = 0;
          if((sm->memdevice.size & 0x8000)) {
            sm->memdevice.size &= 0x7fff;
          }
          else {
            sm->memdevice.size <<= 10;
          }
          sm->memdevice.form.id = sm_data[0xe];
          sm->memdevice.set = sm_data[0xf];
          sm->memdevice.location = get_string(sl_any, sm_data[0x10]);
          sm->memdevice.bank = get_string(sl_any, sm_data[0x11]);
          sm->memdevice.mem_type.id = sm_data[0x12];
          smbios_id2str(&sm->memdevice.form, &smbios_memdevice_form, 1);
          smbios_id2str(&sm->memdevice.mem_type, &smbios_memdevice_type, 1);
          sm->memdevice.type_detail.bitmap[0] = sm_data[0x13];
          sm->memdevice.type_detail.bitmap[1] = sm_data[0x14];
          sm->memdevice.type_detail.bits = 16;
          smbios_bitmap2str(&sm->memdevice.type_detail, &smbios_memdevice_detail);
        }
        if(data_len >= 0x17) {
          sm->memdevice.speed = READ_MEM16(sm_data + 0x15);
        }
        if(data_len >= 0x1b) {
          sm->memdevice.manuf = get_string(sl_any, sm_data[0x17]);
          sm->memdevice.serial = get_string(sl_any, sm_data[0x18]);
          sm->memdevice.asset = get_string(sl_any, sm_data[0x19]);
          sm->memdevice.part = get_string(sl_any, sm_data[0x1a]);
        }
        break;

      case sm_memerror:
        if(data_len >= 0x17) {
          sm->memerror.err_type.id = sm_data[4];
          sm->memerror.granularity.id = sm_data[5];
          sm->memerror.operation.id = sm_data[6];
          sm->memerror.syndrome = READ_MEM32(sm_data + 7);
          sm->memerror.array_addr = READ_MEM32(sm_data + 0xb);
          sm->memerror.device_addr = READ_MEM32(sm_data + 0xf);
          sm->memerror.range = READ_MEM32(sm_data + 0x13);
          smbios_id2str(&sm->memerror.err_type, &smbios_memerror_type, 1);
          smbios_id2str(&sm->memerror.granularity, &smbios_memerror_granularity, 1);
          smbios_id2str(&sm->memerror.operation, &smbios_memerror_operation, 1);
        }
        break;

      case sm_memarraymap:
        if(data_len >= 0x0f) {
          sm->memarraymap.start_addr = READ_MEM32(sm_data + 4);
          sm->memarraymap.start_addr <<= 10;
          sm->memarraymap.end_addr = 1 + READ_MEM32(sm_data + 8);
          sm->memarraymap.end_addr <<= 10;
          sm->memarraymap.array_handle = READ_MEM16(sm_data + 0xc);
          sm->memarraymap.part_width = sm_data[0x0e];
        }
        break;

      case sm_memdevicemap:
        if(data_len >= 0x13) {
          sm->memdevicemap.start_addr = READ_MEM32(sm_data + 4);
          sm->memdevicemap.start_addr <<= 10;
          sm->memdevicemap.end_addr = 1 + READ_MEM32(sm_data + 8);
          sm->memdevicemap.end_addr <<= 10;
          sm->memdevicemap.memdevice_handle = READ_MEM16(sm_data + 0xc);
          sm->memdevicemap.arraymap_handle = READ_MEM16(sm_data + 0xe);
          sm->memdevicemap.row_pos = sm_data[0x10];
          sm->memdevicemap.interleave_pos = sm_data[0x11];
          sm->memdevicemap.interleave_depth = sm_data[0x12];
        }
        break;

      case sm_mouse:
        if(data_len >= 7) {
          sm->mouse.mtype.id = sm_data[4];
          sm->mouse.interface.id = sm_data[5];
          sm->mouse.buttons = sm_data[6];
          smbios_id2str(&sm->mouse.mtype, &smbios_mouse_type, 1);
          smbios_id2str(&sm->mouse.interface, &smbios_mouse_interface, 1);
        }
        break;

      case sm_secure:
        if(data_len >= 5) {
          u = sm_data[4];
          sm->secure.power.id = u >> 6;
          sm->secure.keyboard.id = (u >> 4) & 3;
          sm->secure.admin.id = (u >> 2) & 3;
          sm->secure.reset.id = u & 3;
          smbios_id2str(&sm->secure.power, &smbios_secure_state, 3);
          smbios_id2str(&sm->secure.keyboard, &smbios_secure_state, 3);
          smbios_id2str(&sm->secure.admin, &smbios_secure_state, 3);
          smbios_id2str(&sm->secure.reset, &smbios_secure_state, 3);
        }
        break;

      case sm_power:
        if(data_len >= 9) {
          sm->power.month = sm_data[4];
          sm->power.day = sm_data[5];
          sm->power.hour = sm_data[6];
          sm->power.minute = sm_data[7];
          sm->power.second = sm_data[8];
        }
        break;

      case sm_mem64error:
        if(data_len >= 0x1f) {
          sm->mem64error.err_type.id = sm_data[4];
          sm->mem64error.granularity.id = sm_data[5];
          sm->mem64error.operation.id = sm_data[6];
          sm->mem64error.syndrome = READ_MEM32(sm_data + 7);
          sm->mem64error.array_addr = READ_MEM64(sm_data + 0xb);
          sm->mem64error.device_addr = READ_MEM64(sm_data + 0x13);
          sm->mem64error.range = READ_MEM32(sm_data + 0x1b);
          smbios_id2str(&sm->mem64error.err_type, &smbios_memerror_type, 1);
          smbios_id2str(&sm->mem64error.granularity, &smbios_memerror_granularity, 1);
          smbios_id2str(&sm->mem64error.operation, &smbios_memerror_operation, 1);
        }
        break;

      default:
	break;
    }
  }
}


/*
 * Note: new_sm is directly inserted into the list, so you *must* make sure
 * that new_sm points to a malloc'ed pice of memory.
 */
hd_smbios_t *smbios_add_entry(hd_smbios_t **sm, hd_smbios_t *new_sm)
{
  while(*sm) sm = &(*sm)->next;

  return *sm = new_sm;
}


/*
 * Free the memory allocated by a smbios list.
 */
hd_smbios_t *smbios_free(hd_smbios_t *sm)
{
  hd_smbios_t *next;
  unsigned u;

  for(; sm; sm = next) {
    next = sm->next;

    free_mem(sm->any.data);
    free_str_list(sm->any.strings);

    switch(sm->any.type) {
      case sm_biosinfo:
        free_mem(sm->biosinfo.vendor);
        free_mem(sm->biosinfo.version);
        free_mem(sm->biosinfo.date);
        free_str_list(sm->biosinfo.feature.str);
        break;

      case sm_sysinfo:
        free_mem(sm->sysinfo.manuf);
        free_mem(sm->sysinfo.product);
        free_mem(sm->sysinfo.version);
        free_mem(sm->sysinfo.serial);
        free_mem(sm->sysinfo.wake_up.name);
        break;

      case sm_boardinfo:
        free_mem(sm->boardinfo.manuf);
        free_mem(sm->boardinfo.product);
        free_mem(sm->boardinfo.version);
        free_mem(sm->boardinfo.serial);
        free_mem(sm->boardinfo.asset);
        free_mem(sm->boardinfo.location);
        free_mem(sm->boardinfo.board_type.name);
        free_str_list(sm->boardinfo.feature.str);
        free_mem(sm->boardinfo.objects);
        break;

      case sm_chassis:
        free_mem(sm->chassis.manuf);
        free_mem(sm->chassis.version);
        free_mem(sm->chassis.serial);
        free_mem(sm->chassis.asset);
        free_mem(sm->chassis.ch_type.name);
        free_mem(sm->chassis.bootup.name);
        free_mem(sm->chassis.power.name);
        free_mem(sm->chassis.thermal.name);
        free_mem(sm->chassis.security.name);
        break;

      case sm_processor:
        free_mem(sm->processor.socket);
        free_mem(sm->processor.manuf);
        free_mem(sm->processor.version);
        free_mem(sm->processor.serial);
        free_mem(sm->processor.asset);
        free_mem(sm->processor.part);
        free_mem(sm->processor.upgrade.name);
        free_mem(sm->processor.pr_type.name);
        free_mem(sm->processor.family.name);
        free_mem(sm->processor.cpu_status.name);
        break;

      case sm_cache:
        free_mem(sm->cache.socket);
        free_mem(sm->cache.mode.name);
        free_mem(sm->cache.location.name);
        free_mem(sm->cache.ecc.name);
        free_mem(sm->cache.cache_type.name);
        free_mem(sm->cache.assoc.name);
        free_str_list(sm->cache.supp_sram.str);
        free_str_list(sm->cache.sram.str);
        break;

      case sm_connect:
        free_mem(sm->connect.port_type.name);
        free_mem(sm->connect.i_des);
        free_mem(sm->connect.x_des);
        free_mem(sm->connect.i_type.name);
        free_mem(sm->connect.x_type.name);
        break;

      case sm_slot:
        free_mem(sm->slot.desig);
        free_mem(sm->slot.slot_type.name);
        free_mem(sm->slot.bus_width.name);
        free_mem(sm->slot.usage.name);
        free_mem(sm->slot.length.name);
        free_str_list(sm->slot.feature.str);
        break;

      case sm_onboard:
        for(u = 0; u < sm->onboard.dev_len; u++) {
          free_mem(sm->onboard.dev[u].name);
          free_mem(sm->onboard.dev[u].type.name);
        }
        free_mem(sm->onboard.dev);
        break;

      case sm_oem:
        free_str_list(sm->oem.oem_strings);
        break;

      case sm_config:
        free_str_list(sm->config.options);
        break;

      case sm_lang:
        free_mem(sm->lang.current);
        break;

      case sm_group:
        free_mem(sm->group.name);
        free_mem(sm->group.item_handles);
        break;

      case sm_memarray:
        free_mem(sm->memarray.location.name);
        free_mem(sm->memarray.use.name);
        free_mem(sm->memarray.ecc.name);
        break;

      case sm_memdevice:
        free_mem(sm->memdevice.location);
        free_mem(sm->memdevice.bank);
        free_mem(sm->memdevice.manuf);
        free_mem(sm->memdevice.serial);
        free_mem(sm->memdevice.asset);
        free_mem(sm->memdevice.part);
        free_mem(sm->memdevice.form.name);
        free_mem(sm->memdevice.mem_type.name);
        free_str_list(sm->memdevice.type_detail.str);
        break;

      case sm_memerror:
        free_mem(sm->memerror.err_type.name);
        free_mem(sm->memerror.granularity.name);
        free_mem(sm->memerror.operation.name);
        break;

      case sm_mouse:
        free_mem(sm->mouse.mtype.name);
        free_mem(sm->mouse.interface.name);
        break;

      case sm_secure:
        free_mem(sm->secure.power.name);
        free_mem(sm->secure.keyboard.name);
        free_mem(sm->secure.admin.name);
        free_mem(sm->secure.reset.name);
        break;

      case sm_mem64error:
        free_mem(sm->mem64error.err_type.name);
        free_mem(sm->mem64error.granularity.name);
        free_mem(sm->mem64error.operation.name);
        break;

      default:
	break;
    }

    free_mem(sm);
  }

  return NULL;
}


/*
 * print SMBIOS entries
 */
void smbios_dump(hd_data_t *hd_data, FILE *f)
{
  hd_smbios_t *sm;
  str_list_t *sl;
  char c, *s;
  unsigned u;
  int i;

  if(!hd_data->smbios) return;

  for(sm = hd_data->smbios; sm; sm = sm->next) {
    switch(sm->any.type) {
      case sm_biosinfo:
        fprintf(f, "  BIOS Info: #%d\n", sm->any.handle);
        if(sm->biosinfo.vendor) fprintf(f, "    Vendor: \"%s\"\n", sm->biosinfo.vendor);
        if(sm->biosinfo.version) fprintf(f, "    Version: \"%s\"\n", sm->biosinfo.version);
        if(sm->biosinfo.date) fprintf(f, "    Date: \"%s\"\n", sm->biosinfo.date);
        fprintf(f, "    Start Address: 0x%05x\n", sm->biosinfo.start);
        fprintf(f, "    ROM Size: %d kB\n", sm->biosinfo.rom_size >> 10);
        SMBIOS_PRINT_BITMAP_LONG(biosinfo.feature, "Features");
        break;

      case sm_sysinfo:
        fprintf(f, "  System Info: #%d\n", sm->any.handle);
        if(sm->sysinfo.manuf) fprintf(f, "    Manufacturer: \"%s\"\n", sm->sysinfo.manuf);
        if(sm->sysinfo.product) fprintf(f, "    Product: \"%s\"\n", sm->sysinfo.product);
        if(sm->sysinfo.version) fprintf(f, "    Version: \"%s\"\n", sm->sysinfo.version);
        if(sm->sysinfo.serial) fprintf(f, "    Serial: \"%s\"\n", sm->sysinfo.serial);
        for(i = u = 0; (unsigned) i < sizeof sm->sysinfo.uuid / sizeof *sm->sysinfo.uuid; i++) {
          u |= sm->sysinfo.uuid[i];
        }
        fprintf(f, "    UUID: ");
        if(u == 0 || u == 0xff) {
          fprintf(f, "undefined");
          if(u == 0xff) fprintf(f, ", but settable");
        }
        else {
          for(i = sizeof sm->sysinfo.uuid / sizeof *sm->sysinfo.uuid - 1; i >= 0; i--) {
            fprintf(f, "%02x", sm->sysinfo.uuid[i]);
          }
        }
        fprintf(f, "\n");
        SMBIOS_PRINT_ID(sysinfo.wake_up, "Wake-up");
        break;

      case sm_boardinfo:
        fprintf(f, "  Board Info: #%d\n", sm->any.handle);
        if(sm->boardinfo.manuf) fprintf(f, "    Manufacturer: \"%s\"\n", sm->boardinfo.manuf);
        if(sm->boardinfo.product) fprintf(f, "    Product: \"%s\"\n", sm->boardinfo.product);
        if(sm->boardinfo.version) fprintf(f, "    Version: \"%s\"\n", sm->boardinfo.version);
        if(sm->boardinfo.serial) fprintf(f, "    Serial: \"%s\"\n", sm->boardinfo.serial);
        if(sm->boardinfo.asset) fprintf(f, "    Asset Tag: \"%s\"\n", sm->boardinfo.asset);
        SMBIOS_PRINT_ID(boardinfo.board_type, "Type");
        SMBIOS_PRINT_BITMAP_LONG(boardinfo.feature, "Features");
        if(sm->boardinfo.location) fprintf(f, "    Location: \"%s\"\n", sm->boardinfo.location);
        if(sm->boardinfo.chassis) fprintf(f, "    Chassis: #%d\n", sm->boardinfo.chassis);
        if(sm->boardinfo.objects_len) {
          fprintf(f, "    Contained Objects: ");
          for(i = 0; i < sm->boardinfo.objects_len; i++) {
            fprintf(f, "%s#%d", i ? ", " : "", sm->boardinfo.objects[i]);
          }
          fprintf(f, "\n");
        }
        break;

      case sm_chassis:
        fprintf(f, "  Chassis Info: #%d\n", sm->any.handle);
        if(sm->chassis.manuf) fprintf(f, "    Manufacturer: \"%s\"\n", sm->chassis.manuf);
        if(sm->chassis.version) fprintf(f, "    Version: \"%s\"\n", sm->chassis.version);
        if(sm->chassis.serial) fprintf(f, "    Serial: \"%s\"\n", sm->chassis.serial);
        if(sm->chassis.asset) fprintf(f, "    Asset Tag: \"%s\"\n", sm->chassis.asset);
        SMBIOS_PRINT_ID(chassis.ch_type, "Type");
        if(sm->chassis.lock) fprintf(f, "    Lock: present\n");
        SMBIOS_PRINT_ID(chassis.bootup, "Bootup State");
        SMBIOS_PRINT_ID(chassis.power, "Power Supply State");
        SMBIOS_PRINT_ID(chassis.thermal, "Thermal State");
        SMBIOS_PRINT_ID(chassis.security, "Security Status");
        if(sm->chassis.oem) fprintf(f, "    OEM Info: 0x%08x\n", sm->chassis.oem);
        break;

      case sm_processor:
        fprintf(f, "  Processor Info: #%d\n", sm->any.handle);
        SMBIOS_PRINT_STR(processor.socket, "Socket");
        SMBIOS_PRINT_ID(processor.upgrade, "Socket Type");
        fprintf(f, "    Socket Status: %s\n", sm->processor.sock_status ? "Populated" : "Empty");
        SMBIOS_PRINT_ID(processor.pr_type, "Type");
        SMBIOS_PRINT_ID(processor.family, "Family");
        SMBIOS_PRINT_STR(processor.manuf, "Manufacturer");
        SMBIOS_PRINT_STR(processor.version, "Version");
        SMBIOS_PRINT_STR(processor.serial, "Serial");
        SMBIOS_PRINT_STR(processor.asset, "Asset Tag");
        SMBIOS_PRINT_STR(processor.part, "Part Number");
        if(sm->processor.cpu_id) {
          fprintf(f, "    Processor ID: 0x%016"PRIx64"\n", sm->processor.cpu_id);
        }
        SMBIOS_PRINT_ID(processor.cpu_status, "Status");
        if(sm->processor.voltage) {
          fprintf(f, "    Voltage: %u.%u V\n", sm->processor.voltage / 10, sm->processor.voltage % 10);
        }
        if(sm->processor.ext_clock) fprintf(f, "    External Clock: %u MHz\n", sm->processor.ext_clock);
        if(sm->processor.max_speed) fprintf(f, "    Max. Speed: %u MHz\n", sm->processor.max_speed);
        if(sm->processor.current_speed) fprintf(f, "    Current Speed: %u MHz\n", sm->processor.current_speed);

        if(sm->processor.l1_cache) fprintf(f, "    L1 Cache: #%d\n", sm->processor.l1_cache);
        if(sm->processor.l2_cache) fprintf(f, "    L2 Cache: #%d\n", sm->processor.l2_cache);
        if(sm->processor.l3_cache) fprintf(f, "    L3 Cache: #%d\n", sm->processor.l3_cache);
        break;

      case sm_cache:
        fprintf(f, "  Cache Info: #%d\n", sm->any.handle);
        SMBIOS_PRINT_STR(cache.socket, "Designation");
        fprintf(f, "    Level: L%u\n", sm->cache.level + 1);
        fprintf(f, "    State: %s\n", sm->cache.state ? "Enabled" : "Disabled");
        SMBIOS_PRINT_ID(cache.mode, "Mode");
        if(sm->cache.location.name) {
          fprintf(f, "    Location: 0x%02x (%s, %sSocketed)\n",
            sm->cache.location.id,
            sm->cache.location.name,
            sm->cache.socketed ? "" : "Not "
          );
        }
        SMBIOS_PRINT_ID(cache.ecc, "ECC");
        SMBIOS_PRINT_ID(cache.cache_type, "Type");
        SMBIOS_PRINT_ID(cache.assoc, "Associativity");
        if(sm->cache.max_size) fprintf(f, "    Max. Size: %u kB\n", sm->cache.max_size);
        if(sm->cache.current_size) fprintf(f, "    Current Size: %u kB\n", sm->cache.current_size);
        if(sm->cache.speed) fprintf(f, "    Speed: %u ns\n", sm->cache.speed);
        SMBIOS_PRINT_BITMAP_SHORT(cache.supp_sram, "Supported SRAM Types");
        SMBIOS_PRINT_BITMAP_SHORT(cache.sram, "Current SRAM Type");
        break;

      case sm_connect:
        fprintf(f, "  Port Connector: #%d\n", sm->any.handle);
        SMBIOS_PRINT_ID(connect.port_type, "Type");
        SMBIOS_PRINT_STR(connect.i_des, "Internal Designator");
        SMBIOS_PRINT_ID(connect.i_type, "Internal Connector");
        SMBIOS_PRINT_STR(connect.x_des, "External Designator");
        SMBIOS_PRINT_ID(connect.x_type, "External Connector");
        break;

      case sm_slot:
        fprintf(f, "  System Slot: #%d\n", sm->any.handle);
        SMBIOS_PRINT_STR(slot.desig, "Designation");
        SMBIOS_PRINT_ID(slot.slot_type, "Type");
        SMBIOS_PRINT_ID(slot.bus_width, "Bus Width");
        SMBIOS_PRINT_ID(slot.usage, "Status");
        SMBIOS_PRINT_ID(slot.length, "Length");
        fprintf(f, "    Slot ID: %u\n", sm->slot.id);
        SMBIOS_PRINT_BITMAP_SHORT(slot.feature, "Characteristics");
        break;

      case sm_onboard:
        fprintf(f, "  On Board Devices: #%d\n", sm->any.handle);
        for(u = 0; u < sm->onboard.dev_len; u++) {
          fprintf(f, "    %s: \"%s\"%s\n",
            sm->onboard.dev[u].type.name,
            sm->onboard.dev[u].name,
            sm->onboard.dev[u].status ? "" : " (disabled)"
          );
        }
        break;

      case sm_oem:
        fprintf(f, "  OEM Strings: #%d\n", sm->any.handle);
        for(sl = sm->oem.oem_strings; sl; sl = sl->next) {
          fprintf(f, "    %s\n", sl->str);
        }
        break;

      case sm_config:
        fprintf(f, "  System Config Options (Jumpers & Switches) #%d:\n", sm->any.handle);
        for(sl = sm->config.options; sl; sl = sl->next) {
          fprintf(f, "    %s\n", sl->str);
        }
        break;

      case sm_lang:
        fprintf(f, "  Language Info: #%d\n", sm->any.handle);
        if((sl = sm->lang.strings)) {
          fprintf(f, "    Languages: ");
          for(; sl; sl = sl->next) {
            fprintf(f, "%s%s", sl->str, sl->next ? ", " : "");
          }
          fprintf(f, "\n");
        }
        if(sm->lang.current) fprintf(f, "    Current: %s\n", sm->lang.current);
        break;

      case sm_group:
        fprintf(f, "  Group Associations: #%d\n", sm->any.handle);
        if(sm->group.name) fprintf(f, "    Group Name: \"%s\"\n", sm->group.name);
        if(sm->group.items_len) {
          fprintf(f, "    Items: ");
          for(i = 0; i < sm->group.items_len; i++) {
            fprintf(f, "%s#%d", i ? ", " : "", sm->group.item_handles[i]);
          }
          fprintf(f, "\n");
        }
        break;

      case sm_memarray:
        fprintf(f, "  Physical Memory Array: #%d\n", sm->any.handle);
        SMBIOS_PRINT_ID(memarray.use, "Use");
        SMBIOS_PRINT_ID(memarray.location, "Location");
        fprintf(f, "    Slots: %u\n", sm->memarray.slots);
        if(sm->memarray.max_size) {
          u = sm->memarray.max_size;
          c = 'k';
          if(!(u & 0x3ff)) { u >>= 10; c = 'M'; }
          if(!(u & 0x3ff)) { u >>= 10; c = 'G'; }
          fprintf(f, "    Max. Size: %u %cB\n", u, c);
        }
        SMBIOS_PRINT_ID(memarray.ecc, "ECC");
        if(sm->memarray.error_handle != 0xfffe) {
          fprintf(f, "    Error Info: ");
          if(sm->memarray.error_handle != 0xffff) {
            fprintf(f, "#%d\n", sm->memarray.error_handle);
          }
          else {
            fprintf(f, "No Error\n");
          }
        }
        break;

      case sm_memdevice:
        fprintf(f, "  Memory Device: #%d\n", sm->any.handle);
        SMBIOS_PRINT_STR(memdevice.location, "Location");
        SMBIOS_PRINT_STR(memdevice.bank, "Bank");
        SMBIOS_PRINT_STR(memdevice.manuf, "Manufacturer");
        SMBIOS_PRINT_STR(memdevice.serial, "Serial");
        SMBIOS_PRINT_STR(memdevice.asset, "Asset Tag");
        SMBIOS_PRINT_STR(memdevice.part, "Part Number");
        fprintf(f, "    Memory Array: #%d\n", sm->memdevice.array_handle);
        if(sm->memdevice.error_handle != 0xfffe) {
          fprintf(f, "    Error Info: ");
          if(sm->memdevice.error_handle != 0xffff) {
            fprintf(f, "#%d\n", sm->memdevice.error_handle);
          }
          else {
            fprintf(f, "No Error\n");
          }
        }
        SMBIOS_PRINT_ID(memdevice.form, "Form Factor");
        SMBIOS_PRINT_ID(memdevice.mem_type, "Type");
        SMBIOS_PRINT_BITMAP_SHORT(memdevice.type_detail, "Type Detail");
        fprintf(f, "    Data Width: %u bits", sm->memdevice.width);
        if(sm->memdevice.eccbits) fprintf(f, " (+%u ECC bits)", sm->memdevice.eccbits);
        fprintf(f, "\n");
        if(sm->memdevice.size) {
          u = sm->memdevice.size;
          c = 'k';
          if(!(u & 0x3ff)) { u >>= 10; c = 'M'; }
          if(!(u & 0x3ff)) { u >>= 10; c = 'G'; }
          fprintf(f, "    Size: %u %cB\n", u, c);
        }
        else {
          fprintf(f, "    Size: No Memory Installed\n");
        }
        if(sm->memdevice.speed) fprintf(f, "    Speed: %u MHz\n", sm->memdevice.speed);
        break;

      case sm_memerror:
        fprintf(f, "  32bit-Memory Error Info: #%d\n", sm->any.handle);
        SMBIOS_PRINT_ID(memerror.err_type, "Type");
        SMBIOS_PRINT_ID(memerror.granularity, "Granularity");
        SMBIOS_PRINT_ID(memerror.operation, "Operation");
        if(sm->memerror.syndrome) fprintf(f, "    Syndrome: 0x%08x\n", sm->memerror.syndrome);
        if(sm->memerror.array_addr != (1 << 31)) fprintf(f, "    Mem Array Addr: 0x%08x\n", sm->memerror.array_addr);
        if(sm->memerror.device_addr != (1 << 31)) fprintf(f, "    Mem Device Addr: 0x%08x\n", sm->memerror.device_addr);
        if(sm->memerror.range != (1 << 31)) fprintf(f, "    Range: 0x%08x\n", sm->memerror.range);
        break;

      case sm_memarraymap:
        fprintf(f, "  Memory Array Mapping: #%d\n", sm->any.handle);
        fprintf(f, "    Memory Array: #%d\n", sm->memarraymap.array_handle);
        fprintf(f, "    Partition Width: %u\n", sm->memarraymap.part_width);
        if((sm->memarraymap.start_addr | sm->memarraymap.end_addr) >> 32) {
          fprintf(f, "    Start Address: 0x%016"PRIx64"\n", sm->memarraymap.start_addr);
          fprintf(f, "    End Address: 0x%016"PRIx64"\n", sm->memarraymap.end_addr);
        }
        else {
          fprintf(f, "    Start Address: 0x%08x\n", (unsigned) sm->memarraymap.start_addr);
          fprintf(f, "    End Address: 0x%08x\n", (unsigned) sm->memarraymap.end_addr);
        }
        break;

      case sm_memdevicemap:
        fprintf(f, "  Memory Device Mapping: #%d\n", sm->any.handle);
        fprintf(f, "    Memory Device: #%d\n", sm->memdevicemap.memdevice_handle);
        fprintf(f, "    Array Mapping: #%d\n", sm->memdevicemap.arraymap_handle);
        if(sm->memdevicemap.row_pos != 0xff) fprintf(f, "    Row: %u\n", sm->memdevicemap.row_pos);
        if(
          !sm->memdevicemap.interleave_pos ||
          sm->memdevicemap.interleave_pos != 0xff
        ) {
          fprintf(f, "    Interleave Pos: %u\n", sm->memdevicemap.interleave_pos);
        }
        if(
          !sm->memdevicemap.interleave_depth ||
          sm->memdevicemap.interleave_depth != 0xff
        ) {
          fprintf(f, "    Interleaved Depth: %u\n", sm->memdevicemap.interleave_depth);
        }
        if((sm->memdevicemap.start_addr | sm->memdevicemap.end_addr) >> 32) {
          fprintf(f, "    Start Address: 0x%016"PRIx64"\n", sm->memdevicemap.start_addr);
          fprintf(f, "    End Address: 0x%016"PRIx64"\n", sm->memdevicemap.end_addr);
        }
        else {
          fprintf(f, "    Start Address: 0x%08x\n", (unsigned) sm->memdevicemap.start_addr);
          fprintf(f, "    End Address: 0x%08x\n", (unsigned) sm->memdevicemap.end_addr);
        }
        break;

      case sm_mouse:
        fprintf(f, "  Pointing Device: #%d\n", sm->any.handle);
        SMBIOS_PRINT_ID(mouse.mtype, "Type");
        SMBIOS_PRINT_ID(mouse.interface, "Interface");
        if(sm->mouse.buttons) fprintf(f, "    Buttons: %u\n", sm->mouse.buttons);
        break;

      case sm_secure:
        fprintf(f, "  Hardware Security: #%d\n", sm->any.handle);
        SMBIOS_PRINT_ID(secure.power, "Power-on Password");
        SMBIOS_PRINT_ID(secure.keyboard, "Keyboard Password");
        SMBIOS_PRINT_ID(secure.admin, "Admin Password");
        SMBIOS_PRINT_ID(secure.reset, "Front Panel Reset");
        break;

      case sm_power:
        fprintf(f, "  System Power Controls: #%d\n", sm->any.handle);
        fprintf(f,
          "    Next Power-on: %02x:%02x:%02x %02x/%02x\n",
          sm->power.hour, sm->power.minute, sm->power.second,
          sm->power.day, sm->power.month
        );
        break;

      case sm_mem64error:
        fprintf(f, "  64bit-Memory Error Info: #%d\n", sm->any.handle);
        SMBIOS_PRINT_ID(mem64error.err_type, "Type");
        SMBIOS_PRINT_ID(mem64error.granularity, "Granularity");
        SMBIOS_PRINT_ID(mem64error.operation, "Operation");
        if(sm->mem64error.syndrome) fprintf(f, "    Syndrome: 0x%08x\n", sm->mem64error.syndrome);
        if(
          sm->mem64error.array_addr != (1ll << 63) &&
          sm->mem64error.array_addr != (1ll << 31)
        ) {
          fprintf(f, "    Mem Array Addr: 0x%016"PRIx64"\n", sm->mem64error.array_addr);
        }
        if(
          sm->mem64error.device_addr != (1ll << 63) &&
          sm->mem64error.device_addr != (1ll << 31)
        ) {
          fprintf(f, "    Mem Device Addr: 0x%016"PRIx64"\n", sm->mem64error.device_addr);
        }
        if(sm->mem64error.range != (1 << 31)) fprintf(f, "    Range: 0x%08x\n", sm->mem64error.range);
        break;

      case sm_end:
        break;

      default:
        if(sm->any.type == sm_inactive) {
          fprintf(f, "  Inactive Record: #%d\n", sm->any.handle);
        }
        else {
          fprintf(f, "  Type %d Record: #%d\n", sm->any.type, sm->any.handle);
        }
        if(sm->any.data_len) {
          for(i = 0; i < sm->any.data_len; i += 0x10) {
            u = sm->any.data_len - i;
            if(u > 0x10) u = 0x10;
            s = NULL;
            hexdump(&s, 0, u, sm->any.data + i);
            fprintf(f, "    Data %02x: %s\n", i, s);
            s = free_mem(s);
          }
        }
        for(u = 1, sl = sm->any.strings; sl; sl = sl->next, u++) {
          if(sl->str && *sl->str) fprintf(f, "    String %u: \"%s\"\n", u, sl->str);
        }
	break;
    }
  }
}

/** @} */


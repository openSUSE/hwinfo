#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>
#include <sys/stat.h>

#include "hd.h"
#include "hd_int.h"
#include "manual.h"
#include "hddb.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * hardware in /var/lib/hardware/
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

#ifndef LIBHD_TINY

typedef struct {
  int key;
  char *value;
} hash_t;

/* corresponds to hd_status_value_t */
static hash_t status_names[] = {
  { status_no,      "no"      },
  { status_yes,     "yes"     },
  { status_unknown, "unknown" },
  { status_new,     "new"     },
  { 0,              NULL      }
};

/* corresponds to hd_hw_item_t */
static hash_t hw_items[] = {
  { hw_sys,           "system"              },
  { hw_cpu,           "cpu"                 },
  { hw_keyboard,      "keyboard"            },
  { hw_braille,       "braille"             },
  { hw_mouse,         "mouse"               },
  { hw_joystick,      "joystick"            },
  { hw_printer,       "printer"             },
  { hw_scanner,       "scanner"             },
  { hw_chipcard,      "chipcard"            },
  { hw_monitor,       "monitor"             },
  { hw_tv,            "tv card"             },
  { hw_display,       "graphics card"       },
  { hw_framebuffer,   "framebuffer"         },
  { hw_camera,        "camera"              },
  { hw_sound,         "sound"               },
  { hw_storage_ctrl,  "storage"             },
  { hw_network_ctrl,  "network"             },
  { hw_isdn,          "isdn adapter"        },
  { hw_modem,         "modem"               },
  { hw_network,       "network interface"   },
  { hw_disk,          "disk"                },
  { hw_partition,     "partition"           },
  { hw_cdrom,         "cdrom"               },
  { hw_floppy,        "floppy"              },
  { hw_manual,        "manual"              },
  { hw_usb_ctrl,      "usb controller"      },
  { hw_usb,           "usb"                 },
  { hw_bios,          "bios"                },
  { hw_pci,           "pci"                 },
  { hw_isapnp,        "isapnp"              },
  { hw_bridge,        "bridge"              },
  { hw_hub,           "hub"                 },
  { hw_scsi,          "scsi"                },
  { hw_ide,           "ide"                 },
  { hw_memory,        "memory"              },
  { hw_dvb,           "dvb card"            },
  { hw_pcmcia,        "pcmcia"              },
  { hw_pcmcia_ctrl,   "pcmcia controller"   },
  { hw_ieee1394,      "firewire"            },
  { hw_ieee1394_ctrl, "firewire controller" },
  { hw_hotplug,       "hotplug"             },
  { hw_hotplug_ctrl,  "hotplug controller"  },
  { hw_zip,           "zip"                 },
  { hw_pppoe,         "pppoe"               },
  { hw_wlan,          "wlan card"           },
  { hw_dsl,           "DSL adapter"         },
  { hw_block,         "block device"        },
  { hw_tape,          "tape"                },
  { hw_vbe,           "vesa bios"           },
  { hw_unknown,       "unknown"             },
  { 0,                NULL                  }
};

typedef enum {
  hw_id_unique = 1, hw_id_parent, hw_id_child, hw_id_hwclass, hw_id_model,
  hw_id_configured, hw_id_available, hw_id_needed, hw_id_cfgstring, hw_id_active
} hw_id_t;

#define MAN_SECT_GENERAL	"General"
#define MAN_SECT_STATUS		"Status"
#define MAN_SECT_HARDWARE	"Hardware"

static hash_t hw_ids_general[] = {
  { hw_id_unique,     "UniqueID"   },
  { hw_id_parent,     "ParentID"   },
  { hw_id_child,      "ChildIDs"   },
  { hw_id_hwclass,    "HWClass"    },
  { hw_id_model,      "Model"      },
  { 0,                NULL         }
};

static hash_t hw_ids_status[] = {
  { hw_id_configured, "Configured"   },
  { hw_id_available,  "Available"    },
  { hw_id_needed,     "Needed"       },
  { hw_id_cfgstring,  "ConfigString" },
  { hw_id_active,     "Active"       },
  { 0,                NULL           }
};

/* structure elements from hd_t */
typedef enum {
  hwdi_bus = 1, hwdi_slot, hwdi_func, hwdi_base_class, hwdi_sub_class,
  hwdi_prog_if, hwdi_dev, hwdi_vend, hwdi_sub_dev, hwdi_sub_vend, hwdi_rev,
  hwdi_compat_dev, hwdi_compat_vend, hwdi_dev_name, hwdi_vend_name,
  hwdi_sub_dev_name, hwdi_sub_vend_name, hwdi_rev_name, hwdi_serial,
  hwdi_unix_dev_name, hwdi_unix_dev_name2, hwdi_unix_dev_names, hwdi_rom_id,
  hwdi_broken, hwdi_usb_guid, hwdi_res_mem, hwdi_res_phys_mem, hwdi_res_io,
  hwdi_res_irq, hwdi_res_dma, hwdi_res_size, hwdi_res_baud, hwdi_res_cache,
  hwdi_res_disk_geo, hwdi_res_monitor, hwdi_res_framebuffer, hwdi_features,
  hwdi_hotplug, hwdi_class_list, hwdi_drivers, hwdi_sysfs_id,
  hwdi_sysfs_busid, hwdi_sysfs_link
} hw_hd_items_t;

static hash_t hw_ids_hd_items[] = {
  { hwdi_bus,             "Bus"              },
  { hwdi_slot,            "Slot"             },
  { hwdi_func,            "Function"         },
  { hwdi_base_class,      "BaseClass"        },
  { hwdi_sub_class,       "SubClass"         },
  { hwdi_prog_if,         "ProgIF"           },
  { hwdi_dev,             "DeviceID"         },
  { hwdi_vend,            "VendorID"         },
  { hwdi_sub_dev,         "SubDeviceID"      },
  { hwdi_sub_vend,        "SubVendorID"      },
  { hwdi_rev,             "RevisionID"       },
  { hwdi_compat_dev,      "CompatDeviceID"   },
  { hwdi_compat_vend,     "CompatVendorID"   },
  { hwdi_dev_name,        "DeviceName"       },
  { hwdi_vend_name,       "VendorName"       },
  { hwdi_sub_dev_name,    "SubDeviceName"    },
  { hwdi_sub_vend_name,   "SubVendorName"    },
  { hwdi_rev_name,        "RevisionName"     },
  { hwdi_serial,          "Serial"           },
  { hwdi_unix_dev_name,   "UnixDevice"       },
  { hwdi_unix_dev_name2,  "UnixDeviceAlt"    },
  { hwdi_unix_dev_names,  "UnixDeviceList"   },
  { hwdi_rom_id,          "ROMID"            },
  { hwdi_broken,          "Broken"           },
  { hwdi_usb_guid,        "USBGUID"          },
  { hwdi_res_phys_mem,    "Res.PhysMemory"   },
  { hwdi_res_mem,         "Res.Memory"       },
  { hwdi_res_io,          "Res.IO"           },
  { hwdi_res_irq,         "Res.Interrupts"   },
  { hwdi_res_dma,         "Res.DMA"          },
  { hwdi_res_size,        "Res.Size"         },
  { hwdi_res_baud,        "Res.Baud"         },
  { hwdi_res_cache,       "Res.Cache"        },
  { hwdi_res_disk_geo,    "Res.DiskGeometry" },
  { hwdi_res_monitor,     "Res.Monitor"      },
  { hwdi_res_framebuffer, "Res.Framebuffer"  },
  { hwdi_features,        "Features"         },
  { hwdi_hotplug,         "Hotplug"          },
  { hwdi_class_list,      "HWClassList"      },
  { hwdi_drivers,         "Drivers"          },
  { hwdi_sysfs_id,        "SysfsID"          },
  { hwdi_sysfs_busid,     "SysfsBusID"       },
  { hwdi_sysfs_link,      "SysfsLink"        },
  { 0,                    NULL               }
};

static char *key2value(hash_t *hash, int id);
static int value2key(hash_t *hash, char *str);
static void dump_manual(hd_data_t *hd_data);
static unsigned str2id(char *str);
static void manual2hd(hd_data_t *hd_data, hd_manual_t *entry, hd_t *hd);
static void hd2manual(hd_t *hd, hd_manual_t *entry);

void hd_scan_manual(hd_data_t *hd_data)
{
  DIR *dir;
  struct dirent *de;
  hd_manual_t *entry, **entry_next;
  int i;
  hd_t *hd, *hd1;

  if(!hd_probe_feature(hd_data, pr_manual)) return;

  hd_data->module = mod_manual;

  /* some clean-up */
  remove_hd_entries(hd_data);

  hd_data->manual = hd_free_manual(hd_data->manual);
  entry_next = &hd_data->manual;

  if((dir = opendir(HARDWARE_UNIQUE_KEYS))) {
    i = 0;
    while((de = readdir(dir))) {
      if(*de->d_name == '.') continue;
      PROGRESS(1, ++i, "read");
      if((entry = hd_manual_read_entry(hd_data, de->d_name))) {
        ADD2LOG("  got %s\n", entry->unique_id);
        *entry_next = entry;
        entry_next = &entry->next;
      }
    }
    closedir(dir);
  }

  /* for compatibility: read old files, too */
  if((dir = opendir(HARDWARE_DIR))) {
    i = 0;
    while((de = readdir(dir))) {
      if(*de->d_name == '.') continue;
      for(entry = hd_data->manual; entry; entry = entry->next) {
        if(entry->unique_id && !strcmp(entry->unique_id, de->d_name)) break;
      }
      if(entry) continue;
      PROGRESS(2, ++i, "read");
      if((entry = hd_manual_read_entry(hd_data, de->d_name))) {
        ADD2LOG("  got %s\n", entry->unique_id);
        *entry_next = entry;
        entry_next = &entry->next;
      }
    }
    closedir(dir);
  }

  if(hd_data->debug) dump_manual(hd_data);

  /* initialize some useful status value */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      !hd->status.configured &&
      !hd->status.available &&
      !hd->status.needed &&
      !hd->status.active &&
      !hd->status.invalid
    ) {
      hd->status.configured = status_new;
      hd->status.available = hd->module == mod_manual ? status_unknown : status_yes;
      hd->status.needed = status_no;
      hd->status.active = status_unknown;
    }
  }

  for(entry = hd_data->manual; entry; entry = entry->next) {

    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(hd->unique_id && !strcmp(hd->unique_id, entry->unique_id)) break;
    }

    if(hd) {
      /* just update config status */
      hd->status = entry->status;
      hd->status.available = status_yes;

      hd->config_string = new_str(entry->config_string);
    }
    else {
      /* add new entry */
      hd = add_hd_entry(hd_data, __LINE__, 0);

      manual2hd(hd_data, entry, hd);

      if(hd->status.available != status_unknown) hd->status.available = status_no;

      if(hd->parent_id) {
        for(hd1 = hd_data->hd; hd1; hd1 = hd1->next) {
          if(hd1->unique_id && !strcmp(hd1->unique_id, hd->parent_id)) {
            hd->attached_to = hd1->idx;
            break;
          }
        }
      }
    }
  }

}


void hd_scan_manual2(hd_data_t *hd_data)
{
  hd_t *hd, *hd1;

  /* check if it's necessary to reconfigure this hardware */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    hd->status.reconfig = status_no;

    if(hd->status.needed != status_yes) continue;

    if(hd->status.available == status_no) {
      hd->status.reconfig = status_yes;
      continue;
    }

    if(hd->status.available != status_unknown) continue;

    for(hd1 = hd_data->hd; hd1; hd1 = hd1->next) {
      if(hd1 == hd) continue;

      if(
        hd1->hw_class == hd->hw_class &&
        hd1->status.configured == status_new &&
        hd1->status.available == status_yes
      ) break;
    }

    if(hd1) hd->status.reconfig = status_yes;
  }
}


int value2key(hash_t *hash, char *str)
{
  for(; hash->value; hash++) {
    if(!strcmp(hash->value, str)) break;
  }

  return hash->key;
}

char *key2value(hash_t *hash, int id)
{
  for(; hash->value; hash++) {
    if(hash->key == id) break;
  }

  return hash->value;
}

char *hd_hw_item_name(hd_hw_item_t item)
{
  return key2value(hw_items, item);
}


char *hd_status_value_name(hd_status_value_t status)
{
  return key2value(status_names, status);
}

/*
 * read an entry
 */
hd_manual_t *hd_manual_read_entry(hd_data_t *hd_data, const char *id)
{
  char path[PATH_MAX];
  int i, j, line;
  str_list_t *sl, *sl0;
  hd_manual_t *entry;
  hash_t *sect;
  char *s, *s1, *s2;
  int err = 0;

  snprintf(path, sizeof path, "%s/%s", HARDWARE_UNIQUE_KEYS, id);

  if(!(sl0 = read_file(path, 0, 0))) {
    /* try old location, too */
    snprintf(path, sizeof path, "%s/%s", HARDWARE_DIR, id);
    if(!(sl0 = read_file(path, 0, 0))) return NULL;
  }

  entry = new_mem(sizeof *entry);

  // default list: no valid entries
  sect = hw_ids_general + sizeof hw_ids_general / sizeof *hw_ids_general - 1;

  for(line = 1, sl = sl0; sl; sl = sl->next, line++) {
    s = sl->str;
    while(isspace(*s)) s++;
    if(!*s || *s == '#' || *s == ';') continue;	/* empty lines & comments */

    s2 = s;
    s1 = strsep(&s2, "=");

    if(!s2 && *s == '[') {
      s2 = s + 1;
      s1 = strsep(&s2, "]");
      if(s1) {
        if(!strcmp(s1, MAN_SECT_GENERAL)) {
          sect = hw_ids_general;
          continue;
        }
        if(!strcmp(s1, MAN_SECT_STATUS)) {
          sect = hw_ids_status;
          continue;
        }
        if(!strcmp(s1, MAN_SECT_HARDWARE)) {
          sect = NULL;
          continue;
        }
      }
      s2 = NULL;
    }

    if(!s2) {
      ADD2LOG("  %s: invalid line %d\n", id, line);
      err = 1;
      break;
    }

    if(sect) {
      i = value2key(sect, s1);
      if(!i) {
        ADD2LOG("  %s: invalid line %d\n", id, line);
        err = 1;
        break;
      }
      s = canon_str(s2, strlen(s2));
      switch(i) {
        case hw_id_unique:
          entry->unique_id = s;
          s = NULL;
          break;

        case hw_id_parent:
          entry->parent_id = s;
          s = NULL;
          break;

        case hw_id_child:
          entry->child_ids = s;
          s = NULL;
          break;

        case hw_id_hwclass:
          j = value2key(hw_items, s);
          entry->hw_class = j;
          if(!j) err = 1;
          break;

        case hw_id_model:
          entry->model = s;
          s = NULL;
          break;

        case hw_id_configured:
          j = value2key(status_names, s);
          entry->status.configured = j;
          if(!j) err = 1;
          break;

        case hw_id_available:
          j = value2key(status_names, s);
          entry->status.available_orig =
          entry->status.available = j;
          if(!j) err = 1;
          break;

        case hw_id_needed:
          j = value2key(status_names, s);
          entry->status.needed = j;
          if(!j) err = 1;
          break;

        case hw_id_active:
          j = value2key(status_names, s);
          entry->status.active = j;
          if(!j) err = 1;
          break;

        case hw_id_cfgstring:
          entry->config_string = s;
          s = NULL;
          break;

        default:
          err = 1;
      }

      free_mem(s);

      if(err) {
        ADD2LOG("  %s: invalid line %d\n", id, line);
        break;
      }
    }
    else {
      add_str_list(&entry->key, s1);
      s = canon_str(s2, strlen(s2));
      add_str_list(&entry->value, s);
      free_mem(s);
    }
  }

  free_str_list(sl0);

  /*
   * do some basic consistency checks
   */

  if(!entry->unique_id || strcmp(entry->unique_id, id)) {
    ADD2LOG("  %s: unique id does not match file name\n", id);
    err = 1;
  }

  /*
   * if the status info is completely missing, fake some:
   * new hardware, not autodetectable, not needed
   */
  if(
    !entry->status.configured &&
    !entry->status.available &&
    !entry->status.needed &&
    !entry->status.invalid
  ) {
    entry->status.configured = status_new;
    entry->status.available = status_unknown;
    entry->status.needed = status_no;
  }

  if(!entry->status.active) entry->status.active = status_unknown;

  if(
    !entry->status.configured ||
    !entry->status.available ||
    !entry->status.needed ||
    !entry->status.active
  ) {
    ADD2LOG("  %s: incomplete status\n", id);
    err = 1;
  }

  if(!entry->hw_class) {
    ADD2LOG("  %s: no class\n", id);
    err = 1;
  }

  if(!entry->model) {
    ADD2LOG("  %s: no model\n", id);
    err = 1;
  }

  if(err) {
    entry = hd_free_manual(entry);
  }

  return entry;
}


/*
 * write an entry
 */

int hd_manual_write_entry(hd_data_t *hd_data, hd_manual_t *entry)
{
  FILE *f;
  char path[PATH_MAX];
  int error = 0;
  struct stat sbuf;
  str_list_t *sl1, *sl2;

  if(!entry) return 0;
  if(!entry->unique_id || entry->status.invalid) return 1;

  snprintf(path, sizeof path, "%s/%s", HARDWARE_UNIQUE_KEYS, entry->unique_id);

  if(!(f = fopen(path, "w"))) {
    /* maybe we have to create the HARDWARE_UNIQUE_KEYS directory first... */

    if(lstat(HARDWARE_DIR, &sbuf)) {
      mkdir(HARDWARE_DIR, 0755);
    }

    if(lstat(HARDWARE_UNIQUE_KEYS, &sbuf)) {
      mkdir(HARDWARE_UNIQUE_KEYS, 0755);
    }

    if(!(f = fopen(path, "w"))) return 2;
  }

  fprintf(f, "[%s]\n", MAN_SECT_GENERAL);

  if(
    !fprintf(f, "%s=%s\n",
      key2value(hw_ids_general, hw_id_unique),
      entry->unique_id
    )
  ) error = 3;

  if(
    entry->parent_id &&
    !fprintf(f, "%s=%s\n",
      key2value(hw_ids_general, hw_id_parent),
      entry->parent_id
    )
  ) error = 3;

  if(
    entry->child_ids &&
    !fprintf(f, "%s=%s\n",
      key2value(hw_ids_general, hw_id_child),
      entry->child_ids
    )
  ) error = 3;

  if(
    (entry->hw_class && key2value(hw_items, entry->hw_class)) &&
    !fprintf(f, "%s=%s\n",
      key2value(hw_ids_general, hw_id_hwclass),
      key2value(hw_items, entry->hw_class)
    )
  ) error = 3;

  if(
    entry->model &&
    !fprintf(f, "%s=%s\n",
      key2value(hw_ids_general, hw_id_model),
      entry->model
    )
  ) error = 3;

  fprintf(f, "\n[%s]\n", MAN_SECT_STATUS);

  if(
    (entry->status.configured && key2value(status_names, entry->status.configured)) &&
    !fprintf(f, "%s=%s\n",
      key2value(hw_ids_status, hw_id_configured),
      key2value(status_names, entry->status.configured)
    )
  ) error = 4;

  if(
    (entry->status.available && key2value(status_names, entry->status.available)) &&
    !fprintf(f, "%s=%s\n",
      key2value(hw_ids_status, hw_id_available),
      key2value(status_names, entry->status.available)
    )
  ) error = 4;

  if(
    (entry->status.needed && key2value(status_names, entry->status.needed)) &&
    !fprintf(f, "%s=%s\n",
      key2value(hw_ids_status, hw_id_needed),
      key2value(status_names, entry->status.needed)
    )
  ) error = 4;

  if(
    (entry->status.active && key2value(status_names, entry->status.active)) &&
    !fprintf(f, "%s=%s\n",
      key2value(hw_ids_status, hw_id_active),
      key2value(status_names, entry->status.active)
    )
  ) error = 4;

  if(
    entry->config_string &&
    !fprintf(f, "%s=%s\n",
      key2value(hw_ids_status, hw_id_cfgstring),
      entry->config_string
    )
  ) error = 4;

  fprintf(f, "\n[%s]\n", MAN_SECT_HARDWARE);

  for(
    sl1 = entry->key, sl2 = entry->value;
    sl1 && sl2;
    sl1 = sl1->next, sl2 = sl2->next
  ) {
    if(!fprintf(f, "%s=%s\n", sl1->str, sl2->str)) {
      error = 5;
      break;
    }
  }

  fputs("\n", f);

  fclose(f);

  /* remove old file */
  if(!error) {
    snprintf(path, sizeof path, "%s/%s", HARDWARE_DIR, entry->unique_id);
    unlink(path);
  }

  return error;
}


void dump_manual(hd_data_t *hd_data)
{
  hd_manual_t *entry;
  static const char *txt = "manually configured hardware";
  str_list_t *sl1, *sl2;

  if(!hd_data->manual) return;

  ADD2LOG("----- %s -----\n", txt);
  for(entry = hd_data->manual; entry; entry = entry->next) {
    ADD2LOG("  %s=%s\n",
      key2value(hw_ids_general, hw_id_unique),
      entry->unique_id
    );
    if(entry->parent_id)
      ADD2LOG("    %s=%s\n",
        key2value(hw_ids_general, hw_id_parent),
        entry->parent_id
      );
    if(entry->child_ids)
      ADD2LOG("    %s=%s\n",
        key2value(hw_ids_general, hw_id_child),
        entry->child_ids
      );
    ADD2LOG("    %s=%s\n",
      key2value(hw_ids_general, hw_id_hwclass),
      key2value(hw_items, entry->hw_class)
    );
    ADD2LOG("    %s=%s\n",
      key2value(hw_ids_general, hw_id_model),
      entry->model
    );
    ADD2LOG("    %s=%s\n",
      key2value(hw_ids_status, hw_id_configured),
      key2value(status_names, entry->status.configured)
    );
    ADD2LOG("    %s=%s\n",
      key2value(hw_ids_status, hw_id_available),
      key2value(status_names, entry->status.available)
    );
    ADD2LOG("    %s=%s\n",
      key2value(hw_ids_status, hw_id_needed),
      key2value(status_names, entry->status.needed)
    );
    ADD2LOG("    %s=%s\n",
      key2value(hw_ids_status, hw_id_active),
      key2value(status_names, entry->status.active)
    );
    if(entry->config_string)
      ADD2LOG("    %s=%s\n",
        key2value(hw_ids_status, hw_id_cfgstring),
        entry->config_string
      );
    for(
      sl1 = entry->key, sl2 = entry->value;
      sl1 && sl2;
      sl1 = sl1->next, sl2 = sl2->next
    ) {
      ADD2LOG("    %s=%s\n", sl1->str, sl2->str);
    }
  }
  ADD2LOG("----- %s end -----\n", txt);
}


unsigned str2id(char *str)
{
  unsigned id;
  unsigned tag = 0;

  if(strlen(str) == 3) return name2eisa_id(str);

  switch(*str) {
    case 'p':
      tag = TAG_PCI; str++; break;

    case 'r':
      str++; break;

    case 'u':
      tag = TAG_USB; str++; break;

    case 's':
      tag = TAG_SPECIAL; str++; break;

    case 'P':
      tag = TAG_PCMCIA; str++; break;

  }

  id = strtoul(str, &str, 16);
  if(*str) return 0;

  return MAKE_ID(tag, ID_VALUE(id));
}


/*
 * move info from hd_manual_t to hd_t
 */
void manual2hd(hd_data_t *hd_data, hd_manual_t *entry, hd_t *hd)
{
  str_list_t *sl1, *sl2;
  hw_hd_items_t item;
  unsigned tag, u0, u1, u2, u3, u4;
  hd_res_t *res;
  uint64_t u64_0, u64_1;
  char *s;
  int i;

  if(!hd || !entry) return;

  hd->unique_id = new_str(entry->unique_id);
  hd->parent_id = new_str(entry->parent_id);
  hd->child_ids = hd_split(',', entry->child_ids);
  hd->model = new_str(entry->model);
  hd->hw_class = entry->hw_class;

  hd->config_string = new_str(entry->config_string);

  hd->status = entry->status;

  for(
    sl1 = entry->key, sl2 = entry->value;
    sl1 && sl2;
    sl1 = sl1->next, sl2 = sl2->next
  ) {
    switch(item = value2key(hw_ids_hd_items, sl1->str)) {
      case hwdi_bus:
        hd->bus.id = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_slot:
        hd->slot = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_func:
        hd->func = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_base_class:
        hd->base_class.id = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_sub_class:
        hd->sub_class.id = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_prog_if:
        hd->prog_if.id = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_dev:
        hd->device.id = str2id(sl2->str);
        break;

      case hwdi_vend:
        hd->vendor.id = str2id(sl2->str);
        break;

      case hwdi_sub_dev:
        hd->sub_device.id = str2id(sl2->str);
        break;

      case hwdi_sub_vend:
        hd->sub_vendor.id = str2id(sl2->str);
        break;

      case hwdi_rev:
        hd->revision.id = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_compat_dev:
        hd->compat_device.id = str2id(sl2->str);
        break;

      case hwdi_compat_vend:
        hd->compat_vendor.id = str2id(sl2->str);
        break;

      case hwdi_dev_name:
        hd->device.name = new_str(sl2->str);
        break;

      case hwdi_vend_name:
        hd->vendor.name = new_str(sl2->str);
        break;

      case hwdi_sub_dev_name:
        hd->sub_device.name = new_str(sl2->str);
        break;

      case hwdi_sub_vend_name:
        hd->sub_vendor.name = new_str(sl2->str);
        break;

      case hwdi_rev_name:
        hd->revision.name = new_str(sl2->str);
        break;

      case hwdi_serial:
        hd->serial = new_str(sl2->str);
        break;

      case hwdi_unix_dev_name:
        hd->unix_dev_name = new_str(sl2->str);
        break;

      case hwdi_unix_dev_name2:
        hd->unix_dev_name2 = new_str(sl2->str);
        break;

      case hwdi_unix_dev_names:
        hd->unix_dev_names = hd_split(' ', sl2->str);
        break;

      case hwdi_drivers:
        hd->drivers = hd_split('|', sl2->str);
        break;

      case hwdi_sysfs_id:
        hd->sysfs_id = new_str(sl2->str);
        break;

      case hwdi_sysfs_busid:
        hd->sysfs_bus_id = new_str(sl2->str);
        break;

      case hwdi_sysfs_link:
        hd->sysfs_device_link = new_str(sl2->str);
        break;

      case hwdi_rom_id:
        hd->rom_id = new_str(sl2->str);
        break;

      case hwdi_broken:
        hd->broken = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_usb_guid:
        hd->usb_guid = new_str(sl2->str);
        break;

      case hwdi_hotplug:
        hd->hotplug = strtol(sl2->str, NULL, 0);
        break;

      case hwdi_class_list:
        for(
          u0 = 0, s = sl2->str;
          u0 < sizeof hd->hw_class_list / sizeof *hd->hw_class_list;
          u0++
        ) {
          if(*s && s[1] && (i = hex(s, 2)) >= 0) {
            hd->hw_class_list[u0] = i;
            s += 2;
          }
          else {
            break;
          }
        }
        break;

      case hwdi_res_mem:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_mem;
        if(sscanf(sl2->str, "0x%"SCNx64",0x%"SCNx64",%u,%u,%u", &u64_0, &u64_1, &u0, &u1, &u2) == 5) {
          res->mem.base = u64_0;
          res->mem.range = u64_1;
          res->mem.enabled = u0;
          res->mem.access = u1;
          res->mem.prefetch = u2;
        }
        break;

      case hwdi_res_phys_mem:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_phys_mem;
        if(sscanf(sl2->str, "0x%"SCNx64"", &u64_0) == 1) {
          res->phys_mem.range = u64_0;
        }
        break;

      case hwdi_res_io:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_io;
        if(sscanf(sl2->str, "0x%"SCNx64",0x%"SCNx64",%u,%u", &u64_0, &u64_1, &u0, &u1) == 4) {
          res->io.base = u64_0;
          res->io.range = u64_1;
          res->io.enabled = u0;
          res->io.access = u1;
        }
        break;

      case hwdi_res_irq:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_irq;
        if(sscanf(sl2->str, "%u,%u,%u", &u0, &u1, &u2) == 3) {
          res->irq.base = u0;
          res->irq.triggered = u1;
          res->irq.enabled = u2;
        }
        break;

      case hwdi_res_dma:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_dma;
        if(sscanf(sl2->str, "%u,%u", &u0, &u1) == 2) {
          res->dma.base = u0;
          res->dma.enabled = u1;
        }
        break;

      case hwdi_res_size:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_size;
        if(sscanf(sl2->str, "%u,%u,%u", &u0, &u1, &u2) == 3) {
          res->size.unit = u0;
          res->size.val1 = u1;
          res->size.val2 = u2;
        }
        break;

      case hwdi_res_baud:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_baud;
        if(sscanf(sl2->str, "%u,%u,%u,%u,%u", &u0, &u1, &u2, &u3, &u4) == 5) {
          res->baud.speed = u0;
          res->baud.bits = u1;
          res->baud.stopbits = u2;
          res->baud.parity = (char) u3;
          res->baud.handshake = (char) u4;
        }
        break;

      case hwdi_res_cache:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_cache;
        if(sscanf(sl2->str, "%u", &u0) == 1) {
          res->cache.size = u0;
        }
        break;

      case hwdi_res_disk_geo:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_disk_geo;
        if(sscanf(sl2->str, "%u,%u,%u,%u", &u0, &u1, &u2, &u3) == 4) {
          res->disk_geo.cyls = u0;
          res->disk_geo.heads = u1;
          res->disk_geo.sectors = u2;
          res->disk_geo.logical = u3;
        }
        break;

      case hwdi_res_monitor:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_monitor;
        if(sscanf(sl2->str, "%u,%u,%u,%u", &u0, &u1, &u2, &u3) == 4) {
          res->monitor.width = u0;
          res->monitor.height = u1;
          res->monitor.vfreq = u2;
          res->monitor.interlaced = u3;
        }
        break;

      case hwdi_res_framebuffer:
        res = add_res_entry(&hd->res, new_mem(sizeof *res));
        res->any.type = res_framebuffer;
        if(sscanf(sl2->str, "%u,%u,%u,%u,%u", &u0, &u1, &u2, &u3, &u4) == 5) {
          res->framebuffer.width = u0;
          res->framebuffer.height = u1;
          res->framebuffer.bytes_p_line = u2;
          res->framebuffer.colorbits = u3;
          res->framebuffer.mode = u4;
        }
        break;

      case hwdi_features:
        u0 = strtoul(sl2->str, NULL, 0);
        if(u0 & (1 << 0)) hd->is.agp = 1;
        if(u0 & (1 << 1)) hd->is.isapnp = 1;
        if(u0 & (1 << 2)) hd->is.softraiddisk = 1;
        if(u0 & (1 << 3)) hd->is.zip = 1;
        if(u0 & (1 << 4)) hd->is.cdr = 1;
        if(u0 & (1 << 5)) hd->is.cdrw = 1;
        if(u0 & (1 << 6)) hd->is.dvd = 1;
        if(u0 & (1 << 7)) hd->is.dvdr = 1;
        if(u0 & (1 << 8)) hd->is.dvdram = 1;
        if(u0 & (1 << 9)) hd->is.pppoe = 1;
        if(u0 & (1 << 10)) hd->is.wlan = 1;
        break;
    }
  }

  if(hd->device.id || hd->vendor.id) {
    tag = ID_TAG(hd->device.id);
    tag = tag ? tag : ID_TAG(hd->vendor.id);
    tag = tag ? tag : TAG_PCI;
    hd->device.id = MAKE_ID(tag, ID_VALUE(hd->device.id));
    hd->vendor.id = MAKE_ID(tag, ID_VALUE(hd->vendor.id));
  }

  if(hd->sub_device.id || hd->sub_vendor.id) {
    tag = ID_TAG(hd->sub_device.id);
    tag = tag ? tag : ID_TAG(hd->sub_vendor.id);
    tag = tag ? tag : TAG_PCI;
    hd->sub_device.id = MAKE_ID(tag, ID_VALUE(hd->sub_device.id));
    hd->sub_vendor.id = MAKE_ID(tag, ID_VALUE(hd->sub_vendor.id));
  }

  if(hd->compat_device.id || hd->compat_vendor.id) {
    tag = ID_TAG(hd->compat_device.id);
    tag = tag ? tag : ID_TAG(hd->compat_vendor.id);
    tag = tag ? tag : TAG_PCI;
    hd->compat_device.id = MAKE_ID(tag, ID_VALUE(hd->compat_device.id));
    hd->compat_vendor.id = MAKE_ID(tag, ID_VALUE(hd->compat_vendor.id));
  }

  if(hd->status.available == status_unknown) hd->is.manual = 1;

  /* create some entries, if missing */

  if(!hd->device.id && !hd->vendor.id && !hd->device.name) {
    hd->device.name = new_str(hd->model);
  }

  if(hd->hw_class && !hd->base_class.id) {
    switch(hd->hw_class) {
      case hw_cdrom:
        hd->base_class.id = bc_storage_device;
        hd->sub_class.id = sc_sdev_cdrom;
        break;

      case hw_mouse:
        hd->base_class.id = bc_mouse;
        hd->sub_class.id = sc_mou_other;
        break;

      default:
	break;
    }
  }

  hddb_add_info(hd_data, hd);
}


void hd2manual(hd_t *hd, hd_manual_t *entry)
{
  char *s, *t;
  hd_res_t *res;
  str_list_t *sl;
  unsigned u;

  if(!hd || !entry) return;

  entry->unique_id = new_str(hd->unique_id);
  entry->parent_id = new_str(hd->parent_id);
  entry->child_ids = hd_join(",", hd->child_ids);
  entry->model = new_str(hd->model);
  entry->hw_class = hd->hw_class;

  entry->config_string = new_str(hd->config_string);

  entry->status = hd->status;

  if(
    !entry->status.configured &&
    !entry->status.available &&
    !entry->status.needed &&
    !entry->status.active &&
    !entry->status.invalid
  ) {
    entry->status.configured = status_new;
    entry->status.available = hd->module == mod_manual ? status_unknown : status_yes;
    entry->status.needed = status_no;
    entry->status.active = status_unknown;
  }

  s = NULL;

  if(hd->broken) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_broken));
    str_printf(&s, 0, "0x%x", hd->broken);
    add_str_list(&entry->value, s);
  }

  if(hd->bus.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_bus));
    str_printf(&s, 0, "0x%x", hd->bus.id);
    add_str_list(&entry->value, s);
  }

  if(hd->slot) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_slot));
    str_printf(&s, 0, "0x%x", hd->slot);
    add_str_list(&entry->value, s);
  }

  if(hd->func) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_func));
    str_printf(&s, 0, "0x%x", hd->func);
    add_str_list(&entry->value, s);
  }

  if(hd->base_class.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_base_class));
    str_printf(&s, 0, "0x%x", hd->base_class.id);
    add_str_list(&entry->value, s);
  }

  if(hd->sub_class.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sub_class));
    str_printf(&s, 0, "0x%x", hd->sub_class.id);
    add_str_list(&entry->value, s);
  }

  if(hd->prog_if.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_prog_if));
    str_printf(&s, 0, "0x%x", hd->prog_if.id);
    add_str_list(&entry->value, s);
  }

  if(hd->device.id || hd->vendor.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_vend));
    add_str_list(&entry->value, vend_id2str(hd->vendor.id));
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_dev));
    str_printf(&s, 0, "%04x", ID_VALUE(hd->device.id));
    add_str_list(&entry->value, s);
  }

  if(hd->sub_device.id || hd->sub_vendor.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sub_vend));
    add_str_list(&entry->value, vend_id2str(hd->sub_vendor.id));
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sub_dev));
    str_printf(&s, 0, "%04x", ID_VALUE(hd->sub_device.id));
    add_str_list(&entry->value, s);
  }

  if(hd->revision.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_rev));
    str_printf(&s, 0, "0x%x", hd->revision.id);
    add_str_list(&entry->value, s);
  }

  if(hd->compat_device.id || hd->compat_vendor.id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_compat_vend));
    add_str_list(&entry->value, vend_id2str(hd->compat_vendor.id));
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_compat_dev));
    str_printf(&s, 0, "%04x", ID_VALUE(hd->compat_device.id));
    add_str_list(&entry->value, s);
  }

  if(hd->device.name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_dev_name));
    add_str_list(&entry->value, hd->device.name);
  }

  if(hd->vendor.name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_vend_name));
    add_str_list(&entry->value, hd->vendor.name);
  }

  if(hd->sub_device.name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sub_dev_name));
    add_str_list(&entry->value, hd->sub_device.name);
  }

  if(hd->sub_vendor.name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sub_vend_name));
    add_str_list(&entry->value, hd->sub_vendor.name);
  }

  if(hd->revision.name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_rev_name));
    add_str_list(&entry->value, hd->revision.name);
  }

  if(hd->serial) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_serial));
    add_str_list(&entry->value, hd->serial);
  }

  if(hd->unix_dev_name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_unix_dev_name));
    add_str_list(&entry->value, hd->unix_dev_name);
  }

  if(hd->unix_dev_name2) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_unix_dev_name2));
    add_str_list(&entry->value, hd->unix_dev_name2);
  }

  if(hd->unix_dev_names) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_unix_dev_names));
    s = free_mem(s);
    s = hd_join(" ", hd->unix_dev_names);
    add_str_list(&entry->value, s);
  }

  if(hd->drivers) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_drivers));
    s = free_mem(s);
    s = hd_join("|", hd->drivers);
    add_str_list(&entry->value, s);
  }

  if(hd->sysfs_id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sysfs_id));
    add_str_list(&entry->value, hd->sysfs_id);
  }

  if(hd->sysfs_bus_id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sysfs_busid));
    add_str_list(&entry->value, hd->sysfs_bus_id);
  }

  if(hd->sysfs_device_link) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sysfs_link));
    add_str_list(&entry->value, hd->sysfs_device_link);
  }

  if(hd->rom_id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_rom_id));
    add_str_list(&entry->value, hd->rom_id);
  }

  if(hd->usb_guid) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_usb_guid));
    add_str_list(&entry->value, hd->usb_guid);
  }

  if(hd->hotplug) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_hotplug));
    str_printf(&s, 0, "%d", hd->hotplug);
    add_str_list(&entry->value, s);
  }

  s = free_mem(s);
  for(u = 0; u < sizeof hd->hw_class_list / sizeof *hd->hw_class_list; u++) {
    str_printf(&s, -1, "%02x", hd->hw_class_list[u]);
  }
  add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_class_list));
  add_str_list(&entry->value, s);

  u = 0;
  if(hd->is.agp)          u |= 1 << 0;
  if(hd->is.isapnp)       u |= 1 << 1;
  if(hd->is.softraiddisk) u |= 1 << 2;
  if(hd->is.zip)          u |= 1 << 3;
  if(hd->is.cdr)          u |= 1 << 4;
  if(hd->is.cdrw)         u |= 1 << 5;
  if(hd->is.dvd)          u |= 1 << 6;
  if(hd->is.dvdr)         u |= 1 << 7;
  if(hd->is.dvdram)       u |= 1 << 8;
  if(hd->is.pppoe)        u |= 1 << 9;
  if(hd->is.wlan)         u |= 1 << 10;
  
  if(u) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_features));
    str_printf(&s, 0, "0x%x", u);
    add_str_list(&entry->value, s);
  }

  for(res = hd->res; res; res = res->next) {
    sl = NULL;
    switch(res->any.type) {
      case res_mem:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_mem));
        str_printf(&s, 0, "0x%"PRIx64"", res->mem.base);
        add_str_list(&sl, s);
        str_printf(&s, 0, "0x%"PRIx64"", res->mem.range);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->mem.enabled);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->mem.access);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->mem.prefetch);
        add_str_list(&sl, s);
        break;

      case res_phys_mem:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_phys_mem));
        str_printf(&s, 0, "0x%"PRIx64"", res->phys_mem.range);
        add_str_list(&sl, s);
        break;

      case res_io:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_io));
        str_printf(&s, 0, "0x%"PRIx64"", res->io.base);
        add_str_list(&sl, s);
        str_printf(&s, 0, "0x%"PRIx64"", res->io.range);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->io.enabled);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->io.access);
        add_str_list(&sl, s);
        break;

      case res_irq:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_irq));
        str_printf(&s, 0, "%u", res->irq.base);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->irq.triggered);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->irq.enabled);
        add_str_list(&sl, s);
        break;

      case res_dma:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_dma));
        str_printf(&s, 0, "%u", res->dma.base);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->dma.enabled);
        add_str_list(&sl, s);
        break;

      case res_size:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_size));
        str_printf(&s, 0, "%u", res->size.unit);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%"PRIu64, res->size.val1);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%"PRIu64, res->size.val2);
        add_str_list(&sl, s);
        break;

      case res_baud:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_baud));
        str_printf(&s, 0, "%u", res->baud.speed);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->baud.bits);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->baud.stopbits);
        add_str_list(&sl, s);
        str_printf(&s, 0, "0x%02x", (unsigned) res->baud.parity);
        add_str_list(&sl, s);
        str_printf(&s, 0, "0x%02x", (unsigned) res->baud.handshake);
        add_str_list(&sl, s);
        break;

      case res_cache:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_cache));
        str_printf(&s, 0, "%u", res->cache.size);
        add_str_list(&sl, s);
        break;

      case res_disk_geo:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_disk_geo));
        str_printf(&s, 0, "%u", res->disk_geo.cyls);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->disk_geo.heads);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->disk_geo.sectors);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->disk_geo.logical);
        add_str_list(&sl, s);
        break;

      case res_monitor:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_monitor));
        str_printf(&s, 0, "%u", res->monitor.width);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->monitor.height);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->monitor.vfreq);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->monitor.interlaced);
        add_str_list(&sl, s);
        break;

      case res_framebuffer:
        add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_res_framebuffer));
        str_printf(&s, 0, "%u", res->framebuffer.width);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->framebuffer.height);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->framebuffer.bytes_p_line);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->framebuffer.colorbits);
        add_str_list(&sl, s);
        str_printf(&s, 0, "%u", res->framebuffer.mode);
        add_str_list(&sl, s);
        break;

      default:
        break;
    }
    /* keep entry->key & entry->value symmetrical! */
    if(sl) {
      t = hd_join(",", sl);
      add_str_list(&entry->value, t);
      free_mem(t);
      free_str_list(sl);
    }
  }

  free_mem(s);
}


hd_t *hd_read_config(hd_data_t *hd_data, const char *id)
{
  hd_t *hd = NULL;
  hd_manual_t *entry;

  hddb_init(hd_data);

  entry = hd_manual_read_entry(hd_data, id);

  if(entry) {
    hd = new_mem(sizeof *hd);
    hd->module = hd_data->module;
    hd->line = __LINE__;
    hd->tag.freeit = 1;		/* make it a 'stand alone' entry */
    manual2hd(hd_data, entry, hd);
    hd_free_manual(entry);
  }

  return hd;
}


int hd_write_config(hd_data_t *hd_data, hd_t *hd)
{
  int err = 0;
  hd_manual_t *entry;

  entry = new_mem(sizeof *entry);

  hd2manual(hd, entry);

  err = entry->unique_id ? hd_manual_write_entry(hd_data, entry) : 5;

  hd_free_manual(entry);

  return err;
}


#endif	/* LIBHD_TINY */


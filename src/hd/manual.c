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

/* corresponds to hd_hw_item_t (not all are allowed) */
static hash_t hw_items[] = {
  { hw_cdrom,        "cdrom"             },
  { hw_floppy,       "floppy"            },
  { hw_disk,         "disk"              },
  { hw_network,      "network interface" },
  { hw_display,      "graphics card"     },
  { hw_monitor,      "monitor"           },
  { hw_mouse,        "mouse"             },
  { hw_keyboard,     "keyboard"          },
  { hw_sound,        "sound"             },
  { hw_isdn,         "isdn adapter"      },
  { hw_modem,        "modem"             },
  { hw_storage_ctrl, "storage"           },
  { hw_network_ctrl, "network"           },
  { hw_printer,      "printer"           },
  { hw_tv,           "tv card"           },
  { hw_scanner,      "scanner"           },
  { hw_braille,      "braille"           },
  { hw_sys,          "system"            },
  { hw_cpu,          "cpu"               },
  { hw_partition,    "partition"         },
  { 0,               NULL                }
};

typedef enum {
  hw_id_unique = 1, hw_id_parent, hw_id_hwclass, hw_id_model,
  hw_id_configured, hw_id_available, hw_id_critical
} hw_id_t;

#define MAN_SECT_GENERAL	"General"
#define MAN_SECT_STATUS		"Status"
#define MAN_SECT_HARDWARE	"Hardware"

static hash_t hw_ids_general[] = {
  { hw_id_unique,     "UniqueID"   },
  { hw_id_parent,     "ParentID"   },
  { hw_id_hwclass,    "HWClass"    },
  { hw_id_model,      "Model"      },
  { 0,                NULL         }
};

static hash_t hw_ids_status[] = {
  { hw_id_configured, "Configured" },
  { hw_id_available,  "Available"  },
  { hw_id_critical,   "Critical"   },
  { 0,                NULL         }
};

/* structure elements from hd_t */
typedef enum {
  hwdi_bus = 1, hwdi_slot, hwdi_func, hwdi_base_class, hwdi_sub_class,
  hwdi_prog_if, hwdi_dev, hwdi_vend, hwdi_sub_dev, hwdi_sub_vend, hwdi_rev,
  hwdi_compat_dev, hwdi_compat_vend, hwdi_dev_name, hwdi_vend_name,
  hwdi_sub_dev_name, hwdi_sub_vend_name, hwdi_rev_name, hwdi_serial,
  hwdi_unix_dev_name, hwdi_rom_id
} hw_hd_items_t;

static hash_t hw_ids_hd_items[] = {
  { hwdi_bus,           "Bus"            },
  { hwdi_slot,          "Slot"           },
  { hwdi_func,          "Function"       },
  { hwdi_base_class,    "BaseClass"      },
  { hwdi_sub_class,     "SubClass"       },
  { hwdi_prog_if,       "ProgIF"         },
  { hwdi_dev,           "DeviceID"       },
  { hwdi_vend,          "VendorID"       },
  { hwdi_sub_dev,       "SubDeviceID"    },
  { hwdi_sub_vend,      "SubVendorID"    },
  { hwdi_rev,           "RevisionID"     },
  { hwdi_compat_dev,    "CompatDeviceID" },
  { hwdi_compat_vend,   "CompatVendorID" },
  { hwdi_dev_name,      "DeviceName"     },
  { hwdi_vend_name,     "VendorName"     },
  { hwdi_sub_dev_name,  "SubDeviceName"  },
  { hwdi_sub_vend_name, "SubVendorName"  },
  { hwdi_rev_name,      "RevisionName"   },
  { hwdi_serial,        "Serial"         },
  { hwdi_unix_dev_name, "UnixDevice"     },
  { hwdi_rom_id,        "ROMID"          },
  { 0,                  NULL             }
};

static char *key2value(hash_t *hash, int id);
static int value2key(hash_t *hash, char *str);
static void dump_manual(hd_data_t *hd_data);
static unsigned str2id(char *str);
static void manual2hd(hd_manual_t *entry, hd_t *hd);
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

  if((dir = opendir(MANUAL_DIR))) {
    i = 0;
    while((de = readdir(dir))) {
      if(!strcmp(de->d_name, ".") || !strcmp(de->d_name, "..")) continue;
      PROGRESS(1, ++i, "read");
      if((entry = hd_manual_read_entry(hd_data, de->d_name))) {
        ADD2LOG("  got %s\n", entry->unique_id);
        *entry_next = entry;
        entry_next = &entry->next;
      }
    }
    closedir(dir);
  }

  if(hd_data->debug) dump_manual(hd_data);

  for(entry = hd_data->manual; entry; entry = entry->next) {
    for(hd = hd_data->hd; hd; hd = hd->next) {
      if(hd->unique_id && !strcmp(hd->unique_id, entry->unique_id)) break;
    }

    if(hd) {
      /* just update config status */
      hd->status = entry->status;
    }
    else {
      /* add new entry */
      hd = add_hd_entry(hd_data, __LINE__, 0);

      manual2hd(entry, hd);

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
hd_manual_t *hd_manual_read_entry(hd_data_t *hd_data, char *id)
{
  char path[PATH_MAX];
  int i, j, line;
  str_list_t *sl, *sl0;
  hd_manual_t *entry;
  hash_t *sect;
  char *s, *s1, *s2;
  int err = 0;

  snprintf(path, sizeof path, "%s/%s", MANUAL_DIR, id);

  if(!(sl0 = read_file(path, 0, 0))) return NULL;

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
          entry->status.available = j;
          if(!j) err = 1;
          break;

        case hw_id_critical:
          j = value2key(status_names, s);
          entry->status.critical = j;
          if(!j) err = 1;
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
   * new hardware, not autodetectable, not critical
   */
  if(
    !entry->status.configured &&
    !entry->status.available &&
    !entry->status.critical &&
    !entry->status.invalid
  ) {
    entry->status.configured = status_new;
    entry->status.available = status_unknown;
    entry->status.critical = status_no;
  }

  if(
    !entry->status.configured ||
    !entry->status.available ||
    !entry->status.critical
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

  snprintf(path, sizeof path, "%s/%s", MANUAL_DIR, entry->unique_id);

  if(!(f = fopen(path, "w"))) {
    /* maybe we have to create the MANUAL_DIR directory first... */

    if(lstat(MANUAL_DIR, &sbuf)) {
      mkdir(MANUAL_DIR, 0755);
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
    (entry->status.critical && key2value(status_names, entry->status.critical)) &&
    !fprintf(f, "%s=%s\n",
      key2value(hw_ids_status, hw_id_critical),
      key2value(status_names, entry->status.critical)
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

  return error;
}


void dump_manual(hd_data_t *hd_data)
{
  hd_manual_t *entry;
  static const char *txt = "manually configured harware";
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
      key2value(hw_ids_status, hw_id_critical),
      key2value(status_names, entry->status.critical)
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
  }

  id = strtoul(str, &str, 16);
  if(*str) return 0;

  return MAKE_ID(tag, ID_VALUE(id));
}


/*
 * move info from hd_manual_t to hd_t
 */
void manual2hd(hd_manual_t *entry, hd_t *hd)
{
  str_list_t *sl1, *sl2;
  hw_hd_items_t item;
  unsigned tag;

  if(!hd || !entry) return;

  hd->unique_id = new_str(entry->unique_id);
  hd->parent_id = new_str(entry->parent_id);
  hd->model = new_str(entry->model);
  hd->hw_class = entry->hw_class;

  hd->status = entry->status;

  for(
    sl1 = entry->key, sl2 = entry->value;
    sl1 && sl2;
    sl1 = sl1->next, sl2 = sl2->next
  ) {
    switch(item = value2key(hw_ids_hd_items, sl1->str)) {
      case hwdi_bus:
        hd->bus = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_slot:
        hd->slot = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_func:
        hd->func = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_base_class:
        hd->base_class = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_sub_class:
        hd->sub_class = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_prog_if:
        hd->prog_if = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_dev:
        hd->dev = str2id(sl2->str);
        break;

      case hwdi_vend:
        hd->vend = str2id(sl2->str);
        break;

      case hwdi_sub_dev:
        hd->sub_dev = str2id(sl2->str);
        break;

      case hwdi_sub_vend:
        hd->sub_vend = str2id(sl2->str);
        break;

      case hwdi_rev:
        hd->rev = strtoul(sl2->str, NULL, 0);
        break;

      case hwdi_compat_dev:
        hd->compat_dev = str2id(sl2->str);
        break;

      case hwdi_compat_vend:
        hd->compat_vend = str2id(sl2->str);
        break;

      case hwdi_dev_name:
        hd->dev_name = new_str(sl2->str);
        break;

      case hwdi_vend_name:
        hd->vend_name = new_str(sl2->str);
        break;

      case hwdi_sub_dev_name:
        hd->sub_dev_name = new_str(sl2->str);
        break;

      case hwdi_sub_vend_name:
        hd->sub_vend_name = new_str(sl2->str);
        break;

      case hwdi_rev_name:
        hd->rev_name = new_str(sl2->str);
        break;

      case hwdi_serial:
        hd->serial = new_str(sl2->str);
        break;

      case hwdi_unix_dev_name:
        hd->unix_dev_name = new_str(sl2->str);
        break;

      case hwdi_rom_id:
        hd->rom_id = new_str(sl2->str);
        break;
    }
  }

  if(hd->dev || hd->vend) {
    tag = ID_TAG(hd->dev);
    tag = tag ? tag : ID_TAG(hd->vend);
    tag = tag ? tag : TAG_PCI;
    hd->dev = MAKE_ID(tag, ID_VALUE(hd->dev));
    hd->vend = MAKE_ID(tag, ID_VALUE(hd->vend));
  }

  if(hd->sub_dev || hd->sub_vend) {
    tag = ID_TAG(hd->sub_dev);
    tag = tag ? tag : ID_TAG(hd->sub_vend);
    tag = tag ? tag : TAG_PCI;
    hd->sub_dev = MAKE_ID(tag, ID_VALUE(hd->sub_dev));
    hd->sub_vend = MAKE_ID(tag, ID_VALUE(hd->sub_vend));
  }

  if(hd->compat_dev || hd->compat_vend) {
    tag = ID_TAG(hd->compat_dev);
    tag = tag ? tag : ID_TAG(hd->compat_vend);
    tag = tag ? tag : TAG_PCI;
    hd->compat_dev = MAKE_ID(tag, ID_VALUE(hd->compat_dev));
    hd->compat_vend = MAKE_ID(tag, ID_VALUE(hd->compat_vend));
  }

  if(hd->status.available == status_unknown) hd->is.manual = 1;

  /* create some entries, if missing */

  if(!hd->dev && !hd->vend && !hd->dev_name) {
    hd->dev_name = new_str(hd->model);
  }

  if(hd->hw_class && !hd->base_class) {
    switch(hd->hw_class) {
      case hw_cdrom:
        hd->base_class = bc_storage_device;
        hd->sub_class = sc_sdev_cdrom;
        break;

      case hw_mouse:
        hd->base_class = bc_mouse;
        hd->sub_class = sc_mou_other;
        break;

      default:
    }
  }
}


void hd2manual(hd_t *hd, hd_manual_t *entry)
{
  char *s;

  if(!hd || !entry) return;

  entry->unique_id = new_str(hd->unique_id);
  entry->parent_id = new_str(hd->parent_id);
  entry->model = new_str(hd->model);
  entry->hw_class = hd->hw_class;

  entry->status = hd->status;

  if(
    !entry->status.configured &&
    !entry->status.available &&
    !entry->status.critical &&
    !entry->status.invalid
  ) {
    entry->status.configured = status_new;
    entry->status.available = hd->module == mod_manual ? status_unknown : status_yes;
    entry->status.critical = status_no;
  }

  s = NULL;

  if(hd->bus) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_bus));
    str_printf(&s, 0, "0x%x", hd->bus);
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

  if(hd->base_class) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_base_class));
    str_printf(&s, 0, "0x%x", hd->base_class);
    add_str_list(&entry->value, s);
  }

  if(hd->sub_class) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sub_class));
    str_printf(&s, 0, "0x%x", hd->sub_class);
    add_str_list(&entry->value, s);
  }

  if(hd->prog_if) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_prog_if));
    str_printf(&s, 0, "0x%x", hd->prog_if);
    add_str_list(&entry->value, s);
  }

  if(hd->dev || hd->vend) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_vend));
    add_str_list(&entry->value, vend_id2str(hd->vend));
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_dev));
    str_printf(&s, 0, "%04x", ID_VALUE(hd->dev));
    add_str_list(&entry->value, s);
  }

  if(hd->sub_dev || hd->sub_vend) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sub_vend));
    add_str_list(&entry->value, vend_id2str(hd->sub_vend));
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sub_dev));
    str_printf(&s, 0, "%04x", ID_VALUE(hd->sub_dev));
    add_str_list(&entry->value, s);
  }

  if(hd->rev) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_rev));
    str_printf(&s, 0, "0x%x", hd->rev);
    add_str_list(&entry->value, s);
  }

  if(hd->compat_dev || hd->compat_vend) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_compat_vend));
    add_str_list(&entry->value, vend_id2str(hd->compat_vend));
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_compat_dev));
    str_printf(&s, 0, "%04x", ID_VALUE(hd->compat_dev));
    add_str_list(&entry->value, s);
  }

  if(hd->dev_name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_dev_name));
    add_str_list(&entry->value, hd->dev_name);
  }

  if(hd->vend_name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_vend_name));
    add_str_list(&entry->value, hd->vend_name);
  }

  if(hd->sub_dev_name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sub_dev_name));
    add_str_list(&entry->value, hd->sub_dev_name);
  }

  if(hd->sub_vend_name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_sub_vend_name));
    add_str_list(&entry->value, hd->sub_vend_name);
  }

  if(hd->rev_name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_rev_name));
    add_str_list(&entry->value, hd->rev_name);
  }

  if(hd->serial) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_serial));
    add_str_list(&entry->value, hd->serial);
  }

  if(hd->unix_dev_name) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_unix_dev_name));
    add_str_list(&entry->value, hd->unix_dev_name);
  }

  if(hd->rom_id) {
    add_str_list(&entry->key, key2value(hw_ids_hd_items, hwdi_rom_id));
    add_str_list(&entry->value, hd->rom_id);
  }

  free_mem(s);
}


hd_t *hd_read_config(hd_data_t *hd_data, char *id)
{
  hd_t *hd = NULL;
  hd_manual_t *entry;

  entry = hd_manual_read_entry(hd_data, id);

  if(entry) {
    hd = new_mem(sizeof *hd);
    hd->module = hd_data->module;
    hd->line = __LINE__;
    hd->tag.freeit = 1;		/* make it a 'stand alone' entry */
    manual2hd(entry, hd);
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


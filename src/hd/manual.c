#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <dirent.h>
#include <ctype.h>
#include <sys/types.h>

#include "hd.h"
#include "hd_int.h"
#include "manual.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * hardware in /var/lib/hardware/
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

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

/* corresponds to hd_hw_item_t, but not all items are allowed */
static hash_t hw_items[] = {
  { hw_disk,         "disk"      },
  { hw_partition,    "partition" },
  { hw_cdrom,        "cdrom"     },
  { hw_mouse,        "mouse"     },
  { hw_display,      "gfxcard"   },
  { hw_storage_ctrl, "storage"   },
  { hw_network_ctrl, "network"   },
  { 0,               NULL        }
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

static char *key2value(hash_t *hash, int id);
static int value2key(hash_t *hash, char *str);
hd_manual_t *hd_manual_read_entry(hd_data_t *hd_data, char *id);
static void dump_manual(hd_data_t *hd_data);

void hd_scan_manual(hd_data_t *hd_data)
{
  DIR *dir;
  struct dirent *de;
  hd_manual_t *entry, **entry_next;
  int i;

  if(!hd_probe_feature(hd_data, pr_manual)) return;

  hd_data->module = mod_manual;

  /* some clean-up */
  remove_hd_entries(hd_data);

  hd_data->manual = free_manual(hd_data->manual);
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
    entry = free_manual(entry);
  }

  return entry;
}

#if 0

/*
 * write an entry
 */
int SuSEhw_write_entry(hw_SuSE *entry)
{
  FILE *f;
  char path[PATH_MAX];
  int error = 0;
  struct stat sbuf;

  if(!entry || !entry->unique_id || entry->status.invalid) return HW_ERROR;

  snprintf(path, sizeof path, "%s/%s", HW_SUSE_PATH, entry->unique_id);

  if(!(f = fopen(path, "w"))) {
    /* maybe we have to create the HW_SUSE_PATH directory first... */

    if(stat(HW_SUSE_PATH, &sbuf)) {
      mkdir(HW_SUSE_PATH, 0755);
    }

    if(!(f = fopen(path, "w"))) {
      return HW_OPEN_ERROR;
    }
  }

  fputs("[General]\n", f);
  if(!fprintf(f, "%s=%s\n", HW_F_UNIQ_TAG, entry->unique_id)) error = 1;
  if(entry->parent_id)
    if(!fprintf(f, "%s=%s\n", HW_F_PARENT_TAG, entry->parent_id)) error = 1;
  if(entry->hwclass && entry->hwclass < SHW_CLASS_NAMES_SIZE) {
    if(!fprintf(f, "%s=%s\n", HW_F_HWCLASS_TAG, shw_class_names[entry->hwclass])) error = 1;
  }
  if(entry->model)
    if(!fprintf(f, "%s=%s\n", HW_F_MODEL_TAG, entry->model)) error = 1;
  fputs("\n", f);

  fputs("[Status]\n", f);
  if(entry->status.configured && entry->status.configured < STATUS_NAMES_SIZE) {
    if(!fprintf(f, "%s=%s\n", HW_F_CONFIG_TAG, status_names[entry->status.configured])) error = 1;
  }
  if(entry->status.available && entry->status.available < STATUS_NAMES_SIZE) {
    if(!fprintf(f, "%s=%s\n", HW_F_AVAILABLE_TAG, status_names[entry->status.available])) error = 1;
  }
  if(entry->status.critical && entry->status.critical < STATUS_NAMES_SIZE) {
    if(!fprintf(f, "%s=%s\n", HW_F_CRITICAL_TAG, status_names[entry->status.critical])) error = 1;
  }
  fputs("\n", f);

  fputs("[Hardware]\n", f);
  if(entry->unix_dev)
    if(!fprintf(f, "%s=%s\n", HW_F_UNIX_DEV_TAG, entry->unix_dev)) error = 1;
  if(!fprintf(f, "%s=0x%03x\n", HW_F_BASE_CLASS_TAG, entry->base_class)) error = 1;
  if(entry->sub_class)
    if(!fprintf(f, "%s=0x%02x\n", HW_F_SUB_CLASS_TAG, entry->sub_class)) error = 1;
  if(entry->vendor_name)
    if(!fprintf(f, "%s=%s\n", HW_F_VENDOR_NAME_TAG, entry->vendor_name)) error = 1;
  if(entry->device_name)
    if(!fprintf(f, "%s=%s\n", HW_F_DEVICE_NAME_TAG, entry->device_name)) error = 1;
  fputs("\n", f);

  fclose(f);

  return error ? HW_WRITE_ERROR : HW_OK;
}


#endif

void dump_manual(hd_data_t *hd_data)
{
  hd_manual_t *manual;
  static const char *txt = "manually configured harware";
  str_list_t *sl1, *sl2;

  if(!hd_data->manual) return;

  ADD2LOG("----- %s -----\n", txt);
  for(manual = hd_data->manual; manual; manual = manual->next) {
    ADD2LOG("  UniqueID=%s\n", manual->unique_id);
    if(manual->parent_id) ADD2LOG("    ParentID=%s\n", manual->parent_id);
    ADD2LOG("    HWClass=%s\n", key2value(hw_items, manual->hw_class));
    ADD2LOG("    Model=%s\n", manual->model);
    ADD2LOG("    Configured=%s\n", key2value(status_names, manual->status.configured));
    ADD2LOG("    Available=%s\n", key2value(status_names, manual->status.available));
    ADD2LOG("    Critical=%s\n", key2value(status_names, manual->status.critical));
    for(
      sl1 = manual->key, sl2 = manual->value;
      sl1 && sl2;
      sl1 = sl1->next, sl2 = sl2->next
    ) {
      ADD2LOG("    %s=%s\n", sl1->str, sl2->str);
    }
  }
  ADD2LOG("----- %s end -----\n", txt);
}


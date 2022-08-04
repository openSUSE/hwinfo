typedef struct {
  int key;
  char *value;
} hash_t;

static char *key2value(hash_t *hash, int id);
static int value2key(hash_t *hash, char *str);

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
  { hw_redasd,        "redasd"              },
  { hw_dsl,           "dsl adapter"         },
  { hw_block,         "block device"        },
  { hw_tape,          "tape"                },
  { hw_vbe,           "vesa bios"           },
  { hw_bluetooth,     "bluetooth"           },
  { hw_nvme,          "nvme"                },
  { hw_unknown,       "unknown"             },
  { 0,                NULL                  }
};

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


hd_hw_item_t hd_hw_item_type(char *name)
{
  return name ? value2key(hw_items, name) : hw_none;
}



#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/input.h>

#include "hd.h"
#include "hd_int.h"
#include "hddb.h"
#include "input.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * input devs
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */

static void get_input_devices(hd_data_t *hd_data);
static char *all_bits(char *str);
static int test_bit(char *str, unsigned bit);

void hd_scan_input(hd_data_t *hd_data)
{
  if(!hd_probe_feature(hd_data, pr_input)) return;

  hd_data->module = mod_input;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "joydev mod");
  load_module(hd_data, "joydev");

  PROGRESS(1, 1, "evdev mod");
  load_module(hd_data, "evdev");

  PROGRESS(2, 0, "input");

  get_input_devices(hd_data);
}


#define INP_NAME	"N: Name="
#define INP_HANDLERS	"H: Handlers="
#define INP_KEY		"B: KEY="
#define INP_REL		"B: REL="
#define INP_ABS		"B: ABS="

void get_input_devices(hd_data_t *hd_data)
{
  hd_t *hd;
  str_list_t *input, *sl, *sl1;
  char *s;
  unsigned ok, u, is_mouse;
  unsigned bus, vendor, product, version;
  unsigned mouse_buttons, mouse_wheels;
  char *name = NULL, *handlers = NULL, *key = NULL, *rel = NULL, *abso = NULL;
  size_t len;
  str_list_t *handler_list;
  hd_dev_num_t dev_num = { type: 'c', range: 1 };

  input = read_file("/proc/bus/input/devices", 0, 0);

  ADD2LOG("----- /proc/bus/input/devices -----\n");
  for(sl = input; sl; sl = sl->next) {
    ADD2LOG("  %s", sl->str);
  }
  ADD2LOG("----- /proc/bus/input/devices end -----\n");

  for(ok = 0, sl = input; sl; sl = sl->next) {
    if(*sl->str == '\n') {
      ADD2LOG("bus = %u, name = %s\n", bus, name);
      if(handlers) ADD2LOG("  handlers = %s\n", handlers);
      if(key) ADD2LOG("  key = %s\n", key);
      if(rel) ADD2LOG("  rel = %s\n", rel);
      if(abso) ADD2LOG("  abs = %s\n", abso);

      mouse_buttons = 0;
      if(key) {
        for(u = BTN_MOUSE; u < BTN_MOUSE + 8; u++) {
          if(test_bit(key, u)) mouse_buttons++;
        }
      }
      ADD2LOG("  mouse buttons = %u\n", mouse_buttons);

      mouse_wheels = 0;
      if(rel) {
        for(u = REL_HWHEEL; u <= REL_MAX; u++) {
          if(test_bit(rel, u)) mouse_wheels++;
        }
      }
      ADD2LOG("  mouse wheels = %u\n", mouse_wheels);

      if(ok && handlers) {
        handler_list = hd_split(' ', handlers);

        is_mouse = strstr(handlers, "mouse") ? 1 : 0;
        if(	// HP Virtual Management Device
          vendor == 0x03f0 &&
          product == 0x1126 &&
          mouse_buttons >= 3
        ) is_mouse = 1;

        if(bus == BUS_USB) {
          s = NULL;
          for(sl1 = handler_list; sl1; sl1 = sl1->next) {
            if(sscanf(sl1->str, "mouse%u", &u) == 1) {
              str_printf(&s, 0, "/dev/input/mouse%u", u);
              break;
            }
          }
          if(!s && is_mouse) for(sl1 = handler_list; sl1; sl1 = sl1->next) {
            if(sscanf(sl1->str, "event%u", &u) == 1) {
              str_printf(&s, 0, "/dev/input/event%u", u);
              break;
            }
          }

          if(s) {
            for(hd = hd_data->hd; hd; hd = hd->next) {
              if(
                (hd->unix_dev_name2 && !strcmp(hd->unix_dev_name2, s)) ||
                (hd->unix_dev_name && !strcmp(hd->unix_dev_name, s))
              ) {
                if(!hd->base_class.id) {
                  hd->base_class.id = bc_mouse;
                  hd->sub_class.id = sc_mou_usb;
                }
                hd_set_hw_class(hd, hw_mouse);

                hd->compat_vendor.id = MAKE_ID(TAG_SPECIAL, 0x0210);
                hd->compat_device.id = MAKE_ID(TAG_SPECIAL, (mouse_wheels << 4) + mouse_buttons);

                if(hd->unix_dev_name) add_str_list(&hd->unix_dev_names, hd->unix_dev_name);
                if(hd->unix_dev_name2) add_str_list(&hd->unix_dev_names, hd->unix_dev_name2);

                for(sl1 = handler_list; sl1; sl1 = sl1->next) {
                  if(sscanf(sl1->str, "event%u", &u) == 1) {
                    str_printf(&s, 0, "/dev/input/event%u", u);
                    if(!search_str_list(hd->unix_dev_names, s)) add_str_list(&hd->unix_dev_names, s);
                    s = free_mem(s);
                    break;
                  }
                }

                break;
              }
            }
          }

          s = free_mem(s);

        }
        else {
          if(search_str_list(handler_list, "kbd") && test_bit(key, KEY_1)) {
            hd = add_hd_entry(hd_data, __LINE__, 0);
            hd->base_class.id = bc_keyboard;
            hd->sub_class.id = sc_keyboard_kbd;
            hd->bus.id = bus_ps2;

            hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x0211);
            hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0001);
            hd->device.name = new_str(name);

            for(sl1 = handler_list; sl1; sl1 = sl1->next) {
              if(sscanf(sl1->str, "event%u", &u) == 1) {
                str_printf(&hd->unix_dev_name, 0, "/dev/input/event%u", u);
                dev_num.major = 13;
                dev_num.minor = 64 + u;
                hd->unix_dev_num = dev_num;
                break;
              }
            }
          }
          else if(is_mouse) {
            hd = add_hd_entry(hd_data, __LINE__, 0);

            hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x0210);
            hd->device.id = MAKE_ID(TAG_SPECIAL, (mouse_wheels << 4) + mouse_buttons);

            hd->base_class.id = bc_mouse;
            if(bus == BUS_ADB) {
              hd->sub_class.id = sc_mou_bus;
              hd->bus.id = bus_adb;
              hd->compat_vendor.id = hd->vendor.id;
              hd->compat_device.id = hd->device.id;
              hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x0100);
              hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0300);
            }
            else {
              hd->sub_class.id = sc_mou_ps2;
              hd->bus.id = bus_ps2;
            }

            hd->device.name = new_str(name);

            /* Synaptics/Alps TouchPad */
            if(
              vendor == 2 &&
              (product == 7 || product == 8) &&
              test_bit(abso, ABS_X) &&
              test_bit(abso, ABS_Y) &&
              test_bit(abso, ABS_PRESSURE) &&
              test_bit(key, BTN_TOOL_FINGER)
            ) {
              hd->compat_vendor.id = hd->vendor.id;
              hd->compat_device.id = hd->device.id;
              hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x0212);
              hd->device.id = MAKE_ID(TAG_SPECIAL, product - 6);
            }

            hd->unix_dev_name = new_str(DEV_MICE);
            dev_num.major = 13;
            dev_num.minor = 63;
            hd->unix_dev_num = dev_num;

            for(sl1 = handler_list; sl1; sl1 = sl1->next) {
              if(sscanf(sl1->str, "mouse%u", &u) == 1) {
                str_printf(&hd->unix_dev_name2, 0, "/dev/input/mouse%u", u);
                dev_num.major = 13;
                dev_num.minor = 32 + u;
                hd->unix_dev_num2 = dev_num;
                break;
              }
            }

            add_str_list(&hd->unix_dev_names, hd->unix_dev_name);
            add_str_list(&hd->unix_dev_names, hd->unix_dev_name2);

            for(sl1 = handler_list; sl1; sl1 = sl1->next) {
              if(sscanf(sl1->str, "event%u", &u) == 1) {
                s = NULL;
                str_printf(&s, 0, "/dev/input/event%u", u);
                add_str_list(&hd->unix_dev_names, s);
                s = free_mem(s);
                break;
              }
            }
          }
        }

        handler_list = free_str_list(handler_list);
      }

      ok = 0;

      name = free_mem(name);
      handlers = free_mem(handlers);
      key = free_mem(key);
      rel = free_mem(rel);
      abso = free_mem(abso);
    }

    if(sscanf(sl->str, "I: Bus=%04x Vendor=%04x Product=%04x Version=%04x", &bus, &vendor, &product, &version) == 4) {
      ok = 1;
      continue;
    }

    if(!strncmp(sl->str, INP_NAME, sizeof INP_NAME - 1)) {
      s = sl->str + sizeof INP_NAME;
      len = strlen(s);
      if(len > 2) {
        name = canon_str(s, len - 2);
      }
      continue;
    }

    if(!strncmp(sl->str, INP_HANDLERS, sizeof INP_HANDLERS - 1)) {
      s = sl->str + sizeof INP_HANDLERS - 1;
      handlers = canon_str(s, strlen(s));
      continue;
    }

    if(!strncmp(sl->str, INP_KEY, sizeof INP_KEY - 1)) {
      s = sl->str + sizeof INP_KEY - 1;
      key = canon_str(s, strlen(s));
      key = all_bits(key);
      continue;
    }

    if(!strncmp(sl->str, INP_REL, sizeof INP_REL - 1)) {
      s = sl->str + sizeof INP_REL - 1;
      rel = canon_str(s, strlen(s));
      rel = all_bits(rel);
      continue;
    }

    if(!strncmp(sl->str, INP_ABS, sizeof INP_ABS - 1)) {
      s = sl->str + sizeof INP_ABS - 1;
      abso = canon_str(s, strlen(s));
      abso = all_bits(abso);
      continue;
    }
  }

  free_str_list(input);

}


char *all_bits(char *str)
{
  str_list_t *sl, *sl0;
  char *s = NULL;
  unsigned long u;

  if(!str) return NULL;

  sl = sl0 = hd_split(' ', str);
  for(; sl; sl = sl->next) {
    u = strtoul(sl->str, NULL, 16);
    str_printf(&s, -1, "%0*lx", sizeof (unsigned long) * 2, u);
  }
  free_str_list(sl0);
  free_mem(str);

  return s;
}


int test_bit(char *str, unsigned bit)
{
  size_t len, ofs;
  unsigned u, mask;

  if(!str) return 0;

  len = strlen(str);

  ofs = bit >> 2;
  mask = 1 << (bit & 3);

  if(ofs >= len) return 0;

  ofs = len - ofs - 1;

  u = str[ofs] >= 'a' ? str[ofs] - 'a' + 10 : str[ofs] - '0';

  return (u & mask) ? 1 : 0;
}



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

/**
 * @defgroup INPUTint Input devices
 * @ingroup libhdDEVint
 * @brief Input device scan functions
 *
 * @{
 */

static void get_input_devices(hd_data_t *hd_data);
static char *all_bits(char *str);
static int test_bit(const char *str, unsigned bit);
static hd_base_classes_t test_pointers(
    const __u16 *id_bus,
    const unsigned long *bitmask_ev,
    const unsigned long *bitmask_abs,
    const unsigned long *bitmask_key,
    const unsigned long *bitmask_rel,
    const unsigned long *bitmask_props
    );

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

// note: hd_data parameter is needed for ADD2LOG macro
void add_joystick_details(hd_data_t *hd_data, hd_t *h, const char *key, const char *abso)
{
  // replace existing details
  if (h->detail)
  {
    free_hd_detail(h->detail);
  }

  // add buttons and axis details
  h->detail = new_mem(sizeof *h->detail);
  h->detail->type = hd_detail_joystick;

  joystick_t *jt = new_mem(sizeof jt);
  unsigned u;

  if(key) {
    for(u = BTN_JOYSTICK; u < BTN_JOYSTICK + 16; u++) {
      if(test_bit(key, u)) jt->buttons++;
    }
  }
  ADD2LOG("  joystick buttons = %u\n", jt->buttons);

  if(abso) {
    for(u = ABS_X; u < ABS_VOLUME; u++) {
      if(test_bit(abso, u)) jt->axes++;
    }
  }
  ADD2LOG("  joystick axes = %u\n", jt->axes);

  h->detail->joystick.data = jt;
}

#define INP_NAME	"N: Name="
#define INP_HANDLERS	"H: Handlers="
#define INP_KEY		"B: KEY="
#define INP_REL		"B: REL="
#define INP_ABS		"B: ABS="
#define INP_EV    "B: EV="
#define INP_PROP  "B: PROP="
#define INP_SYS   "S: Sysfs="

void get_input_devices(hd_data_t *hd_data)
{
  hd_t *hd;
  str_list_t *input, *sl, *sl1;
  char *s;
  unsigned ok, u, is_mouse, is_joystick,is_touchpad;
  unsigned bus, vendor, product, version;
  unsigned mouse_buttons, mouse_wheels;
  char *name = NULL, *handlers = NULL, *key = NULL, *rel = NULL, *abso = NULL, *ev = NULL, *prop = NULL, *sysfs = NULL;
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
      if (ev) ADD2LOG("  rel = %s\n", rel);
      if (prop) ADD2LOG("  prop = %s\n", prop);
      if (sysfs) {
          ADD2LOG("  sysfs = %s\n", sysfs);
      }

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
        is_joystick = strstr(handlers, "js") ? 1 : 0;
        is_touchpad = test_pointers(bus, ev, abso, key, rel,prop) ? 1 : 0;

        if(	// HP Virtual Management Device
          vendor == 0x03f0 &&
          product == 0x1126 &&
          mouse_buttons >= 3
        ) is_mouse = 1;

        ADD2LOG("  is_mouse = %u\n", is_mouse);
        ADD2LOG("  is_joystick = %u\n", is_joystick);

        if(bus == BUS_USB) {
          s = NULL;
          for(sl1 = handler_list; sl1; sl1 = sl1->next) {
            if(sscanf(sl1->str, "mouse%u", &u) == 1) {
              str_printf(&s, 0, "/dev/input/mouse%u", u);
              break;
            }
          }

          if(!s && (is_mouse || is_joystick|| is_touchpad)) for(sl1 = handler_list; sl1; sl1 = sl1->next) {
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
		  if (is_mouse)
		  {
		    hd->base_class.id = bc_mouse;
		    hd->sub_class.id = sc_mou_usb;
		  }
		  else if (is_joystick)
		  {
		    hd->base_class.id = bc_joystick;
		  }
      else if (is_touchpad)
		  {
		    hd->base_class.id = bc_touchpad;
		  }
                }

		if (is_mouse)
		{
		  hd_set_hw_class(hd, hw_mouse);
                  hd->compat_vendor.id = MAKE_ID(TAG_SPECIAL, 0x0210);
                  hd->compat_device.id = MAKE_ID(TAG_SPECIAL, (mouse_wheels << 4) + mouse_buttons);
		}
		else if (is_joystick)
		{
		  hd_set_hw_class(hd, hw_joystick);

		  /* add buttons and axis details */
		  add_joystick_details(hd_data, hd, key, abso);
		}

                if(hd->unix_dev_name) add_str_list(&hd->unix_dev_names, hd->unix_dev_name);
                if(hd->unix_dev_name2) add_str_list(&hd->unix_dev_names, hd->unix_dev_name2);

                for(sl1 = handler_list; sl1; sl1 = sl1->next) {
                  if(sscanf(sl1->str, "event%u", &u) == 1) {
                    str_printf(&s, 0, "/dev/input/event%u", u);
                    if(!search_str_list(hd->unix_dev_names, s)) add_str_list(&hd->unix_dev_names, s);
                    s = free_mem(s);
                  }
		  /* add /dev/input/jsN joystick device name */
		  else if (is_joystick)
		  {
		    if(sscanf(sl1->str, "js%u", &u) == 1) {
		      str_printf(&s, 0, "/dev/input/js%u", u);
		      if(!search_str_list(hd->unix_dev_names, s)) add_str_list(&hd->unix_dev_names, s);
		      if(!hd->unix_dev_name2) hd->unix_dev_name2 = new_str(s);
		      s = free_mem(s);
		    }
		  }
                }

                break;
              }
            }
          }

          s = free_mem(s);

        }
        else {
	  // keyboard
          if(search_str_list(handler_list, "kbd") && test_bit(key, KEY_1)) {
            hd = add_hd_entry(hd_data, __LINE__, 0);
            hd->base_class.id = bc_keyboard;
            hd->sub_class.id = sc_keyboard_kbd;
            hd->bus.id = bus_ps2;

            hd->vendor.id = MAKE_ID(0, vendor);
            hd->device.id = MAKE_ID(0, product);
            hd->compat_vendor.id = MAKE_ID(TAG_SPECIAL, 0x0211);
            hd->compat_device.id = MAKE_ID(TAG_SPECIAL, 0x0001);
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
	  // mouse
          else if(is_mouse) {
            hd = add_hd_entry(hd_data, __LINE__, 0);

            hd->vendor.id = MAKE_ID(0, vendor);
            hd->device.id = MAKE_ID(0, product);

            hd->compat_vendor.id = MAKE_ID(TAG_SPECIAL, 0x0210);
            hd->compat_device.id = MAKE_ID(TAG_SPECIAL, (mouse_wheels << 4) + mouse_buttons);

            hd->base_class.id = bc_mouse;
                        if(bus == BUS_ADB) {
              hd->sub_class.id = sc_mou_bus;
              hd->bus.id = bus_adb;
            }
            else {
              hd->sub_class.id = sc_mou_ps2;
              hd->bus.id = bus_ps2;
            }

            hd->device.name = new_str(name);

#if 0
            /* Synaptics/Alps TouchPad */
            if(
              vendor == 2 &&
              (product == 7 || product == 8) &&
              test_bit(abso, ABS_X) &&
              test_bit(abso, ABS_Y) &&
              test_bit(abso, ABS_PRESSURE) &&
              test_bit(key, BTN_TOOL_FINGER)
            ) {
            }
#endif

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
            if(sysfs)
              hd->sysfs_id = new_str(sysfs);

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
	  // touchpad
          else if(is_touchpad) {
            hd = add_hd_entry(hd_data, __LINE__, 0);

            hd->vendor.id = MAKE_ID(0, vendor);
            hd->device.id = MAKE_ID(0, product);

            hd->base_class.id = bc_mouse;
            if (bus == BUS_I2C) {
                hd->sub_class.id = sc_mou_bus;
                hd->bus.id = bus_serial;
            }

            hd->device.name = new_str(name);
            if(sysfs)
            hd->sysfs_id = new_str(sysfs);


            for (sl1 = handler_list; sl1; sl1 = sl1->next) {
                if (sscanf(sl1->str, "event%u", &u) == 1) {
                    s = NULL;
                    str_printf(&s, 0, "/dev/input/event%u", u);
                    add_str_list(&hd->unix_dev_names, s);
                    s = free_mem(s);
                    break;
                }
            }
          }
	  // joystick
          else if(is_joystick) {
            hd = add_hd_entry(hd_data, __LINE__, 0);

            hd->vendor.id = MAKE_ID(0, vendor);
            hd->device.id = MAKE_ID(0, product);

	    hd_set_hw_class(hd, hw_joystick);
            hd->base_class.id = bc_joystick;
            hd->device.name = new_str(name);

	    // gameport? (see <linux/input.h>)
	    if (bus == BUS_GAMEPORT) hd->bus.id = bus_gameport;

	    /* add buttons and axis details */
	    add_joystick_details(hd_data, hd, key, abso);

	    // add eventX device
            for(sl1 = handler_list; sl1; sl1 = sl1->next) {
              if(sscanf(sl1->str, "event%u", &u) == 1) {
                str_printf(&hd->unix_dev_name, 0, "/dev/input/event%u", u);
                dev_num.major = 13;
                dev_num.minor = 64 + u;
                hd->unix_dev_num = dev_num;
                break;
              }
            }

	    // add jsX device
            for(sl1 = handler_list; sl1; sl1 = sl1->next) {
              if(sscanf(sl1->str, "js%u", &u) == 1) {
                str_printf(&hd->unix_dev_name2, 0, "/dev/input/js%u", u);
                break;
              }
            }

            add_str_list(&hd->unix_dev_names, hd->unix_dev_name);
            add_str_list(&hd->unix_dev_names, hd->unix_dev_name2);
	  }
	  else
	  {
	    ADD2LOG("unknown non-USB input device\n");
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
      ev = free_mem(ev);
      prop = free_mem(prop);
      sysfs = free_mem(sysfs);
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

    if (!strncmp(sl->str, INP_EV, sizeof INP_EV - 1)) {
        s = sl->str + sizeof INP_EV - 1;
        ev = canon_str(s, strlen(s));
        ev = all_bits(ev);
        continue;
    }

    if (!strncmp(sl->str, INP_PROP, sizeof INP_PROP - 1)) {
        s = sl->str + sizeof INP_PROP - 1;
        prop = canon_str(s, strlen(s));
        prop = all_bits(prop);
        continue;
    }

    if (!strncmp(sl->str, INP_SYS, sizeof INP_SYS - 1)) {
        s = sl->str + sizeof INP_SYS - 1;
        sysfs = canon_str(s, strlen(s));
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
    str_printf(&s, -1, "%0*lx", (int) sizeof (unsigned long) * 2, u);
  }
  free_str_list(sl0);
  free_mem(str);

  return s;
}


int test_bit(const char *str, unsigned bit)
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

#define FALSE 0
#define TRUE 1
#define test2_bit(bit,str)   (test_bit(str,bit) ? TRUE : FALSE)
/* pointer devices */
hd_base_classes_t test_pointers(const __u16 *id_bus,
    const unsigned long *bitmask_ev,
    const unsigned long *bitmask_abs,
    const unsigned long *bitmask_key,
    const unsigned long *bitmask_rel,
    const unsigned long *bitmask_props
    )
{
    unsigned has_abs_coordinates = FALSE;
    unsigned has_rel_coordinates = FALSE;
    unsigned has_mt_coordinates = FALSE;
    unsigned has_joystick_axes_or_buttons = FALSE;
    unsigned has_pad_buttons = FALSE;
    unsigned is_direct = FALSE;
    unsigned has_touch = FALSE;
    unsigned has_3d_coordinates = FALSE;
    unsigned has_keys = FALSE;
    unsigned has_stylus = FALSE;
    unsigned has_pen = FALSE;
    unsigned finger_but_no_pen = FALSE;
    unsigned has_mouse_button = FALSE;
    unsigned is_mouse = FALSE;
    unsigned is_touchpad = FALSE;
    unsigned is_touchscreen = FALSE;
    unsigned is_tablet = FALSE;
    unsigned is_tablet_pad = FALSE;
    unsigned is_joystick = FALSE;
    unsigned is_accelerometer = FALSE;
    unsigned is_pointing_stick = FALSE;

    has_keys = test2_bit(EV_KEY, bitmask_ev);
    has_abs_coordinates = test2_bit(ABS_X, bitmask_abs) && test2_bit(ABS_Y, bitmask_abs);
    has_3d_coordinates = has_abs_coordinates && test2_bit(ABS_Z, bitmask_abs);
    is_accelerometer = test2_bit(INPUT_PROP_ACCELEROMETER, bitmask_props);

    if (!has_keys && has_3d_coordinates)
        is_accelerometer = TRUE;

    if (is_accelerometer) {
        return bc_none;
    }

    is_pointing_stick = test2_bit(INPUT_PROP_POINTING_STICK, bitmask_props);
    has_stylus = test2_bit(BTN_STYLUS, bitmask_key);
    has_pen = test2_bit(BTN_TOOL_PEN, bitmask_key);
    finger_but_no_pen = test2_bit(BTN_TOOL_FINGER, bitmask_key) && !test2_bit(BTN_TOOL_PEN, bitmask_key);
    for (int button = BTN_MOUSE; button < BTN_JOYSTICK && !has_mouse_button; button++)
        has_mouse_button = test2_bit(button, bitmask_key);
    has_rel_coordinates = test2_bit(EV_REL, bitmask_ev) && test2_bit(REL_X, bitmask_rel) && test2_bit(REL_Y, bitmask_rel);
    has_mt_coordinates = test2_bit(ABS_MT_POSITION_X, bitmask_abs) && test2_bit(ABS_MT_POSITION_Y, bitmask_abs);

    /* unset has_mt_coordinates if devices claims to have all abs axis */
    if (has_mt_coordinates && test2_bit(ABS_MT_SLOT, bitmask_abs) && test2_bit(ABS_MT_SLOT - 1, bitmask_abs))
        has_mt_coordinates = FALSE;
    is_direct = test2_bit(INPUT_PROP_DIRECT, bitmask_props);
    has_touch = test2_bit(BTN_TOUCH, bitmask_key);
    has_pad_buttons = test2_bit(BTN_0, bitmask_key) && has_stylus && !has_pen;

    /* joysticks don't necessarily have buttons; e. g.
     * rudders/pedals are joystick-like, but buttonless; they have
     * other fancy axes. Others have buttons only but no axes.
     *
     * The BTN_JOYSTICK range starts after the mouse range, so a mouse
     * with more than 16 buttons runs into the joystick range (e.g. Mad
     * Catz Mad Catz M.M.O.TE). Skip those.
     */
    if (!test2_bit(BTN_JOYSTICK - 1, bitmask_key)) {
        for (int button = BTN_JOYSTICK; button < BTN_DIGI && !has_joystick_axes_or_buttons; button++)
            has_joystick_axes_or_buttons = test2_bit(button, bitmask_key);
        for (int button = BTN_TRIGGER_HAPPY1; button <= BTN_TRIGGER_HAPPY40 && !has_joystick_axes_or_buttons; button++)
            has_joystick_axes_or_buttons = test2_bit(button, bitmask_key);
        for (int button = BTN_DPAD_UP; button <= BTN_DPAD_RIGHT && !has_joystick_axes_or_buttons; button++)
            has_joystick_axes_or_buttons = test2_bit(button, bitmask_key);
    }
    for (int axis = ABS_RX; axis < ABS_PRESSURE && !has_joystick_axes_or_buttons; axis++)
        has_joystick_axes_or_buttons = test2_bit(axis, bitmask_abs);

    if (has_abs_coordinates) {
        if (has_stylus || has_pen)
            is_tablet = TRUE;
        else if (finger_but_no_pen && !is_direct)
            is_touchpad = TRUE;
        else if (has_mouse_button)
            /* This path is taken by VMware's USB mouse, which has
             * absolute axes, but no touch/pressure button. */
            is_mouse = TRUE;
        else if (has_touch || is_direct)
            is_touchscreen = TRUE;
        else if (has_joystick_axes_or_buttons)
            is_joystick = TRUE;
    } else if (has_joystick_axes_or_buttons)
        is_joystick = TRUE;

    if (has_mt_coordinates) {
        if (has_stylus || has_pen)
            is_tablet = TRUE;
        else if (finger_but_no_pen && !is_direct)
            is_touchpad = TRUE;
        else if (has_touch || is_direct)
            is_touchscreen = TRUE;
    }

    if (is_tablet && has_pad_buttons)
        is_tablet_pad = TRUE;

    if (!is_tablet && !is_touchpad && !is_joystick &&
        has_mouse_button &&
        (has_rel_coordinates ||
         !has_abs_coordinates)) /* mouse buttons and no axis */
        is_mouse = TRUE;

    /* There is no such thing as an i2c mouse */
    if (is_mouse && id_bus == BUS_I2C)
        is_pointing_stick = TRUE;

    if (is_touchpad)
        return  bc_touchpad;// printf("ID_INPUT_TOUCHPAD \n");

    return  bc_none;
}
#undef FALSE
#undef TRUE

/** @} */


/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 *
 * type defs for internal data base
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - */

#define DATA_VALUE(a)	((a) & ~(-1 << 28))
#define DATA_FLAG(a)	(((a) >> 28) & 0xf)
#define MAKE_DATA(a, b)	((a << 28) | (b))

#define FLAG_ID		0
#define FLAG_RANGE	1
#define FLAG_MASK	2
#define FLAG_STRING	3
#define FLAG_REGEXP	4
/* 5 - 7 reserved */
#define FLAG_CONT	8	/* bit mask, _must_ be bit 31 */


typedef enum hddb_entry_e {
  he_other, he_bus_id, he_baseclass_id, he_subclass_id, he_progif_id,
  he_vendor_id, he_device_id, he_subvendor_id, he_subdevice_id, he_rev_id,
  he_bus_name, he_baseclass_name, he_subclass_name, he_progif_name,
  he_vendor_name, he_device_name, he_subvendor_name, he_subdevice_name,
  he_rev_name, he_serial, he_driver, he_requires,
  he_detail_ccw_data_cu_model, he_hwclass /* 23 */,

  /* add new entries _here_! */

  he_nomask,

  /* if he_nomask exceeds 31, adjust entry_mask_t & hddb_entry_mask_t */

  he_class_id = he_nomask, he_driver_module_insmod, he_driver_module_modprobe,
  he_driver_module_config, he_driver_xfree, he_driver_xfree_config,
  he_driver_mouse, he_driver_display, he_driver_any
} hddb_entry_t;

static hddb_entry_t hddb_is_numeric[] = {
  he_bus_id, he_baseclass_id, he_subclass_id, he_progif_id, he_vendor_id,
  he_device_id, he_subvendor_id, he_subdevice_id, he_rev_id,
  he_detail_ccw_data_cu_model, he_hwclass
};

static char *hddb_entry_strings[] = {
  "other", "bus.id", "baseclass.id", "subclass.id", "progif.id",
  "vendor.id", "device.id", "subvendor.id", "subdevice.id", "rev.id",
  "bus.name", "baseclass.name", "subclass.name", "progif.name",
  "vendor.name", "device.name", "subvendor.name", "subdevice.name",
  "rev.name", "serial", "driver", "requires",
  "detail.ccw.data.cu_model", "hwclass",
  "class.id", "driver.module.insmod", "driver.module.modprobe",
  "driver.module.config", "driver.xfree", "driver.xfree.config",
  "driver.mouse", "driver.display", "driver.any"
};


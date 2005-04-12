#ifndef ISDN_CDB_DEF
#define ISDN_CDB_DEF

enum {
	vendor,
	device,
	vendor_id,
	device_id,
	subvendor_id,
	subdevice_id,
	revision,
	device_class,
	bus_type,
	vario,
	SMP,
	drv_id,
	drv_subtyp,
	drv_typ,
	interface,
	line_cnt,
	line_protocol,
	module,
	need_packages,
	supported,
	feature,
	info,
	special,
	firmware,
	short_description,
	IRQ,
	IO,
	MEMBASE,
	alternative_name,
};

extern void add_current_item(int, char *);
extern int new_entry(void);

#endif /* ISDN_CDB_DEF */

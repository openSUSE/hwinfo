void init_hddb(hd_data_t *hd_data);

void add_device_name(hd_data_t *hd_data, unsigned vendor, unsigned device, char *name);
void add_sub_device_name(hd_data_t *hd_data, unsigned vendor, unsigned device, unsigned sub_vendor, unsigned sub_device, char *name);

unsigned device_class(hd_data_t *hd_data, unsigned vendor, unsigned device);
unsigned sub_device_class(hd_data_t *hd_data, unsigned vendor, unsigned device, unsigned sub_vendor, unsigned sub_device);

driver_info_t *device_driver(hd_data_t *hd_data, unsigned vendor, unsigned device);
driver_info_t *sub_device_driver(hd_data_t *hd_data, unsigned vendor, unsigned device, unsigned sub_vendor, unsigned sub_device);

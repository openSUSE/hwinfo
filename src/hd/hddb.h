void hddb_init(hd_data_t *hd_data);

unsigned device_class(hd_data_t *hd_data, unsigned vendor, unsigned device);
unsigned sub_device_class(hd_data_t *hd_data, unsigned vendor, unsigned device, unsigned sub_vendor, unsigned sub_device);

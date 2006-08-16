void hd_scan_hal(hd_data_t *hd_data);
void hd_scan_hal_basic(hd_data_t *hd_data);
void hd_scan_hal_assign_udi(hd_data_t *hd_data);

const char* get_sysfs_attr(const char* bus, const char* device, const char* attr);

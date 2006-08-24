#include <dirent.h>

void hd_scan_hal(hd_data_t *hd_data);
void hd_scan_hal_basic(hd_data_t *hd_data);
void hd_scan_hal_assign_udi(hd_data_t *hd_data);

char* get_sysfs_attr(const char* bus, const char* device, const char* attr);
char* get_sysfs_attr_by_path(const char* path, const char* attr);
DIR* open_sys_bus_devices(const char* bus);
char* get_sysfs_path(const char* bus, const char* device);

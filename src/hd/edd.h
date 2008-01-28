#if defined(__i386__) || defined(__x86_64__)
void hd_scan_sysfs_edd(hd_data_t *hd_data);
unsigned edd_disk_signature(hd_t *hd);
void assign_edd_info(hd_data_t *hd_data);
#endif

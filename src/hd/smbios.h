hd_smbios_t *smbios_free(hd_smbios_t *sm);
hd_smbios_t *smbios_add_entry(hd_smbios_t **sm, hd_smbios_t *new_sm);
void smbios_dump(hd_data_t *hd_data, FILE *f);
void smbios_parse(hd_data_t *hd_data);

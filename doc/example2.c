#include <stdio.h>
#include <stdlib.h>

#include <hd.h>

int main(int argc, char **argv)
{
  hd_data_t *hd_data;
  hd_t *hd;
  unsigned display_idx;

  hd_data = calloc(1, sizeof *hd_data);

  hd = hd_list(hd_data, hw_display, 1, NULL);
  display_idx = hd_display_adapter(hd_data);

  hd_dump_entry(hd_data, hd_get_device_by_idx(hd_data, display_idx), stdout)

  hd_free_hd_list(hd);
  hd_free_hd_data(hd_data);

  free(hd_data);

  return 0;
}


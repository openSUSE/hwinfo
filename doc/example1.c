#include <stdio.h>
#include <stdlib.h>

#include <hd.h>

int main(int argc, char **argv)
{
  hd_data_t *hd_data;
  hd_t *hd;

  hd_data = calloc(1, sizeof *hd_data);

  hd = hd_list(hd_data, hw_scsi, 1, NULL);

  for(; hd; hd = hd->next) {
    hd_dump_entry(hd_data, hd, stdout)
  }

  hd_free_hd_list(hd);		/* free it */
  hd_free_hd_data(hd_data);

  free(hd_data);

  return 0;
}


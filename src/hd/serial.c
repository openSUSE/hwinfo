#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "hd.h"
#include "hd_int.h"
#include "serial.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * serial interface
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


static void get_serial_info(hd_data_t *hd_data);
static serial_t *add_serial_entry(serial_t **ser, serial_t *new_ser);
static void dump_serial_data(hd_data_t *hd_data);

void hd_scan_serial(hd_data_t *hd_data)
{
  hd_t *hd;
  serial_t *ser;
  hd_res_t *res;

  if(!hd_probe_feature(hd_data, pr_serial)) return;

  hd_data->module = mod_serial;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->serial = NULL;

  PROGRESS(1, 0, "read info");

  get_serial_info(hd_data);
  if((hd_data->debug & HD_DEB_SERIAL)) dump_serial_data(hd_data);

  PROGRESS(2, 0, "build list");

  for(ser = hd_data->serial; ser; ser = ser->next) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class = bc_comm;
    hd->sub_class = sc_com_ser;
    hd->dev_name = new_str(ser->name);
    hd->func = ser->line;
    str_printf(&hd->unix_dev_name, 0, "/dev/ttyS%u", ser->line);
    if(ser->baud) {
      res = add_res_entry(&hd->res, new_mem(sizeof *res));
      res->baud.type = res_baud;
      res->baud.speed = ser->baud;
    }
    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->io.type = res_io;
    res->io.enabled = 1;
    res->io.base = ser->port;
    res->io.access = acc_rw;

    res = add_res_entry(&hd->res, new_mem(sizeof *res));
    res->irq.type = res_irq;
    res->irq.enabled = 1;
    res->irq.base = ser->irq;
  }  
}

void get_serial_info(hd_data_t *hd_data)
{
  char buf[32];
  unsigned u0, u1, u2, u3;
  int i;
  str_list_t *sl, *sl0;
  serial_t *ser;

  /*
   * Max. 44 serial lines at the moment; the serial proc interface is
   * somewhat buggy at the moment (2.2.13), hence the explicit 44 lines
   * limit. That may be dropped later.
   */
  if(!(sl0 = read_file(PROC_DRIVER_SERIAL, 1, 44))) return;

  for(sl = sl0; sl; sl = sl->next) {
    i = 0;
    if(
      sscanf(sl->str, "%u: uart:%31s port:%x irq:%u baud:%u", &u0, buf, &u1, &u2, &u3) == 5 ||
      (i = 1, sscanf(sl->str, "%u: uart:%31s port:%x irq:%u tx:%*u", &u0, buf, &u1, &u2) == 5)
    ) {
      /*
       * The 'baud' or 'tx' entries are only present for real interfaces.
       */
      ser = add_serial_entry(&hd_data->serial, new_mem(sizeof *ser));
      ser->line = u0;
      ser->port = u1;
      ser->irq = u2;
      if(!i) ser->baud = u3;
      ser->name = new_str(buf);
    }
  }

  if((hd_data->debug & HD_DEB_SERIAL)) {
    /* log just the first 16 entries */
    ADD2LOG("----- /proc/tty/driver/serial -----\n");
    for(sl = sl0, i = 16; sl && i--; sl = sl->next) {
      ADD2LOG("  %s", sl->str);
    }
    ADD2LOG("----- /proc/tty/driver/serial end -----\n");
  }

  free_str_list(sl0);
}

serial_t *add_serial_entry(serial_t **ser, serial_t *new_ser)
{
  while(*ser) ser = &(*ser)->next;
  return *ser = new_ser;
}

void dump_serial_data(hd_data_t *hd_data)
{
  serial_t *ser;

  ADD2LOG("----- serial info -----\n");
  for(ser = hd_data->serial; ser; ser = ser->next) {
    ADD2LOG(
      "  uart %s, line %d, port 0x%03x, irq %d, baud %d\n",
      ser->name, ser->line, ser->port, ser->irq, ser->baud
    );
  }
  ADD2LOG("----- serial info end -----\n");
}


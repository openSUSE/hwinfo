#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <linux/iso_fs.h>

#include "hd.h"
#include "hd_int.h"
#include "cdrom.h"

/* - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 * special CDROM scanning
 *
 * Although CDROM devices have already been handled by the normal scan
 * functions, this goes through the kernel info file
 * /proc/sys/dev/cdrom/info just to check that we didn't miss one.
 *
 * The main purpose however is that you can just call this function to get
 * CDROM drives added to the hardware list *without* going through a full
 * scan again.
 *
 * This is particularly handy if you just loaded a module and want to know
 * just if this has activated a CDROM drive.
 *
 * AND, if requested, it will look for CDs in those drives. (And read the
 * disk label.)
 *
 * - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - - -
 */


static void read_cdroms(hd_data_t *hd_data);
static cdrom_info_t *new_cdrom_entry(cdrom_info_t **ci);

void hd_scan_cdrom(hd_data_t *hd_data)
{
  int found;
  hd_t *hd;
  cdrom_info_t *ci, **prev, *next;

  if(!hd_probe_feature(hd_data, pr_cdrom)) return;

  hd_data->module = mod_cdrom;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->cdrom = NULL;

  PROGRESS(1, 0, "get devices");

  read_cdroms(hd_data);

  PROGRESS(2, 0, "build list");

  for(hd = hd_data->hd; hd; hd = hd->next) {
    /* look for existing entries... */
    if(
      hd->base_class.id == bc_storage_device &&
      hd->sub_class.id == sc_sdev_cdrom &&
      hd->unix_dev_name
    ) {
      found = 0;
      for(ci = *(prev = &hd_data->cdrom); ci; ci = *(prev = &ci->next)) {
        /* ...and remove those from the CDROM list */
        if(!strcmp(hd->unix_dev_name, ci->name)) {
          hd->detail = free_hd_detail(hd->detail);
          hd->detail = new_mem(sizeof *hd->detail);
          hd->detail->type = hd_detail_cdrom;
          hd->detail->cdrom.data = ci;
          *prev = ci->next;
          ci = *prev;
          hd->detail->cdrom.data->next = NULL;
          found = 1;
          break;
        }
      }
      /* Oops, we didn't find our entries in the 'official' kernel list? */
      if(!found && (hd_data->debug & HD_DEB_CDROM)) {
        ADD2LOG("CD drive \"%s\" not in kernel CD list\n", hd->unix_dev_name);
      }
    }
  }

  /*
   * everything still in hd_data->cdrom are new entries
   */
  for(ci = hd_data->cdrom; ci; ci = next) {
    next = ci->next;
    ci->next = NULL;
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class.id = bc_storage_device;
    hd->sub_class.id = sc_sdev_cdrom;
    hd->unix_dev_name = new_str(ci->name);
    hd->bus.id = bus_none;
    hd->detail = free_hd_detail(hd->detail);
    hd->detail = new_mem(sizeof *hd->detail);
    hd->detail->type = hd_detail_cdrom;
    hd->detail->cdrom.data = ci;
  }

  hd_data->cdrom = NULL;

  /* update prog_if: cdr, cdrw, ... */
  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_storage_device &&
      hd->sub_class.id == sc_sdev_cdrom &&
      hd->detail &&
      hd->detail->type == hd_detail_cdrom
    ) {
      ci = hd->detail->cdrom.data;

      if(
        ci->name &&
        ci->name[5] == 's' &&
        ci->name[6] == 'r' &&
        search_str_list(hd->drivers, "ide-scsi")	/* could be ide, though */
      ) {	/* "/dev/sr..." */
        /* scsi devs lie */
        if(ci->dvd && (ci->cdrw || ci->dvdr || ci->dvdram)) {
          ci->dvd = ci->dvdr = ci->dvdram = 0;
        }
        ci->dvdr = ci->dvdram = 0;
        ci->cdr = ci->cdrw = 0;
        if(hd->prog_if.id == pif_cdr) ci->cdr = 1;
      }

      /* trust ide info */
      if(ci->dvd) {
        hd->is.dvd = 1;
        hd->prog_if.id = pif_dvd;
      }
      if(ci->cdr) {
        hd->is.cdr = 1;
        hd->prog_if.id = pif_cdr;
      }
      if(ci->cdrw) {
        hd->is.cdrw = 1;
        hd->prog_if.id = pif_cdrw;
      }
      if(ci->dvdr) {
        hd->is.dvdr = 1;
        hd->prog_if.id = pif_dvdr;
      }
      if(ci->dvdram) {
        hd->is.dvdram = 1;
        hd->prog_if.id = pif_dvdram;
      }
    }
  }
}


/*
 * Read CD data and get ISO9660 info.
 */
void hd_scan_cdrom2(hd_data_t *hd_data)
{
  hd_t *hd;
  int i;

  if(!hd_probe_feature(hd_data, pr_cdrom)) return;

  hd_data->module = mod_cdrom;

  /*
   * look for a CD and get some info
   */
  if(!hd_probe_feature(hd_data, pr_cdrom_info)) return;

  for(i = 0, hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class.id == bc_storage_device &&
      hd->sub_class.id == sc_sdev_cdrom &&
      hd->status.available != status_no &&
      hd->unix_dev_name
    ) {
      PROGRESS(3, ++i, "read cdrom");
      hd_read_cdrom_info(hd_data, hd);
    }
  }
}


/* return nth entry */
cdrom_info_t *get_cdrom_entry(cdrom_info_t *ci, int n)
{
  if(n < 0) return NULL;

  while(n--) {
    if(!ci) return NULL;
    ci = ci->next;
  }

  return ci;
}

/*
 * Read the list of CDROM devices known to the kernel. The info is taken
 * from /proc/sys/dev/cdrom/info.
 */
void read_cdroms(hd_data_t *hd_data)
{
  char *s, *t, *v;
  str_list_t *sl, *sl0;
  cdrom_info_t *ci;
  int i, line, entries = 0;
  unsigned val;

  if(!(sl0 = read_file(PROC_CDROM_INFO, 2, 0))) return;

  if((hd_data->debug & HD_DEB_CDROM)) {
    ADD2LOG("----- "PROC_CDROM_INFO" -----\n");
    for(sl = sl0; sl; sl = sl->next) {
      ADD2LOG("  %s", sl->str);
    }
    ADD2LOG("----- "PROC_CDROM_INFO" end -----\n");
  }

  for(sl = sl0; sl; sl = sl->next) {
    if(
      (line = 0, strstr(sl->str, "drive name:") == sl->str) ||
      (line++, strstr(sl->str, "drive speed:") == sl->str) ||
      (line++, strstr(sl->str, "Can write CD-R:") == sl->str) ||
      (line++, strstr(sl->str, "Can write CD-RW:") == sl->str) ||
      (line++, strstr(sl->str, "Can read DVD:") == sl->str) ||
      (line++, strstr(sl->str, "Can write DVD-R:") == sl->str) ||
      (line++, strstr(sl->str, "Can write DVD-RAM:") == sl->str)
    ) {
      s = strchr(sl->str, ':') + 1;
      i = 0;
      while((t = strsep(&s, " \t\n"))) {
        if(!*t) continue;
        i++;
        switch(line) {
          case 0:	/* drive name */
            ci = new_cdrom_entry(&hd_data->cdrom);
            entries++;
            v = new_mem(sizeof "/dev/" + strlen(t));
            strcat(strcpy(v, "/dev/"), t);
            ci->name = v;
            break;

          case 1:	/* drive speed */
          case 2:	/* Can write CD-R */
          case 3:	/* Can write CD-RW */
          case 4:	/* Can read DVD */
          case 5:	/* Can write DVD-R */
          case 6:	/* Can write DVD-RAM */
            ci = get_cdrom_entry(hd_data->cdrom, entries - i);
            if(ci) {
              val = strtoul(t, &v, 10);
              if(!*v) {
                switch(line) {
                  case 1:
                    ci->speed = val;
                    break;
                  case 2:
                    ci->cdr = val;
                    break;
                  case 3:
                    ci->cdrw = val;
                    break;
                  case 4:
                    ci->dvd = val;
                    break;
                  case 5:
                    ci->dvdr = val;
                    break;
                  case 6:
                    ci->dvdram = val;
                    break;
                }
              }
            }
            break;
        }
      }
    }  
  }

  free_str_list(sl0);
}


/* add new entries at the _start_ of the list */
cdrom_info_t *new_cdrom_entry(cdrom_info_t **ci)
{
  cdrom_info_t *new_ci = new_mem(sizeof *new_ci);

  new_ci->next = *ci;
  return *ci = new_ci;
}



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
 * Although CDROM devices hasve already been handled by the normal scan
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
static void dump_cdrom_data(hd_data_t *hd_data);

void hd_scan_cdrom(hd_data_t *hd_data)
{
  int found, prog_cnt = 0;
  hd_t *hd;
  str_list_t *sl, **prev;

  if(!(hd_data->probe & (1 << pr_cdrom))) return;

  hd_data->module = mod_cdrom;

  /* some clean-up */
  remove_hd_entries(hd_data);
  hd_data->cdrom = NULL;

  PROGRESS(1, 0, "get devices");

  read_cdroms(hd_data);
  if((hd_data->debug & HD_DEB_CDROM)) dump_cdrom_data(hd_data);

  PROGRESS(2, 0, "build list");

  for(hd = hd_data->hd; hd; hd = hd->next) {
    /* look for existing entries... */
    if(
      hd->base_class == bc_storage_device &&
      hd->sub_class == sc_sdev_cdrom
    ) {
      found = 0;
      for(sl = *(prev = &hd_data->cdrom); sl; sl = *(prev = &sl->next)) {
        /* ...and remove those from the CDROM list */
        if(sl->str && !strcmp(hd->unix_dev_name, sl->str)) {
          sl->str = free_mem(sl->str);
          *prev = sl->next;
          free_mem(sl);
          sl = *prev;
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
  for(sl = hd_data->cdrom; sl; sl = sl->next) {
    hd = add_hd_entry(hd_data, __LINE__, 0);
    hd->base_class = bc_storage_device;
    hd->sub_class = sc_sdev_cdrom;
    hd->unix_dev_name = new_str(sl->str);
    hd->bus = bus_none;
  }

  /*
   * look for a CD and get some info
   */
  if(!(hd_data->probe & (1 << pr_cdrom_info))) return;

  for(hd = hd_data->hd; hd; hd = hd->next) {
    if(
      hd->base_class == bc_storage_device &&
      hd->sub_class == sc_sdev_cdrom &&
      hd->unix_dev_name
    ) {
      PROGRESS(3, ++prog_cnt, "read cdrom");
      hd_read_cdrom_info(hd);
    }
  }
}


/*
 * Read the CDROM info, if there is a CD inserted.
 * returns NULL if nothing was found
 */
cdrom_info_t *hd_read_cdrom_info(hd_t *hd)
{
  int fd;
  char *s;
  cdrom_info_t *ci;
  struct iso_primary_descriptor iso_desc;

  /* free existing entry */
  if(hd->detail) {
    if(hd->detail->type == hd_detail_cdrom) {
      ci = hd->detail->cdrom.data;
      if(ci->volume) free_mem(ci->volume);
      if(ci->publisher) free_mem(ci->publisher);
      if(ci->preparer) free_mem(ci->preparer);
      if(ci->application) free_mem(ci->application);
      if(ci->creation_date) free_mem(ci->creation_date);
      free_mem(ci);
    }
    hd->detail = free_mem(hd->detail);
  }

  if((fd = open(hd->unix_dev_name, O_RDONLY)) < 0) {
    /* we are here if there is no CD in the drive */
    return NULL;
  }

  if(
    lseek(fd, 0x8000, SEEK_SET) >= 0 &&
    read(fd, &iso_desc, sizeof iso_desc) == sizeof iso_desc
  ) {
    hd->detail = new_mem(sizeof *hd->detail);
    hd->detail->type = hd_detail_cdrom;
    hd->detail->cdrom.data = ci = new_mem(sizeof *ci);

    /* now, fill in the fields */
    s = canon_str(iso_desc.volume_id, sizeof iso_desc.volume_id);
    if(!*s) s = free_mem(s);
    ci->volume = s;

    s = canon_str(iso_desc.publisher_id, sizeof iso_desc.publisher_id);
    if(!*s) s = free_mem(s);
    ci->publisher = s;

    s = canon_str(iso_desc.preparer_id, sizeof iso_desc.preparer_id);
    if(!*s) s = free_mem(s);
    ci->preparer = s;

    s = canon_str(iso_desc.application_id, sizeof iso_desc.application_id);
    if(!*s) s = free_mem(s);
    ci->application = s;

    s = canon_str(iso_desc.creation_date, sizeof iso_desc.creation_date);
    if(!*s) s = free_mem(s);
    ci->creation_date = s;
  }
  else {
    ci = NULL;
  }

  close(fd);

  return ci;
}


/*
 * Read the list of CDROM devices known to the kernel. The info is taken
 * from /proc/sys/dev/cdrom/info.
 */
void read_cdroms(hd_data_t *hd_data)
{
  char *s, *t, *v;
  str_list_t *sl, *sl0, *sl1;

  if(!(sl0 = read_file(PROC_CDROM_INFO, 2, 1))) return;

  for(sl = sl0; sl; sl = sl->next) {
    if(strstr(sl->str, "drive name:") == sl->str) {
      s = sl->str + sizeof "drive name:" - 1;
      while((t = strsep(&s, " \t\n"))) {
        if(!*t) continue;
        v = new_mem(sizeof "/dev/" + strlen(t));
        strcat(strcpy(v, "/dev/"), t);
        add_str_list(&hd_data->cdrom, v);
      }
      break;
    }  
  }
  free_str_list(sl0);

  /*
   * reverse the list order (cf. PROC_PCI_DEVICES)
   */
  for(sl0 = NULL, sl = hd_data->cdrom; sl; sl = sl->next) {
    if(sl->str) {
      sl1 = new_mem(sizeof *sl1);
      sl1->str = sl->str;
      sl->str = NULL;
      sl1->next = sl0;
      sl0 = sl1;
    }
  }
  free_str_list(hd_data->cdrom);
  hd_data->cdrom = sl0;
}


/*
 * Add some CDROM data to the global log.
 */
void dump_cdrom_data(hd_data_t *hd_data)
{
  str_list_t *sl;

  ADD2LOG("----- kernel CDROM list -----\n");
  for(sl = hd_data->cdrom; sl; sl = sl->next) {
    ADD2LOG("  %s\n", sl->str);
  }
  ADD2LOG("----- kernel CDROM list end -----\n");
}



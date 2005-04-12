#include <fcntl.h>
#include <stdio.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>

#include "hd.h"
#include "hd_int.h"
#include "sbus.h"

#ifdef __sparc__

#ifdef DIET
typedef unsigned int u_int;
#endif

#include <asm/openpromio.h>

#define MAX_VAL (4096-128-4)

static int prom_fd;

static int
prom_nextnode (int node)
{
  char buf[OPROMMAXPARAM];
  struct openpromio *op = (struct openpromio *)buf;

  op->oprom_size = sizeof (int);
  if (node == -1)
    return 0;

  *(int *)op->oprom_array = node;
  if (ioctl (prom_fd, OPROMNEXT, op) < 0)
    return 0;

  return *(int *)op->oprom_array;
}

static int
prom_getchild (int node)
{
  char buf[OPROMMAXPARAM];
  struct openpromio *op = (struct openpromio *)buf;

  op->oprom_size = sizeof (int);

  if (!node || node == -1)
    return 0;

  *(int *)op->oprom_array = node;
  if (ioctl (prom_fd, OPROMCHILD, op) < 0)
    return 0;

  return *(int *)op->oprom_array;
}

static char
*prom_getproperty (char *prop, int *lenp, char *buf)
{
  struct openpromio *op = (struct openpromio *)buf;

  op->oprom_size = MAX_VAL;

  strcpy (op->oprom_array, prop);

  if (ioctl (prom_fd, OPROMGETPROP, op) < 0)
    return 0;

  if (lenp)
    *lenp = op->oprom_size;

  if (strncmp ("SUNW,", op->oprom_array, 5) == 0)
    return op->oprom_array + 5;
  else
    return op->oprom_array;
}

static int
prom_getbool (char *prop)
{
  char buf[OPROMMAXPARAM];
  struct openpromio *op = (struct openpromio *)buf;

  op->oprom_size = 0;

  *(int *)op->oprom_array = 0;
  for (;;) {
    op->oprom_size = 128; /* MAX_PROP */
    if (ioctl (prom_fd, OPROMNXTPROP, op) < 0)
      return 0;
    if (!op->oprom_size)
      return 0;
    if (!strcmp (op->oprom_array, prop))
      return 1;
  }
}

static void
prom_parse (int node, int sbus, int ebus, hd_data_t *hd_data)
{
  hd_t *hd;
  char buf1[OPROMMAXPARAM], buf2[OPROMMAXPARAM];
  int nextnode;
  int len, nsbus = sbus, nebus = ebus;
  char *prop1 = prom_getproperty ("device_type", &len, buf1);

  if (strcmp (prop1, "network") == 0)
    {
      char *prop2 = prom_getproperty ("name", &len, buf2);
      if (prop2 && len >= 0)
	{
	  if (strcmp (prop2, "hme") == 0)
	    {
	      ADD2LOG ("NETWORK: type=Sun Happy Meal Ethernet, module=sunhme\n");
	      hd = add_hd_entry (hd_data, __LINE__, 0);
	      hd->base_class.id = bc_network;
	      hd->sub_class.id = 0x00;
	      hd->bus.id = bus_sbus;

	      hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
	      hd->device.id = MAKE_ID(TAG_SPECIAL, 0x3001);
	    }
	  else if (strcmp (prop2, "le") == 0)
	    {
	      ADD2LOG ("NETWORK: type=Sun Lance Ethernet, module=ignore\n");
	      hd = add_hd_entry (hd_data, __LINE__, 0);
	      hd->base_class.id = bc_network;
	      hd->sub_class.id = 0x00;
	      hd->bus.id = bus_sbus;

	      hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
	      hd->device.id = MAKE_ID(TAG_SPECIAL, 0x3002);
	    }
	  else if (strcmp (prop2, "qe") == 0)
	    {
	      prop2 = prom_getproperty("channel#", &len, buf2);
	      if (prop2 && len == 4 && *(int *)prop2 == 0)
		{
		  ADD2LOG ("NETWORK: type=Sun Quad Ethernet, module=sunqe\n");
		  hd = add_hd_entry (hd_data, __LINE__, 0);
		  hd->base_class.id = bc_network;
		  hd->sub_class.id = 0x00;
		  hd->bus.id = bus_sbus;

		  hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
		  hd->device.id = MAKE_ID(TAG_SPECIAL, 0x3003);
		}
	    }
          else if (strcmp (prop2, "qfe") == 0)
            {
              ADD2LOG ("NETWORK: type=Sun Quad Ethernet (qfe), module=sunhme\n");
              hd = add_hd_entry (hd_data, __LINE__, 0);
              hd->base_class.id = bc_network;
              hd->sub_class.id = 0x00;
              hd->bus.id = bus_sbus;

              hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
              hd->device.id = MAKE_ID(TAG_SPECIAL, 0x3001);
            }
	  else if (strcmp (prop2, "mlanai") == 0 || strcmp (prop2, "myri") == 0)
	    {
	      ADD2LOG ("NETWORK: type=MyriCOM MyriNET Gigabit Ethernet, module=myri_sbus\n");
	      hd = add_hd_entry (hd_data, __LINE__, 0);
	      hd->base_class.id = bc_network;
	      hd->sub_class.id = 0x00;
	      hd->bus.id = bus_sbus;

	      hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
	      hd->device.id = MAKE_ID(TAG_SPECIAL, 0x3004);
	    }
	  else
	    ADD2LOG ("NETWORK: Unknown device \"%s\"\n", prop2);
	}
    }
  else if (strcmp (prop1, "scsi") == 0)
    {
      char *prop2 = prom_getproperty ("name", &len, buf2);
      if (prop2 && len >= 0)
	{
	  if (sbus)
	    {
	      if (strcmp (prop2, "esp") == 0)
		{
		  ADD2LOG ("SCSI: type=Sun Enhanced SCSI Processor (ESP), module=ignore\n");
		  hd = add_hd_entry (hd_data, __LINE__, 0);
		  hd->base_class.id = bc_storage;
		  hd->sub_class.id = sc_sto_scsi;
		  hd->bus.id = bus_sbus;

		  hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
		  hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1001);
		}
	      else if (strcmp (prop2, "soc") == 0)
		{
		  ADD2LOG ("SCSI: type=Sun SPARCStorage Array, module=fc4:soc:pluto\n");
		  hd = add_hd_entry (hd_data, __LINE__, 0);
		  hd->base_class.id = bc_storage;
		  hd->sub_class.id = sc_sto_scsi;
		  hd->bus.id = bus_sbus;

		  hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
		  hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1101);
		}
	      else if (strcmp (prop2, "socal") == 0)
		{
		  ADD2LOG ("SCSI: type=Sun Enterprise Network Array, module=fc4:socal:fcal\n");
		  hd = add_hd_entry (hd_data, __LINE__, 0);
		  hd->base_class.id = bc_storage;
		  hd->sub_class.id = sc_sto_scsi;
		  hd->bus.id = bus_sbus;

		  hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
		  hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1102);
		}
	      else if (strcmp (prop2, "fas") == 0)
		{
		  ADD2LOG ("SCSI: type=Sun Swift (ESP), module=ignore\n");
		  hd = add_hd_entry (hd_data, __LINE__, 0);
		  hd->base_class.id = bc_storage;
		  hd->sub_class.id = sc_sto_scsi;
		  hd->bus.id = bus_sbus;

		  hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
		  hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1002);
		}
	      else if (strcmp (prop2, "ptisp") == 0)
		{
		  ADD2LOG ("SCSI: type=Performance Technologies ISP, module=qlogicpti\n");
		  hd = add_hd_entry (hd_data, __LINE__, 0);
		  hd->base_class.id = bc_storage;
		  hd->sub_class.id = sc_sto_scsi;
		  hd->bus.id = bus_sbus;

		  hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
		  hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1003);
		}
	      else if (strcmp (prop2, "isp") == 0)
		{
		  ADD2LOG ("SCSI: type=QLogic ISP, module=qlogicpti\n");
		  hd = add_hd_entry (hd_data, __LINE__, 0);
		  hd->base_class.id = bc_storage;
		  hd->sub_class.id = sc_sto_scsi;
		  hd->bus.id = bus_sbus;

		  hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
		  hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1004);
		}
	      else
		ADD2LOG ("SCSI: Unknown SBUS device \"%s\"\n", prop2);
	    }
	  else
	    ADD2LOG ("SCSI: Unknown device \"%s\"\n", prop2);
	}
    }
  else if (strcmp (prop1, "display") == 0)
    {
      char *prop2 = prom_getproperty ("name", &len, buf2);
      if (prop2 && len >= 0)
	if (sbus || strcmp (prop2, "ffb") == 0 ||
	    strcmp (prop2, "afb") == 0 || strcmp (prop2, "cgfourteen") == 0)
	  {
	    if (strcmp (prop2, "bwtwo") == 0)
	      {
		ADD2LOG ("DISPLAY: Sun|Monochrome (bwtwo), depth=1\n");
		hd = add_hd_entry (hd_data, __LINE__, 0);
		hd->base_class.id = bc_display;
		hd->sub_class.id = sc_dis_vga;
		hd->bus.id = bus_sbus;

		hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
		hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0001);
	      }
	    else if (strcmp (prop2, "cgthree") == 0)
	      {
		ADD2LOG ("DISPLAY: Sun|Color3 (cgthree), depth=8\n");
		hd = add_hd_entry (hd_data, __LINE__, 0);
		hd->base_class.id = bc_display;
		hd->sub_class.id = sc_dis_vga;
		hd->bus.id = bus_sbus;

		hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
		hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0002);
	      }
	    else if (strcmp (prop2, "cgeight") == 0)
	      {
		ADD2LOG ("DISPLAY: Sun|CG8/RasterOps, depth=8\n");
		hd = add_hd_entry (hd_data, __LINE__, 0);
		hd->base_class.id = bc_display;
		hd->sub_class.id = sc_dis_vga;
		hd->bus.id = bus_sbus;

		hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
		hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0003);
	      }
	    else if (strcmp (prop2, "cgtwelve") == 0)
	      {
		ADD2LOG ("DISPLAY: Sun|GS (cgtwelve), depth=24\n");
		hd = add_hd_entry (hd_data, __LINE__, 0);
		hd->base_class.id = bc_display;
		hd->sub_class.id = sc_dis_vga;
		hd->bus.id = bus_sbus;

		hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
		hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0004);
	      }
	    else if (strcmp (prop2, "gt") == 0)
	      {
		ADD2LOG ("DISPLAY: Sun|Graphics Tower (gt), depth=24\n");
		hd = add_hd_entry (hd_data, __LINE__, 0);
		hd->base_class.id = bc_display;
		hd->sub_class.id = sc_dis_vga;
		hd->bus.id = bus_sbus;

		hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
		hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0005);
	      }
	    else if (strcmp (prop2, "mgx") == 0)
	      {
		hd = add_hd_entry (hd_data, __LINE__, 0);
		hd->base_class.id = bc_display;
		hd->sub_class.id = sc_dis_vga;
		hd->bus.id = bus_sbus;
		hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
		prop2 = prom_getproperty ("fb_size", &len, buf2);
		if (prop2 && len == 4 && *(int *)prop2 == 0x400000)
		  {
		    ADD2LOG ("DISPLAY: Quantum 3D MGXplus with 4M VRAM (mgx), depth=24\n");
		    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0006);
		  }
		else
		  {
		    ADD2LOG ("DISPLAY: Quantum 3D MGXplus (mgx), depth=24\n");
		    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0007);
		  }
	      }
	    else if (strcmp (prop2, "cgsix") == 0)
	      {
		int chiprev = 0;
		int vmsize = 0;

		hd = add_hd_entry (hd_data, __LINE__, 0);
		hd->base_class.id = bc_display;
		hd->sub_class.id = sc_dis_vga;
		hd->bus.id = bus_sbus;
		hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);

		prop2 = prom_getproperty("chiprev", &len, buf2);
		if (prop2 && len == 4)
		  chiprev = *(int *)prop2;
		prop2 = prom_getproperty("vmsize", &len, buf2);
		if (prop2 && len == 4)
		  vmsize = *(int *)prop2;
		switch (chiprev)
		  {
		  case 1 ... 4:
		    ADD2LOG ("DISPLAY: Sun|Double width GX (cgsix), depth=8\n");
		    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0008);
		    break;
		  case 5 ... 9:
		    ADD2LOG ("DISPLAY: Sun|Single width GX (cgsix), depth=8\n");
		    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0009);
		    break;
		  case 11:
		    switch (vmsize)
		      {
		      case 2:
			ADD2LOG ("DISPLAY: Sun|Turbo GX with 1M VSIMM (cgsix), depth=8\n");
			hd->device.id = MAKE_ID(TAG_SPECIAL, 0x000a);
			break;
		      case 4:
			ADD2LOG ("DISPLAY: Sun|Turbo GX Plus (cgsix), depth=8\n");
			hd->device.id = MAKE_ID(TAG_SPECIAL, 0x000b);
			break;
		      default:
			ADD2LOG ("DISPLAY: Sun|Turbo GX (cgsix), depth=8\n");
			hd->device.id = MAKE_ID(TAG_SPECIAL, 0x000c);
			break;
		      }
		    break;
		  default:
		    ADD2LOG ("DISPLAY: Sun|Unknown GX (cgsix), depth=8\n");
		    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x000d);
		    break;
		  }
	      }
	    else if (strcmp (prop2, "cgfourteen") == 0)
	      {
		int size = 0;

		hd = add_hd_entry (hd_data, __LINE__, 0);
		hd->base_class.id = bc_display;
		hd->sub_class.id = sc_dis_vga;
		hd->bus.id = bus_sbus;
		hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);

		prop2 = prom_getproperty ("reg", &len, buf2);
		if (prop2 && !(len % 12) && len > 0)
		  size = *(int *)(prop2 + len - 4);
		switch (size)
		  {
		  case 0x400000:
		    ADD2LOG ("DISPLAY: Sun|SX with 4M VSIMM (cgfourteen), depth=24\n");
		    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x000e);
		    break;
		  case 0x800000:
		    ADD2LOG ("DISPLAY: Sun|SX with 8M VSIMM (cgfourteen), depth=24\n");
		    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x000f);
		    break;
		  default:
		    ADD2LOG ("DISPLAY: Sun|SX (cgfourteen), depth=24\n");
		    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0010);
		    break;
		  }
	      }
	    else if (strcmp (prop2, "leo") == 0)
	      {
		hd = add_hd_entry (hd_data, __LINE__, 0);
		hd->base_class.id = bc_display;
		hd->sub_class.id = sc_dis_vga;
		hd->bus.id = bus_sbus;
		hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);

		prop2 = prom_getproperty("model", &len, buf2);
		if (prop2 && len > 0 && !strstr(prop2, "501-2503"))
		  {
		    ADD2LOG ("DISPLAY: Sun|Turbo ZX (leo), depth=24\n");
		    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0012);
		  }
		else
		 {
		   ADD2LOG ("DISPLAY: Sun|ZX or Turbo ZX (leo), depth=24\n");
		   hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0011);
		 }
	      }
	    else if (strcmp (prop2, "tcx") == 0)
	      {
		hd = add_hd_entry (hd_data, __LINE__, 0);
		hd->base_class.id = bc_display;
		hd->sub_class.id = sc_dis_vga;
		hd->bus.id = bus_sbus;
		hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);

		if (prom_getbool ("tcx-8-bit"))
		  {
		    ADD2LOG ("DISPLAY: Sun|TCX (8bit), depth=8\n");
		    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0013);
		  }
		else
		  {
		    ADD2LOG ("DISPLAY: Sun|TCX (S24), depth=24\n");
		    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0014);
		  }
	      }
	    else if (strcmp (prop2, "afb") == 0)
	      {
		int btype = 0;

		prop2 = prom_getproperty("board_type", &len, buf2);
		if (prop2 && len == 4)
		  btype = *(int *)prop2;

		hd = add_hd_entry (hd_data, __LINE__, 0);
		hd->base_class.id = bc_display;
		hd->sub_class.id = sc_dis_vga;
		hd->bus.id = bus_sbus;
		hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);

		if (btype == 3)
		  {
		    ADD2LOG ("DISPLAY: Sun|Elite3D-M6 Horizontal (afb), depth=24\n");
		    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0015);
		  }
		else
		  {
		    ADD2LOG ("DISPLAY: Sun|Elite3D (afb), depth=24\n");
		    hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0016);
		  }
	      }
	    else if (strcmp (prop2, "ffb") == 0)
	      {
		int btype = 0;

		prop2 = prom_getproperty("board_type", &len, buf2);
		if (prop2 && len == 4)
		  btype = *(int *)prop2;

		hd = add_hd_entry (hd_data, __LINE__, 0);
		hd->base_class.id = bc_display;
		hd->sub_class.id = sc_dis_vga;
		hd->bus.id = bus_sbus;
		hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);

		 switch (btype)
		   {
		   case 0x08:
		     ADD2LOG ("DISPLAY: Sun|FFB 67MHz Creator (ffb), depth=24\n");
		     hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0017);
		     break;
		   case 0x0b:
		     ADD2LOG ("DISPLAY: Sun|FFB 67MHz Creator 3D (ffb), depth=24\n");
		     hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0018);
		     break;
		   case 0x1b:
		     ADD2LOG ("DISPLAY: Sun|FFB 75MHz Creator 3D (ffb), depth=24\n");
		     hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0019);
		     break;
		   case 0x20:
		   case 0x28:
		     ADD2LOG ("DISPLAY: Sun|FFB2 Vertical Creator (ffb), depth=24\n");
		     hd->device.id = MAKE_ID(TAG_SPECIAL, 0x001a);
		     break;
		   case 0x23:
		   case 0x2b:
		     ADD2LOG ("DISPLAY: Sun|FFB2 Vertical Creator 3D (ffb), depth=24\n");
		     hd->device.id = MAKE_ID(TAG_SPECIAL, 0x001b);
		     break;
		   case 0x30:
		     ADD2LOG ("DISPLAY: Sun|FFB2+ Vertical Creator (ffb), depth=24\n");
		     hd->device.id = MAKE_ID(TAG_SPECIAL, 0x001c);
		     break;
		   case 0x33:
		     ADD2LOG ("DISPLAY: Sun|FFB2+ Vertical Creator 3D (ffb), depth=24\n");
		     hd->device.id = MAKE_ID(TAG_SPECIAL, 0x001d);
		     break;
		   case 0x40:
		   case 0x48:
		     ADD2LOG ("DISPLAY: Sun|FFB2 Horizontal Creator (ffb), depth=24\n");
		     hd->device.id = MAKE_ID(TAG_SPECIAL, 0x001e);
		     break;
		   case 0x43:
		   case 0x4b:
		     ADD2LOG ("DISPLAY: Sun|FFB2 Horizontal Creator 3D (ffb), depth=24\n");
		     hd->device.id = MAKE_ID(TAG_SPECIAL, 0x001f);
		     break;
		   default:
		     ADD2LOG ("DISPLAY: Sun|FFB (ffb), type=%xi, depth=24\n",
			      btype);
		     hd->device.id = MAKE_ID(TAG_SPECIAL, 0x0020);
		     break;
		   }
	      }
	    else
	      ADD2LOG ("DISPLAY: Unknown SBUS device \"%s\"\n", prop2);
	  }
	else
	  ADD2LOG ("DISPLAY: Unknown device \"%s\"\n", prop2);
    }
  else if (strcmp (prop1, "cpu") == 0)
    {
      char *prop2 = prom_getproperty ("name", &len, buf2);

      if (prop2 && len >= 0)
	ADD2LOG ("CPU: %s\n", prop2);
    }
  else
    {
      char *prop2 = prom_getproperty ("name", &len, buf2);

      if (prop2 && len >= 0)
	{
	  if (strcmp (prop2, "audio") == 0)
	    {
	      ADD2LOG ("AUDIO: type=AMD7930, module=amd7930\n");
	      hd = add_hd_entry (hd_data, __LINE__, 0);
	      hd->base_class.id = bc_multimedia;
	      hd->sub_class.id = sc_multi_audio;
	      hd->bus.id = bus_sbus;
	      hd->unix_dev_name = new_str ("/dev/audio");

	      hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
	      hd->device.id = MAKE_ID(TAG_SPECIAL, 0x2001);
	    }
	  else if (strcmp (prop2, "CS4231") == 0)
	    {
	      hd = add_hd_entry (hd_data, __LINE__, 0);
	      hd->base_class.id = bc_multimedia;
	      hd->sub_class.id = sc_multi_audio;
	      hd->bus.id = bus_sbus;
	      hd->unix_dev_name = new_str ("/dev/audio");

	      if (ebus)
		{
		  ADD2LOG ("AUDIO: type=CS4231 EB2 DMA (PCI), module=cs4231\n");
		  hd->device.id = MAKE_ID(TAG_SPECIAL, 0x2002);
		}
	      else
		{
		  ADD2LOG ("AUDIO: type=CS4231 APC DMA (SBUS), module=cs4231\n");
		  hd->device.id = MAKE_ID(TAG_SPECIAL, 0x2003);
		}
	      hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
	    }
	  else if (strcmp (prop2, "DBRIe") == 0)
	    {
	      ADD2LOG ("AUDIO: type=SS10/SS20 DBRI, module=dbri\n");
	      hd = add_hd_entry (hd_data, __LINE__, 0);
	      hd->base_class.id = bc_multimedia;
	      hd->sub_class.id = sc_multi_audio;
	      hd->bus.id = bus_sbus;
	      hd->unix_dev_name = new_str ("/dev/audio");

	      hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
	      hd->device.id = MAKE_ID(TAG_SPECIAL, 0x2004);
	    }
	  else if (strcmp (prop2, "bpp") == 0)
	    {
	      ADD2LOG ("PARPORT: type=bpp, module=unknown\n");
	    }
	  else if (strcmp (prop2, "soc") == 0)
	    {
	      ADD2LOG ("SCSI: type=Sun SPARCStorage Array, module=fc4:soc:pluto\n");
	      hd = add_hd_entry (hd_data, __LINE__, 0);
	      hd->base_class.id = bc_storage;
	      hd->sub_class.id = sc_sto_scsi;
	      hd->bus.id = bus_sbus;

	      hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
	      hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1101);
	    }
	  else if (strcmp (prop2, "socal") == 0)
	    {
	      ADD2LOG ("SCSI: type=Sun Enterprise Network Array, module=fc4:socal:fcal\n");
	      hd = add_hd_entry (hd_data, __LINE__, 0);
	      hd->base_class.id = bc_storage;
	      hd->sub_class.id = sc_sto_scsi;
	      hd->bus.id = bus_sbus;

	      hd->vendor.id = MAKE_ID(TAG_SPECIAL, 0x4001);
	      hd->device.id = MAKE_ID(TAG_SPECIAL, 0x1102);
	    }
	  else if (strcmp(prop2, "sbus") == 0 || strcmp(prop2, "sbi") == 0)
	    nsbus = 1;
	  else if (!strcmp(prop2, "ebus"))
	    nebus = 1;
	  else
	    ADD2LOG ("%s: unknown device \"%s\"\n", prop1, prop2);
	}
    }


  nextnode = prom_getchild (node);
  if (nextnode)
    prom_parse (nextnode, nsbus, nebus, hd_data);
  nextnode = prom_nextnode (node);
  if (nextnode)
    prom_parse (nextnode, sbus, ebus, hd_data);

  return;
}

void
hd_scan_sbus (hd_data_t *hd_data)
{
  int prom_root_node;

  if(!hd_probe_feature(hd_data, pr_sbus))
    return;

  hd_data->module = mod_sbus;

  /* some clean-up */
  remove_hd_entries(hd_data);

  PROGRESS(1, 0, "sun sbus");

  if((prom_fd = open(DEV_OPENPROM, O_RDWR | O_NONBLOCK | O_NOCTTY)) < 0)
    return;

  prom_root_node = prom_nextnode(0);
  if (!prom_root_node)
    goto failed;

  prom_parse (prom_root_node, 0, 0, hd_data);

 failed:
  close (prom_fd);

  return;
}

#else

void
hd_scan_sbus (hd_data_t *hd_data)
{
  return;
}

#endif

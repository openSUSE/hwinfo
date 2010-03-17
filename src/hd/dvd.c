/*
 * taken from HAL 0.5.14
 * http://www.freedesktop.org/Software/hal
 *
 * Originally part of dvd+rw-tools by Andy Polyakov <appro@fy.chalmers.se>
 * http://fy.chalmers.se/~appro/linux/DVD+RW/
 */

#define CREAM_ON_ERRNO(s)	do {				\
    switch ((s)[2]&0x0F)					\
    {	case 2:	if ((s)[12]==4) errno=EAGAIN;	break;		\
	case 5:	errno=EINVAL;					\
		if ((s)[13]==0)					\
		{   if ((s)[12]==0x21)		errno=ENOSPC;	\
		    else if ((s)[12]==0x20)	errno=ENODEV;	\
		}						\
		break;						\
    }								\
} while(0)
#define ERRCODE(s)	((((s)[2]&0x0F)<<16)|((s)[12]<<8)|((s)[13]))
#define	SK(errcode)	(((errcode)>>16)&0xF)
#define	ASC(errcode)	(((errcode)>>8)&0xFF)
#define ASCQ(errcode)	((errcode)&0xFF)

#ifndef _LARGEFILE_SOURCE
#define _LARGEFILE_SOURCE
#endif
#ifndef _LARGEFILE64_SOURCE
#define _LARGEFILE64_SOURCE
#endif
#ifndef _FILE_OFFSET_BITS
#define _FILE_OFFSET_BITS 64
#endif
#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <stdint.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <sys/ioctl.h>
#include <linux/cdrom.h>
#include <errno.h>
#include <string.h>
#include <mntent.h>
#include <sys/wait.h>
#include <sys/utsname.h>
#include <scsi/scsi.h>
#include <scsi/sg.h>
#include <poll.h>
#include <sys/time.h>

#include "dvd.h"

#if !defined(SG_FLAG_LUN_INHIBIT)
# if defined(SG_FLAG_UNUSED_LUN_INHIBIT)
#  define SG_FLAG_LUN_INHIBIT SG_FLAG_UNUSED_LUN_INHIBIT
# else
#  define SG_FLAG_LUN_INHIBIT 0
# endif
#endif

typedef enum {
	NONE = CGC_DATA_NONE,	// 3
	READ = CGC_DATA_READ,	// 2
	WRITE = CGC_DATA_WRITE	// 1
} Direction;

typedef struct ScsiCommand ScsiCommand;

struct ScsiCommand {
	int fd;
	int autoclose;
	char *filename;
	struct cdrom_generic_command cgc;
	union {
		struct request_sense s;
		unsigned char u[18];
	} _sense;
	struct sg_io_hdr sg_io;
};

#define DIRECTION(i) (Dir_xlate[i]);

/* 1,CGC_DATA_WRITE
 * 2,CGC_DATA_READ
 * 3,CGC_DATA_NONE
 */
const int Dir_xlate[4] = {
	0,			// implementation-dependent...
	SG_DXFER_TO_DEV,	// 1,CGC_DATA_WRITE
	SG_DXFER_FROM_DEV,	// 2,CGC_DATA_READ
	SG_DXFER_NONE		// 3,CGC_DATA_NONE
};

static ScsiCommand *
scsi_command_new (void)
{
	ScsiCommand *cmd;

	cmd = (ScsiCommand *) malloc (sizeof (ScsiCommand));
	memset (cmd, 0, sizeof (ScsiCommand));
	cmd->fd = -1;
	cmd->filename = NULL;
	cmd->autoclose = 1;

	return cmd;
}

static ScsiCommand *
scsi_command_new_from_fd (int f)
{
	ScsiCommand *cmd;

	cmd = scsi_command_new ();
	cmd->fd = f;
cmd->autoclose = 0;

	return cmd;
}

static void
scsi_command_free (ScsiCommand * cmd)
{
	if (cmd->fd >= 0 && cmd->autoclose) {
		close (cmd->fd);
		cmd->fd = -1;
	}
	if (cmd->filename) {
		free (cmd->filename);
		cmd->filename = NULL;
	}

	free (cmd);
}

static int
scsi_command_transport (ScsiCommand * cmd, Direction dir, void *buf,
			size_t sz)
{
	int ret = 0;

	cmd->sg_io.dxferp = buf;
	cmd->sg_io.dxfer_len = sz;
	cmd->sg_io.dxfer_direction = DIRECTION (dir);

	if (ioctl (cmd->fd, SG_IO, &cmd->sg_io))
		return -1;

	if ((cmd->sg_io.info & SG_INFO_OK_MASK) != SG_INFO_OK) {
		errno = EIO;
		ret = -1;
		if (cmd->sg_io.masked_status & CHECK_CONDITION) {
			CREAM_ON_ERRNO ((unsigned char*)cmd->sg_io.sbp);
			ret = ERRCODE ((unsigned char*)cmd->sg_io.sbp);
			if (ret == 0)
				ret = -1;
		}
	}

	return ret;
}

static void
scsi_command_init (ScsiCommand * cmd, size_t i, int arg)
{
	if (i == 0) {
		memset (&cmd->cgc, 0, sizeof (cmd->cgc));
		memset (&cmd->_sense, 0, sizeof (cmd->_sense));
		cmd->cgc.quiet = 1;
		cmd->cgc.sense = &cmd->_sense.s;
		memset (&cmd->sg_io, 0, sizeof (cmd->sg_io));
		cmd->sg_io.interface_id = 'S';
		cmd->sg_io.mx_sb_len = sizeof (cmd->_sense);
		cmd->sg_io.cmdp = cmd->cgc.cmd;
		cmd->sg_io.sbp = cmd->_sense.u;
		cmd->sg_io.flags = SG_FLAG_LUN_INHIBIT | SG_FLAG_DIRECT_IO;
	}
	cmd->sg_io.cmd_len = i + 1;
	cmd->cgc.cmd[i] = arg;
}

int
get_dvd_profile(int fd)
{
	ScsiCommand *cmd;
	int retval = 0;
	unsigned char page[20];
	unsigned char *list;
	int i, len;

	cmd = scsi_command_new_from_fd (fd);

	scsi_command_init (cmd, 0, 0x46);
	scsi_command_init (cmd, 1, 2);
	scsi_command_init (cmd, 8, 8);
	scsi_command_init (cmd, 9, 0);
	if (scsi_command_transport (cmd, READ, page, 8)) {
		/* GET CONFIGURATION failed */
		scsi_command_free (cmd);
		return -1;
	}

	/* See if it's 2 gen drive by checking if DVD+R profile is an option */
	len = 4 + (page[0] << 24 | page[1] << 16 | page[2] << 8 | page[3]);
	if (len > 264) {
		scsi_command_free (cmd);
		/* insane profile list length */
		return -1;
	}

	list = (unsigned char *) malloc (len);

	scsi_command_init (cmd, 0, 0x46);
	scsi_command_init (cmd, 1, 2);
	scsi_command_init (cmd, 7, len >> 8);
	scsi_command_init (cmd, 8, len);
	scsi_command_init (cmd, 9, 0);
	if (scsi_command_transport (cmd, READ, list, len)) {
		/* GET CONFIGURATION failed */
		scsi_command_free (cmd);
		free (list);
		return -1;
	}

	for (i = 12; i < list[11]; i += 4) {
		int profile = (list[i] << 8 | list[i + 1]);
		/* 0x13: DVD-RW Restricted Overwrite
		 * 0x14: DVD-RW Sequential
		 * 0x15: DVD-R Dual Layer Sequential
		 * 0x16: DVD-R Dual Layer Jump
		 * 0x1A: DVD+RW  
		 * 0x1B: DVD+R 
		 * 0x2A: DVD+RW DL
		 * 0x2B: DVD+R DL 
		 * 0x40: BD-ROM
		 * 0x41: BD-R SRM
		 * 0x42: BR-R RRM
		 * 0x43: BD-RE
		 * 0x50: HD DVD-ROM
		 * 0x51: HD DVD-R 
		 * 0x52: HD DVD-Rewritable 
		 */

		switch (profile) {
			case 0x13:
			case 0x14:
				retval |= DRIVE_CDROM_CAPS_DVDRW;
				break;
			case 0x15:
			case 0x16:
				retval |= DRIVE_CDROM_CAPS_DVDRDL;
				break;
			case 0x1B:
				retval |= DRIVE_CDROM_CAPS_DVDPLUSR;
				break;
			case 0x1A:
				retval |= DRIVE_CDROM_CAPS_DVDPLUSRW;
				break;
			case 0x2A:
				retval |= DRIVE_CDROM_CAPS_DVDPLUSRWDL;
				break;
			case 0x2B:
				retval |= DRIVE_CDROM_CAPS_DVDPLUSRDL;
				break;
			case 0x40:
				retval |= DRIVE_CDROM_CAPS_BDROM;
				break;
			case 0x41:
			case 0x42:
				retval |= DRIVE_CDROM_CAPS_BDR;
				break;
			case 0x43:
				retval |= DRIVE_CDROM_CAPS_BDRE;
				break;
			case 0x50:
				retval |= DRIVE_CDROM_CAPS_HDDVDROM;
				break;
			case 0x51:
				retval |= DRIVE_CDROM_CAPS_HDDVDR;
				break;
			case 0x52:
				retval |= DRIVE_CDROM_CAPS_HDDVDRW;
				break;
			default:
				break;
		}
	}

	scsi_command_free (cmd);
	free (list);
	
	return retval;
	
}


/*****************************************************************************
**
** iopl.c
*/
static char rcsid[] __attribute__((unused)) = "$Id: iopl.c,v 1.1 2000/02/12 12:36:27 snwint Exp $";
/*
**
** Acquire/Relinquish I/O port access privileges.
**
** Copyright (C) 1999  Omer Zak (omerz@actcom.co.il)
**
**
** This library is free software; you can redistribute it and/or
** modify it under the terms of the GNU Library General Public
** License as published by the Free Software Foundation; either
** version 2 of the License, or (at your option) any later version.
**
** This library is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
** Library General Public License for more details.
**
** You should have received a copy of the GNU Library General Public
** License along with this library; if not, write to the 
** Free Software Foundation, Inc., 59 Temple Place - Suite 330, 
** Boston, MA  02111-1307  USA.
**
******************************************************************************
**
** Bug reports and fixes - to  P.J.H.Fox (fox@roestock.demon.co.uk)
** Note:  by sending unsolicited commercial/political/religious
**        E-mail messages (known also as "spam") to any E-mail address
**        mentioned in this file, you irrevocably agree to pay the
**        receipient US$500.- (plus any legal expenses incurred while
**        trying to collect the amount due) per unsolicited
**        commercial/political/religious E-mail message - for
**        the service of receiving your E-mail message.
**
*****************************************************************************/

#include <unistd.h>
#include <errno.h>
#include "iopl.h"

#ifdef __powerpc__
unsigned long isa_io_base;
#include <stdio.h>
#include <sys/mman.h>
#include <fcntl.h>
#include <string.h>
#endif

#if 1
#undef printf
#undef fprintf
#undef perror
#undef putchar
#define printf(a...)
#define fprintf(a...)
#define perror(a)
#define putchar(a)
#endif

/****************************************************************************/

#if defined _OS2_
unsigned 
iopl(unsigned port)
{
	return (0);
}
#endif /* _OS2_ */

#ifdef __DJGPP__
unsigned 
iopl(unsigned port)
{
	return (0);
}
#endif /* __DJGPP__ */


/****************************************************************************/

/* Acquire I/O port privileges needed for ISA PnP configuration.
** The return value is 0 on success, or the value of errno on failure.
*/
int
acquire_pnp_io_privileges(void)
{
#ifdef __powerpc__
  {
	FILE *fd1;
	int fd2;
	unsigned char buffer[1024];
	unsigned long phys_io_base=0;
	
	fd1=fopen("/proc/cpuinfo","r");
	if(fd1 == NULL)
	{
	        printf("Cannot open /proc/cpuinfo, unable to determine architecture.\n");
	        return(42);
	}
	
	while(fgets(buffer,1024,fd1))
	{
#ifdef PPCIODEBUG
	        printf("buffer: %s\n",buffer);
#endif
	        if(strncmp(buffer,"machine",7)==0)
	        {
#ifdef PPCIODEBUG
	                printf("found\n");
#endif
       		        if(strstr(buffer,"CHRP"))
                        phys_io_base=CHRP_ISA_IO_BASE;
                else if(strstr(buffer,"PReP") || strstr(buffer,"PREP"))
                        phys_io_base=PREP_ISA_IO_BASE;
	        }
	}
	fclose(fd1);
	if (phys_io_base==0)
	{
	        printf("Unknown architecture (currently supported: CHRP, PREP)\n");
	        return(43);
		}

	fd2=open("/dev/mem",O_RDWR);
	if(fd2 < 0)
	{
	        printf("Cannot open /dev/mem, unable to mmap IO space.\n");
	        return(44);
	}
	
	isa_io_base=(unsigned int) mmap((caddr_t)0,
	                                64<<10,
	                                PROT_READ|PROT_WRITE,
	                                MAP_SHARED,
	                                fd2,
	                                phys_io_base);
	
	if(isa_io_base==(unsigned int)MAP_FAILED)
	{
	        printf("mmap'ing IO space failed.\n");
	        close(fd2);
	        return(45);
	}
	
	close(fd2);
  	
  }
  return 0;

#else /* __powerpc__ */

  int ret;
#ifdef _AXP_
  /* ALPHA only has ioperm, apparently, so cover all with one permission */
  ret = ioperm(MIN_READ_ADDR, WRITEDATA_ADDR - MIN_READ_ADDR + 1, 1);
#else
  /*
   * Have to get unrestricted access to io ports, as WRITE_DATA port > 0x3ff
   */
  ret = iopl(3);
#endif

  if (ret < 0) {
    return(errno);
  }
  else {
    return(0);
  }
#endif /* __powerpc__ */
}

/* Relinquish I/O port privileges needed for ISA PnP configuration.
** The return value is 0 on success, or the value of errno on failure.
*/
int
relinquish_pnp_io_privileges(void)
{
#ifdef __powerpc__
  return 0;
#else
  int ret;
#ifdef _AXP_
  ret = ioperm(MIN_READ_ADDR, WRITEDATA_ADDR - MIN_READ_ADDR + 1, 0);
#else
  ret = iopl(0);
#endif

  if (ret < 0) {
    return(errno);
  }
  else {
    return(0);
  }
#endif /* __powerpc__ */
}

/****************************************************************************/

/* End of iopl.c */

/*****************************************************************************
**
** iopl.h
** $Header: /abuild/poeml/svn-convert/hwinfo/../libhd-cvs/src/pnpdump/Attic/iopl.h,v 1.2 2000/07/06 12:56:26 snwint Exp $
**
** Acquire/Relinquish I/O port access privileges.
** This header file also provides platform-independent inb/outb declarations.
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

#ifndef _IOPL_H_
#define _IOPL_H_

#ifdef __cplusplus
extern "C" {
#endif

/****************************************************************************/

#ifdef __DJGPP__
#undef REALTIME
#include <inlines/pc.h>
#include <dos.h>
#else
#ifndef _OS2_
#  if defined __GLIBC__ && __GLIBC__ >= 2 && !defined __powerpc__
#    include <sys/io.h>
#  else
#    ifdef _AXP_
#       include <sys/io.h>
#    else
#       include <asm/io.h>
#    endif
#  endif	      	   /* __GLIBC__ */
#else
#include <sys/hw.h>
#endif /* _OS2_ */
#endif

/****************************************************************************/

/* Acquire I/O port privileges needed for ISA PnP configuration.
** The return value is 0 on success, or the value of errno on failure.
*/
int acquire_pnp_io_privileges(void);

/* Relinquish I/O port privileges needed for ISA PnP configuration.
** The return value is 0 on success, or the value of errno on failure.
*/
int relinquish_pnp_io_privileges(void);

/****************************************************************************/

#ifdef __cplusplus
}; /* end of extern "C" */
#endif

#endif /* _IOPL_H_ */
/* End of iopl.h */

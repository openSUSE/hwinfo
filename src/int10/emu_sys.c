/****************************************************************************
*
*						Realmode X86 Emulator Library
*
*            	Copyright (C) 1996-1999 SciTech Software, Inc.
* 				     Copyright (C) David Mosberger-Tang
* 					   Copyright (C) 1999 Egbert Eich
*
*  ========================================================================
*
*  Permission to use, copy, modify, distribute, and sell this software and
*  its documentation for any purpose is hereby granted without fee,
*  provided that the above copyright notice appear in all copies and that
*  both that copyright notice and this permission notice appear in
*  supporting documentation, and that the name of the authors not be used
*  in advertising or publicity pertaining to distribution of the software
*  without specific, written prior permission.  The authors makes no
*  representations about the suitability of this software for any purpose.
*  It is provided "as is" without express or implied warranty.
*
*  THE AUTHORS DISCLAIMS ALL WARRANTIES WITH REGARD TO THIS SOFTWARE,
*  INCLUDING ALL IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS, IN NO
*  EVENT SHALL THE AUTHORS BE LIABLE FOR ANY SPECIAL, INDIRECT OR
*  CONSEQUENTIAL DAMAGES OR ANY DAMAGES WHATSOEVER RESULTING FROM LOSS OF
*  USE, DATA OR PROFITS, WHETHER IN AN ACTION OF CONTRACT, NEGLIGENCE OR
*  OTHER TORTIOUS ACTION, ARISING OUT OF OR IN CONNECTION WITH THE USE OR
*  PERFORMANCE OF THIS SOFTWARE.
*
*  ========================================================================
*
* Language:		ANSI C
* Environment:	Any
* Developer:    Kendall Bennett
*
* Description:  This file includes subroutines which are related to
*				programmed I/O and memory access. Included in this module
*				are default functions with limited usefulness. For real
*				uses these functions will most likely be overriden by the
*				user library.
*
****************************************************************************/

#include "emu_x86emu.h"
#include "emu_regs.h"
#include "emu_prim_ops.h"
#include <string.h>
/*------------------------- Global Variables ------------------------------*/

X86EMU_sysEnv		_X86EMU_env;		/* Global emulator machine state */
X86EMU_intrFuncs	_X86EMU_intrTab[256];
u8  	(X86APIP sys_rdb)(u32 addr);
u16 	(X86APIP sys_rdw)(u32 addr);
u32 	(X86APIP sys_rdl)(u32 addr);
void 	(X86APIP sys_wrb)(u32 addr,u8 val);
void 	(X86APIP sys_wrw)(u32 addr,u16 val);
void 	(X86APIP sys_wrl)(u32 addr,u32 val);
u8  	(X86APIP sys_inb)(X86EMU_pioAddr addr);
u16 	(X86APIP sys_inw)(X86EMU_pioAddr addr);
u32 	(X86APIP sys_inl)(X86EMU_pioAddr addr);
void 	(X86APIP sys_outb)(X86EMU_pioAddr addr, u8 val);
void 	(X86APIP sys_outw)(X86EMU_pioAddr addr, u16 val);
void 	(X86APIP sys_outl)(X86EMU_pioAddr addr, u32 val);

/*----------------------------- Setup -------------------------------------*/

/****************************************************************************
PARAMETERS:
funcs	- New memory function pointers to make active

REMARKS:
This function is used to set the pointers to functions which access
memory space, allowing the user application to override these functions
and hook them out as necessary for their application.
****************************************************************************/
void X86EMU_setupMemFuncs(
	X86EMU_memFuncs *funcs)
{
	sys_rdb = funcs->rdb;
    sys_rdw = funcs->rdw;
    sys_rdl = funcs->rdl;
    sys_wrb = funcs->wrb;
    sys_wrw = funcs->wrw;
    sys_wrl = funcs->wrl;
}

/****************************************************************************
PARAMETERS:
funcs	- New programmed I/O function pointers to make active

REMARKS:
This function is used to set the pointers to functions which access
I/O space, allowing the user application to override these functions
and hook them out as necessary for their application.
****************************************************************************/
void X86EMU_setupPioFuncs(
	X86EMU_pioFuncs *funcs)
{
    sys_inb = funcs->inb;
    sys_inw = funcs->inw;
    sys_inl = funcs->inl;
    sys_outb = funcs->outb;
    sys_outw = funcs->outw;
    sys_outl = funcs->outl;
}

/****************************************************************************
PARAMETERS:
funcs	- New interrupt vector table to make active

REMARKS:
This function is used to set the pointers to functions which handle
interrupt processing in the emulator, allowing the user application to
hook interrupts as necessary for their application. Any interrupts that
are not hooked by the user application, and reflected and handled internally
in the emulator via the interrupt vector table. This allows the application
to get control when the code being emulated executes specific software
interrupts.
****************************************************************************/
void X86EMU_setupIntrFuncs(
	X86EMU_intrFuncs funcs[])
{
    int i;
    
	for (i=0; i < 256; i++)
		_X86EMU_intrTab[i] = NULL;
	if (funcs) {
		for (i = 0; i < 256; i++)
			_X86EMU_intrTab[i] = funcs[i];
		}
}

/****************************************************************************
PARAMETERS:
int	- New software interrupt to prepare for

REMARKS:
This function is used to set up the emulator state to exceute a software
interrupt. This can be used by the user application code to allow an
interrupt to be hooked, examined and then reflected back to the emulator
so that the code in the emulator will continue processing the software
interrupt as per normal. This essentially allows system code to actively
hook and handle certain software interrupts as necessary.
****************************************************************************/
void X86EMU_prepareForInt(
	int num)
{
    push_word((u16)M.x86.R_FLG);
    CLEAR_FLAG(F_IF);
    CLEAR_FLAG(F_TF);
    push_word(M.x86.R_CS);
    M.x86.R_CS = mem_access_word(num * 4 + 2);
    push_word(M.x86.R_IP);
    M.x86.R_IP = mem_access_word(num * 4);
	M.x86.intr = 0;
}

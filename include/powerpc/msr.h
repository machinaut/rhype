/*
 * Copyright (C) 2005 Jimi Xenidis <jimix@watson.ibm.com>, IBM Corporation
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 * 
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 * 
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 * $Id$
 */
#ifndef _POWERPC_MSR_H
#define _POWERPC_MSR_H

#include <regs.h>

/* Flags in MSR: */
#define MSR_SF		ULL(0x8000000000000000)
#define MSR_TA		ULL(0x4000000000000000)
#define MSR_ISF		ULL(0x2000000000000000)
#define MSR_HV		ULL(0x1000000000000000)
#define MSR_VMX		ULL(0x0000000002000000)
#define MSR_MER		ULL(0x0000000000200000)
#define MSR_POW		ULL(0x0000000000040000)
#define MSR_ILE		ULL(0x0000000000010000)
#define MSR_EE		ULL(0x0000000000008000)
#define MSR_PR		ULL(0x0000000000004000)
#define MSR_FP		ULL(0x0000000000002000)
#define MSR_ME		ULL(0x0000000000001000)
#define MSR_FE0		ULL(0x0000000000000800)
#define MSR_SE		ULL(0x0000000000000400)
#define MSR_BE		ULL(0x0000000000000200)
#define MSR_FE1		ULL(0x0000000000000100)
#define MSR_IP		ULL(0x0000000000000040)
#define MSR_IR		ULL(0x0000000000000020)
#define MSR_DR		ULL(0x0000000000000010)
#define MSR_PMM		ULL(0x0000000000000004)
#define MSR_RI		ULL(0x0000000000000002)
#define MSR_LE		ULL(0x0000000000000001)

#ifndef HAS_MSR_IP
#undef MSR_IP
#endif

#ifndef HAS_MSR_SF
#undef MSR_SF
#endif

#ifndef HAS_MSR_ISF
#undef MSR_ISF
#endif

#ifndef HAS_MSR_HV
#undef MSR_HV
#endif

/* On a trap, srr1's copy of msr defines some bits as follows: */
#define MSR_TRAP_FE     ULL(0x0000000000100000)	/* Floating Point Exception */
#define MSR_TRAP_IOP    ULL(0x0000000000080000)	/* Illegal Instruction */
#define MSR_TRAP_PRIV   ULL(0x0000000000040000)	/* Privileged Instruction */
#define MSR_TRAP        ULL(0x0000000000020000)	/* Trap Instruction */
#define MSR_TRAP_NEXT   ULL(0x0000000000010000)	/* PC is next instruction */
#define MSR_TRAP_BITS  (MSR_TRAP_FE|MSR_TRAP_IOP|MSR_TRAP_PRIV|MSR_TRAP)

#endif /* _POWERPC_MSR_H */

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
 $Id$
 *
 */

#include <config.h>
#include <h_proto.h>
#include <lib.h>

/*
 * Implement the H_RTOS  hcall() to set the current OS as RTOS
 */
sval
h_rtos(struct cpu_thread *thread, uval flags, uval time)
{

	/* request realtime processing */
	if (flags & H_RTOS_REQUEST) {

		if (rtos_cpu == NULL) {
			/* if rtos_cpu is null then save current OS as RTOS */
			rtos_processing_time = time;
			rtos_cpu = thread->cpu;
			rtos_thread = thread;
			hprintf("\n\n%s: cpu %p flags %ld  time %ld\n",
				__func__, thread->cpu, flags, time);
		} else {
			/* Already another OS is registered as RTOS */
			return (H_UNAVAIL);
		}
	} else {
		/* de-register RTOS */
		hprintf("\n\n%s: cpu %p flags %ld  time %ld\n",
			__func__, thread->cpu, flags, time);
		rtos_cpu = NULL;
		rtos_thread = NULL;
	}
	return (H_Success);
}

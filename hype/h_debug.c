/*
 * Copyright (C) 2002,2005 IBM Corporation
 *
 */

#include <config.h>
#include <h_proto.h>
#include <hype.h>
#include <hcall.h>
#include <debug.h>
#include <thinwire.h>
#include <os.h>


static sval
probe_regs(struct cpu_thread *thread, uval target, uval start)
{
	uval cpu = (target >> 8) & 0xff;
	uval thr = target & 0xff;
	uval lpid = target >> 16;
	if (cpu >= MAX_CPU || thr >= THREADS_PER_CPU) return H_Parameter;

	struct os* t = os_lookup(lpid);
	if (!t || !t->cpu[cpu]) return H_Parameter;

	struct cpu_thread *x = &t->cpu[cpu]->thread[thr];
	int i = 0;
	for (; i < 4; ++i) {
		return_arg(thread, i + 1, probe_reg(x, start + i));
	}
	return H_Success;
}


sval
h_debug(struct cpu_thread *thread, uval cmd, uval arg1,
	uval arg2, uval arg3, uval arg4)
{

	switch (cmd) {
#ifdef DEBUG
	case H_BREAKPOINT:
		breakpoint();
		break;
	case H_SET_DEBUG_LEVEL:
		debug_level = arg1;
		break;

	case H_PROBE_REG:
		probe_regs(thread, arg1, arg2);
		break;
#ifdef USE_THINWIRE_IO
	case H_THINWIRE_RESET:
		resetThinwire();
		break;
#endif
#endif
	}
	(void)thread;
	(void)cmd;
	(void)arg1;
	(void)arg2;
	(void)arg3;
	(void)arg4;

	return 0;
}

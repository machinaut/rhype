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

#ifdef DEBUG

uval
probe_reg(struct cpu_thread *thread, uval idx)
{
#define MAP_REG(idx, name) case idx: return thread->reg_##name;
	switch (idx) {
	case 0 ... 31:
		return thread->reg_gprs[idx];
#ifdef HAS_HDECR
	MAP_REG(32, hsrr0);
	MAP_REG(33, hsrr1);
#endif
#ifdef HAS_ASR
	MAP_REG(34, asr);
#endif
#ifdef HAS_FP
	MAP_REG(35, fpscr);
#endif
	MAP_REG(36, sprg[0]);
	MAP_REG(37, sprg[1]);
	MAP_REG(38, sprg[2]);
	MAP_REG(39, sprg[3]);

	MAP_REG(40, lr);
	MAP_REG(41, ctr);
	MAP_REG(42, srr0);
	MAP_REG(43, srr1);
	MAP_REG(44, dar);
	default:
		return 0;
	}
	return 0;
}


#endif /* DEBUG */

/*
 * Copyright (C) 2005, IBM Corporation
 *
 */

#include <config.h>
#include <types.h>
#include <asm.h>
#include <util.h>
#include <gdbstub.h>

int
main(void)
{
	DECLARE(GDB_ZERO, 0);
#ifdef USE_GDB_STUB
	DECLARE(GDB_MSR, offsetof(struct cpu_state, msr));
	DECLARE(GDB_PC, offsetof(struct cpu_state, pc));
	DECLARE(GDB_CR, offsetof(struct cpu_state, cr));
	DECLARE(GDB_XER, offsetof(struct cpu_state, xer));
	DECLARE(GDB_CTR, offsetof(struct cpu_state, ctr));
	DECLARE(GDB_LR, offsetof(struct cpu_state, lr));
	DECLARE(GDB_DAR, offsetof(struct cpu_state, dar));
	DECLARE(GDB_DSISR, offsetof(struct cpu_state, dsisr));
	DECLARE(GDB_GPR0, offsetof(struct cpu_state, gpr[0]));
	DECLARE(GDB_HSRR0, offsetof(struct cpu_state, hsrr0));
	DECLARE(GDB_HSRR1, offsetof(struct cpu_state, hsrr1));
	DECLARE(GDB_HDEC, offsetof(struct cpu_state, hdec));
	DECLARE(GDB_CPU_STATE_SIZE, sizeof (struct cpu_state));
#endif

	return 0;
}

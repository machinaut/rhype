/*
 * Copyright (C) 2005, IBM Corporation
 *
 */

#include <cpu.h>
#include <lpidtag.h>
#include <machine_config.h>

void
cpu_core_init(void)
{
	lpidtag_init(NUM_LPIDR_BITS);
}

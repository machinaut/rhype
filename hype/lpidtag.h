/*
 * Copyright (C) 2005, IBM Corporation
 *
 */

#include <types.h>
#include <os.h>

extern void lpidtag_acquire(struct os *os);
extern void lpidtag_release(struct os *os);
extern void lpidtag_init(int num_lpidr_bits);

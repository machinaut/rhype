/*
 * Copyright (C) 2005, IBM Corporation
 *
 */

/* Manage the tags used for the LPIDR register. These are hardware-recognized
 * values, unlike the plain "lpid" field. */

#include <lpidtag.h>
#include <bitmap.h>
#include <os.h>

static struct lbm *lpidtags;

void
lpidtag_acquire(struct os *os)
{
	os->po_lpid_tag = lbm_acquire(lpidtags);
	/* TODO: remove assert; this case just means we need to tlbia more */
	assert(os->po_lpid_tag != (uval16)-1, "couldn't reserve an LPID tag\n");
}

void
lpidtag_release(struct os *os)
{
	lbm_release(lpidtags, os->po_lpid_tag);
}

void
lpidtag_init(int num_lpidr_bits)
{
	lpidtags = lbm_alloc(1UL<<num_lpidr_bits);
	lbm_set_all_bits(lpidtags, 0);
}

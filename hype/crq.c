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

#include <crq.h>
#include <cpu.h>
#include <lpar.h>
#include <lib.h>
#include <os.h>
#include <pmm.h>
#include <xir.h>
#include <tce.h>
#include <liob.h>
#include <vio.h>
#include <objalloc.h>
#include <bitops.h>
#include <resource.h>
#include <debug.h>

/* local data structures */
struct crq_entry {
	uval64 msg_hi;
	uval64 msg_lo;
};

struct crq_partner {
	uval16 cp_flags;
	uval16 cp_num_entries;
	uval32 cp_next_entry;
	struct crq_entry *cp_crqs;
	struct os *cp_os;
	xirr_t cp_interrupt;
	struct tce_data cp_tce_data;
	struct vios_resource cp_res;
};

struct crq {
	struct crq_partner crq_partner[2];
	lock_t crq_lock;
};

#define CRQ_FLAG_USED		1
#define CRQ_FLAG_HAS_PARTNER    2
#define CRQ_FLAG_VALID		4

/* local defines */

/*
 * We need to base the following on endianess
 * to be able to treat CRQ entries as 2 64-bit
 * entries.
 */
#ifdef __PPC__
#define CRQ_HEADER_BYTE    0
#else
#define CRQ_HEADER_BYTE    7
#endif

/* Some defines for messages and used on CRQs */
#define CRQ_EVT_CLOSE_HI        0xFF04000000000000ULL
#define CRQ_EVT_CLOSE_LO        0x0000000000000000ULL

#define INVALID_IDX  -1


#define NUM_CRQ_LUT_ENTRIES	(1 << 8)
#define CRQ_LUT_MASK		(NUM_CRQ_LUT_ENTRIES - 1)

/* local variables */
static struct crq *crq_entries;
static sval crq_lut[NUM_CRQ_LUT_ENTRIES];
static uval volatile crq_map;


#define CRQ_MAP_TYPE_BITS	(sizeof (crq_map) * 8)
#define CRQ_SLOT_MASK		(CRQ_MAP_TYPE_BITS - 1)

static inline sval
crq_slot(uval val)
{
	uval slot;
	sval idx = val & CRQ_LUT_MASK;

	slot = crq_lut[idx];

	if (slot >= MAX_GLOBAL_CRQ) {
		return -1;
	}

	return slot;
}

static inline uval
crq_slot_set(uval val, uval type)
{

	val &= ~((uval)CRQ_SLOT_MASK);
	val |= type & CRQ_SLOT_MASK;

	return val;
}

static inline sval
crq_map_acquire(void)
{
	uval bit;
	uval map;
	uval new;

	do {
		map = crq_map;
		bit = ffz(map);
		if (bit >= CRQ_MAP_TYPE_BITS) {
			return -1;
		}

		new = map | (1U << bit);
	} while (!cas_uval(&crq_map, map, new));

	return bit;
}

static inline sval
crq_map_release(uval bit)
{
	uval map;
	uval new;

	if (bit >= CRQ_MAP_TYPE_BITS) {
		assert(0, "bit value out of range\n");
		return -1;
	}

	do {
		map = crq_map;
		new = map & ~(1U << bit);
	} while (!cas_uval(&crq_map, map, new));

	return bit;
}

static inline struct crq *
crq_get_crq(uval slot)
{
	if (slot >= MAX_GLOBAL_CRQ) {
		assert(0, "slot out of range");
		return NULL;
	}
	return &crq_entries[slot];
}

static inline sval
crq_get_idx(struct crq *my_crq, struct os *os)
{
	if (my_crq->crq_partner[0].cp_os == os) {
		return 0;
	} else if (my_crq->crq_partner[1].cp_os == os) {
		return 1;
	}
	return INVALID_IDX;
}


static inline struct crq_partner *
crq_partner_get(uval liobn)
{
	sval slot = crq_slot(liobn);
	struct crq *my_crq;

	if (slot < 0) {
		return NULL;
	}

	my_crq = crq_get_crq(slot);
	if (NULL == my_crq) {
		return NULL;
	}

	if (liobn == my_crq->crq_partner[0].cp_res.vr_liobn) {
		return &my_crq->crq_partner[0];
	} else if (liobn == my_crq->crq_partner[1].cp_res.vr_liobn) {
		return &my_crq->crq_partner[1];
	} else {
		assert(0, "could not find os for liobn=0x%lx",
		       liobn);
	}
	return NULL;
}

static inline struct os *
crq_get_os(uval liobn)
{
	struct crq_partner *cp = crq_partner_get(liobn);
	if (NULL != cp) {
		return cp->cp_os;
	}
	return NULL;
}

static inline sval
crq_claim_idx(struct crq *my_crq, struct os *os)
{
	if (NULL == my_crq->crq_partner[0].cp_os) {
		my_crq->crq_partner[0].cp_os = os;
		return 0;
	} else if (NULL == my_crq->crq_partner[1].cp_os) {
		my_crq->crq_partner[1].cp_os = os;
		return 1;
	}
	return INVALID_IDX;
}

static inline sval
crq_init(struct crq *crq)
{
	memset(crq, 0x0, sizeof (*crq));
	lock_init(&crq->crq_lock);
	return 1;
}

static inline sval
crq_init_entries(void)
{
	uval i = 0;

	while (i < MAX_GLOBAL_CRQ) {
		crq_init(crq_get_crq(i));
		i++;
	}
	return 1;
}

static inline uval
crq_flags_test(struct crq_partner *cp, uval16 flags)
{
	return (flags == (cp->cp_flags & flags));
}

static inline struct crq *
crq_check_unused(uval slot, sval *idx, struct os *os)
{
	struct crq *my_crq;

	if (slot >= MAX_GLOBAL_CRQ) {
		assert(0, "slot number out of range!\n");
		return NULL;
	}

	my_crq = crq_get_crq(slot);
	*idx = crq_get_idx(my_crq, os);
	if (INVALID_IDX == *idx) {
		*idx = crq_claim_idx(my_crq, os);
		if (INVALID_IDX == *idx) {
			my_crq = NULL;
		}
	}

	return my_crq;
}

static sval
crq_put_entry_raw_locked(struct crq *crq, uval srcidx, const uval8 *msg)
{
	uval ret = H_Success;
	uval destidx = srcidx ^ 1;

	if (crq_flags_test(&crq->crq_partner[destidx],
			   CRQ_FLAG_USED | CRQ_FLAG_HAS_PARTNER)) {
		uval entry = crq->crq_partner[destidx].cp_next_entry;

		DEBUG_OUT(DBG_CRQ, "%s: Sending message to partner.\n",
			  __func__);

		copy_out(&((crq->crq_partner[destidx].cp_crqs)[entry].msg_hi),
			 msg, 8 * 2);

		/*
		 * Send signal to partner OS; other layer will determine
		 * whether to actually post it.
		 */
		DEBUG_OUT(DBG_CRQ,
			  "%s: Trying to raise an interrupt for the partner "
			  "to pick up the msg.\n", __func__);

		xir_raise(crq->crq_partner[destidx].cp_interrupt,
			  NULL);

		crq->crq_partner[destidx].cp_next_entry++;
		if (crq->crq_partner[destidx].cp_next_entry ==
		    crq->crq_partner[destidx].cp_num_entries) {
			crq->crq_partner[destidx].cp_next_entry = 0;
		}

	} else {
		DEBUG_OUT(DBG_CRQ,
			  "%s: There's no partner who could receive the "
			  "message!\n", __func__);
		ret = H_Closed;
	}

	return ret;
}

static inline sval
crq_put_entry_locked(struct crq *crq, uval srcidx, const uval8 *msg)
{
	sval ret;

	if ((msg[CRQ_HEADER_BYTE] & 0xff) != 0xff &&
	    (msg[CRQ_HEADER_BYTE] & 0x80) != 0x00) {
		ret = crq_put_entry_raw_locked(crq, srcidx, msg);
	} else {
		ret = H_Parameter;
	}
	return ret;
}

static const uval64 close_msg[2] = {
	CRQ_EVT_CLOSE_HI,
	CRQ_EVT_CLOSE_LO
};

static sval
crq_release_locked(struct crq *crq, uval slot, uval idx)
{
	sval ret = H_Success;

	/*
	 * Check whether I have a partner and if
	 * so, let's send him a message that we closed this crq.
	 */
	if (crq_flags_test(&crq->crq_partner[idx ^ 1],
			   CRQ_FLAG_HAS_PARTNER | CRQ_FLAG_USED)) {
		ret = crq_put_entry_raw_locked(crq, idx,
					       (const uval8 *)close_msg);
	} else if (0 == (crq->crq_partner[idx ^ 1].cp_flags & CRQ_FLAG_USED)) {
		/*
		 * If there's not even a partner I was the last one
		 * to have used the slot. So I just release the slot as
		 * well.
		 */
		crq_map_release(slot);
	}

	/*
	 * We cannot clear the TCE entries anymore, because
	 * otherwise kernel modules will fail even when rmmod
	 * is done or later when insmod is done. The only one
	 * who is setting up TCE entries is the controller.
	 * Clearing TCE entries will also be a job of the controller.
	 */

	crq->crq_partner[idx].cp_os = NULL;
	/*
	 * The partner does not have a partner anymore.
	 */
	crq->crq_partner[idx].cp_flags = 0;
	crq->crq_partner[idx ^ 1].cp_flags &= ~CRQ_FLAG_HAS_PARTNER;
	return ret;
}

/*
 * In case a partition dies and some crqs need to be cleaned up,
 * this function walks all the slots and releases all the
 * crqs that belonged to that partition.
 */
sval
crq_os_release_all(struct os *os)
{
	uval slot = 0;

	while (slot < MAX_GLOBAL_CRQ) {
		struct crq *my_crq;
		sval idx;

		my_crq = crq_get_crq(slot);
		if (NULL == my_crq) {
			break;
		}
		lock_acquire(&my_crq->crq_lock);

		idx = crq_get_idx(my_crq, os);

		if (INVALID_IDX != idx) {
			crq_release_locked(my_crq, slot, idx);
		}
		lock_release(&my_crq->crq_lock);
		slot++;
	}
	return 1;
}

/*
 * Check whether a slot is used by someone who has no partner, yet.
 */
static inline sval
crq_is_partner_and_open(struct crq *my_crq, uval idx)
{
	return (CRQ_FLAG_USED  ==
		(my_crq->crq_partner[idx].cp_flags &
		 (CRQ_FLAG_USED | CRQ_FLAG_HAS_PARTNER)));
}

static inline struct crq *
crq_find_open_partner(uval slot, uval *idx)
{
	uval j = 0;
	struct crq *my_crq = crq_get_crq(slot);

	if (NULL == my_crq) {
		return NULL;
	}

	lock_acquire(&my_crq->crq_lock);

	while (j <= 1) {
		if (crq_is_partner_and_open(my_crq, j)) {
			if (crq_flags_test(&my_crq->crq_partner[j],
					   CRQ_FLAG_HAS_PARTNER)) {
				DEBUG_OUT(DBG_CRQ,
					  "%s: Partner already matched up!\n",
					  __func__);
				lock_release(&my_crq->crq_lock);
				return NULL;
			} else {
				DEBUG_OUT(DBG_CRQ,
					  "%s: Found partner! SLOT=%ld\n",
					  __func__, slot);
				/*
				 * this one is used, but does not have a
				 * partner
				 */
				*idx = (j ^ 1);
				lock_release(&my_crq->crq_lock);
				return my_crq;
			}
		}
		j++;
	}

	lock_release(&my_crq->crq_lock);

	return NULL;
}

static inline void *
crq_addr_lookup(struct crq_partner *cp, uval addr, uval size)
{
	union tce_bdesc bd;

	bd.lbd_bits.lbd_len = size;
	bd.lbd_bits.lbd_addr = addr;
	return tce_bd_xlate(&cp->cp_tce_data, bd);
}

static sval
crq_setup_locked(struct os *os, struct crq *crq, uval idx,
		 uval buffer, uval buffersize)
{
	struct crq_partner *cp = crq->crq_partner;

	cp[idx].cp_os = os;
	cp[idx].cp_flags = CRQ_FLAG_USED;

	if (crq_flags_test(&cp[idx ^ 1], CRQ_FLAG_USED)) {
		cp[0].cp_flags |= CRQ_FLAG_HAS_PARTNER;
		cp[1].cp_flags |= CRQ_FLAG_HAS_PARTNER;
	}

	cp[idx].cp_num_entries = buffersize / sizeof (struct crq_entry);
	cp[idx].cp_next_entry = 0;
	cp[idx].cp_crqs = (struct crq_entry *)
		crq_addr_lookup(&cp[idx], buffer, buffersize);

	assert(cp[idx].cp_tce_data.t_tce != NULL,
	       "TCE was not set up!\n");

	DEBUG_OUT(DBG_CRQ, "%s: ---- crq buffer at %p  irq=0x%x\n",
		  __func__, cp[idx].cp_crqs, cp[idx].cp_interrupt);
	return 1;
}

/*
 * The following functions are directly called through the
 * h-calls.
 */

static sval
crq_tce_put(struct os *os, uval32 liobn, uval ioba, union tce ltce)
{
	sval slot;
	struct crq *my_crq;
	sval idx;

	slot = crq_slot(liobn);
	if (slot < 0) {
		return H_Parameter;
	}

	my_crq = crq_get_crq(slot);
	if (NULL == my_crq) {
		return H_Parameter;
	}

	idx = crq_get_idx(my_crq, os);	

	if (idx == INVALID_IDX) {
		idx = crq_claim_idx(my_crq, os);
	}

	if (idx != INVALID_IDX) {
		sval ret;

		assert(my_crq->crq_partner[idx].cp_tce_data.t_tce != NULL,
		       "TCE was not set up (liobn=0x%x)!\n",
		       liobn);

		/* FIXME should not need this lock */
		lock_acquire(&my_crq->crq_lock);
		ret = tce_put(os, &my_crq->crq_partner[idx].cp_tce_data,
			      ioba, ltce);
		lock_release(&my_crq->crq_lock);

		return ret;
	}
	return H_Parameter;
}

sval
crq_try_copy(struct cpu_thread *thread, uval32 sliobn, uval sioba,
	     uval32 dliobn, uval dioba, uval len)
{
	sval ret;
	struct os *os = thread->cpu->os;
	struct crq *my_crq = NULL;
	uval idx = 0;
	sval slot;

	/*
	 * There must be a better way of doing this on PPC
	 * with a valid device tree.
	 */
	if ((dliobn & ~0U) == ~0U) {
		/* source: from IO partition */
		slot = crq_slot(sliobn);
		idx = 1;
	} else {
		/* destination: to IO partition */
		slot = crq_slot(dliobn);
	}

	if (slot < 0) {
		return H_Parameter;
	}

	my_crq = crq_get_crq(slot);
	if (NULL == my_crq) {
		return H_Parameter;
	}

	if (my_crq->crq_partner[0].cp_interrupt == dliobn) {
		idx = 0;
	} else if (my_crq->crq_partner[1].cp_interrupt == dliobn) {
		idx = 1;
	}

	lock_acquire(&my_crq->crq_lock);

	if (INVALID_IDX != crq_get_idx(my_crq, os)) {
		uval *s_pa;
		uval *d_pa;
		struct os *os1;
		struct os *os2;

		os1 = my_crq->crq_partner[idx].cp_os;
		os2 = my_crq->crq_partner[idx ^ 1].cp_os;

		d_pa = crq_addr_lookup(&my_crq->crq_partner[idx], dioba, len);
		if (d_pa) {
			s_pa = crq_addr_lookup(&my_crq->crq_partner[idx ^ 1],
					       sioba, len);
			if (s_pa) {
				DEBUG_OUT(DBG_CRQ,
					  " %s: Copying from %p to %p\n",
					  __func__, s_pa, d_pa);
				copy_mem(d_pa, s_pa, len);
				ret = H_Success;
			} else {
				ret = H_Parameter;
			}

		} else {
			ret = H_Parameter;
		}

	} else {
		ret = H_Parameter;
	}

	lock_release(&my_crq->crq_lock);

	return ret;
}

sval
crq_free_crq(struct os *os, uval uaddr)
{
	sval ret = H_Success;
	sval myidx = 0;
	struct crq *my_crq;
	uval slot = crq_slot(uaddr);

	DEBUG_OUT(DBG_CRQ, "%s: u_addr: 0x%lx\n", __func__, uaddr);

	if (slot >= MAX_GLOBAL_CRQ) {
		assert(0, "slot value out of range!\n");
		return H_Parameter;
	}

	my_crq = crq_get_crq(slot);
	if (NULL == my_crq) {
		return H_Parameter;
	}
	lock_acquire(&my_crq->crq_lock);

	myidx = crq_get_idx(my_crq, os);

	if (INVALID_IDX == myidx) {
		ret = H_Parameter;
	} else {
		crq_release_locked(my_crq, slot, myidx);
	}

	lock_release(&my_crq->crq_lock);

	return ret;
}

sval
crq_reg_crq(struct os *os, uval uaddr, uval queue, uval len)
{
	sval ret;
	struct crq *my_crq = NULL;
	uval slot = crq_slot(uaddr);
	uval again_ctr = 5;

	DEBUG_OUT(DBG_CRQ, "%s: uaddr: 0x%lx queue: 0x%lx len: 0x%lx\n",
		  __func__, uaddr, queue, len);

	if (len < CRQ_MIN_BUFFER_SIZE ||
	    0 != (len & (CRQ_MIN_BUFFER_SIZE - 1)) ||
	    queue != ALIGN_DOWN(queue, len)) {
		return H_Parameter;
	}

	if (slot >= MAX_GLOBAL_CRQ) {
		assert(0, "slot value out of range!\n");
		return H_Parameter;
	}

	/*
	 * Try to find an empty crq entry in the global list
	 */

	for (;;) {
		sval availidx;

		my_crq = crq_find_open_partner(slot, &availidx);

		if (NULL != my_crq) {
			lock_acquire(&my_crq->crq_lock);
			/*
			 * Check whether this partner is still good
			 * now that we have a lock!
			 */
			if (!crq_is_partner_and_open(my_crq, availidx ^ 1)) {
				lock_release(&my_crq->crq_lock);
				if (--again_ctr) {
					continue;
				} else {
					return H_Parameter;
				}
			}

			DEBUG_OUT(DBG_CRQ,
				  "%s: A matching partner exists for "
				  "the CRQ.\n", __func__);

			if (1 == crq_setup_locked(os, my_crq, availidx,
						  queue, len)) {
				ret = H_Success;
			} else {
				ret = H_Busy;
			}
			lock_release(&my_crq->crq_lock);
		} else {
			my_crq = crq_check_unused(slot, &availidx, os);
			if (NULL != my_crq) {
				uval lk;

				lock_acquire(&my_crq->crq_lock);

				DEBUG_OUT(DBG_CRQ,
					  "%s: There's no matching partner "
					  "for the CRQ!\n", __func__);

				lk = crq_setup_locked(os, my_crq, 0,
						      queue, len);
				if (1 == lk) {
					ret = H_Closed;
				} else {
					ret = H_Busy;
				}
				lock_release(&my_crq->crq_lock);
			} else {
				ret = H_Resource;
			}
		}
		break;
	}

	return ret;
}

sval
crq_send(struct os *os, uval uaddr, uval8 *msg)
{
	sval ret;
	sval idx;
	uval slot = crq_slot(uaddr);
	struct crq *my_crq;

	my_crq = crq_get_crq(slot);
	if (NULL == my_crq) {
		return H_Parameter;
	}
	lock_acquire(&my_crq->crq_lock);

	idx = crq_get_idx(my_crq, os);

	if (INVALID_IDX == idx) {

		DEBUG_OUT(DBG_CRQ, "%s: Not sending this message since "
			  "CRQ not found.\n", __func__);

		ret = H_Parameter;
	} else {
		ret = crq_put_entry_locked(my_crq, idx, msg);
	}
	lock_release(&my_crq->crq_lock);

	return ret;
}

static sval
crq_set_slot(struct cpu_thread *thread, struct crq *crq,
	     uval xenc1, uval xenc2, uval dma_range, uval slot)
{
	crq_lut[xenc1 & CRQ_LUT_MASK] = slot;
	crq_lut[xenc2 & CRQ_LUT_MASK] = slot;

	/* setup tce areas */
	if (tce_alloc(&crq->crq_partner[0].cp_tce_data,
		      0x0, dma_range)) {
		if (tce_alloc(&crq->crq_partner[1].cp_tce_data,
			      0x0, dma_range)) {
			struct os *os = thread->cpu->os;

			resource_init(&crq->crq_partner[0].cp_res.vr_res,
				      NULL, INTR_SRC);
			resource_init(&crq->crq_partner[1].cp_res.vr_res,
				      NULL, INTR_SRC);
			
			/* I am the owner of both 'sides' */
			crq->crq_partner[0].cp_os = os;
			crq->crq_partner[1].cp_os = os;
			
			crq->crq_partner[0].cp_res.vr_liobn = xenc1;
			crq->crq_partner[1].cp_res.vr_liobn = xenc2;
			
			crq->crq_partner[0].cp_interrupt = xenc1;
			crq->crq_partner[1].cp_interrupt = xenc2;
			
			crq->crq_partner[0].cp_flags |= CRQ_FLAG_VALID;
			crq->crq_partner[1].cp_flags |= CRQ_FLAG_VALID;
			
			return_arg(thread, 1, xenc1);
			return_arg(thread, 2, xenc2);
			return_arg(thread, 3, dma_range);
			

			DEBUG_OUT(DBG_CRQ, "%s: Successfully set up TCEs for "
				  "CRQ on xenc1=0x%lx, xenc2=0x%lx.\n",
				  __func__,
				  xenc1, xenc2);

			return 1;
		}
		tce_free(&crq->crq_partner[0].cp_tce_data);
	}
	crq_lut[xenc1 & CRQ_LUT_MASK] = -1;
	crq_lut[xenc2 & CRQ_LUT_MASK] = -1;

	return 0;
}

static sval
crq_acquire(struct cpu_thread *thread, uval dma_range)
{
	sval slot;

	slot = crq_map_acquire();
	if (slot >= 0) {
		struct crq *crq;
		sval isrc;
		uval xenc1, xenc2;

		isrc = xir_find(XIRR_CLASS_LLAN);
		assert(isrc != -1, "Can't register crq!");
		
		if (isrc >= 0) {

			xenc1 = xirr_encode(isrc, XIRR_CLASS_CRQ);

			isrc = xir_find(XIRR_CLASS_LLAN);
			assert(isrc != -1, "Can't register crq!");
			
			if (isrc >= 0) {
				xenc2 = xirr_encode(isrc, XIRR_CLASS_CRQ);

				DEBUG_OUT(DBG_CRQ,
					  "SLOT=%ld xenc1=0x%lx xenc2=0x%lx\n",
					  slot, xenc1, xenc2);

				crq = crq_get_crq(slot);
			
				if (NULL != crq) {
					uval rc;

					rc = crq_set_slot(thread, crq,
							  xenc1, xenc2,
							  dma_range,
							  slot);
					if (rc) {
						return H_Success;
					}
				}
				xir_default_config(xenc2, NULL, NULL);
			}
			xir_default_config(xenc1, NULL, NULL);
		}
		crq_map_release(slot);
	}

	DEBUG_OUT(DBG_CRQ, "%s: Could NOT set up TCEs for CRQ",
	        __func__);

	return H_UNAVAIL;
}

static sval
crq_release(struct os *os, uval32 liobn)
{
	(void)liobn;
	(void)os;

	DEBUG_OUT(DBG_CRQ, "--- %s: Not doing anything!", __func__);

	return H_Function;
}

static sval
crq_accept(struct os *os, uval liobn, uval *retval)
{
	struct crq *my_crq;
	sval slot;
	
	(void)os;
	slot = crq_slot(liobn);
	if (slot < 0) {
		return H_Parameter;
	}

	my_crq = crq_get_crq(slot);
	if (NULL == my_crq) {
		return H_Parameter;
	}

	*retval = liobn;
	return H_Success;
}

static sval
crq_invalidate(struct os *os, uval liobn)
{
	(void)os;
	struct crq_partner *cp = crq_partner_get(liobn);
	if (cp) {
		cp->cp_flags &= ~CRQ_FLAG_VALID;
		return H_Success;
	}

	return H_Parameter;
}

static sval
crq_return(struct os *os, uval liobn)
{
	(void)liobn;
	(void)os;
	return H_Success;
}

static sval
crq_grant(struct sys_resource **res,
	  struct os *src, struct os *dst, uval liobn)
{
	sval slot;
	sval idx = INVALID_IDX;
	struct crq *my_crq;
	
	slot = crq_slot(liobn);
	if (slot < 0) {
		return H_Parameter;
	}

	my_crq = crq_get_crq(slot);
	if (NULL == my_crq) {
		return H_Parameter;
	}

	if (my_crq->crq_partner[0].cp_interrupt == liobn) {
		idx = 0;
	} else if (my_crq->crq_partner[1].cp_interrupt == liobn) {
		idx = 1;
	}

	if (my_crq->crq_partner[idx].cp_os != src) {
		return H_Parameter;
	}
		

	if (INVALID_IDX != idx) {
		struct sys_resource *my_res;
		struct cpu_thread *thread = &dst->cpu[0]->thread[0];

		uval xenc;
		sval rc;
		uval dum;

		/* set new owner */
		my_crq->crq_partner[idx].cp_os = dst;

		xenc = my_crq->crq_partner[idx].cp_res.vr_liobn;
		xir_default_config(xenc, NULL, NULL);
		xir_default_config(xenc, 
				   thread,
				   &my_crq->crq_partner[idx]);
		
		my_res = &my_crq->crq_partner[idx].cp_res.vr_res;
		rc = insert_resource(my_res, dst);
		assert(rc >= 0,
		       "Can't give crq to os");
		lock_acquire(&my_res->sr_lock);
		rc = accept_locked_resource(my_res, &dum);
		assert(rc == H_Success, "Can't bind crq to os\n");
		lock_release(&my_res->sr_lock);

		*res = my_res;
		return H_Success;
	}
	
	return H_UNAVAIL;
}

static sval
crq_rescind(struct os *os, uval liobn)
{
	struct os *cos = crq_get_os(liobn);

	(void)os;
	if (NULL == cos) {
		return H_Parameter;
	}
	return crq_release(cos, liobn);
}

static sval
crq_conflict(struct sys_resource *res1,
             struct sys_resource *res2,
             uval liobn)
{
	struct crq *my_crq;
	sval slot;
	
	slot = crq_slot(liobn);
	if (slot < 0) {
		return H_Parameter;
	}

	my_crq = crq_get_crq(slot);
	if (NULL == my_crq) {
		return H_Parameter;
	}

	(void)res1;
	(void)res2;
	assert(0, "what do we do here?\n");

	return H_Success;
}

static inline void 
crq_init_lut()
{
	uval i = 0;
	while (i < NUM_CRQ_LUT_ENTRIES) {
		crq_lut[i] = -1;
		i++;
	}
}


static struct vios crq_vios = {
	.vs_name = "crq",
	.vs_tce_put = crq_tce_put,
	.vs_acquire = crq_acquire,
	.vs_release = crq_release,
	.vs_grant = crq_grant,
	.vs_accept = crq_accept,
	.vs_invalidate = crq_invalidate,
	.vs_return = crq_return,
	.vs_rescind = crq_rescind,
	.vs_conflict = crq_conflict
};

sval
crq_sys_init(void)
{
	uval size = sizeof (struct crq[MAX_GLOBAL_CRQ]);

	crq_entries = halloc(size);

	assert(NULL != crq_entries, "could not allocate memory for CRQs\n");

	if (NULL != crq_entries) {
		if (vio_register(&crq_vios, HVIO_CRQ) == HVIO_CRQ) {
			crq_init_entries();
			xir_init_class(XIRR_CLASS_CRQ, NULL, NULL);
			
			crq_init_lut();
			return 1;
		}
	}
	assert(0, "failed to register: %s\n", crq_vios.vs_name);
	return 0;
}

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
 *
 * XXX references to 'bolted' should go away. This 'bolted' bit is just a
 * software-use PTE bit which Linux uses to always keep the PTE in the HTAB.
 * The bit should not be hardcoded (other OS's may use a different bit to mean
 * the same thing).
 */

#include <config.h>
#include <asm.h>
#include <lpar.h>
#include <lib.h>
#include <htab.h>
#include <h_proto.h>

#include <hype.h>
#include <os.h>
#include <mmu.h>
#include <pmm.h>
#include <objalloc.h>

#ifdef HTAB_PER_CPU
#define NUM_HTABS MAX_CPU * MAX_OS
#else
#define NUM_HTABS MAX_OS;
#endif

/* private functions */

static inline uval
HtabCalcSdr1(uval htab_addr, uval htab_log_size)
{
	uval sdr1_htaborg;
	uval sdr1_htabsize;

	assert((htab_addr & ((1UL << htab_log_size) - 1)) == 0,
	       "htab_addr is not aligned 0x%lx\n", htab_addr);

	sdr1_htaborg = htab_addr;

	assert(((sdr1_htaborg & ~SDR1_HTABORG_MASK) == 0UL),
	       "sdr1_htaborg: 0x%lx extra bits\n", sdr1_htaborg);

	assert((htab_log_size >= SDR1_HTABSIZE_BASE),
	       "htab_log_size(%ld) < SDR1_HTABSIZE_BASE(%d)\n",
	       htab_log_size, SDR1_HTABSIZE_BASE);

	/* log number of PTEGS */
	sdr1_htabsize = htab_log_size - LOG_PTEG_SIZE;
	/* subtract the minumum assumed value */
	sdr1_htabsize -= SDR1_HTABSIZE_BASE;

	assert((sdr1_htabsize <= SDR1_HTABSIZE_MAX),
	       "sdr1_htabsize: %ld too big\n", sdr1_htabsize);

	return (sdr1_htaborg | sdr1_htabsize);
}

static uval64
stegIndex(uval64 eaddr)
{
	/* select 5 low-order bits from ESID part of EA */
	return ((eaddr >> 28) & 0x1F);
}

static uval64
ptegIndex(uval64 vaddr, uval64 vsid)
{
	uval64 hash;

	hash = (vsid & VSID_HASH_MASK) ^
		((vaddr & EA_HASH_MASK) >> EA_HASH_SHIFT);

	return (hash & HTAB_HASH_MASK);
}

/* make_ste()
 * Calculate ESID and VSID from eaddr.
 * If not present, create a new STE and enter it into the segment table.
 * Return VSID (for use in PTE).
 *
 *	eaddr - effective address being mapped
 */
static uval64 make_ste(struct steg *stab, uval64 eaddr)
	__attribute__ ((unused));
static uval64
make_ste(struct steg *stab, uval64 eaddr)
{
	struct steg *stegPtr;
	struct ste *stePtr = 0;
	uval64 vsid;
	uval64 esid;
	int segment_mapped = 0;
	uval i;

	/*
	 * Kernel effective addresses have the high-order bit on, thus a
	 * kernel ESID will be something like 0x80000000C (36 bits).
	 * The 52-bit VSID for a kernel address is formed by prepending
	 * 0x7FFF to the top of the ESID, thus 0x7FFF80000000C.
	 */
	esid = (eaddr >> 28) & 0xFFFFFFFFFULL;
	/* FIXME: what hack is this! */
	if (esid == 0xc00000000)
		vsid = 0x06a99b4b14;
	else
		vsid = esid | 0x7FFF000000000ULL;

	/*
	 * Make an entry in the segment table to map the ESID to VSID,
	 * it it's not already present.
	 */
	stegPtr = &stab[stegIndex(eaddr)];

	for (i = 0; i < NUM_STES_IN_STEG; i++) {
		stePtr = &stegPtr->entry[i];
		if (STE_PTR_V_GET(stePtr) &&	/* entry valid, and */
		    STE_PTR_VSID_GET(stePtr) == vsid) {
			/* vsid matches */
			segment_mapped = 1;
			break;
		}
		if (!STE_PTR_V_GET(stePtr))	/* entry not yet used */
			break;
	}
	if (!segment_mapped) {
#ifdef DEBUG
		hprintf("Creating STE for 0x%llx\n"
			"  esid = 0x%llx\n"
			"  vsid = 0x%llx\n", eaddr, esid, vsid);
#endif
		assert(i < NUM_STES_IN_STEG,
		       "Boot-time segment table overflowed %ld\n", i);

		/*
		 * Create segment table entry in stegPtr->entry[i].
		 * We don't need to be careful about the order in
		 * which we do this, as address translation hasn't
		 * been turned on yet.
		 */
		STE_PTR_CLEAR(stePtr);
		STE_PTR_ESID_SET(stePtr, esid);
		STE_PTR_VSID_SET(stePtr, vsid);
		STE_PTR_V_SET(stePtr, 1);
	}

	return vsid;
}

/* find_pte()
 * locate an empty PTE in the appropriate PTEG
 * return PTE index (relative to HTAB, not this PTEG)
 *
 *	htab - pointer to CPU's hardware pagetable
 *	eaddr - effective address being mapped
 *	vsid - vsid from the approprite STE
 */
static uval64 find_pte(struct pteg *htab, uval64 eaddr, uval64 vsid)
	__attribute__ ((unused));

static uval64
find_pte(struct pteg *htab, uval64 eaddr, uval64 vsid)
{
	struct pteg *ptegPtr;
	union pte *ptePtr;
	uval pteIndex = (uval)-1;
	int i;

	/*
	 * Make a page table entry
	 */
	ptegPtr = &htab[ptegIndex(eaddr, vsid)];

	for (i = 0; i < NUM_PTES_IN_PTEG; i++) {
		ptePtr = &ptegPtr->entry[i];
		if (ptePtr->bits.v == 0) {
			pteIndex = NUM_PTES_IN_PTEG * ptegIndex(eaddr, vsid);
			pteIndex += i;
			break;
		}
	}
	assert(i < NUM_PTES_IN_PTEG,
	       "Boot-time page table overflowed %d\n", i);

	return pteIndex;
}

/* pte_insert: called by h_enter
 *	pt: extended hpte to use
 * 	ptex: index into page-table to use
 * 	vsidWord: word containing the vsid
 * 	rpnWord: word containing the rpn
 *	lrpn: logial address corresponding to pte
 */
void
pte_insert(struct logical_htab *pt, uval ptex, const uval64 vsidWord,
	   const uval64 rpnWord, uval lrpn)
{
	union pte *chosen =
		((union pte *)(pt->sdr1 & SDR1_HTABORG_MASK)) + ptex;
	uval *shadow = pt->shadow + ptex;

	/*
	 * It's required that external locking be done to
	 * provide exclusion between the choices of insertion
	 * points.
	 * Any valid choice of pte requires that the pte be
	 * invalid upon entry to this function.
	 * XXX lock etc
	 * LPAR 18.5.4.1.2 "Algorithm" requires ldarx/stdcx etc.
	 * and using bit 57 for per-pte locking.
	 */
	assert((chosen->bits.v == 0), "Attempt to insert over valid PTE\n");

	/* Set the second word first so the valid bit is the last thing set */
	chosen->words.rpnWord = rpnWord;

	/* Set shadow word. */
	*shadow = lrpn;

	/* Guarantee the second word is visible before the valid bit */
	eieio();

	/* Now set the first word including the valid bit */
	chosen->words.vsidWord = vsidWord;
	ptesync();
}

/* htab_alloc()
 * - reserve an HTAB for this cpu
 * - set cpu's SDR1 register
 */
void
htab_alloc(struct os *os)
{
	uval sdr1_val = 0;
	htab_t *ht;
	uval ht_ra = get_pages_aligned(&phys_pa,
				       1 << LOG_HTAB_BYTES, LOG_HTAB_BYTES);
	int i;

	assert(ht_ra != INVALID_PHYSICAL_PAGE, "htab allocation failure\n");

	ht = (htab_t *)ht_ra;

	/* hack.. should make the hv_ra delta global */
	ht_ra += get_hrmor();

	memset(ht, 0, sizeof (*ht));

	sdr1_val = HtabCalcSdr1(ht_ra, LOG_HTAB_BYTES);

	os->htab.num_ptes = (1ULL << (LOG_HTAB_BYTES - LOG_PTE_SIZE));
	os->htab.sdr1 = sdr1_val;
	os->htab.shadow = (uval *)halloc(os->htab.num_ptes * sizeof (uval));

	assert(os->htab.shadow, "Can't allocate shadow table\n");

	for (i = 0; i < os->installed_cpus; i++) {
		os->cpu[i]->reg_sdr1 = sdr1_val;
	}

#ifdef DEBUG
	hprintf("%s: allocated 0x%lx entry table to LPAR[0x%x] at %p\n",
		__func__, os->htab.num_ptes, os->po_lpid, ht);
#endif
}

void
htab_free(struct os *os)
{
	uval ht_ra = os->htab.sdr1 & SDR1_HTABORG_MASK;
	uval ht_ea;

	/* hack.. should make the hv_ra delta global */
	ht_ea = ht_ra - get_hrmor();
	if (ht_ea) {
		free_pages(&phys_pa, ht_ea,
			   (os->htab.num_ptes << LOG_PTE_SIZE) >> LOG_PGSIZE);
	}

	if (os->htab.shadow)
		hfree(os->htab.shadow, os->htab.num_ptes * sizeof (uval));

#ifdef DEBUG
	hprintf("HTAB: freed htab at 0x%lx\n", ht_ea);
#endif
}

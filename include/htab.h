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
#ifndef _HYP_HTAB_H
#define _HYP_HTAB_H

#include <config.h>
#include <types.h>
#include <hypervisor.h>

/* log of the minimum number of PTEs allowed */
#define LOG_NUM_PTES_MIN	14
#define NUM_PTES_MIN		(1UL << LOG_NUM_PTES_MIN)

/* fixed HTAB size */
/* about: log2(memsize) >> 6 which is 1/64 or about 1.5% */
#define LOG_HTAB_BYTES		20	/* minimum HTAB size */
#define HTAB_BYTES		(1UL << LOG_HTAB_BYTES)
#define LOG_NUM_PTES_IN_PTEG	3
#define NUM_PTES_IN_PTEG	(1 << LOG_NUM_PTES_IN_PTEG)
#define LOG_PTE_SIZE		4
#define PTE_SIZE		(1 << LOG_PTE_SIZE)
#define LOG_PTEG_SIZE		(LOG_NUM_PTES_IN_PTEG + LOG_PTE_SIZE)
#define LOG_HTAB_HASH		(LOG_HTAB_BYTES - LOG_PTEG_SIZE)
/* mask for hash function */
#define HTAB_HASH_MASK		((1UL << LOG_HTAB_HASH) - 1)

/* used to turn a vsid into a number usable in the hash function */
#define VSID_HASH_MASK		0x0007fffffffffUL

/* used to take a vaddr so it can be used in the hashing function */
#define EA_HASH_VEC		0xffffUL
#define EA_HASH_SHIFT		LOG_PGSIZE
#define EA_HASH_MASK		(EA_HASH_VEC << EA_HASH_SHIFT)

#define VADDR_TO_API(vaddr)  	(((vaddr) & API_MASK) >> API_SHIFT)

/* used to turn a vaddr into an api for a pte */
#define API_VEC   		0x1fUL
#define API_SHIFT 		23
#define API_MASK  		(API_VEC << API_SHIFT)

/* real page number shift to create the rpn field of the pte */
#define RPN_SHIFT 		12

/* page protection bits in pp1 (name format: MSR:PR=0 | MSR:PR=1) */
#define PP_RWxx 0x0UL
#define PP_RWRW 0x2UL
#define PP_RWRx 0x4UL
#define PP_RxRx 0x6UL

union pte {
	struct PTEwords {
		uval64 vsidWord;
		uval64 rpnWord;
	} words;
	struct PTEbits {
		/* *INDENT-OFF* */
		/* high word */
		uval64 avpn:	57;	/* abbreviated virtual page number */
		uval64 lock:	1;	/* hypervisor lock bit */
		uval64 res:	1;	/* reserved for hypervisor */
		uval64 bolted:	1;	/* XXX software-reserved; temp hack */
		uval64 sw:	1;	/* reserved for software */
		uval64 l:	1;	/* Large Page */
		uval64 h:	1;	/* hash function id */
		uval64 v:	1;	/* valid */

		/* low word */
		uval64 pp0:	1;	/* page protection bit 0 (current PPC
					 *   AS says it can always be 0) */
		uval64 ts:	1;	/* tag select */
		uval64 rpn:	50;	/* real page number */
		uval64 res2:	2;	/* reserved */
		uval64 ac:	1;	/* address compare */
		uval64 r:	1;	/* referenced */
		uval64 c:	1;	/* changed */
		uval64 w:	1;	/* write through */
		uval64 i:	1;	/* cache inhibited */
		uval64 m:	1;	/* memory coherent */
		uval64 g:	1;	/* guarded */
		uval64 n:	1;	/* no-execute */
		uval64 pp1:	2;	/* page protection bits 1:2 */
		/* *INDENT-ON* */
	} bits;

};

union ptel {
	uval64 word;
	struct PTELbits {
	    /* *INDENT-OFF* */

		uval64 pp0:	1;	/* page protection bit 0 (current PPC
					 *   AS says it can always be 0) */
		uval64 ts:	1;	/* tag select */
		uval64 rpn:	50;	/* real page number */
		uval64 res2:	2;	/* reserved */
		uval64 ac:	1;	/* address compare */
		uval64 r:	1;	/* referenced */
		uval64 c:	1;	/* changed */
		uval64 w:	1;	/* write through */
		uval64 i:	1;	/* cache inhibited */
		uval64 m:	1;	/* memory coherent */
		uval64 g:	1;	/* guarded */
		uval64 n:	1;	/* no-execute */
		uval64 pp1:	2;	/* page protection bits 1:2 */
		/* *INDENT-ON* */
	} bits;
};

struct tlb_vpn {
	uval64 word;
	struct VPNbits {
		/* *INDENT-OFF* */

		uval64 avpn:	57;	/* abreviated VPN */
		uval64 sw:       4;     /* reserved for SW */
		uval64 l:        1;     /* large page */
		uval64 h:        1;     /* hash function ID */
		uval64 v:        1;     /* valid */
		/* *INDENT-ON* */
	} bits;
};

union tlb_index {
	uval64 word;
	struct INDEXbits {
		/* *INDENT-OFF* */

		uval64 w:        1;     /* HW managed TLB */
		uval64 res:      15;    /* reserved*/
		uval64 lvpn:     11;    /* low-order VPN bits */
		uval64 index:    37;    /* index */
		/* *INDENT-ON* */
	} bits;
};

struct pteg {
	union pte entry[8];
};

/*
 *
 * Segment Table
 *
 */

#define LOG_NUM_STES_IN_STEG	3
#define NUM_STES_IN_STEG	(1ULL << LOG_NUM_STES_IN_STEG)

#define LOG_STE_SIZE		4
#define STE_SIZE		(1ULL << LOG_STE_SIZE)

#define STE_ESID_SHIFT		28
#define STE_ESID_VEC		0xfffffffffULL
#define STE_ESID_MASK		(STE_ESID_VEC << STE_ESID_SHIFT)

#define STE_V_SHIFT	7
#define STE_V_VEC	1UL
#define STE_V_MASK	(STE_V_VEC << STE_V_SHIFT)

#define STE_T_SHIFT	6
#define STE_T_VEC	1UL
#define STE_T_MASK	(STE_T_VEC << STE_T_SHIFT)

#define STE_KS_SHIFT	5
#define STE_KS_VEC	1UL
#define STE_KS_MASK	(STE_KS_VEC << STE_KS_SHIFT)

#define STE_KP_SHIFT	4
#define STE_KP_VEC	1UL
#define STE_KP_MASK	(STE_KP_VEC << STE_KP_SHIFT)

#define STE_VSID_SHIFT	12
#define STE_VSID_VEC	0xfffffffffffffULL
#define STE_VSID_MASK	(STE_VSID_VEC << STE_VSID_SHIFT)

#define STE_PTR_CLEAR(ste)         (ste->esidWord = 0, ste->vsidWord = 0)

#define STE_PTR_SET(newSTE, ste) \
	(ste->esidWord = newSTE->esidWord, ste->vsidWord = newSTE->vsidWord)

#define STE_PTR_ESID_GET(ste) \
	((ste->esidWord >> STE_ESID_SHIFT) & STE_ESID_VEC)

#define STE_PTR_ESID_SET(ste, val) \
	(ste->esidWord |= (((val) & STE_ESID_VEC) << STE_ESID_SHIFT))

#define STE_PTR_V_GET(ste)         ((ste->esidWord >> STE_V_SHIFT) & STE_V_VEC)
#define STE_PTR_V_SET(ste, val) \
	(ste->esidWord |= (((val) & STE_V_VEC) << STE_V_SHIFT))

#define STE_PTR_T_GET(ste)         ((ste->esidWord >> STE_T_SHIFT) & STE_T_VEC)
#define STE_PTR_T_SET(ste, val) \
	(ste->esidWord |= (((val) & STE_T_VEC) << STE_T_SHIFT))

#define STE_PTR_KS_GET(ste) \
        ((ste->esidWord >> STE_KS_SHIFT) & STE_KS_VEC)
#define STE_PTR_KS_SET(ste, val) \
	(ste->esidWord |= (((val) & STE_KS_VEC) << STE_KS_SHIFT))

#define STE_PTR_KP_GET(ste) \
        ((ste->esidWord >> STE_KP_SHIFT) & STE_KP_VEC)
#define STE_PTR_KP_SET(ste, val) \
	(ste->esidWord |= (((val) & STE_KP_VEC) << STE_KP_SHIFT))

#define STE_PTR_VSID_GET(ste) \
	((ste->vsidWord >> STE_VSID_SHIFT) & STE_VSID_VEC)
#define STE_PTR_VSID_SET(ste, val) (ste->vsidWord |= (((val) & STE_VSID_VEC) << STE_VSID_SHIFT))

#define STE_CLEAR(ste)             (ste.esidWord = 0, ste.vsidWord = 0)
#define STE_SET(newSTE, ste) \
       (ste->esidWord = newSTE.esidWord, ste->vsidWord = newSTE.vsidWord)

#define STE_ESID_GET(ste) \
	((ste.esidWord >> STE_ESID_SHIFT) & STE_ESID_VEC)

#define STE_ESID_SET(ste, val) \
	(ste.esidWord |= (((val) & STE_ESID_VEC) << STE_ESID_SHIFT))

#define STE_V_GET(ste)             ((ste.esidWord >> STE_V_SHIFT) & STE_V_VEC)
#define STE_V_SET(ste, val) \
        (ste.esidWord |= (((val) & STE_V_VEC) << STE_V_SHIFT))

#define STE_T_GET(ste)             ((ste.esidWord >> STE_T_SHIFT) & STE_T_VEC)
#define STE_T_SET(ste, val) \
        (ste.esidWord |= (((val) & STE_T_VEC) << STE_T_SHIFT))

#define STE_KS_GET(ste)	((ste.esidWord >> STE_KS_SHIFT) & STE_KS_VEC)
#define STE_KS_SET(ste, val) \
       (ste.esidWord |= (((val) & STE_KS_VEC) << STE_KS_SHIFT))

#define STE_KP_GET(ste)	((ste.esidWord >> STE_KP_SHIFT) & STE_KP_VEC)
#define STE_KP_SET(ste, val) \
	(ste.esidWord |= (((val) & STE_KP_VEC) << STE_KP_SHIFT))

#define STE_VSID_GET(ste) \
	((ste.vsidWord >> STE_VSID_SHIFT) & STE_VSID_VEC)
#define STE_VSID_SET(ste, val) \
	(ste.vsidWord |= (((val) & STE_ESID_VEC) << STE_VSID_SHIFT))

struct ste {
	uval64 esidWord;
	uval64 vsidWord;
};

struct steg {
	struct ste entry[8];
};

static __inline__ uval
get_sdr1(void)
{
	uval ret;
	__asm__ ("mfsdr1 %0" : "=r"(ret));
	return ret;
}

static __inline__ uval
get_asr(void)
{
	uval ret;
	__asm__ ("mfasr %0" : "=r"(ret));
	return ret;
}

#define SDR1_HTABORG_MASK	0x3ffffffffffc0000ULL
#define SDR1_HTABSIZE_MASK	0x1fUL
#define SDR1_HTABSIZE_MAX	28
#define SDR1_HTABSIZE_BASE	11
#define GET_HTAB(os) ((os)->htab.sdr1 & SDR1_HTABORG_MASK)
#ifdef HAS_ASR
#define GET_STAB(pcop) ((pcop)->reg_asr & ~0x1UL)
#else
#define GET_STAB(pcop) 0
#endif

/* hv maintains a PTE eXtension which associates other bits of data
 * with a PTE, and these are kept in a shadow array.  Currently this
 * value is the logical page number used to create the PTE.
 */

struct logical_htab {
	uval64 sdr1;
	uval num_ptes;		/* number of PTEs in HTAB. */
	uval *shadow;		/* idx -> logical translation array */
};

typedef char htab_t[HTAB_BYTES];

extern void pte_insert(struct logical_htab *pt, uval ptex,
		       const uval64 vsidWord, const uval64 rpnWord, uval lrpn);

/* pte_invalidate:
 *	pt: extended hpte to use
 *	pte: PTE to invalidate
 */
static inline void
pte_invalidate(struct logical_htab *pt, union pte *pte)
{
	union pte *base = (union pte *)(pt->sdr1 & SDR1_HTABORG_MASK);
	uval ptex = ((uval)pte - (uval)base) / sizeof (union pte);

	pte->bits.v = 0;
	pt->shadow[ptex] = 0;
}

#endif

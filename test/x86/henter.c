/*
 * Test loading from shadow page table
 */
#include <test.h>
#include <lpar.h>
#include <hcall.h>
#include <mmu.h>

/*
 * We're running on a page table that maps CHUNKSIZE bytes
 * starting at 0, with vma == lma.  See crt_init.c.
 *
 * We remap the last 4M read-only at vma CHUNKSIZE.
 *
 * Do an hcall_page_dir to reinstall cr3.
 *
 * Then write the values through vma CHUNKSIZE and read them through
 * vma (CHUNKSIZE - 4M).  The values should be consistent.
 */

extern uval pgdir[];

uval
test_os(uval argc __attribute__ ((unused)),
	uval argv[] __attribute__ ((unused)))
{
	uval i;
	uval pt_flags = 7, pd_flags = 7;
	uval ret[1];
	uval new_vma, old_vma, lma;
	uval *pgtbl, *ptr1, *ptr2;

	new_vma = (1ul << LOG_CHUNKSIZE);
	old_vma = new_vma - 0x400000;
	lma = old_vma;

	pgdir[new_vma >> LOG_PDSIZE] |= pd_flags;
	pgtbl = (uval *) pte_pageaddr(pgdir[new_vma >> LOG_PDSIZE]);

	hprintf("pgdir: %p, pgtbl: %p\n", pgdir, pgtbl);

	for (i = 0; i < 1024; i++) {
		pgtbl[i] = (lma + (i << LOG_PGSIZE)) | pt_flags;
	}

        rrupts_off();

        i = hcall_page_dir(ret, 0, (uval)pgdir);
        assert(i == H_Success, "h_enter failed\n");

        rrupts_on();

	/* start accessing from 4M, 8M */
	hprintf("Start accessing ...\n");
	ptr1 = (uval *) new_vma;
	ptr2 = (uval *) old_vma;
	for (i = 0; i < (0x400000/sizeof(uval)); i += 0x1000) {
		ptr1[i] = i;
		if (ptr2[i] != i) {
			hprintf("i = 0x%lx, ptr1[i] = 0x%lx, ptr2[i] = 0x%lx\n",
				i, ptr1[i], ptr2[i]);
		} else {
			hprintf("i = 0x%lx matches\n", i);
		}

	}
	hprintf("Done, returning ...");

	return 0;
}

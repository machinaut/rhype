/*
 * FPU persistence test
 */
#include <test.h>
#include <lpar.h>
#include <hcall.h>

uval
test_os(uval argc __attribute__ ((unused)),
	uval argv[] __attribute__ ((unused)))
{
	uval32 id = pinfo[1].lpid & 0xF;
	char s[2];

	/*
	 * Store id in floating-point unit once and for all.
	 */
	asm("fildl %0" : : "m" (id));

	for (;;) {
		int x;
		/*
		 * Retrieve value from floating-point unit and convert
		 * it to a printable character.
		 */
		asm("fistl %0" : "=m" (x) : );
		s[0] = x + '0';
		s[1] = '\0';

		hputs(s);
	}

	return 0;
}

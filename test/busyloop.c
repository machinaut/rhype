/*
 * Busy-loop test
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

	if (id < 10)
		s[0] = id + '0';
	else
		s[0] = id + 'A';
	s[1] = '\0';


	for (;;)
		hputs(s);

	return 0;
}


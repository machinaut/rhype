#include <config.h>
#include <types.h>
#include <debug.h>

uval debug_level = 0;

struct debug_info debugs[32] = {
	{ .prefix = NULL, .output_fn = NULL },
};


#include <asm.h>
#include <hypervisor.h>

#define TEST_STACK_SIZE 0x40000

.text

C_TEXT_ENTRY(_text_start) /* for V2P() */
C_TEXT_ENTRY(_start)
	jmp __start

	/* at offset 0x10 is a pointer to the partition info */
	. = 0x10
_partition_info: 
	.quad HYPE_PARTITION_INFO_MAGIC_NUMBER 
        .long pinfo 

__start:
	/* initialize stack */
	movl	$teststack, %esp
	movl	$teststack, %ebp

	call	crt_init
	jmp	.

.data

	## Used as stack for this test os
teststackguard:
	.long 0x01234567
	.space TEST_STACK_SIZE - MIN_FRAME_SZ
teststack:
	. = . + MIN_FRAME_SZ


/* exceptions.S
 * low level exception handling code for test OSs
 *
 */

.text
.global default_handler
default_handler:
	int	$0xF1		// iret
	jmp	.

.global async_handler
async_handler:
	pusha
	pushf
	call aipc_handler
	popf
	popa
	int	$0xF1		// iret
	jmp	.

tmp:
	.long 	0

.global pfault_handler
pfault_handler:
	pusha
	call do_pfault
	popa
	popl 	tmp		// fault address
	popl	tmp
	int	$0xF1		// iret
	jmp	.

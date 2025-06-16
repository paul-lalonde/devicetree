
.option arch, +zicsr
		.global pt
        .global _start
		.global mmuenable
        .section .text.kernel

_start:

		# set up a stack at 0x80400000
		li a0, 0x80400000
		add	sp, x0, a0

		# set up our function call.  Allocate 32 bytes
    	addi    sp, sp, -32     # Allocate 32 bytes from the stack
    	sd      ra, 0(sp)       # Since we are making calls, we need the original ra


# This shennanigan rebases the pc to 0 instead of 0x200000;
# this however, turns out to be bad: we're system software
# and have to cooperate with SBI which doesn't have its own
# page tables.
#readpc:
#		auipc a0, 0x0
#		li a1, 0x001FFFFF
#		and a0, a0, a1
#		addi a0, a0, %lo(donemmu)
#		addi a1, x0, %lo(readpc)
#		sub a0, a0, a1
#		jr a0

		mv a0, a1
		call pre_main

        jal ra, mmuenable

		call main

    # Restore original RA and "return"
    	ld      ra, 0(sp)
    	addi    sp, sp, 32       # Always deallocate the stack!
	
donemmu:		

	    li a7, 0x4442434E
        li a6, 0x00
        li a0, 20
        lla a1, debug_string
        li a2, 0
        ecall


loop:   j loop

mmuenable:
	lla	a0, pt
	li a1, (8<<60)
	srli a0, a0, 12
	or a0, a0, a1
	sfence.vma
	csrw satp,a0
	sfence.vma
	ret
.global asm_print

asm_print:
		addi sp, sp, -32
		sd ra, 24(sp) # return address
		sd s0, 16(sp) # frame pointer
		addi s0, sp, 32  # set up our new frame pointer, 32 bytes past our stack
	    li a7, 0x4442434E
        li a6, 0x00
		li a2, 0
		# a0 and a1 already have our params
		ecall

 		ld ra, 24(sp) # restore return address
		ld s0, 16(sp) # restore return frame pointer
		addi sp, sp, 32 # drop the stack
		ret

        .section .rodata
debug_string:
        .string "Done.  Infinite loop.\n"


	.balign 8192
pt: 
	.dword 0 
	.dword 0
	.dword 0x20081401
	.fill 509, 8, 0
	.dword 0 
	.dword 0x200800cf
	.fill 510, 8, 0

.global fdt_header

fdt_header:
	.fill 0x2000, 1, 0

.globl cstrlen
.globl cvt
.type cstrlen, %function
.type cvt, %function
.text

/*
 * counts the length of the string pointed to
 * by %r4, returning len in %r6.
 * Changes r4, r5, r6
 * Doesn't follow calling convention, but only used here so whatever
 */
cstrlen:
	eor %r6, %r6

.lenloop:
	ldrb %r5, [%r4], #1 /* load byte into r1 from address in r0, increment r0 by 1 */
	cmp %r5, #0
	addne %r6, %r6, #1 /* Increment r2 if r1 not equal to '\0' */
	bne .lenloop /* repeat if r1 not equal to 0 */
	
	mov pc, lr /* Copy contents of link register to program counter to return */

/*
 * iterate over the strings for argc, and put
 * them into the args array. 
 * Entries of dest vector composed of int and char*.
 * 
 * argc in r0, argv in r1, dest vector in r2
 */
 cvt:
	push {r4-r6, lr} /* Push ip as dummy register to keep stack 8-byte aligned */

.cvtloop:
 	cmp %r0, #0
	beq .cvtdone
	
	sub %r0, %r0, #1
	ldr %r3, [%r1] /* load char* pointed to by r1 into r2 */
	ldr %r4, [%r1], #4 /* load char* from r1 into r4, increment pointer by word size */
	bl cstrlen /* get string length of r4, store in r6 */

	str %r3, [%r2] /* store string pointer in vector */
	str %r6, [%r2, #4] /* store length in vector, 4 bytes after pointer */
	add %r2, %r2, #8 /* move to next vector entry */
	b .cvtloop

.cvtdone:
	mov %r0, #0
	pop {r4-r6, pc} /* return by loading old lr contents to pc */

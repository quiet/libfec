/* Intel SIMD SSE implementation of Viterbi ACS butterflies
   for 256-state (k=9) convolutional code
   Copyright 2004 Phil Karn, KA9Q
   This code may be used under the terms of the GNU Lesser General Public License (LGPL)

   void update_viterbi29_blk_sse(struct v29 *vp,unsigned char syms[],int nbits); 
*/
	# SSE (64-bit integer SIMD) version
	# Requires Pentium III or better
	# These are offsets into struct v29, defined in viterbi29.h
	.set DP,512
	.set OLDMETRICS,516
	.set NEWMETRICS,520
	.text	
	.global update_viterbi29_blk_sse,Branchtab29_sse
	.type update_viterbi29_blk_sse,@function
	.align 16
	
update_viterbi29_blk_sse:
	pushl %ebp
	movl %esp,%ebp
	pushl %esi
	pushl %edi
	pushl %edx
	pushl %ebx

	movl 8(%ebp),%edx	# edx = vp
	testl %edx,%edx
	jnz  0f
	movl -1,%eax
	jmp  err		
0:	movl OLDMETRICS(%edx),%esi	# esi -> old metrics
	movl NEWMETRICS(%edx),%edi	# edi -> new metrics
	movl DP(%edx),%edx	# edx -> decisions

1:	movl 16(%ebp),%eax	# eax = nbits
	decl %eax
	jl   2f			# passed zero, we're done
	movl %eax,16(%ebp)
	
	xorl %eax,%eax
	movl 12(%ebp),%ebx	# ebx = syms
	movb (%ebx),%al
	movd %eax,%mm6		# mm6[0] = first symbol
	movb 1(%ebx),%al
	movd %eax,%mm5		# mm5[0] = second symbol
	addl $2,%ebx
	movl %ebx,12(%ebp)

	punpcklbw %mm6,%mm6	# mm6[1] = mm6[0]
	punpcklbw %mm5,%mm5

	movq thirtyones,%mm7
	pshufw $0,%mm6,%mm6	# copy low word to upper 3
	pshufw $0,%mm5,%mm5
	# mm6 now contains first symbol in each byte, mm5 the second

	# each invocation of this macro does 8 butterflies in parallel
	.MACRO butterfly GROUP
	# compute branch metrics
	movq Branchtab29_sse+(8*\GROUP),%mm4
	movq Branchtab29_sse+128+(8*\GROUP),%mm3
	pxor %mm6,%mm4
	pxor %mm5,%mm3
	pavgb %mm3,%mm4			# mm4 contains branch metrics
	psrlw $3,%mm4
	pand %mm7,%mm4

	movq (8*\GROUP)(%esi),%mm0	# Incoming path metric, high bit = 0
	movq ((8*\GROUP)+128)(%esi),%mm3	# Incoming path metric, high bit = 1
	movq %mm0,%mm2
	movq %mm3,%mm1
	paddusb %mm4,%mm0
	paddusb %mm4,%mm3
	
	# invert branch metrics. This works only because they're 5 bits
	pxor %mm7,%mm4
	
	paddusb %mm4,%mm1
	paddusb %mm4,%mm2
	
	# Find survivors, leave in mm0,2
	pminub %mm1,%mm0
	pminub %mm3,%mm2
	# get decisions, leave in mm1,3
	pcmpeqb %mm0,%mm1
	pcmpeqb %mm2,%mm3
	
	# interleave and store new branch metrics in mm0,2
	movq %mm0,%mm4
	punpckhbw %mm2,%mm0	# interleave second 8 new metrics
	punpcklbw %mm2,%mm4	# interleave first 8 new metrics
	movq %mm0,(16*\GROUP+8)(%edi)
	movq %mm4,(16*\GROUP)(%edi)

	# interleave decisions, accumulate into %ebx
	movq %mm1,%mm4
	punpckhbw %mm3,%mm1
	punpcklbw %mm3,%mm4
	# Due to an error in the Intel instruction set ref (the register
	# fields are swapped), gas assembles pmovmskb incorrectly
	# See http://mail.gnu.org/pipermail/bug-gnu-utils/2000-August/002341.html
	.byte 0x0f,0xd7,0xc1	# pmovmskb %mm1,%eax
	shll $((16*\GROUP+8)&31),%eax
	orl %eax,%ebx
	.byte 0x0f,0xd7,0xc4	# pmovmskb %mm4,%eax
	shll $((16*\GROUP)&31),%eax
	orl %eax,%ebx
	.endm

	# invoke macro 16 times for a total of 128 butterflies
	xorl %ebx,%ebx		# clear decisions
	butterfly GROUP=0
	butterfly GROUP=1
	movl %ebx,(%edx)	# stash first 32 decisions
	xorl %ebx,%ebx
	butterfly GROUP=2
	butterfly GROUP=3
	movl %ebx,4(%edx)	# stash second 32 decisions
	xorl %ebx,%ebx		# clear decisions
	butterfly GROUP=4
	butterfly GROUP=5
	movl %ebx,8(%edx)	# stash first 32 decisions
	xorl %ebx,%ebx
	butterfly GROUP=6
	butterfly GROUP=7
	movl %ebx,12(%edx)	# stash second 32 decisions
	xorl %ebx,%ebx		# clear decisions
	butterfly GROUP=8
	butterfly GROUP=9
	movl %ebx,16(%edx)	# stash first 32 decisions
	xorl %ebx,%ebx
	butterfly GROUP=10
	butterfly GROUP=11
	movl %ebx,20(%edx)	# stash second 32 decisions
	xorl %ebx,%ebx		# clear decisions
	butterfly GROUP=12
	butterfly GROUP=13
	movl %ebx,24(%edx)	# stash first 32 decisions
	xorl %ebx,%ebx
	butterfly GROUP=14
	butterfly GROUP=15
	movl %ebx,28(%edx)	# stash second 32 decisions

	addl $32,%edx		# bump decision pointer
		
	# see if we have to normalize
	movl (%edi),%eax	# extract first output metric
	andl $255,%eax
	cmp $50,%eax		# is it greater than 50?
	movl $0,%eax
	jle done		# No, no need to normalize

	# Normalize by finding smallest metric and subtracting it
	# from all metrics
	movq (%edi),%mm0
	pminub 8(%edi),%mm0
	pminub 16(%edi),%mm0
	pminub 24(%edi),%mm0
	pminub 32(%edi),%mm0
	pminub 40(%edi),%mm0
	pminub 48(%edi),%mm0
	pminub 56(%edi),%mm0
	pminub 64(%edi),%mm0
	pminub 72(%edi),%mm0
	pminub 80(%edi),%mm0	
	pminub 88(%edi),%mm0
	pminub 96(%edi),%mm0
	pminub 104(%edi),%mm0
	pminub 112(%edi),%mm0
	pminub 120(%edi),%mm0
	pminub 128(%edi),%mm0
	pminub 136(%edi),%mm0
	pminub 144(%edi),%mm0
	pminub 152(%edi),%mm0
	pminub 160(%edi),%mm0
	pminub 168(%edi),%mm0
	pminub 176(%edi),%mm0
	pminub 184(%edi),%mm0
	pminub 192(%edi),%mm0
	pminub 200(%edi),%mm0
	pminub 208(%edi),%mm0
	pminub 216(%edi),%mm0
	pminub 224(%edi),%mm0
	pminub 232(%edi),%mm0
	pminub 240(%edi),%mm0
	pminub 248(%edi),%mm0
	# mm0 contains 8 smallest metrics
	# crunch down to single lowest metric
	movq %mm0,%mm1
	psrlq $32,%mm0
	pminub %mm1,%mm0
	movq %mm0,%mm1
	psrlq $16,%mm0
	pminub %mm1,%mm0
	movq %mm0,%mm1
	psrlq $8,%mm0
	pminub %mm1,%mm0
	movq 8(%edi),%mm1	# reload
	punpcklbw %mm0,%mm0	# expand to all 8 bytes
	pshufw $0,%mm0,%mm0

	# mm0 now contains lowest metric in all 8 bytes
	# subtract it from every output metric
	# Trashes %mm7
	.macro PSUBUSBM REG,MEM
	movq \MEM,%mm7
	psubusb \REG,%mm7
	movq %mm7,\MEM
	.endm
	
	PSUBUSBM %mm0,(%edi)
	PSUBUSBM %mm0,8(%edi)
	PSUBUSBM %mm0,16(%edi)
	PSUBUSBM %mm0,24(%edi)
	PSUBUSBM %mm0,32(%edi)
	PSUBUSBM %mm0,40(%edi)
	PSUBUSBM %mm0,48(%edi)
	PSUBUSBM %mm0,56(%edi)
	PSUBUSBM %mm0,64(%edi)
	PSUBUSBM %mm0,72(%edi)
	PSUBUSBM %mm0,80(%edi)	
	PSUBUSBM %mm0,88(%edi)
	PSUBUSBM %mm0,96(%edi)
	PSUBUSBM %mm0,104(%edi)
	PSUBUSBM %mm0,112(%edi)
	PSUBUSBM %mm0,120(%edi)
	PSUBUSBM %mm0,128(%edi)
	PSUBUSBM %mm0,136(%edi)
	PSUBUSBM %mm0,144(%edi)
	PSUBUSBM %mm0,152(%edi)
	PSUBUSBM %mm0,160(%edi)
	PSUBUSBM %mm0,168(%edi)
	PSUBUSBM %mm0,176(%edi)
	PSUBUSBM %mm0,184(%edi)
	PSUBUSBM %mm0,192(%edi)
	PSUBUSBM %mm0,200(%edi)
	PSUBUSBM %mm0,208(%edi)
	PSUBUSBM %mm0,216(%edi)
	PSUBUSBM %mm0,224(%edi)
	PSUBUSBM %mm0,232(%edi)
	PSUBUSBM %mm0,240(%edi)
	PSUBUSBM %mm0,248(%edi)

done:		
	# swap metrics
	movl %esi,%eax
	movl %edi,%esi
	movl %eax,%edi
	jmp 1b
	
2:	emms
	movl 8(%ebp),%ebx	# ebx = vp
	# stash metric pointers
	movl %esi,OLDMETRICS(%ebx)
	movl %edi,NEWMETRICS(%ebx)
	movl %edx,DP(%ebx)	# stash incremented value of vp->dp
	xorl %eax,%eax
err:	popl %ebx
	popl %edx
	popl %edi
	popl %esi
	popl %ebp
	ret

	.data
	.align 8
thirtyones:	
	.byte 31,31,31,31,31,31,31,31
	


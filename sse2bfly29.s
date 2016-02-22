/* Intel SIMD SSE2 implementation of Viterbi ACS butterflies
   for 256-state (k=9) convolutional code
   Copyright 2004 Phil Karn, KA9Q
   This code may be used under the terms of the GNU Lesser General Public License (LGPL)

   void update_viterbi29_blk_sse2(struct v29 *vp,unsigned char *syms,int nbits) ; 
*/

	# SSE2 (128-bit integer SIMD) version
	# Requires Pentium 4 or better
	# These are offsets into struct v29, defined in viterbi29.h
	.set DP,512
	.set OLDMETRICS,516
	.set NEWMETRICS,520

	.text	
	.global update_viterbi29_blk_sse2,Branchtab29_sse2
	.type update_viterbi29_blk_sse2,@function
	.align 16
	
update_viterbi29_blk_sse2:
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
	movd %eax,%xmm6		# xmm6[0] = first symbol
	movb 1(%ebx),%al
	movd %eax,%xmm5		# xmm5[0] = second symbol
	addl $2,%ebx
	movl %ebx,12(%ebp)

	punpcklbw %xmm6,%xmm6	# xmm6[1] = xmm6[0]
	punpcklbw %xmm5,%xmm5
	movdqa thirtyones,%xmm7
	pshuflw $0,%xmm6,%xmm6	# copy low word to low 3
	pshuflw $0,%xmm5,%xmm5
	punpcklqdq %xmm6,%xmm6  # propagate to all 16
	punpcklqdq %xmm5,%xmm5
	# xmm6 now contains first symbol in each byte, xmm5 the second

	movdqa thirtyones,%xmm7
	
	# each invocation of this macro does 16 butterflies in parallel
	.MACRO butterfly GROUP
	# compute branch metrics
	movdqa Branchtab29_sse2+(16*\GROUP),%xmm4
	movdqa Branchtab29_sse2+128+(16*\GROUP),%xmm3
	pxor %xmm6,%xmm4
	pxor %xmm5,%xmm3
	pavgb %xmm3,%xmm4
	psrlw $3,%xmm4

	pand %xmm7,%xmm4	# xmm4 contains branch metrics
	
	movdqa (16*\GROUP)(%esi),%xmm0	# Incoming path metric, high bit = 0
	movdqa ((16*\GROUP)+128)(%esi),%xmm3	# Incoming path metric, high bit = 1
	movdqa %xmm0,%xmm2
	movdqa %xmm3,%xmm1
	paddusb %xmm4,%xmm0
	paddusb %xmm4,%xmm3
	
	# invert branch metrics
	pxor %xmm7,%xmm4
	
	paddusb %xmm4,%xmm1
	paddusb %xmm4,%xmm2
	
	# Find survivors, leave in mm0,2
	pminub %xmm1,%xmm0
	pminub %xmm3,%xmm2
	# get decisions, leave in mm1,3
	pcmpeqb %xmm0,%xmm1
	pcmpeqb %xmm2,%xmm3
	
	# interleave and store new branch metrics in mm0,2
	movdqa %xmm0,%xmm4
	punpckhbw %xmm2,%xmm0	# interleave second 16 new metrics
	punpcklbw %xmm2,%xmm4	# interleave first 16 new metrics
	movdqa %xmm0,(32*\GROUP+16)(%edi)
	movdqa %xmm4,(32*\GROUP)(%edi)

	# interleave decisions & store
	movdqa %xmm1,%xmm4
	punpckhbw %xmm3,%xmm1
	punpcklbw %xmm3,%xmm4
	# work around bug in gas due to Intel doc error
	.byte 0x66,0x0f,0xd7,0xd9	# pmovmskb %xmm1,%ebx
	shll $16,%ebx
	.byte 0x66,0x0f,0xd7,0xc4	# pmovmskb %xmm4,%eax
	orl %eax,%ebx
	movl %ebx,(4*\GROUP)(%edx)
	.endm

	# invoke macro 8 times for a total of 128 butterflies
	butterfly GROUP=0
	butterfly GROUP=1
	butterfly GROUP=2
	butterfly GROUP=3
	butterfly GROUP=4
	butterfly GROUP=5
	butterfly GROUP=6
	butterfly GROUP=7

	addl $32,%edx		# bump decision pointer
		
	# see if we have to normalize
	movl (%edi),%eax	# extract first output metric
	andl $255,%eax
	cmp $50,%eax		# is it greater than 50?
	movl $0,%eax
	jle done		# No, no need to normalize

	# Normalize by finding smallest metric and subtracting it
	# from all metrics
	movdqa (%edi),%xmm0
	pminub 16(%edi),%xmm0
	pminub 32(%edi),%xmm0
	pminub 48(%edi),%xmm0
	pminub 64(%edi),%xmm0
	pminub 80(%edi),%xmm0
	pminub 96(%edi),%xmm0	
	pminub 112(%edi),%xmm0	
	pminub 128(%edi),%xmm0
	pminub 144(%edi),%xmm0
	pminub 160(%edi),%xmm0
	pminub 176(%edi),%xmm0
	pminub 192(%edi),%xmm0
	pminub 208(%edi),%xmm0
	pminub 224(%edi),%xmm0
	pminub 240(%edi),%xmm0							

	# crunch down to single lowest metric
	movdqa %xmm0,%xmm1
	psrldq $8,%xmm0     # the count to psrldq is bytes, not bits!
	pminub %xmm1,%xmm0
	movdqa %xmm0,%xmm1
	psrlq $32,%xmm0
	pminub %xmm1,%xmm0
	movdqa %xmm0,%xmm1
	psrlq $16,%xmm0
	pminub %xmm1,%xmm0
	movdqa %xmm0,%xmm1
	psrlq $8,%xmm0
	pminub %xmm1,%xmm0

	punpcklbw %xmm0,%xmm0	# lowest 2 bytes
	pshuflw $0,%xmm0,%xmm0  # lowest 8 bytes
	punpcklqdq %xmm0,%xmm0	# all 16 bytes

	# xmm0 now contains lowest metric in all 16 bytes
	# subtract it from every output metric
	movdqa (%edi),%xmm1
	psubusb %xmm0,%xmm1
	movdqa %xmm1,(%edi)
	movdqa 16(%edi),%xmm1
	psubusb %xmm0,%xmm1
	movdqa %xmm1,16(%edi)	
	movdqa 32(%edi),%xmm1
	psubusb %xmm0,%xmm1	
	movdqa %xmm1,32(%edi)	
	movdqa 48(%edi),%xmm1
	psubusb %xmm0,%xmm1	
	movdqa %xmm1,48(%edi)	
	movdqa 64(%edi),%xmm1
	psubusb %xmm0,%xmm1	
	movdqa %xmm1,64(%edi)	
	movdqa 80(%edi),%xmm1
	psubusb %xmm0,%xmm1	
	movdqa %xmm1,80(%edi)	
	movdqa 96(%edi),%xmm1	
	psubusb %xmm0,%xmm1	
	movdqa %xmm1,96(%edi)	
	movdqa 112(%edi),%xmm1	
	psubusb %xmm0,%xmm1	
	movdqa %xmm1,112(%edi)	
	movdqa 128(%edi),%xmm1
	psubusb %xmm0,%xmm1	
	movdqa %xmm1,128(%edi)	
	movdqa 144(%edi),%xmm1
	psubusb %xmm0,%xmm1	
	movdqa %xmm1,144(%edi)	
	movdqa 160(%edi),%xmm1
	psubusb %xmm0,%xmm1	
	movdqa %xmm1,160(%edi)	
	movdqa 176(%edi),%xmm1
	psubusb %xmm0,%xmm1
	movdqa %xmm1,176(%edi)	
	movdqa 192(%edi),%xmm1
	psubusb %xmm0,%xmm1	
	movdqa %xmm1,192(%edi)	
	movdqa 208(%edi),%xmm1
	psubusb %xmm0,%xmm1
	movdqa %xmm1,208(%edi)	
	movdqa 224(%edi),%xmm1
	psubusb %xmm0,%xmm1	
	movdqa %xmm1,224(%edi)	
	movdqa 240(%edi),%xmm1							
	psubusb %xmm0,%xmm1	
	movdqa %xmm1,240(%edi)	
	
done:		
	# swap metrics
	movl %esi,%eax
	movl %edi,%esi
	movl %eax,%edi
	jmp 1b
	
2:	movl 8(%ebp),%ebx	# ebx = vp
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
	.align 16
thirtyones:	
	.byte 31,31,31,31,31,31,31,31,31,31,31,31,31,31,31,31


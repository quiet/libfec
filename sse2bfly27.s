/* Intel SIMD (SSE2) implementations of Viterbi ACS butterflies
   for 64-state (k=7) convolutional code
   Copyright 2003 Phil Karn, KA9Q
   This code may be used under the terms of the GNU Lesser General Public License (LGPL)

   void update_viterbi27_blk_sse2(struct v27 *vp,unsigned char syms[],int nbits) ; 
*/
	# SSE2 (128-bit integer SIMD) version
	# Requires Pentium 4 or better

	# These are offsets into struct v27, defined in viterbi27.h
	.set DP,128
	.set OLDMETRICS,132
	.set NEWMETRICS,136
	.text	
	.global update_viterbi27_blk_sse2,Branchtab27_sse2
	.type update_viterbi27_blk_sse2,@function
	.align 16
	
update_viterbi27_blk_sse2:
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
	pshuflw $0,%xmm6,%xmm6	# copy low word to low 3
	pshuflw $0,%xmm5,%xmm5
	punpcklqdq %xmm6,%xmm6  # propagate to all 16
	punpcklqdq %xmm5,%xmm5
	# xmm6 now contains first symbol in each byte, xmm5 the second

	movdqa thirtyones,%xmm7
	
	# each invocation of this macro does 16 butterflies in parallel
	.MACRO butterfly GROUP
	# compute branch metrics
	movdqa Branchtab27_sse2+(16*\GROUP),%xmm4
	movdqa Branchtab27_sse2+32+(16*\GROUP),%xmm3
	pxor %xmm6,%xmm4
	pxor %xmm5,%xmm3
	
	# compute 5-bit branch metric in xmm4 by adding the individual symbol metrics
	# This is okay for this
	# code because the worst-case metric spread (at high Eb/No) is only 120,
	# well within the range of our unsigned 8-bit path metrics, and even within
	# the range of signed 8-bit path metrics
	pavgb %xmm3,%xmm4
	psrlw $3,%xmm4

	pand %xmm7,%xmm4

	movdqa (16*\GROUP)(%esi),%xmm0	# Incoming path metric, high bit = 0
	movdqa ((16*\GROUP)+32)(%esi),%xmm3	# Incoming path metric, high bit = 1
	movdqa %xmm0,%xmm2
	movdqa %xmm3,%xmm1
	paddusb %xmm4,%xmm0	# note use of saturating arithmetic
	paddusb %xmm4,%xmm3	# this shouldn't be necessary, but why not?
	
	# negate branch metrics
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

	# invoke macro 2 times for a total of 32 butterflies
	butterfly GROUP=0
	butterfly GROUP=1

	addl $8,%edx		# bump decision pointer
		
	# See if we have to normalize. This requires an explanation. We don't want
	# our path metrics to exceed 255 on the *next* iteration. Since the
	# largest branch metric is 30, that means we don't want any to exceed 225
	# on *this* iteration. Rather than look them all, we just pick an arbitrary one
	# (the first) and see if it exceeds 225-120=105, where 120 is the experimentally-
	# determined worst-case metric spread for this code and branch metrics in the range 0-30.
	
	# This is extremely conservative, and empirical testing at a variety of Eb/Nos might
	# show that a higher threshold could be used without affecting BER performance
	movl (%edi),%eax	# extract first output metric
	andl $255,%eax
	cmp $105,%eax
	jle done		# No, no need to normalize

	# Normalize by finding smallest metric and subtracting it
	# from all metrics. We can't just pick an arbitrary small constant because
	# the minimum metric might be zero!
	movdqa (%edi),%xmm0
	movdqa %xmm0,%xmm4	
	movdqa 16(%edi),%xmm1
	pminub %xmm1,%xmm4
	movdqa 32(%edi),%xmm2
	pminub %xmm2,%xmm4	
	movdqa 48(%edi),%xmm3	
	pminub %xmm3,%xmm4

	# crunch down to single lowest metric
	movdqa %xmm4,%xmm5
	psrldq $8,%xmm5     # the count to psrldq is bytes, not bits!
	pminub %xmm5,%xmm4
	movdqa %xmm4,%xmm5
	psrlq $32,%xmm5
	pminub %xmm5,%xmm4
	movdqa %xmm4,%xmm5
	psrlq $16,%xmm5
	pminub %xmm5,%xmm4
	movdqa %xmm4,%xmm5
	psrlq $8,%xmm5
	pminub %xmm5,%xmm4	# now in lowest byte of %xmm4

	punpcklbw %xmm4,%xmm4	# lowest 2 bytes
	pshuflw $0,%xmm4,%xmm4  # lowest 8 bytes
	punpcklqdq %xmm4,%xmm4	# all 16 bytes
	
	# xmm4 now contains lowest metric in all 16 bytes
	# subtract it from every output metric
	psubusb %xmm4,%xmm0
	psubusb %xmm4,%xmm1
	psubusb %xmm4,%xmm2
	psubusb %xmm4,%xmm3	
	movdqa %xmm0,(%edi)
	movdqa %xmm1,16(%edi)	
	movdqa %xmm2,32(%edi)	
	movdqa %xmm3,48(%edi)	
	
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

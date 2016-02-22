# SSE2 assist routines for peakval
# Copyright 2001 Phil Karn, KA9Q
# May be used under the terms of the GNU Lesser General Public License (LGPL)

	.text

# Find peak absolute value in signed 16-bit input samples
#  int peakval_sse2_assist(signed short *in,int cnt);
	.global peakval_sse2_assist
	.type peakval_sse2_assist,@function
	.align 16
peakval_sse2_assist:
	pushl %ebp
	movl %esp,%ebp
	pushl %esi
	pushl %ecx

	movl 8(%ebp),%esi
	movl 12(%ebp),%ecx

	pxor %xmm7,%xmm7	# clear peak
	
1:	subl $8,%ecx
	jl 2f
	movaps (%esi),%xmm0
	movaps %xmm0,%xmm1	
	psraw $15,%xmm1		# xmm1 = 1's if negative, 0's if positive
	pxor %xmm1,%xmm0	# complement negatives
	psubw %xmm1,%xmm0	# add 1 to negatives
	pmaxsw %xmm0,%xmm7	# store peak
	
	addl $16,%esi
	jmp 1b

2:	movaps %xmm7,%xmm0
	psrldq $8,%xmm0
	pmaxsw %xmm0,%xmm7
	movaps %xmm7,%xmm0
	psrlq $32,%xmm0
	pmaxsw %xmm0,%xmm7
	movaps %xmm7,%xmm0
	psrlq $16,%xmm0
	pmaxsw %xmm0,%xmm7	# min value in low word of %xmm7
	
	movd %xmm7,%eax
	andl $0xffff,%eax

	popl %ecx
	popl %esi
	popl %ebp
	ret

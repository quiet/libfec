# SSE assist routines for peakval
# Copyright 2001 Phil Karn, KA9Q
# May be used under the terms of the GNU Lesser General Public License (LGPL)

	.text

# Find peak absolute value in signed 16-bit input samples
#  int peakval_sse_assist(signed short *in,int cnt);
	.global peakval_sse_assist
	.type peakval_sse_assist,@function
	.align 16
peakval_sse_assist:
	pushl %ebp
	movl %esp,%ebp
	pushl %esi
	pushl %ecx

	movl 8(%ebp),%esi
	movl 12(%ebp),%ecx

	pxor %mm7,%mm7		# clear peak
	
1:	subl $4,%ecx
	jl 2f
	movq (%esi),%mm0
	movq %mm0,%mm1	
	psraw $15,%mm1		# mm1 = 1's if negative, 0's if positive
	pxor %mm1,%mm0		# complement negatives
	psubw %mm1,%mm0		# add 1 to negatives
	pmaxsw %mm0,%mm7	# store peak
	
	addl $8,%esi
	jmp 1b	

2:	movq %mm7,%mm0
	psrlq $32,%mm0
	pmaxsw %mm0,%mm7
	movq %mm7,%mm0
	psrlq $16,%mm0
	pmaxsw %mm0,%mm7	# min value in low word of %mm7
	
	movd %mm7,%eax
	andl $0xffff,%eax

	emms
	popl %ecx
	popl %esi
	popl %ebp
	ret

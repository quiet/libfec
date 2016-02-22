# SSE2 assist routines for sumsq
# Copyright 2001 Phil Karn, KA9Q
# May be used under the terms of the GNU Public License (GPL)

	.text
# Evaluate sum of squares of signed 16-bit input samples
#  long long sumsq_sse2_assist(signed short *in,int cnt);	
	.global sumsq_sse2_assist
	.type sumsq_sse2_assist,@function
	.align 16
sumsq_sse2_assist:
	pushl %ebp
	movl %esp,%ebp
	pushl %esi
	pushl %ecx

	movl 8(%ebp),%esi
	movl 12(%ebp),%ecx
	pxor %xmm2,%xmm2		# zero sum
	movaps low,%xmm3		# load mask

1:	subl $8,%ecx
	jl 2f
	movaps (%esi),%xmm0	# S0 S1 S2 S3 S4 S5 S6 S7
	pmaddwd %xmm0,%xmm0	# (S0*S0+S1*S1) (S2*S2+S3*S3) (S4*S4+S5*S5) (S6*S6+S7*S7)
	movaps %xmm0,%xmm1
	pand %xmm3,%xmm1	# (S0*S0+S1*S1) 0 (S4*S4+S5*S5) 0
	paddq %xmm1,%xmm2	# sum even-numbered dwords
	psrlq $32,%xmm0		# (S2*S2+S3*S3) 0 (S6*S6+S7*S7) 0
	paddq %xmm0,%xmm2	# sum odd-numbered dwords
	addl $16,%esi
	jmp 1b	

2:	movaps %xmm2,%xmm0
	psrldq $8,%xmm0
	paddq %xmm2,%xmm0	# combine 64-bit sums

	movd %xmm0,%eax		# low 32 bits of sum
	psrldq $4,%xmm0
	movd %xmm0,%edx		# high 32 bits of sum
	
	popl %ecx
	popl %esi
	popl %ebp
	ret

	.data
	.align 16
low:	.byte 255,255,255,255,0,0,0,0,255,255,255,255,0,0,0,0

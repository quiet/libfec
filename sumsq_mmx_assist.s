# MMX assist routines for sumsq
# Copyright 2001 Phil Karn, KA9Q
# May be used under the terms of the GNU Public License (GPL)

	.text

# Evaluate sum of squares of signed 16-bit input samples
#  long long sumsq_mmx_assist(signed short *in,int cnt);	
	.global sumsq_mmx_assist
	.type sumsq_mmx_assist,@function
	.align 16
sumsq_mmx_assist:
	pushl %ebp
	movl %esp,%ebp
	pushl %esi
	pushl %ecx
	pushl %ebx

	movl 8(%ebp),%esi
	movl 12(%ebp),%ecx
	xor %eax,%eax
	xor %edx,%edx

	# Since 4 * 32767**2 < 2**32, we can accumulate two at a time
1:	subl $8,%ecx
	jl 2f
	movq (%esi),%mm0	# S0 S1 S2 S3
	pmaddwd %mm0,%mm0	# (S0^2+S1^2) (S2^2+S3^2)
	movq 8(%esi),%mm6	# S4 S5 S6 S7
	pmaddwd %mm6,%mm6	# (S4^2+S5^2) (S6^2+S7^2)
	paddd %mm6,%mm0		# (S0^2+S1^2+S4^2+S5^2)(S2^2+S3^2+S6^2+S7^2)
	movd %mm0,%ebx
	addl %ebx,%eax
	adcl $0,%edx
	psrlq $32,%mm0
	movd %mm0,%ebx
	addl %ebx,%eax
	adcl $0,%edx
	addl $16,%esi
	jmp 1b
	
2:	emms
	popl %ebx
	popl %ecx
	popl %esi
	popl %ebp
	ret
	
# Evaluate sum of squares of signed 16-bit input samples
#  long sumsq_wd_mmx_assist(signed short *in,int cnt);
#  Quick version, only safe for small numbers of small input values...
	.global sumsq_wd_mmx_assist
	.type sumsq_wd_mmx_assist,@function
	.align 16
sumsq_wd_mmx_assist:
	pushl %ebp
	movl %esp,%ebp
	pushl %esi

	movl 8(%ebp),%esi
	movl 12(%ebp),%ecx
	pxor %mm2,%mm2		# zero sum

1:	subl $8,%ecx
	jl 2f
	movq (%esi),%mm0	# S0 S1 S2 S3
	pmaddwd %mm0,%mm0	# (S0*S0+S1*S1) (S2*S2+S3*S3)
	movq 8(%esi),%mm1
	pmaddwd %mm1,%mm1
	paddd %mm1,%mm2
	paddd %mm0,%mm2		# accumulate

	addl $16,%esi
	jmp 1b	

2:	movd %mm2,%eax		# even sum	
	psrlq $32,%mm2
	movd %mm2,%edx		# odd sum
	addl %edx,%eax
	emms
	popl %esi
	popl %ebp
	ret

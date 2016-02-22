# SIMD SSE2 dot product
# Equivalent to the following C code:
# long dotprod(signed short *a,signed short *b,int cnt)
# {
#	long sum = 0; 
#	cnt *= 8; 
#	while(cnt--)
#		sum += *a++ + *b++;
#	return sum;
# }
# a and b must be 128-bit aligned
# Copyright 2001, Phil Karn KA9Q
# May be used under the terms of the GNU Lesser General Public License (LGPL)
	
	.text
	.global dotprod_sse2_assist
	.type dotprod_sse2_assist,@function
dotprod_sse2_assist:
	pushl %ebp
	movl %esp,%ebp
	pushl %esi
	pushl %edi
	pushl %ecx
	pushl %ebx
	movl 8(%ebp),%esi	# a
	movl 12(%ebp),%edi	# b
	movl 16(%ebp),%ecx	# cnt
	pxor %xmm0,%xmm0		# clear running sum (in two 32-bit halves)
	
# SSE2 dot product loop unrolled 4 times, crunching 32 terms per loop
	.align 16
.Loop1:	subl $4,%ecx
	jl   .Loop1Done
	
	movdqa (%esi),%xmm1
 	pmaddwd (%edi),%xmm1
	paddd %xmm1,%xmm0
	
	movdqa 16(%esi),%xmm1
	pmaddwd 16(%edi),%xmm1
	paddd %xmm1,%xmm0

	movdqa 32(%esi),%xmm1
	pmaddwd 32(%edi),%xmm1
	paddd %xmm1,%xmm0

	movdqa 48(%esi),%xmm1
	addl $64,%esi	
	pmaddwd 48(%edi),%xmm1
	addl $64,%edi	
	paddd %xmm1,%xmm0

	jmp .Loop1
.Loop1Done:
	
	addl $4,%ecx	
	
# SSE2 dot product loop, not unrolled, crunching 4 terms per loop
# This could be redone as Duff's Device on the unrolled loop above
.Loop2:	subl $1,%ecx
	jl   .Loop2Done
	
	movdqa (%esi),%xmm1
	addl $16,%esi
	pmaddwd (%edi),%xmm1
	addl $16,%edi
	paddd %xmm1,%xmm0
	jmp .Loop2
.Loop2Done:

	movdqa %xmm0,%xmm1
	psrldq $8,%xmm0
	paddd %xmm1,%xmm0
	movd %xmm0,%eax		# right-hand word to eax
	psrldq $4,%xmm0
	movd %xmm0,%ebx
	addl %ebx,%eax

	popl %ebx
	popl %ecx
	popl %edi
	popl %esi
	movl %ebp,%esp
	popl %ebp
	ret

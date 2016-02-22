# MMX assist routines for peakval
# Copyright 2001 Phil Karn, KA9Q
# May be used under the terms of the GNU Lesser General Public License (LGPL)

	.text

# Find peak value in signed 16-bit input samples
#  int peakval_mmx(signed short *in,int cnt);	
	.global peakval_mmx
	.type peakval_mmx,@function
	.align 16
peakval_mmx:
	pushl %ebp
	movl %esp,%ebp
	pushl %esi
	pushl %ecx
	pushl %ebx

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
	movq %mm7,%mm6		# copy previous peak
	pcmpgtw %mm0,%mm6	# ff == old peak greater
	pand %mm6,%mm7		# select old peaks that are greater
	pandn %mm0,%mm6		# select new values that are greater
	por %mm6,%mm7
	
	addl $8,%esi
	jmp 1b	

2:	movd %mm7,%eax
	psrlq $16,%mm7
	andl $0xffff,%eax
	
	movd %mm7,%edx
	psrlq $16,%mm7
	andl $0xffff,%edx
	cmpl %edx,%eax
	jnl  3f
	movl %edx,%eax
3:		
	movd %mm7,%edx
	psrlq $16,%mm7
	andl $0xffff,%edx
	cmpl %edx,%eax
	jnl 4f
	movl %edx,%eax
4:		
	movd %mm7,%edx
	andl $0xffff,%edx
	cmpl %edx,%eax
	jnl 5f
	movl %edx,%eax
5:	
	emms
	popl %ebx
	popl %ecx
	popl %esi
	popl %ebp
	ret
	

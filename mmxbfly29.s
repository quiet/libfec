/* Intel SIMD MMX implementation of Viterbi ACS butterflies
   for 256-state (k=9) convolutional code
   Copyright 2004 Phil Karn, KA9Q
   This code may be used under the terms of the GNU Lesser General Public License (LGPL)

   void update_viterbi29_blk_mmx(struct v29 *vp,unsigned char *syms,int nbits); 
*/

	# These are offsets into struct v29, defined in viterbi29.h
	.set DP,512
	.set OLDMETRICS,516
	.set NEWMETRICS,520
	.text	
	.global update_viterbi29_blk_mmx,Mettab29_1,Mettab29_2
	.type update_viterbi29_blk_mmx,@function
	.align 16
	
	# MMX (64-bit SIMD) version
	# requires Pentium-MMX, Pentium-II or better

update_viterbi29_blk_mmx:
	pushl %ebp
	movl %esp,%ebp
	pushl %esi
	pushl %edi
	pushl %edx
	pushl %ebx
	
	movl 8(%ebp),%edx	# edx = vp
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

	movl 12(%ebp),%ebx	# ebx = syms
	movw (%ebx),%ax		# ax = second symbol : first symbol
	addl $2,%ebx
	movl %ebx,12(%ebp)

	movb %ah,%bl
	andl $255,%eax
	andl $255,%ebx
	
	# shift into first array index dimension slot
	shll $7,%eax
	shll $7,%ebx

	# each invocation of this macro will do 8 butterflies in parallel
	.MACRO butterfly GROUP
	# Compute branch metrics
	movq (Mettab29_1+8*\GROUP)(%eax),%mm3
	movq fifteens,%mm0	
	paddb (Mettab29_2+8*\GROUP)(%ebx),%mm3
	paddb ones,%mm3  # emulate pavgb - this may not be necessary
	psrlq $1,%mm3
	pand %mm0,%mm3

	movq (8*\GROUP)(%esi),%mm6	# Incoming path metric, high bit = 0
	movq ((8*\GROUP)+128)(%esi),%mm2 # Incoming path metric, high bit = 1
	movq %mm6,%mm1	
	movq %mm2,%mm7
	
	paddb %mm3,%mm6
	paddb %mm3,%mm2
	pxor  %mm0,%mm3		 # invert branch metric
	paddb %mm3,%mm7		 # path metric for inverted symbols
	paddb %mm3,%mm1

	# live registers 1 2 6 7
	# Compare mm6 and mm7;  mm1 and mm2
	pxor %mm3,%mm3	
	movq %mm6,%mm4
	movq %mm1,%mm5	
	psubb %mm7,%mm4		# mm4 = mm6 - mm7
	psubb %mm2,%mm5		# mm5 = mm1 - mm2
	pcmpgtb %mm3,%mm4	# mm4 = first set of decisions (ff = 1 better)
	pcmpgtb %mm3,%mm5	# mm5 = second set of decisions		

	# live registers 1 2 4 5 6 7
	# select survivors
	movq %mm4,%mm0
	pand %mm4,%mm7	
	movq %mm5,%mm3	
	pand %mm5,%mm2	
	pandn %mm6,%mm0
	pandn %mm1,%mm3	
	por %mm0,%mm7		# mm7 = first set of survivors
	por %mm3,%mm2		# mm2 = second set of survivors	

	# live registers 2 4 5 7
	# interleave & store decisions in mm4, mm5
	# interleave & store new branch metrics in mm2, mm7		
	movq %mm4,%mm3
	movq %mm7,%mm0	
	punpckhbw %mm5,%mm4
	punpcklbw %mm5,%mm3
	punpcklbw %mm2,%mm7	# interleave second 8 new metrics
	punpckhbw %mm2,%mm0	# interleave first 8 new metrics
	movq %mm4,(16*\GROUP+8)(%edx)
	movq %mm3,(16*\GROUP)(%edx)
	movq %mm7,(16*\GROUP)(%edi)
	movq %mm0,(16*\GROUP+8)(%edi)	

	.endm

# invoke macro 16 times for a total of 128 butterflies
	butterfly GROUP=0
	butterfly GROUP=1
	butterfly GROUP=2
	butterfly GROUP=3
	butterfly GROUP=4
	butterfly GROUP=5
	butterfly GROUP=6
	butterfly GROUP=7
	butterfly GROUP=8
	butterfly GROUP=9
	butterfly GROUP=10
	butterfly GROUP=11
	butterfly GROUP=12
	butterfly GROUP=13
	butterfly GROUP=14
	butterfly GROUP=15

	addl $256,%edx		# bump decision pointer			

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
fifteens:	
	.byte 15,15,15,15,15,15,15,15

	.align 8
ones:	.byte 1,1,1,1,1,1,1,1

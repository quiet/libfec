.text
.global cpu_features
	.type cpu_features,@function
cpu_features:	
	pushl %ebx
	pushl %ecx
	pushl %edx
	movl $1,%eax
	cpuid
	movl %edx,%eax
	popl %edx
	popl %ecx
	popl %ebx
	ret
	
/* arch/i386/gdt.c */
.set SEG_r0code, 1
.set SEG_r3code, 3
.set SEG_r3data, 4

.set IA32_SYSENTER_CS, 0x174
.set IA32_SYSENTER_ESP, 0x175
.set IA32_SYSENTER_EIP, 0x176

.section .text
.global sysexit
.type sysexit, @function
sysexit:
	mov 4(%esp), %edx
	mov 8(%esp), %ecx

	mov $(SEG_r3data << 3 | 3), %ax
	mov %ax, %ds
	mov %ax, %es
	mov %ax, %fs
	mov %ax, %gs

	// enable paging
	mov %cr0, %eax
	or $0x80000000, %eax
	mov %eax, %cr0

	sysexit


.global sysenter_setup
.type sysenter_setup, @function
sysenter_setup:
	xor %edx, %edx

	mov $(SEG_r0code << 3), %eax
	mov $IA32_SYSENTER_CS, %ecx
	wrmsr

	mov $IA32_SYSENTER_ESP, %ecx
	mov $_bss_end, %eax
	wrmsr

	mov $IA32_SYSENTER_EIP, %ecx
	mov $sysenter_handler, %eax
	wrmsr

	ret

sysenter_handler:
	pushal
	push %edx
	push %ecx
	push %edi
	push %esi
	push %ebx
	push %eax

	mov %cr0, %eax
	and $0x7FFFFFFF, %eax  // disable paging
	mov %eax, %cr0

	call syscall_handler

	// save the return value
	mov %eax, 52(%esp) // 24 [top of eflags] + 7*4 [skip until EAX]
	mov %edx, 40(%esp) // 24                 + 4*4 [skip until EBX]

	mov %cr0, %eax
	or  $0x80000000, %eax  // enable paging
	mov %eax, %cr0

	add $24, %esp
	popal
	sysexit

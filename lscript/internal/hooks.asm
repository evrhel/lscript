PUBLIC vm_call_extern_asm

.data

.code

;
; Calls a function given the function pointer, the arguments, and their types
;
; rcx - argCount
; rdx - argTypes
; r8 - args
; r9 - proc
vm_call_extern_asm PROC
	push rbp
	mov rbp, rsp

	; Save the argument count in rax
	mov rax, rcx

	; Save the arguments in r10
	mov r10, r8

	; Save the proc address in r11
	mov r11, r9


	; Save needed registers
	push r12
	mov r12, rdx
	push r13
	
	shl rax, 3
	sub rsp, rax
	mov rax, rcx
	
	test rax, rax
	je startCall

	mov r12, rax
	shl r12, 3

	mov rcx, qword ptr[r10]
	sub rax, 1
	test rax, rax
	je startCall

	mov rdx, qword ptr[r10 + 8]
	sub rax, 1
	test rax, rax
	je startCall

	mov r8, qword ptr[r10 + 16]
	sub rax, 1
	test rax, rax
	je startCall

	mov r9, qword ptr[r10 + 24]
	sub rax, 1
	test rax, rax
	je startCall

	add r10, 32
	mov r13, rsp
	add r13, 32
	pushLoop:
		mov r12, qword ptr[r10]
		mov qword ptr[r13], r12

		add r10, 8
		add r13, 8
		sub rax, 1
		test rax, rax
		jnz pushLoop

	startCall:
	call r11

	; Clean up the stack
	add rsp, 28

	; Restore the pushed registers
	pop r13
	pop r12

	leave
	ret

vm_call_extern_asm ENDP

END
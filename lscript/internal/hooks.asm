PUBLIC vm_call_extern_asm

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


	; Save the arg types in r12
	push r12
	mov r12, rdx
	push r13
	
	test rax, rax
	je startCall

	; Begin preparing arguments for calling, starting with the registers
	
	; First integer argument goes in rcx and floating point in xmm0
		mov r13b, byte ptr[r12]
		cmp r13b, 0

		jne rcxFloat
			mov rcx, qword ptr[r10]
			jmp rcxDone
		rcxFloat:
			cmp r13b, 2
			jne rcxDouble
				movsd xmm0, real8 ptr[r10]
				jmp rcxDone
			rcxDouble:
				movss xmm0, real4 ptr[r10]
		rcxDone:

		add r10, 8
		add r12, 1
		sub rax, 1
		test rax, rax
		je startCall

	; Second integer argument goes in rdx and floating point in xmm1

		mov r13b, byte ptr[r12]
		cmp r13b, 0

		jne rdxFloat
			mov rdx, qword ptr[r10]
			jmp rdxDone
		rdxFloat:
			cmp r13b, 2
			jne rdxDouble
				movsd xmm1, real8 ptr[r10]
				jmp rdxDone
			rdxDouble:
				movss xmm1, real4 ptr[r10]
		rdxDone:

		add r10, 8
		add r12, 1
		sub rax, 1
		test rax, rax
		je startCall

	; Third integer argument goes in r8 and floating point in xmm2

		mov r13b, byte ptr[r12]
		cmp r13b, 0

		jne r8Float
			mov r8, qword ptr[r10]
			jmp r8Done
		r8Float:
			cmp r13b, 2
			jne r8Double
				movsd xmm2, real8 ptr[r10]
				jmp r8Done
			r8Double:
				movss xmm2, real4 ptr[r10]
		r8Done:

		add r10, 8
		add r12, 1
		sub rax, 1
		test rax, rax
		je startCall

	; Fourth integer argument goes in r9 and floating point in xmm3

		mov r9, qword ptr[r10]

		mov r13b, byte ptr[r12]
		cmp r13b, 0

		jne r9Float
			mov r9, qword ptr[r10]
			jmp r9Done
		r9Float:
			cmp r13b, 2
			jne r9Double
				movsd xmm3, real8 ptr[r10]
				jmp r9Done
			r9Double:
				movss xmm3, real4 ptr[r10]
		r9Done:

		add r10, 8
		add r12, 1
		sub rax, 1
		test rax, rax
		je startCall

	; The rest of the arguments must go on the stack

		pushLoop:
			push qword ptr[r10]

			add r10, 8
			sub rax, 1
			test rax, rax
			jnz pushLoop

	startCall:
	call r11
	; Something here is stomping on where r12 and r13 were pushed to the stack

	; Restore the stack
	pop r13
	pop r12

	; No need to clean up the stack of the called function - all procedures
	; passed to this function should be declared __stdcall - i.e. the callee
	; cleans the stack

	leave
	ret

vm_call_extern_asm ENDP

END
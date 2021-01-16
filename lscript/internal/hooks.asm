PUBLIC vm_call_extern_asm

.data
	table0 dq byte0, word0, dword0, qword0, real40, real80
	table1 dq byte1, word1, dword1, qword1, real41, real81
	table2 dq byte2, word2, dword2, qword2, real42, real82
	table3 dq byte3, word3, dword3, qword3, real43, real83

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
	push r14
	
	test rax, rax
	je startCall

	; Begin preparing arguments for calling, starting with the registers
	; space must be allocated on the stack for the callee
	
	; First integer argument goes in rcx and floating point in xmm0
		mov r13b, byte ptr[r12]
		cmp r13b, 0

		; Set the register and push the correct size onto the stack

		; Get r13 to contain the offset into the jump table
		sub r13b, 0B5h
		jmp table0[8 * r13]

		byte0::
			mov rcx, qword ptr[r10]
			sub rsp, 1
			add r10, 1
			jmp table0Done
		word0::
			mov rcx, qword ptr[r10]
			sub rsp, 2
			add r10, 2
			jmp table0Done
		dword0::
			mov rcx, qword ptr[r10]
			sub rsp, 4
			add r10, 4
			jmp table0Done
		qword0::
			mov rcx, qword ptr[r10]
			sub rsp, 8
			add r10, 8
			jmp table0Done
		real40::
			movss xmm0, real4 ptr[r10]
			sub rsp, 4
			add r10, 4
			jmp table0Done
		real80::
			movsd xmm0, real8 ptr[r10]
			sub rsp, 8
			add r10,4
		table0Done:

		add r12, 1
		sub rax, 1
		test rax, rax
		je startCall

	; Second integer argument goes in rdx and floating point in xmm1

		mov r13b, byte ptr[r12]
		cmp r13b, 0

		sub r13b, 0B5h
		jmp table1[8 * r13]

		byte1::
			mov rdx, qword ptr[r10]
			sub rsp, 1
			add r10, 1
			jmp table1Done
		word1::
			mov rdx, qword ptr[r10]
			sub rsp, 2
			add r10, 2
			jmp table1Done
		dword1::
			mov rdx, qword ptr[r10]
			sub rsp, 4
			add r10, 4
			jmp table1Done
		qword1::
			mov rdx, qword ptr[r10]
			sub rsp, 8
			add r10, 8
			jmp table1Done
		real41::
			movss xmm1, real4 ptr[r10]
			sub rsp, 4
			add r10, 4
			jmp table1Done
		real81::
			movsd xmm1, real8 ptr[r10]
			sub rsp, 8
			add r10, 8
		table1Done:

		add r12, 1
		sub rax, 1
		test rax, rax
		je startCall

	; Third integer argument goes in r8 and floating point in xmm2

		mov r13b, byte ptr[r12]
		cmp r13b, 0

		sub r13b, 0B5h
		jmp table2[8 * r13]

		byte2::
			mov r8, qword ptr[r10]
			sub rsp, 1
			add r10, 1
			jmp table2Done
		word2::
			mov r8, qword ptr[r10]
			sub rsp, 2
			add r10, 2
			jmp table2Done
		dword2::
			mov r8, qword ptr[r10]
			sub rsp, 4
			add r10, 4
			jmp table2Done
		qword2::
			mov r8, qword ptr[r10]
			sub rsp, 8
			add r10, 8
			jmp table2Done
		real42::
			movss xmm2, real4 ptr[r10]
			sub rsp, 4
			add r10, 4
			jmp table2Done
		real82::
			movsd xmm2, real8 ptr[r10]
			sub rsp, 8
			add r10, 8
		table2Done:

		add r12, 1
		sub rax, 1
		test rax, rax
		je startCall

	; Fourth integer argument goes in r9 and floating point in xmm3

		mov r9, qword ptr[r10]

		mov r13b, byte ptr[r12]
		cmp r13b, 0

		sub r13b, 0B5h
		jmp table3[8 * r13]

		byte3::
			mov r9, qword ptr[r10]
			sub rsp, 1
			add r10, 1
			jmp table3Done
		word3::
			mov r9, qword ptr[r10]
			sub rsp, 2
			add r10, 2
			jmp table3Done
		dword3::
			mov r9, qword ptr[r10]
			sub rsp, 4
			add r10, 4
			jmp table3Done
		qword3::
			mov r9, qword ptr[r10]
			sub rsp, 8
			add r10, 8
			jmp table3Done
		real43::
			movss xmm3, real4 ptr[r10]
			sub rsp, 4
			add r10, 4
			jmp table3Done
		real83::
			movsd xmm3, real8 ptr[r10]
			sub rsp, 8
			add r10, 8
		table3Done:

		add r12, 1
		sub rax, 1
		test rax, rax
		je startCall

	; The rest of the arguments must go on the stack

		pushLoop:
			push qword ptr[r10]

			sub rax, 1
			test rax, rax
			jnz pushLoop

	startCall:
	call r11

	; Clean up the stack
	add rsp, 28

	; Restore the pushed registers
	pop r14
	pop r13
	pop r12

	leave
	ret

vm_call_extern_asm ENDP

END
PUBLIC vm_call_extern_asm

.code

;
; Calls a function with the desired arguments stored in some
; buffer
;
; rcx - env
; rdx - argSize
; r8 - args
; r9 - proc
vm_call_extern_asm PROC
	
	; Push base pointer to stack
	push rbp
	mov rbp, rsp

	mov rax, rdx
	test rax, rax
	je noLoop
	
	; Push all arguments to the stack
	loop1:
	push qword ptr[r8 + rax]
	sub rax, 8
	jnz loop1

	noLoop:

	; Call the target function
	call r9

	leave
	ret

vm_call_extern_asm ENDP

END
global _start
extern main

_start:
    mov eax, [esp]      ; argc
    lea ebx, [esp+4]    ; argv
    push ebx
    push eax
    call main
    mov ebx, eax
    mov eax, 1          ; SYS_EXIT
    int 0x80
.hang:
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits

global _start
extern main
extern environ

_start:
    mov eax, [esp]      ; argc
    lea ebx, [esp+4]    ; argv

    ; Calculate envp = argv + (argc + 1) * 4
    ; envp is right after the NULL terminator of argv
    mov ecx, eax        ; ecx = argc
    inc ecx             ; ecx = argc + 1 (for NULL terminator)
    shl ecx, 2          ; ecx = (argc + 1) * 4
    lea ecx, [ebx+ecx]  ; ecx = argv + (argc+1)*4 = envp

    ; Only set environ if envp[0] is non-NULL (i.e., there's at least one env var)
    mov edx, [ecx]      ; edx = envp[0]
    test edx, edx
    jz .skip_environ
    mov [environ], ecx
.skip_environ:

    push ebx
    push eax
    call main
    mov ebx, eax
    mov eax, 1          ; SYS_EXIT
    int 0x80
.hang:
    jmp .hang

section .note.GNU-stack noalloc noexec nowrite progbits

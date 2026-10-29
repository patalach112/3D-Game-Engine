; Bounded SPSC input ring, Win64 and SysV AMD64. No shared global queue.
; write=0, read=64, slots=128; capacity=1024; packet=320 bytes.
; x86 TSO: packet stores precede the release index store; index loads precede
; packet loads. Exactly ONE producer and ONE consumer, naturally aligned u64.
bits 64
default rel
global engine_input_push
global engine_input_pop
section .text
%macro arguments 0
%ifidn __OUTPUT_FORMAT__,win64
    mov r8,rcx
    mov r9,rdx
%elifidn __OUTPUT_FORMAT__,elf64
    mov r8,rdi
    mov r9,rsi
%else
%error "input_queue.asm requires win64 or elf64"
%endif
%endmacro
engine_input_push:
    arguments
    mov r10,[r8]
    mov rax,r10
    sub rax,[r8+64]
    cmp rax,1024
    jae .full
    mov rax,r10
    and eax,1023
    imul rax,320
    lea r11,[r8+rax+128]
%assign off 0
%rep 20
    movdqu xmm0,[r9+off]
    movdqu [r11+off],xmm0
%assign off off+16
%endrep
    inc r10
    mov [r8],r10
    mov eax,1
    ret
.full:
    xor eax,eax
    ret
engine_input_pop:
    arguments
    mov r10,[r8+64]
    cmp r10,[r8]
    je .empty
    mov rax,r10
    and eax,1023
    imul rax,320
    lea r11,[r8+rax+128]
%assign off 0
%rep 20
    movdqu xmm0,[r11+off]
    movdqu [r9+off],xmm0
%assign off off+16
%endrep
    inc r10
    mov [r8+64],r10
    mov eax,1
    ret
.empty:
    xor eax,eax
    ret
%ifidn __OUTPUT_FORMAT__,elf64
section .note.GNU-stack noalloc noexec nowrite progbits
%endif

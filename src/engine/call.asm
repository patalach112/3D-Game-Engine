; NASM x86-64: Win64 and Linux ELF64, position independent.
; Tail calls preserve the caller's return address, stack alignment, Win64 shadow
; space and ALL nonvolatile registers. The compiler-generated native backend
; handles messages, coordinates, allocation failures and per-window state.
; No global dispatch table, RECT pointer, byte callback pointer or unsafe pushes.
bits 64
default rel

extern engine_platform_allocate
extern engine_platform_free
extern engine_register_resize_callback

global createheap
global freeheap
global WindowResizedCallback

section .text
%ifidn __OUTPUT_FORMAT__,win64
extern engine_window_proc
global WindowProc
WindowProc:
    jmp engine_window_proc
createheap:
    jmp engine_platform_allocate
freeheap:
    jmp engine_platform_free
WindowResizedCallback:
    jmp engine_register_resize_callback
%elifidn __OUTPUT_FORMAT__,elf64
createheap:
    jmp engine_platform_allocate wrt ..plt
freeheap:
    jmp engine_platform_free wrt ..plt
WindowResizedCallback:
    jmp engine_register_resize_callback wrt ..plt
section .note.GNU-stack noalloc noexec nowrite progbits
%else
%error "call.asm supports win64 and elf64 only"
%endif

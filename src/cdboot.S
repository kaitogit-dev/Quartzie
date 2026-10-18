.intel_syntax noprefix
.code16
.section .text
.global _start
_start:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7c00
    sti
    mov [drive], dl
    mov ah, 0x41
    mov bx, 0x55aa
    mov dl, [drive]
    int 0x13
    jc failure
    cmp bx, 0xaa55
    jne failure
    mov word ptr [dap_segment], 0x1000
    mov word ptr [remaining], 80
load_next:
    mov si, offset dap
    mov ah, 0x42
    mov dl, [drive]
    int 0x13
    jc failure
    add word ptr [dap_segment], 0x0080
    add dword ptr [dap_lba], 1
    dec word ptr [remaining]
    jnz load_next
    mov dl, [drive]
    cli
    .byte 0xea
    .word 0x0000
    .word 0x1000
failure:
    mov si, offset failure_message
print_char:
    lodsb
    test al, al
    jz freeze
    mov ah, 0x0e
    mov bx, 0x0007
    int 0x10
    jmp print_char
freeze:
    cli
    hlt
    jmp freeze
.align 4
dap:
    .byte 0x10, 0
    .word 1
    .word 0
dap_segment:
    .word 0x1000
dap_lba:
    .long 33
    .long 0
remaining:
    .word 80
drive:
    .byte 0
failure_message:
    .asciz "Quartzie: CD read failed\r\n"
.org 510
.word 0xaa55

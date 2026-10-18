.intel_syntax noprefix
.code16
.set chosen_mode, 0x7200
.set boot_fb, 0x7210
.set boot_pitch, 0x7214
.set boot_width, 0x7216
.set boot_height, 0x7218
.set boot_bpp, 0x721a
.section .init,"ax"
.global stage_entry
.global boot_fb
.global boot_pitch
.global boot_width
.global boot_height
.global boot_bpp
.extern kmain
.extern __bss_start
.extern __bss_end
stage_entry:
    cli
    xor ax, ax
    mov ds, ax
    mov es, ax
    mov ss, ax
    mov sp, 0x7900
    sti
    mov word ptr [chosen_mode], 0
    mov bx, 0x100
search_mode:
    mov ax, 0x4f01
    mov cx, bx
    mov di, 0x7000
    push bx
    int 0x10
    pop bx
    cmp ax, 0x004f
    jne next_mode
    mov ax, word ptr [di]
    test ax, 0x0080
    jz next_mode
    cmp byte ptr [di+25], 32
    jne next_mode
    cmp byte ptr [di+27], 6
    jne next_mode
    cmp word ptr [di+18], 640
    jb next_mode
    cmp word ptr [di+18], 1920
    ja next_mode
    cmp word ptr [di+20], 480
    jb next_mode
    cmp word ptr [di+20], 1080
    ja next_mode
    cmp word ptr [di+18], 1920
    jne maybe_preferred
    cmp word ptr [di+20], 1080
    jne maybe_preferred
    call store_mode
    jmp enable_vbe
maybe_preferred:
    cmp word ptr [di+18], 1280
    jne maybe_standard
    cmp word ptr [di+20], 720
    jne maybe_standard
    call store_mode
    jmp next_mode
maybe_standard:
    cmp word ptr [di+18], 800
    jne next_mode
    cmp word ptr [di+20], 600
    jne next_mode
    cmp word ptr [chosen_mode], 0
    jne next_mode
    call store_mode
next_mode:
    inc bx
    cmp bx, 0x1ff
    jbe search_mode
enable_vbe:
    call try_bochs_1080p
    test ax, ax
    jnz switch_mode
    cmp word ptr [chosen_mode], 0
    je fallback
    mov bx, word ptr [chosen_mode]
    or bx, 0x4000
    mov ax, 0x4f02
    int 0x10
    cmp ax, 0x004f
    je switch_mode
fallback:
    mov ax, 0x0013
    int 0x10
    mov dword ptr [boot_fb], 0x000a0000
    mov word ptr [boot_pitch], 320
    mov word ptr [boot_width], 320
    mov word ptr [boot_height], 200
    mov byte ptr [boot_bpp], 8
    jmp switch_mode
try_bochs_1080p:
    mov dx, 0x01ce
    xor ax, ax
    out dx, ax
    mov dx, 0x01cf
    in ax, dx
    cmp ax, 0xb0c2
    jb bochs_unavailable
    cmp ax, 0xb0c5
    ja bochs_unavailable
    mov dx, 0x01ce
    mov ax, 4
    out dx, ax
    mov dx, 0x01cf
    xor ax, ax
    out dx, ax
    mov dx, 0x01ce
    mov ax, 1
    out dx, ax
    mov dx, 0x01cf
    mov ax, 1920
    out dx, ax
    mov dx, 0x01ce
    mov ax, 2
    out dx, ax
    mov dx, 0x01cf
    mov ax, 1080
    out dx, ax
    mov dx, 0x01ce
    mov ax, 3
    out dx, ax
    mov dx, 0x01cf
    mov ax, 32
    out dx, ax
    mov dx, 0x01ce
    mov ax, 1
    out dx, ax
    mov dx, 0x01cf
    in ax, dx
    cmp ax, 1920
    jne bochs_unavailable
    mov dx, 0x01ce
    mov ax, 2
    out dx, ax
    mov dx, 0x01cf
    in ax, dx
    cmp ax, 1080
    jne bochs_unavailable
    mov dx, 0x01ce
    mov ax, 3
    out dx, ax
    mov dx, 0x01cf
    in ax, dx
    cmp ax, 32
    jne bochs_unavailable
    mov dx, 0x01ce
    mov ax, 4
    out dx, ax
    mov dx, 0x01cf
    mov ax, 0x41
    out dx, ax
    mov dword ptr [boot_fb], 0xe0000000
    mov word ptr [boot_pitch], 7680
    mov word ptr [boot_width], 1920
    mov word ptr [boot_height], 1080
    mov byte ptr [boot_bpp], 32
    mov ax, 1
    ret
bochs_unavailable:
    xor ax, ax
    ret
switch_mode:
    cli
    in al, 0x92
    or al, 2
    out 0x92, al
    mov eax, offset gdt_descriptor
    lgdt [eax]
    mov eax, cr0
    or eax, 1
    mov cr0, eax
    .byte 0x66, 0xea
    .long pm_entry
    .word 0x0008
store_mode:
    mov word ptr [chosen_mode], bx
    mov ax, word ptr [di+16]
    mov word ptr [boot_pitch], ax
    mov ax, word ptr [di+18]
    mov word ptr [boot_width], ax
    mov ax, word ptr [di+20]
    mov word ptr [boot_height], ax
    mov eax, dword ptr [di+40]
    mov dword ptr [boot_fb], eax
    mov byte ptr [boot_bpp], 32
    ret
.code32
pm_entry:
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov ss, ax
    mov esp, 0x200000
    cld
    mov edi, offset __bss_start
    mov ecx, offset __bss_end
    sub ecx, edi
    xor eax, eax
    rep stosb
    call kmain
idle_forever:
    cli
    hlt
    jmp idle_forever
.align 8
gdt:
    .quad 0
gdt_code:
    .word 0xffff, 0
    .byte 0, 0x9a, 0xcf, 0
gdt_data:
    .word 0xffff, 0
    .byte 0, 0x92, 0xcf, 0
gdt_end:
gdt_descriptor:
    .word gdt_end-gdt-1
    .long gdt

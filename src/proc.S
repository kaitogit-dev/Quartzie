.intel_syntax noprefix
.code32
.section .text
.global cm_enter_userspace
.global cm_irq0_stub
.global cm_irq_unexpected_stub
.global cm_syscall_stub
.global cm_gp_stub
.global cm_pf_stub
.global cm_unhandled_stub
.global cm_user_binary_start
.global cm_user_binary_end
.global cm_user_fault_label
.extern cm_host_stack
.extern cm_run_address_space
.extern cm_run_frame
.extern cm_kernel_address_space
.extern cm_fault_vector
.extern cm_fault_code
.extern cm_proc_interrupt
cm_enter_userspace:
    push ebp
    push ebx
    push esi
    push edi
    mov dword ptr [cm_host_stack], esp
    cli
    mov eax, dword ptr [cm_run_address_space]
    mov cr3, eax
    mov esp, dword ptr [cm_run_frame]
    jmp cm_restore_trap
cm_irq_unexpected_stub:
    pushad
    push ds
    push es
    push fs
    push gs
    mov eax, 3
    jmp cm_dispatch_trap
cm_irq0_stub:
    pushad
    push ds
    push es
    push fs
    push gs
    mov eax, 0
    jmp cm_dispatch_trap
cm_syscall_stub:
    pushad
    push ds
    push es
    push fs
    push gs
    mov eax, 1
    jmp cm_dispatch_trap
cm_gp_stub:
    push eax
    mov eax, dword ptr [esp + 4]
    mov dword ptr [cm_fault_code], eax
    pop eax
    add esp, 4
    mov dword ptr [cm_fault_vector], 13
    jmp cm_fault_trap
cm_pf_stub:
    push eax
    mov eax, dword ptr [esp + 4]
    mov dword ptr [cm_fault_code], eax
    pop eax
    add esp, 4
    mov dword ptr [cm_fault_vector], 14
cm_fault_trap:
    pushad
    push ds
    push es
    push fs
    push gs
    mov eax, 2
cm_dispatch_trap:
    mov edx, eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    push edx
    push esp
    add dword ptr [esp], 4
    call cm_proc_interrupt
    add esp, 8
    test eax, eax
    jz cm_return_kernel
    mov ebx, eax
    mov eax, dword ptr [cm_run_address_space]
    mov cr3, eax
    mov esp, ebx
cm_restore_trap:
    pop gs
    pop fs
    pop es
    pop ds
    popad
    iretd
cm_return_kernel:
    cli
    mov eax, dword ptr [cm_kernel_address_space]
    mov cr3, eax
    mov ax, 0x10
    mov ds, ax
    mov es, ax
    mov fs, ax
    mov gs, ax
    mov esp, dword ptr [cm_host_stack]
    pop edi
    pop esi
    pop ebx
    pop ebp
    ret
cm_unhandled_stub:
    cli
1:
    hlt
    jmp 1b
.align 16
cm_user_binary_start:
    cmp eax, 1
    je cm_user_spin
cm_user_paint_loop:
    inc dword ptr [0x40002000]
    mov esi, 0x40002100
    cmp dword ptr [esi], 1
    jne cm_user_yield
    mov ebx, dword ptr [esi + 4]
    mov ebp, dword ptr [esi + 8]
    mov edx, dword ptr [esi + 12]
    neg edx
cm_user_paint_row:
    mov ecx, dword ptr [esi + 12]
    neg ecx
cm_user_paint_column:
    mov eax, ebx
    add eax, ecx
    cmp eax, 0
    jl cm_user_paint_next
    cmp eax, 224
    jge cm_user_paint_next
    mov eax, ebp
    add eax, edx
    cmp eax, 0
    jl cm_user_paint_next
    cmp eax, 110
    jge cm_user_paint_next
    mov eax, ecx
    imul eax, eax
    push eax
    mov eax, edx
    imul eax, eax
    add eax, dword ptr [esp]
    add esp, 4
    push eax
    mov eax, dword ptr [esi + 12]
    imul eax, eax
    add eax, dword ptr [esi + 12]
    cmp dword ptr [esp], eax
    lea esp, [esp + 4]
    jg cm_user_paint_next
    mov eax, ebp
    add eax, edx
    imul eax, eax, 224
    add eax, ebx
    add eax, ecx
    push ecx
    mov ecx, dword ptr [esi + 16]
    mov byte ptr [0x40010000 + eax], cl
    pop ecx
cm_user_paint_next:
    inc ecx
    cmp ecx, dword ptr [esi + 12]
    jle cm_user_paint_column
    inc edx
    cmp edx, dword ptr [esi + 12]
    jle cm_user_paint_row
    mov dword ptr [esi], 0
    inc dword ptr [esi + 20]
cm_user_yield:
    mov eax, 2
    int 0x80
    jmp cm_user_paint_loop
cm_user_spin:
    mov edi, 0x40002000
    inc dword ptr [edi]
    cmp dword ptr [edi + 4], 0
    jne cm_user_spin
    mov eax, 2
    int 0x80
    jmp cm_user_spin
cm_user_fault_label:
    cli
    jmp cm_user_fault_label
cm_user_binary_end:
.section .note.GNU-stack,"",@progbits

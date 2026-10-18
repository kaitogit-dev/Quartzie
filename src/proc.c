#include "proc.h"
#include "pe.h"
#include <stddef.h>

#define CM_PAGE_DIRECTORY_ITEMS 1024u
#define CM_USER_PAGE_INDEX (CM_USER_BASE >> 22)
#define CM_USER_PHYSICAL_BASE 0x03000000u
#define CM_USER_PAGE_SIZE 0x00400000u
#define CM_KERNEL_INTERRUPT_STACK 4096u

typedef struct __attribute__((packed)) {
    uint16_t limit;
    uint32_t base;
} CmRegisterDescriptor;

typedef struct __attribute__((packed)) {
    uint16_t offset_low;
    uint16_t segment;
    uint8_t reserved;
    uint8_t flags;
    uint16_t offset_high;
} CmGateDescriptor;

static uint32_t cm_page_directories[CM_USER_COUNT + 1][CM_PAGE_DIRECTORY_ITEMS] __attribute__((aligned(4096)));
static uint64_t cm_gdt[6] __attribute__((aligned(8)));
static CmGateDescriptor cm_idt[256];
static uint8_t cm_tss[104] __attribute__((aligned(16)));
static uint8_t cm_task_kernel_stacks[CM_USER_COUNT][CM_KERNEL_INTERRUPT_STACK] __attribute__((aligned(16)));
static int cm_proc_ready;
static int cm_current;
static uint32_t cm_budget;
static uint32_t cm_last_selected;
static uint32_t cm_timer_acks;
static uint32_t cm_syscalls_in_slice;

CmUserTask cm_user_tasks[CM_USER_COUNT];
volatile uint32_t cm_host_stack;
volatile uint32_t cm_run_address_space;
volatile uint32_t cm_run_frame;
volatile uint32_t cm_fault_code;
volatile uint32_t cm_fault_vector;
uint32_t cm_kernel_address_space;
volatile uint32_t cm_preemptions;
volatile uint32_t cm_user_syscalls;
volatile uint32_t cm_user_ticks;
volatile uint32_t cm_protection_faults;
volatile uint32_t cm_user_slices;
volatile uint32_t cm_pe_last_exit;
volatile uint32_t cm_pe_completed;
volatile uint32_t cm_pe_launches;
static uint32_t cm_pe_pid = 200u;

static void cm_zero_memory(void *dst, uint32_t length) {
    uint8_t *out = (uint8_t *)dst;
    for (uint32_t i = 0; i < length; i++) out[i] = 0;
}

static void cm_copy_memory(void *dst, const void *src, uint32_t length) {
    uint8_t *out = (uint8_t *)dst;
    const uint8_t *input = (const uint8_t *)src;
    for (uint32_t i = 0; i < length; i++) out[i] = input[i];
}

static void cm_prepare_pages(void) {
    for (uint32_t i = 0; i <= CM_USER_COUNT; i++) {
        for (uint32_t page = 0; page < CM_PAGE_DIRECTORY_ITEMS; page++) {
            cm_page_directories[i][page] = (page << 22) | CM_PAGE_PRESENT | CM_PAGE_WRITE | CM_PAGE_LARGE;
        }
    }
    cm_kernel_address_space = (uint32_t)(uintptr_t)&cm_page_directories[0][0];
    for (uint32_t i = 0; i < CM_USER_COUNT; i++) {
        uint32_t physical = CM_USER_PHYSICAL_BASE + i * CM_USER_PAGE_SIZE;
        cm_page_directories[i + 1][CM_USER_PAGE_INDEX] = physical | CM_PAGE_PRESENT | CM_PAGE_WRITE | CM_PAGE_USER | CM_PAGE_LARGE;
        cm_user_tasks[i].address_space = (uint32_t)(uintptr_t)&cm_page_directories[i + 1][0];
        cm_user_tasks[i].physical_base = physical;
    }
}

static uint32_t cm_task_page(uint32_t task, uint32_t virtual_address, int user) {
    if (task >= CM_USER_COUNT) return 0;
    uint32_t entry = cm_page_directories[task + 1][virtual_address >> 22];
    if (!(entry & CM_PAGE_PRESENT) || (user && !(entry & CM_PAGE_USER))) return 0;
    return (entry & 0xffc00000u) | (virtual_address & 0x003fffffu);
}

static void cm_prepare_tasks(uint32_t user_code_length, const uint8_t *user_code) {
    cm_last_selected = CM_USER_COUNT - 1u;
    for (uint32_t i = 0; i < CM_USER_COUNT; i++) {
        CmUserTask *task = &cm_user_tasks[i];
        uint8_t *physical = (uint8_t *)(uintptr_t)task->physical_base;
        cm_zero_memory(physical, 0x4000u);
        cm_zero_memory(physical + 0x003ff000u, 0x1000u);
        if (user_code && user_code_length <= 0x1000u) {
            cm_copy_memory(physical + 0x1000u, user_code, user_code_length);
        }
        task->pid = 100u + i;
        task->state = CM_THREAD_READY;
        task->switches = 0;
        task->faults = 0;
        task->last_error = 0;
        task->last_fault = 0;
        task->kernel_stack_top = (uint32_t)(uintptr_t)&cm_task_kernel_stacks[i][CM_KERNEL_INTERRUPT_STACK];
        CmInterruptFrame *frame = (CmInterruptFrame *)(uintptr_t)(task->kernel_stack_top - sizeof(CmInterruptFrame));
        cm_zero_memory(frame, sizeof(*frame));
        frame->gs = CM_USER_DATA_SELECTOR;
        frame->fs = CM_USER_DATA_SELECTOR;
        frame->es = CM_USER_DATA_SELECTOR;
        frame->ds = CM_USER_DATA_SELECTOR;
        frame->eax = i;
        frame->eip = CM_USER_CODE;
        frame->cs = CM_USER_CODE_SELECTOR;
        frame->eflags = 0x202u;
        frame->user_esp = CM_USER_TOP;
        frame->user_ss = CM_USER_DATA_SELECTOR;
        task->frame = (uint32_t)(uintptr_t)frame;
    }
}

static uint32_t cm_select_next(void) {
    for (uint32_t offset = 1; offset <= CM_USER_COUNT; offset++) {
        uint32_t candidate = (cm_last_selected + offset) % CM_USER_COUNT;
        if (cm_user_tasks[candidate].state == CM_THREAD_READY) {
            cm_last_selected = candidate;
            cm_current = (int)candidate;
            cm_run_frame = cm_user_tasks[candidate].frame;
            cm_run_address_space = cm_user_tasks[candidate].address_space;
            cm_user_tasks[candidate].switches++;
            *((uint32_t *)(void *)(cm_tss + 4)) = cm_user_tasks[candidate].kernel_stack_top;
            return cm_run_frame;
        }
    }
    return 0;
}

static void cm_mark_fault(CmInterruptFrame *frame) {
    CmUserTask *task = &cm_user_tasks[cm_current];
    task->frame = (uint32_t)(uintptr_t)frame;
    task->state = CM_THREAD_FAULTED;
    task->faults++;
    task->last_fault = cm_fault_vector;
    task->last_error = cm_fault_code;
    cm_protection_faults++;
}

static uint32_t cm_interrupt_policy(CmInterruptFrame *frame, int reason) {
    if (!cm_proc_ready || cm_current < 0 || cm_current >= CM_USER_COUNT) return 0;
    CmUserTask *task = &cm_user_tasks[cm_current];
    if (task->state != CM_THREAD_READY || (frame->cs & 3u) != 3u) return 0;
    task->frame = (uint32_t)(uintptr_t)frame;
    if (reason == 0) {
        cm_user_ticks++;
        cm_preemptions++;
        if (cm_budget) cm_budget--;
        if (!cm_budget) return 0;
        return cm_select_next();
    }
    if (reason == 2) {
        cm_mark_fault(frame);
        return cm_select_next();
    }
    if (reason == 1) {
        cm_user_syscalls++;
        cm_syscalls_in_slice++;
        if (cm_syscalls_in_slice >= 1024u) return 0;
        uint32_t code = frame->eax;
        if (code == CM_SYSCALL_GETPID) frame->eax = task->pid;
        else if (code == CM_SYSCALL_GETTICKS) frame->eax = cm_user_ticks;
        else if (code == CM_SYSCALL_EXIT) {
            task->last_error = frame->ebx;
            task->state = CM_THREAD_STOPPED;
            cm_pe_last_exit = frame->ebx;
            cm_pe_completed++;
            return cm_select_next();
        }
        else if (code == CM_SYSCALL_YIELD) {
            frame->eax = 0;
            return cm_select_next();
        } else frame->eax = 0xc0000002u;
        return task->frame;
    }
    return 0;
}

uint32_t cm_proc_interrupt(CmInterruptFrame *frame, int reason) {
#ifndef CINMIX_HOST_TEST
    if (reason == 3) {
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0xa0));
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x20));
        return 0;
    }
    if (reason == 0) {
        __asm__ volatile ("outb %0, %1" : : "a"((uint8_t)0x20), "Nd"((uint16_t)0x20));
        cm_timer_acks++;
    }
#endif
    return cm_interrupt_policy(frame, reason);
}

static int cm_win32_equal(const char *a, const char *b) {
    while (*a && *b) {
        char x=*a++, y=*b++;
        if (x>='A' && x<='Z') x+=32;
        if (y>='A' && y<='Z') y+=32;
        if (x!=y) return 0;
    }
    return !*a && !*b;
}

static uint32_t cm_win32_resolve(const char *dll, const char *name) {
    if (!cm_win32_equal(dll, "KERNEL32.DLL")) return 0;
    if (cm_win32_equal(name, "GetCurrentProcessId")) return CM_USER_BASE + CM_PE_THUNK_OFFSET + CM_PE_THUNK_PID;
    if (cm_win32_equal(name, "GetTickCount")) return CM_USER_BASE + CM_PE_THUNK_OFFSET + CM_PE_THUNK_TICKS;
    if (cm_win32_equal(name, "ExitProcess")) return CM_USER_BASE + CM_PE_THUNK_OFFSET + CM_PE_THUNK_EXIT;
    return 0;
}

static void cm_make_thunk(uint8_t *dst, uint32_t syscall) {
    dst[0]=0xb8;
    dst[1]=(uint8_t)syscall;
    dst[2]=(uint8_t)(syscall>>8);
    dst[3]=(uint8_t)(syscall>>16);
    dst[4]=(uint8_t)(syscall>>24);
    dst[5]=0xcd;
    dst[6]=0x80;
    dst[7]=0xc3;
}

int cm_proc_launch_pe(const uint8_t *data, uint32_t count) {
    if (!cm_proc_ready || !data || !count) return CM_PE_BAD_IMAGE;
    CmUserTask *task = &cm_user_tasks[1];
    uint8_t *physical = (uint8_t *)(uintptr_t)task->physical_base;
    CmPeImage image;
    task->state=CM_THREAD_STOPPED;
    int status = cm_pe_load(data,count,physical,CM_PE_LIMIT,&image,cm_win32_resolve);
    if (status!=CM_PE_OK) return status;
    uint8_t *thunks=physical+CM_PE_THUNK_OFFSET;
    cm_zero_memory(thunks,48);
    cm_make_thunk(thunks+CM_PE_THUNK_PID,CM_SYSCALL_GETPID);
    cm_make_thunk(thunks+CM_PE_THUNK_TICKS,CM_SYSCALL_GETTICKS);
    cm_make_thunk(thunks+CM_PE_THUNK_EXIT,CM_SYSCALL_EXIT);
    thunks[CM_PE_THUNK_EXIT+0]=0x8b;
    thunks[CM_PE_THUNK_EXIT+1]=0x5c;
    thunks[CM_PE_THUNK_EXIT+2]=0x24;
    thunks[CM_PE_THUNK_EXIT+3]=0x04;
    cm_make_thunk(thunks+CM_PE_THUNK_EXIT+4,CM_SYSCALL_EXIT);
    cm_zero_memory(physical+0x003ff000u,0x1000);
    CmInterruptFrame *frame = (CmInterruptFrame *)(uintptr_t)(task->kernel_stack_top-sizeof(CmInterruptFrame));
    cm_zero_memory(frame,sizeof(*frame));
    frame->gs=CM_USER_DATA_SELECTOR;
    frame->fs=CM_USER_DATA_SELECTOR;
    frame->es=CM_USER_DATA_SELECTOR;
    frame->ds=CM_USER_DATA_SELECTOR;
    frame->eip=image.entry;
    frame->cs=CM_USER_CODE_SELECTOR;
    frame->eflags=0x202u;
    frame->user_esp=CM_USER_TOP;
    frame->user_ss=CM_USER_DATA_SELECTOR;
    task->pid=++cm_pe_pid;
    task->frame=(uint32_t)(uintptr_t)frame;
    task->state=CM_THREAD_READY;
    task->faults=0;
    task->last_error=0;
    task->last_fault=0;
    cm_pe_last_exit=0xffffffffu;
    cm_pe_launches++;
    return CM_PE_OK;
}

uint32_t cm_proc_active_tasks(void) {
    uint32_t count = 0;
    for (uint32_t i = 0; i < CM_USER_COUNT; i++) if (cm_user_tasks[i].state == CM_THREAD_READY) count++;
    return count;
}

uint32_t cm_proc_counter(int task) {
    if (task < 0 || task >= CM_USER_COUNT) return 0;
    volatile uint32_t *value = (volatile uint32_t *)(uintptr_t)(cm_user_tasks[task].physical_base + 0x2000u);
    return *value;
}

static CmPaintMailbox *cm_paint_mailbox(void) {
    return (CmPaintMailbox *)(uintptr_t)(cm_user_tasks[0].physical_base + 0x2100u);
}

int cm_paint_pending(void) {
    if (!cm_proc_ready || cm_user_tasks[0].state != CM_THREAD_READY) return 0;
    return cm_paint_mailbox()->command == CM_PAINT_STAMP;
}

int cm_proc_has_work(void) {
    if (!cm_proc_ready) return 0;
    if (cm_paint_pending()) return 1;
    return cm_pe_launches != cm_pe_completed && cm_user_tasks[1].state == CM_THREAD_READY;
}

int cm_paint_submit(int x, int y, uint32_t color, uint32_t radius) {
    if (!cm_proc_ready || cm_user_tasks[0].state != CM_THREAD_READY || x < 0 || y < 0 ||
        x >= (int)CM_PAINT_WIDTH || y >= (int)CM_PAINT_HEIGHT || color > 255 || radius > 12) return 0;
    CmPaintMailbox *box = cm_paint_mailbox();
    if (box->command || box->completed) return 0;
    box->x = x;
    box->y = y;
    box->color = color;
    box->radius = radius;
    __asm__ volatile("" ::: "memory");
    box->command = CM_PAINT_STAMP;
    return 1;
}

int cm_paint_collect(uint8_t *pixels, uint32_t count) {
    if (!cm_proc_ready || cm_user_tasks[0].state != CM_THREAD_READY || !pixels ||
        count != CM_PAINT_WIDTH * CM_PAINT_HEIGHT) return 0;
    CmPaintMailbox *box = cm_paint_mailbox();
    if (!box->completed || box->command) return 0;
    const uint8_t *source = (const uint8_t *)(uintptr_t)(cm_user_tasks[0].physical_base + 0x10000u);
    int x = box->x, y = box->y, radius = (int)box->radius;
    for (int dy = -radius; dy <= radius; dy++) for (int dx = -radius; dx <= radius; dx++) {
        int px = x + dx, py = y + dy;
        if (px >= 0 && py >= 0 && px < (int)CM_PAINT_WIDTH && py < (int)CM_PAINT_HEIGHT && dx * dx + dy * dy <= radius * radius + radius) {
            uint32_t index = (uint32_t)py * CM_PAINT_WIDTH + (uint32_t)px;
            pixels[index] = source[index];
        }
    }
    box->completed = 0;
    return 1;
}

int cm_proc_enabled(void) { return cm_proc_ready; }

int cm_proc_fault_demo(void) {
#ifndef CINMIX_HOST_TEST
    if (!cm_proc_ready || cm_user_tasks[1].state != CM_THREAD_READY) return 0;
    CmInterruptFrame *frame = (CmInterruptFrame *)(uintptr_t)cm_user_tasks[1].frame;
    extern uint8_t cm_user_binary_start;
    extern uint8_t cm_user_fault_label;
    frame->eip = CM_USER_CODE + (uint32_t)(&cm_user_fault_label - &cm_user_binary_start);
    return 1;
#else
    return 0;
#endif
}


void cm_proc_reset_stats(void) {
    cm_user_ticks = 0;
    cm_preemptions = 0;
    cm_user_syscalls = 0;
    cm_user_slices = 0;
    cm_protection_faults = 0;
    cm_timer_acks = 0;
    cm_syscalls_in_slice = 0;
}

#ifndef CINMIX_HOST_TEST
extern void cm_enter_userspace(void);
extern void cm_irq0_stub(void);
extern void cm_irq_unexpected_stub(void);
extern void cm_syscall_stub(void);
extern void cm_gp_stub(void);
extern void cm_pf_stub(void);
extern void cm_unhandled_stub(void);
extern uint8_t cm_user_binary_start;
extern uint8_t cm_user_binary_end;
extern uint8_t cm_user_fault_label;

static void cm_write_port(uint16_t port, uint8_t value) {
    __asm__ volatile("outb %0,%1" : : "a"(value), "Nd"(port));
}

static void cm_fill_gate(uint32_t vector, uintptr_t target, uint8_t attributes) {
    cm_idt[vector].offset_low = (uint16_t)target;
    cm_idt[vector].offset_high = (uint16_t)(target >> 16);
    cm_idt[vector].segment = CM_KERNEL_CODE;
    cm_idt[vector].reserved = 0;
    cm_idt[vector].flags = attributes;
}

static void cm_prepare_tables(void) {
    cm_gdt[0] = 0;
    cm_gdt[1] = 0x00cf9a000000ffffull;
    cm_gdt[2] = 0x00cf92000000ffffull;
    cm_gdt[3] = 0x00cffa000000ffffull;
    cm_gdt[4] = 0x00cff2000000ffffull;
    cm_zero_memory(cm_tss, sizeof(cm_tss));
    *((uint32_t *)(void *)(cm_tss + 8)) = CM_KERNEL_DATA;
    *((uint16_t *)(void *)(cm_tss + 102)) = (uint16_t)sizeof(cm_tss);
    uintptr_t address = (uintptr_t)cm_tss;
    cm_gdt[5] = (uint64_t)(sizeof(cm_tss) - 1u) | ((uint64_t)(address & 0x00ffffffu) << 16)
        | (0x89ull << 40) | ((uint64_t)(address >> 24) << 56);
    for (uint32_t index = 0; index < 256u; index++) cm_fill_gate(index, (uintptr_t)cm_unhandled_stub, 0x8eu);
    cm_fill_gate(13u, (uintptr_t)cm_gp_stub, 0x8eu);
    cm_fill_gate(14u, (uintptr_t)cm_pf_stub, 0x8eu);
    cm_fill_gate(32u, (uintptr_t)cm_irq0_stub, 0x8eu);
    for (uint32_t irq = 33u; irq <= 47u; irq++) cm_fill_gate(irq, (uintptr_t)cm_irq_unexpected_stub, 0x8eu);
    cm_fill_gate(128u, (uintptr_t)cm_syscall_stub, 0xeeu);
    CmRegisterDescriptor gdt_descriptor = {(uint16_t)(sizeof(cm_gdt) - 1u), (uint32_t)(uintptr_t)cm_gdt};
    CmRegisterDescriptor idt_descriptor = {(uint16_t)(sizeof(cm_idt) - 1u), (uint32_t)(uintptr_t)cm_idt};
    __asm__ volatile("lgdt %0" : : "m"(gdt_descriptor) : "memory");
    __asm__ volatile("lidt %0" : : "m"(idt_descriptor) : "memory");
    uint16_t selector = 0x28u;
    __asm__ volatile("ltr %0" : : "r"(selector) : "memory");
}

static void cm_prepare_pic(void) {
    cm_write_port(0x20, 0x11);
    cm_write_port(0xa0, 0x11);
    cm_write_port(0x21, 0x20);
    cm_write_port(0xa1, 0x28);
    cm_write_port(0x21, 0x04);
    cm_write_port(0xa1, 0x02);
    cm_write_port(0x21, 0x01);
    cm_write_port(0xa1, 0x01);
    cm_write_port(0x21, 0xff);
    cm_write_port(0xa1, 0xff);
}

static void cm_program_pit(uint16_t reload) {
    cm_write_port(0x43, 0x34);
    cm_write_port(0x40, (uint8_t)reload);
    cm_write_port(0x40, (uint8_t)(reload >> 8));
}

int cm_proc_initialize(void) {
    uint32_t feature_bits, leaf = 1;
    __asm__ volatile("cpuid" : "+a"(leaf), "=d"(feature_bits) : : "ebx", "ecx");
    if (!(feature_bits & (1u << 3))) return 0;
    cm_prepare_pages();
    for (uint32_t i = 0; i < CM_USER_COUNT; i++) {
        if (cm_task_page(i, CM_USER_CODE, 1) != cm_user_tasks[i].physical_base + 0x1000u) return 0;
        if (cm_task_page(i, 0x00400000u, 1)) return 0;
    }
    cm_prepare_tasks((uint32_t)(&cm_user_binary_end - &cm_user_binary_start), &cm_user_binary_start);
    cm_prepare_tables();
    cm_prepare_pic();
    uint32_t address = cm_kernel_address_space;
    __asm__ volatile(
        "mov %%cr4, %%eax\n\t"
        "or $0x10, %%eax\n\t"
        "mov %%eax, %%cr4\n\t"
        "mov %0, %%cr3\n\t"
        "mov %%cr0, %%eax\n\t"
        "or $0x80000000, %%eax\n\t"
        "mov %%eax, %%cr0\n\t"
        : : "r"(address) : "eax", "memory");
    cm_proc_ready = 1;
    cm_current = -1;
    return 1;
}

void cm_proc_run(void) {
    if (!cm_proc_ready || !cm_proc_active_tasks()) return;
    cm_budget = 1;
    cm_syscalls_in_slice = 0;
    if (cm_user_ticks) {
        volatile uint32_t *preemptive = (volatile uint32_t *)(uintptr_t)(cm_user_tasks[1].physical_base + 0x2004u);
        *preemptive = 1;
    }
    if (cm_paint_pending()) cm_last_selected = CM_USER_COUNT - 1u;
    else if (cm_pe_launches != cm_pe_completed && cm_user_tasks[1].state == CM_THREAD_READY) cm_last_selected = 0;
    if (!cm_select_next()) return;
    cm_user_slices++;
    cm_program_pit((uint16_t)CM_PIT_DIVISOR);
    cm_prepare_pic();
    cm_write_port(0x21, 0xfe);
    cm_enter_userspace();
    cm_write_port(0x21, 0xff);
    cm_program_pit(0xffff);
}
#else
int cm_proc_initialize(void) {
    cm_prepare_pages();
    cm_prepare_tasks(0, (const uint8_t *)0);
    cm_proc_ready = 1;
    cm_current = -1;
    return 1;
}
void cm_proc_run(void) {
    if (!cm_proc_ready || !cm_proc_active_tasks()) return;
    cm_budget = 1;
    cm_syscalls_in_slice = 0;
    if (cm_select_next()) cm_user_slices++;
}
#endif

#ifndef QUARTZIE_PROC_H
#define QUARTZIE_PROC_H
#include <stdint.h>
#define CM_USER_BASE 0x40000000u
#define CM_USER_CODE (CM_USER_BASE + 0x1000u)
#define CM_USER_DATA (CM_USER_BASE + 0x2000u)
#define CM_USER_TOP (CM_USER_BASE + 0x003fffe0u)
#define CM_USER_COUNT 2
#define CM_PAINT_MAILBOX (CM_USER_BASE + 0x2100u)
#define CM_PAINT_BUFFER (CM_USER_BASE + 0x10000u)
#define CM_PAINT_WIDTH 224u
#define CM_PAINT_HEIGHT 110u
#define CM_PAINT_STAMP 1u
#define CM_KERNEL_CODE 0x08u
#define CM_KERNEL_DATA 0x10u
#define CM_USER_CODE_SELECTOR 0x1bu
#define CM_USER_DATA_SELECTOR 0x23u
#define CM_PIT_DIVISOR 11932u
#define CM_PAGE_PRESENT 0x001u
#define CM_PAGE_WRITE 0x002u
#define CM_PAGE_USER 0x004u
#define CM_PAGE_LARGE 0x080u
#define CM_SYSCALL_GETPID 1u
#define CM_SYSCALL_YIELD 2u
#define CM_SYSCALL_GETTICKS 3u
#define CM_SYSCALL_EXIT 4u
#define CM_THREAD_READY 1u
#define CM_THREAD_STOPPED 2u
#define CM_THREAD_FAULTED 3u

typedef struct {
    uint32_t gs, fs, es, ds;
    uint32_t edi, esi, ebp, pusha_esp, ebx, edx, ecx, eax;
    uint32_t eip, cs, eflags, user_esp, user_ss;
} CmInterruptFrame;

typedef struct {
    uint32_t frame;
    uint32_t address_space;
    uint32_t kernel_stack_top;
    uint32_t physical_base;
    uint32_t pid;
    uint32_t state;
    uint32_t switches;
    uint32_t faults;
    uint32_t last_fault;
    uint32_t last_error;
} CmUserTask;

typedef struct {
    volatile uint32_t command;
    int32_t x;
    int32_t y;
    uint32_t radius;
    uint32_t color;
    volatile uint32_t completed;
} CmPaintMailbox;

extern volatile uint32_t cm_host_stack;
extern volatile uint32_t cm_run_address_space;
extern volatile uint32_t cm_run_frame;
extern volatile uint32_t cm_fault_code;
extern volatile uint32_t cm_fault_vector;
extern uint32_t cm_kernel_address_space;
extern CmUserTask cm_user_tasks[CM_USER_COUNT];
extern volatile uint32_t cm_preemptions;
extern volatile uint32_t cm_user_syscalls;
extern volatile uint32_t cm_user_ticks;
extern volatile uint32_t cm_protection_faults;
extern volatile uint32_t cm_user_slices;

int cm_proc_initialize(void);
void cm_proc_run(void);
int cm_proc_enabled(void);
uint32_t cm_proc_counter(int task);
uint32_t cm_proc_active_tasks(void);
uint32_t cm_proc_interrupt(CmInterruptFrame *frame, int reason);
void cm_proc_reset_stats(void);
int cm_proc_fault_demo(void);
int cm_paint_submit(int x, int y, uint32_t color, uint32_t radius);
int cm_paint_collect(uint8_t *pixels, uint32_t count);
int cm_paint_pending(void);
int cm_proc_has_work(void);
int cm_proc_launch_pe(const uint8_t *data, uint32_t count);
extern volatile uint32_t cm_pe_last_exit;
extern volatile uint32_t cm_pe_completed;
extern volatile uint32_t cm_pe_launches;

#endif

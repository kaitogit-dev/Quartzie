#define _GNU_SOURCE
#define CINMIX_HOST_TEST
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include "../src/proc.c"
#define CHECK(ok) do { if (!(ok)) { fprintf(stderr,"FAIL proc line %d: %s\n",__LINE__,#ok);exit(1); } } while (0)

int main(void) {
    void *mapped = mmap((void *)0x03000000, 0x00800000, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED, -1, 0);
    CHECK(mapped != MAP_FAILED);
    CHECK(sizeof(CmInterruptFrame) == 68);
    CHECK(cm_proc_initialize());
    CHECK(cm_proc_enabled());
    CHECK(!cm_proc_has_work());
    CHECK(cm_proc_active_tasks() == 2);
    CHECK((cm_kernel_address_space & 0xfffu) == 0);
    CHECK((cm_page_directories[0][CM_USER_PAGE_INDEX] & CM_PAGE_USER) == 0);
    for (int i = 0; i < CM_USER_COUNT; i++) {
        CHECK(cm_user_tasks[i].state == CM_THREAD_READY);
        CHECK(cm_user_tasks[i].pid == (uint32_t)(100 + i));
        CHECK((cm_user_tasks[i].address_space & 0xfffu) == 0);
        CHECK(cm_task_page(i, CM_USER_CODE, 1) == cm_user_tasks[i].physical_base + 0x1000);
        CHECK(cm_task_page(i, CM_USER_DATA, 1) == cm_user_tasks[i].physical_base + 0x2000);
        CHECK(cm_task_page(i, CM_USER_TOP, 1) == cm_user_tasks[i].physical_base + 0x003fffe0);
        CHECK(cm_task_page(i, 0x00400000, 1) == 0);
        CHECK(cm_task_page(i, 0x00090000, 1) == 0);
        CHECK(cm_task_page(i, 0xe0000000u, 1) == 0);
        CHECK(cm_task_page(i, CM_USER_DATA, 0) == cm_user_tasks[i].physical_base + 0x2000);
        CHECK(((CmInterruptFrame *)(uintptr_t)cm_user_tasks[i].frame)->cs == CM_USER_CODE_SELECTOR);
        CHECK(((CmInterruptFrame *)(uintptr_t)cm_user_tasks[i].frame)->user_ss == CM_USER_DATA_SELECTOR);
        CHECK(((CmInterruptFrame *)(uintptr_t)cm_user_tasks[i].frame)->eflags == 0x202);
        CHECK(((CmInterruptFrame *)(uintptr_t)cm_user_tasks[i].frame)->user_esp == CM_USER_TOP);
    }
    CHECK(cm_task_page(0, CM_USER_DATA, 1) != cm_task_page(1, CM_USER_DATA, 1));
    *(uint32_t *)(uintptr_t)(cm_user_tasks[0].physical_base + 0x2000) = 17;
    *(uint32_t *)(uintptr_t)(cm_user_tasks[1].physical_base + 0x2000) = 99;
    CHECK(cm_proc_counter(0) == 17);
    CHECK(cm_proc_counter(1) == 99);
    CmPaintMailbox *pending = (CmPaintMailbox *)(uintptr_t)(cm_user_tasks[0].physical_base + 0x2100u);
    pending->command = CM_PAINT_STAMP;
    CHECK(cm_proc_has_work());
    pending->command = 0;
    CHECK(!cm_proc_has_work());
    cm_proc_run();
    CHECK(cm_current == 0);
    CmInterruptFrame *first = (CmInterruptFrame *)(uintptr_t)cm_user_tasks[0].frame;
    first->eax = CM_SYSCALL_GETPID;
    CHECK(cm_proc_interrupt(first, 1) == cm_user_tasks[0].frame);
    CHECK(first->eax == 100);
    first->eax = CM_SYSCALL_GETTICKS;
    CHECK(cm_proc_interrupt(first, 1) == cm_user_tasks[0].frame && first->eax == 0);
    first->eax = 0xffffffffu;
    CHECK(cm_proc_interrupt(first, 1) == cm_user_tasks[0].frame && first->eax == 0xc0000002u);
    first->eax = CM_SYSCALL_YIELD;
    CHECK(cm_proc_interrupt(first, 1) == cm_user_tasks[1].frame);
    CHECK(cm_current == 1 && cm_user_tasks[0].switches == 1 && cm_user_tasks[1].switches == 1);
    CmInterruptFrame *second = (CmInterruptFrame *)(uintptr_t)cm_user_tasks[1].frame;
    CHECK(cm_proc_interrupt(second, 0) == 0);
    CHECK(cm_user_ticks == 1 && cm_preemptions == 1);
    CHECK(cm_proc_active_tasks() == 2);
    cm_proc_run();
    CHECK(cm_current == 0);
    first = (CmInterruptFrame *)(uintptr_t)cm_user_tasks[0].frame;
    first->eax = CM_SYSCALL_YIELD;
    CHECK(cm_proc_interrupt(first, 1) == cm_user_tasks[1].frame);
    CHECK(cm_current == 1);
    cm_fault_vector = 13;
    cm_fault_code = 0;
    CHECK(cm_proc_interrupt(second, 2) == cm_user_tasks[0].frame);
    CHECK(cm_user_tasks[1].state == CM_THREAD_FAULTED);
    CHECK(cm_user_tasks[1].faults == 1 && cm_user_tasks[1].last_fault == 13);
    CHECK(cm_protection_faults == 1);
    CHECK(cm_proc_active_tasks() == 1);
    CHECK(cm_proc_counter(1) == 99);
    first = (CmInterruptFrame *)(uintptr_t)cm_user_tasks[0].frame;
    first->cs = CM_KERNEL_CODE;
    CHECK(cm_proc_interrupt(first, 1) == 0);
    first->cs = CM_USER_CODE_SELECTOR;
    cm_syscalls_in_slice = 1023;
    first->eax = CM_SYSCALL_YIELD;
    CHECK(cm_proc_interrupt(first, 1) == 0);
    CHECK(cm_user_syscalls >= 6);
    cm_proc_reset_stats();
    CHECK(cm_user_ticks == 0 && cm_preemptions == 0 && cm_user_syscalls == 0 && cm_protection_faults == 0);
    printf("PASS: distinct supervisor/user 4MiB page tables, TSS stack frames, ring3 selectors, syscalls, timer preemption model, protection faults and bounded syscall fallback\n");
    return 0;
}

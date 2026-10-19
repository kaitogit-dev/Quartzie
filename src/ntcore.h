#ifndef QUARTZIE_NTCORE_H
#define QUARTZIE_NTCORE_H
#include <stddef.h>
#include <stdint.h>
#define QNT_PROCESSES 24
#define QNT_THREADS 96
#define QNT_OBJECTS 192
#define QNT_FILES 32
#define QNT_DRIVERS 24
#define QNT_REGIONS 32
#define QNT_FILE_CAPACITY 4096
enum {QNT_OK=0,QNT_INVALID=-1,QNT_NOT_FOUND=-2,QNT_FULL=-3,QNT_ACCESS=-4,QNT_BUSY=-5};
enum {QNT_NONE=0,QNT_PROCESS=1,QNT_THREAD=2,QNT_FILE=3,QNT_EVENT=4,QNT_DRIVER=5};
enum {QNT_READY=1,QNT_RUNNING=2,QNT_WAITING=3,QNT_STOPPED=4};
enum {QNT_VM_READ=1,QNT_VM_WRITE=2,QNT_VM_EXECUTE=4,QNT_VM_USER=8};
enum {QNT_SC_YIELD=1,QNT_SC_OPEN=2,QNT_SC_READ=3,QNT_SC_WRITE=4,QNT_SC_CLOSE=5,QNT_SC_TICKS=6,QNT_SC_GETPID=7};
typedef struct {
    uint64_t base,length;
    uint32_t protection;
} QNtRegion;
typedef struct {
    uint32_t pid,alive,token,region_count;
    QNtRegion regions[QNT_REGIONS];
} QNtProcess;
typedef struct {
    uint32_t tid,pid,state,priority,quantum,elapsed;
    uint64_t entry,stack,context;
} QNtThread;
typedef struct {
    uint32_t generation,kind,owner,refs,index;
    char name[48];
} QNtObject;
typedef struct {
    int active;
    char name[64];
    uint32_t length;
    uint8_t data[QNT_FILE_CAPACITY];
} QNtFile;
typedef int (*QNtDriverDispatch)(void *context,uint32_t operation,void *buffer,size_t length);
typedef struct {
    int active;
    char name[48];
    QNtDriverDispatch dispatch;
    void *context;
} QNtDriver;
typedef struct {
    QNtProcess processes[QNT_PROCESSES];
    QNtThread threads[QNT_THREADS];
    QNtObject objects[QNT_OBJECTS];
    QNtFile files[QNT_FILES];
    QNtDriver drivers[QNT_DRIVERS];
    uint32_t next_pid,next_tid,current_tid,next_object;
    uint64_t clock_ticks,switches,syscalls;
} QNtKernel;
void qnt_init(QNtKernel *kernel);
int qnt_process_create(QNtKernel *kernel,uint32_t *pid);
int qnt_process_destroy(QNtKernel *kernel,uint32_t pid);
int qnt_thread_create(QNtKernel *kernel,uint32_t pid,uint64_t entry,uint64_t stack,uint32_t *tid);
int qnt_thread_state(QNtKernel *kernel,uint32_t tid,uint32_t state);
int qnt_schedule(QNtKernel *kernel);
void qnt_tick(QNtKernel *kernel);
int qnt_vm_map(QNtKernel *kernel,uint32_t pid,uint64_t base,uint64_t length,uint32_t protection);
int qnt_vm_query(QNtKernel *kernel,uint32_t pid,uint64_t address,QNtRegion *out);
int qnt_object_create(QNtKernel *kernel,uint32_t pid,uint32_t kind,uint32_t index,const char *name,uint64_t *handle);
int qnt_object_lookup(QNtKernel *kernel,uint32_t pid,uint64_t handle,uint32_t kind,uint32_t *index);
int qnt_object_close(QNtKernel *kernel,uint32_t pid,uint64_t handle);
int qnt_file_create(QNtKernel *kernel,uint32_t pid,const char *name,uint64_t *handle);
int qnt_file_open(QNtKernel *kernel,uint32_t pid,const char *name,uint64_t *handle);
int qnt_file_read(QNtKernel *kernel,uint32_t pid,uint64_t handle,uint32_t offset,void *buffer,size_t length,size_t *read);
int qnt_file_write(QNtKernel *kernel,uint32_t pid,uint64_t handle,uint32_t offset,const void *buffer,size_t length,size_t *written);
int qnt_driver_register(QNtKernel *kernel,const char *name,QNtDriverDispatch fn,void *context,uint32_t *id);
int qnt_driver_request(QNtKernel *kernel,uint32_t driver,uint32_t operation,void *buffer,size_t length);
int64_t qnt_syscall(QNtKernel *kernel,uint32_t pid,uint32_t operation,uint64_t a,uint64_t b,uint64_t c,uint64_t d);
#endif

#include "ntcore.h"
static void clear(void *ptr,size_t size){uint8_t *p=(uint8_t *)ptr;for(size_t i=0;i<size;i++)p[i]=0;}
static int equal(const char *a,const char *b){while(*a&&*a==*b){a++;b++;}return *a==*b;}
static void name_copy(char *to,size_t size,const char *name){
    size_t i=0;
    while(name&&name[i]&&i+1<size){to[i]=name[i];i++;}
    to[i]=0;
}
static QNtProcess *process(QNtKernel *k,uint32_t pid){
    for(int i=0;i<QNT_PROCESSES;i++)if(k->processes[i].alive&&k->processes[i].pid==pid)return &k->processes[i];
    return 0;
}
static QNtThread *thread(QNtKernel *k,uint32_t tid){
    for(int i=0;i<QNT_THREADS;i++)if(k->threads[i].state&&k->threads[i].tid==tid)return &k->threads[i];
    return 0;
}
void qnt_init(QNtKernel *k){
    if(!k)return;
    clear(k,sizeof(*k));
    k->next_pid=4;k->next_tid=8;
    k->next_object=1;
}
int qnt_process_create(QNtKernel *k,uint32_t *pid){
    if(!k||!pid)return QNT_INVALID;
    for(int i=0;i<QNT_PROCESSES;i++)if(!k->processes[i].alive){
        QNtProcess *p=&k->processes[i];
        clear(p,sizeof(*p));
        p->alive=1;p->pid=k->next_pid++;
        p->token=p->pid^0x57a0u;
        *pid=p->pid;
        return QNT_OK;
    }
    return QNT_FULL;
}
int qnt_process_destroy(QNtKernel *k,uint32_t pid){
    QNtProcess *p=process(k,pid);
    if(!p)return QNT_NOT_FOUND;
    for(int i=0;i<QNT_THREADS;i++)if(k->threads[i].pid==pid&&k->threads[i].state){
        if(k->current_tid==k->threads[i].tid)k->current_tid=0;
        clear(&k->threads[i],sizeof(k->threads[i]));
    }
    for(int i=0;i<QNT_OBJECTS;i++)if(k->objects[i].owner==pid)clear(&k->objects[i],sizeof(k->objects[i]));
    clear(p,sizeof(*p));
    return QNT_OK;
}
int qnt_thread_create(QNtKernel *k,uint32_t pid,uint64_t entry,uint64_t stack,uint32_t *tid){
    if(!k||!tid||!process(k,pid)||!entry||!stack)return QNT_INVALID;
    for(int i=0;i<QNT_THREADS;i++)if(!k->threads[i].state){
        QNtThread *t=&k->threads[i];
        clear(t,sizeof(*t));
        t->tid=k->next_tid++;t->pid=pid;t->entry=entry;t->stack=stack;
        t->priority=8;t->state=QNT_READY;t->quantum=4;
        *tid=t->tid;
        return QNT_OK;
    }
    return QNT_FULL;
}
int qnt_thread_state(QNtKernel *k,uint32_t tid,uint32_t state){
    QNtThread *t=thread(k,tid);
    if(!t||state<QNT_READY||state>QNT_STOPPED)return QNT_INVALID;
    t->state=state;t->elapsed=0;
    if(state!=QNT_RUNNING&&k->current_tid==tid)k->current_tid=0;
    return QNT_OK;
}
int qnt_schedule(QNtKernel *k){
    if(!k)return QNT_INVALID;
    int previous=-1;
    for(int i=0;i<QNT_THREADS;i++)if(k->threads[i].tid==k->current_tid&&k->threads[i].state==QNT_RUNNING){
        previous=i;k->threads[i].state=QNT_READY;k->threads[i].elapsed=0;break;
    }
    for(int n=1;n<=QNT_THREADS;n++){
        int i=(previous+n+QNT_THREADS)%QNT_THREADS;
        if(k->threads[i].state==QNT_READY){
            k->threads[i].state=QNT_RUNNING;
            k->current_tid=k->threads[i].tid;
            k->switches++;
            return (int)k->current_tid;
        }
    }
    k->current_tid=0;
    return 0;
}
void qnt_tick(QNtKernel *k){
    if(!k)return;
    k->clock_ticks++;
    QNtThread *t=thread(k,k->current_tid);
    if(!t||t->state!=QNT_RUNNING){qnt_schedule(k);return;}
    t->elapsed++;
    if(t->elapsed>=t->quantum)qnt_schedule(k);
}
int qnt_vm_map(QNtKernel *k,uint32_t pid,uint64_t base,uint64_t length,uint32_t protection){
    QNtProcess *p=process(k,pid);
    if(!p||!length||base+length<base||(base&4095)||(length&4095)||(protection&~15u))return QNT_INVALID;
    if((protection&QNT_VM_WRITE)&&(protection&QNT_VM_EXECUTE))return QNT_ACCESS;
    for(uint32_t i=0;i<p->region_count;i++){
        QNtRegion *r=&p->regions[i];
        if(base<r->base+r->length&&r->base<base+length)return QNT_BUSY;
    }
    if(p->region_count>=QNT_REGIONS)return QNT_FULL;
    QNtRegion *r=&p->regions[p->region_count++];
    r->base=base;r->length=length;r->protection=protection;
    return QNT_OK;
}
int qnt_vm_query(QNtKernel *k,uint32_t pid,uint64_t addr,QNtRegion *out){
    QNtProcess *p=process(k,pid);
    if(!p||!out)return QNT_INVALID;
    for(uint32_t i=0;i<p->region_count;i++){
        QNtRegion *r=&p->regions[i];
        if(addr>=r->base&&addr-r->base<r->length){*out=*r;return QNT_OK;}
    }
    return QNT_NOT_FOUND;
}
int qnt_object_create(QNtKernel *k,uint32_t pid,uint32_t kind,uint32_t index,const char *name,uint64_t *handle){
    if(!k||!handle||!process(k,pid)||kind<QNT_PROCESS||kind>QNT_DRIVER)return QNT_INVALID;
    for(uint32_t i=0;i<QNT_OBJECTS;i++)if(!k->objects[i].kind){
        QNtObject *o=&k->objects[i];
        uint32_t generation=o->generation+1;
        if(!generation)generation=1;
        clear(o,sizeof(*o));
        o->generation=generation;o->kind=kind;o->owner=pid;o->refs=1;o->index=index;
        name_copy(o->name,sizeof(o->name),name);
        *handle=((uint64_t)generation<<32)|(uint64_t)(i+1);
        return QNT_OK;
    }
    return QNT_FULL;
}
static QNtObject *lookup(QNtKernel *k,uint32_t pid,uint64_t handle,uint32_t kind){
    uint32_t id=(uint32_t)handle;
    uint32_t generation=(uint32_t)(handle>>32);
    if(!id||id>QNT_OBJECTS)return 0;
    QNtObject *o=&k->objects[id-1];
    if(!o->kind||o->owner!=pid||o->generation!=generation||(kind&&kind!=o->kind))return 0;
    return o;
}
int qnt_object_lookup(QNtKernel *k,uint32_t pid,uint64_t handle,uint32_t kind,uint32_t *index){
    if(!k||!index)return QNT_INVALID;
    QNtObject *o=lookup(k,pid,handle,kind);
    if(!o)return QNT_ACCESS;
    *index=o->index;
    return QNT_OK;
}
int qnt_object_close(QNtKernel *k,uint32_t pid,uint64_t handle){
    if(!k)return QNT_INVALID;
    QNtObject *o=lookup(k,pid,handle,0);
    if(!o)return QNT_ACCESS;
    if(o->refs>1)o->refs--;
    else{
        uint32_t generation=o->generation;
        clear(o,sizeof(*o));
        o->generation=generation;
    }
    return QNT_OK;
}
int qnt_file_create(QNtKernel *k,uint32_t pid,const char *name,uint64_t *handle){
    if(!k||!process(k,pid)||!name||!*name||!handle)return QNT_INVALID;
    for(int i=0;i<QNT_FILES;i++)if(k->files[i].active&&equal(k->files[i].name,name))return qnt_object_create(k,pid,QNT_FILE,(uint32_t)i,name,handle);
    for(int i=0;i<QNT_FILES;i++)if(!k->files[i].active){
        k->files[i].active=1;k->files[i].length=0;
        name_copy(k->files[i].name,sizeof(k->files[i].name),name);
        return qnt_object_create(k,pid,QNT_FILE,(uint32_t)i,name,handle);
    }
    return QNT_FULL;
}
int qnt_file_open(QNtKernel *k,uint32_t pid,const char *name,uint64_t *handle){
    if(!k||!name||!handle)return QNT_INVALID;
    for(int i=0;i<QNT_FILES;i++)if(k->files[i].active&&equal(k->files[i].name,name))return qnt_object_create(k,pid,QNT_FILE,(uint32_t)i,name,handle);
    return QNT_NOT_FOUND;
}
int qnt_file_read(QNtKernel *k,uint32_t pid,uint64_t handle,uint32_t offset,void *buffer,size_t length,size_t *read){
    uint32_t id;
    if(!k||!buffer||!read)return QNT_INVALID;
    *read=0;
    if(qnt_object_lookup(k,pid,handle,QNT_FILE,&id)!=QNT_OK||id>=QNT_FILES)return QNT_ACCESS;
    QNtFile *f=&k->files[id];
    if(offset>=f->length)return QNT_OK;
    size_t n=length;
    if(n>f->length-offset)n=f->length-offset;
    for(size_t i=0;i<n;i++)((uint8_t *)buffer)[i]=f->data[offset+i];
    *read=n;
    return QNT_OK;
}
int qnt_file_write(QNtKernel *k,uint32_t pid,uint64_t handle,uint32_t offset,const void *buffer,size_t length,size_t *written){
    uint32_t id;
    if(!k||!buffer||!written)return QNT_INVALID;
    *written=0;
    if(qnt_object_lookup(k,pid,handle,QNT_FILE,&id)!=QNT_OK||id>=QNT_FILES)return QNT_ACCESS;
    QNtFile *f=&k->files[id];
    if(offset>QNT_FILE_CAPACITY||length>QNT_FILE_CAPACITY-offset)return QNT_FULL;
    for(size_t i=f->length;i<offset;i++)f->data[i]=0;
    for(size_t i=0;i<length;i++)f->data[offset+i]=((const uint8_t *)buffer)[i];
    if((size_t)offset+length>f->length)f->length=(uint32_t)((size_t)offset+length);
    *written=length;
    return QNT_OK;
}
int qnt_driver_register(QNtKernel *k,const char *name,QNtDriverDispatch fn,void *context,uint32_t *id){
    if(!k||!name||!fn||!id)return QNT_INVALID;
    for(int i=0;i<QNT_DRIVERS;i++)if(!k->drivers[i].active){
        k->drivers[i].active=1;k->drivers[i].dispatch=fn;k->drivers[i].context=context;
        name_copy(k->drivers[i].name,sizeof(k->drivers[i].name),name);
        *id=(uint32_t)i+1;
        return QNT_OK;
    }
    return QNT_FULL;
}
int qnt_driver_request(QNtKernel *k,uint32_t driver,uint32_t operation,void *buffer,size_t length){
    if(!k||!driver||driver>QNT_DRIVERS||!k->drivers[driver-1].active)return QNT_NOT_FOUND;
    QNtDriver *d=&k->drivers[driver-1];
    return d->dispatch(d->context,operation,buffer,length);
}
int64_t qnt_syscall(QNtKernel *k,uint32_t pid,uint32_t operation,uint64_t a,uint64_t b,uint64_t c,uint64_t d){
    if(!k||!process(k,pid))return QNT_ACCESS;
    k->syscalls++;
    switch(operation){
        case QNT_SC_YIELD:return qnt_schedule(k);
        case QNT_SC_TICKS:return (int64_t)k->clock_ticks;
        case QNT_SC_GETPID:return (int64_t)pid;
        case QNT_SC_OPEN:return qnt_file_open(k,pid,(const char *)(uintptr_t)a,(uint64_t *)(uintptr_t)b);
        case QNT_SC_READ:return qnt_file_read(k,pid,a,(uint32_t)b,(void *)(uintptr_t)c,(size_t)d,(size_t *)(uintptr_t)0);
        case QNT_SC_WRITE:{
            size_t written=0;
            int status=qnt_file_write(k,pid,a,(uint32_t)b,(const void *)(uintptr_t)c,(size_t)d,&written);
            return status==QNT_OK?(int64_t)written:status;
        }
        case QNT_SC_CLOSE:return qnt_object_close(k,pid,a);
        default:return QNT_INVALID;
    }
}

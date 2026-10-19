#include "pe64.h"
static uint16_t get16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t get32(const uint8_t *p){return (uint32_t)get16(p)|((uint32_t)get16(p+2)<<16);}
static uint64_t get64(const uint8_t *p){return (uint64_t)get32(p)|((uint64_t)get32(p+4)<<32);}
static void put64(uint8_t *p,uint64_t value){for(int i=0;i<8;i++)p[i]=(uint8_t)(value>>(i*8));}
static int span(size_t base,size_t length,size_t total){return base<=total&&length<=total-base;}
static void copybytes(uint8_t *dst,const uint8_t *src,size_t length){for(size_t i=0;i<length;i++)dst[i]=src[i];}
static int string_ok(const uint8_t *mem,size_t size,size_t offset){
    if(offset>=size)return 0;
    for(size_t i=offset;i<size;i++)if(mem[i]==0)return 1;
    return 0;
}
static int name_equals(const char *a,const char *b){
    for(;;){if(*a!=*b)return 0;if(!*a)return 1;a++;b++;}
}
static int relocate(uint8_t *mem,size_t size,uint32_t rva,uint32_t length,uint64_t delta){
    if(!length)return delta==0?Q_PE_OK:Q_PE_RELOCATION;
    if(!span(rva,length,size))return Q_PE_BOUNDS;
    size_t pos=rva,end=(size_t)rva+length;
    while(pos<end){
        if(!span(pos,8,end))return Q_PE_RELOCATION;
        uint32_t page=get32(mem+pos),block=get32(mem+pos+4);
        if(block<8||!span(pos,block,end)||((block-8)&1))return Q_PE_RELOCATION;
        for(size_t i=8;i<block;i+=2){
            uint16_t record=get16(mem+pos+i);
            uint32_t kind=record>>12,offset=record&4095;
            uint64_t at=(uint64_t)page+offset;
            if(kind==0)continue;
            if(kind!=10||at>size||!span((size_t)at,8,size))return Q_PE_RELOCATION;
            put64(mem+at,get64(mem+at)+delta);
        }
        pos+=block;
    }
    return Q_PE_OK;
}
static int bind_imports(uint8_t *mem,size_t size,uint32_t rva,uint32_t length,const QPe64Resolver *resolver,uint32_t *imports){
    if(!length)return Q_PE_OK;
    if(!resolver||!resolver->resolve||!span(rva,length,size))return Q_PE_IMPORT;
    size_t pos=rva,end=(size_t)rva+length;
    int terminated=0;
    while(pos<end){
        if(!span(pos,20,end))return Q_PE_IMPORT;
        uint32_t lookup=get32(mem+pos),name_rva=get32(mem+pos+12),iat=get32(mem+pos+16);
        if(!lookup&&!name_rva&&!iat){terminated=1;break;}
        if(!name_rva||!iat||!string_ok(mem,size,name_rva))return Q_PE_IMPORT;
        if(!lookup)lookup=iat;
        for(size_t index=0;index<size/8;index++){
            uint64_t source=(uint64_t)lookup+index*8,target=(uint64_t)iat+index*8;
            if(source>size||target>size||!span((size_t)source,8,size)||!span((size_t)target,8,size))return Q_PE_IMPORT;
            uint64_t symbol=get64(mem+source);
            if(!symbol)break;
            const char *name=0;
            uint16_t ordinal=0;
            if(symbol>>63)ordinal=(uint16_t)symbol;
            else{
                if(symbol>size||!span((size_t)symbol,3,size)||!string_ok(mem,size,(size_t)symbol+2))return Q_PE_IMPORT;
                name=(const char *)(mem+(size_t)symbol+2);
            }
            void *resolved=resolver->resolve(resolver->context,(const char *)(mem+name_rva),name,ordinal);
            if(!resolved)return Q_PE_IMPORT;
            put64(mem+target,(uint64_t)(uintptr_t)resolved);
            (*imports)++;
        }
        pos+=20;
    }
    return terminated?Q_PE_OK:Q_PE_IMPORT;
}
int q_pe64_load(const void *file,size_t file_size,void *memory,size_t capacity,const QPe64Resolver *resolver,QPe64Image *out){
    if(!file||!memory||!out)return Q_PE_INVALID;
    const uint8_t *src=(const uint8_t *)file;
    uint8_t *dst=(uint8_t *)memory;
    if(!span(0,64,file_size)||get16(src)!=0x5a4d)return Q_PE_INVALID;
    uint32_t pe=get32(src+60);
    if(!span(pe,24,file_size)||get32(src+pe)!=0x4550)return Q_PE_INVALID;
    const uint8_t *coff=src+pe+4;
    if(get16(coff)!=0x8664)return Q_PE_ARCH;
    uint16_t sections=get16(coff+2),optional_size=get16(coff+16);
    size_t opt=(size_t)pe+24;
    if(!sections||sections>96||!span(opt,optional_size,file_size)||optional_size<112)return Q_PE_INVALID;
    if(get16(src+opt)!=0x20b)return Q_PE_ARCH;
    uint32_t entry=get32(src+opt+16);
    uint64_t preferred=get64(src+opt+24);
    uint32_t image_size=get32(src+opt+56),headers=get32(src+opt+60);
    uint32_t directory_count=get32(src+opt+108);
    if(!image_size||image_size>capacity||headers>image_size||!span(0,headers,file_size)||entry>=image_size)return Q_PE_MEMORY;
    if(!span(opt+optional_size,(size_t)sections*40,file_size))return Q_PE_BOUNDS;
    for(size_t i=0;i<image_size;i++)dst[i]=0;
    copybytes(dst,src,headers);
    for(uint16_t i=0;i<sections;i++){
        size_t s=opt+optional_size+(size_t)i*40;
        uint32_t virtual_size=get32(src+s+8),rva=get32(src+s+12);
        uint32_t raw_size=get32(src+s+16),raw=get32(src+s+20);
        size_t mapped=virtual_size>raw_size?virtual_size:raw_size;
        if(!span(rva,mapped,image_size)||!span(raw,raw_size,file_size))return Q_PE_BOUNDS;
        if(raw_size)copybytes(dst+rva,src+raw,raw_size);
    }
    uint32_t import_rva=0,import_size=0,reloc_rva=0,reloc_size=0;
    if(directory_count>1&&optional_size>=112+16){
        import_rva=get32(src+opt+120);import_size=get32(src+opt+124);
    }
    if(directory_count>5&&optional_size>=112+48){
        reloc_rva=get32(src+opt+152);reloc_size=get32(src+opt+156);
    }
    uint64_t actual=(uint64_t)(uintptr_t)memory;
    if(actual!=preferred){
        int result=relocate(dst,image_size,reloc_rva,reloc_size,actual-preferred);
        if(result!=Q_PE_OK)return result;
    }
    uint32_t count=0;
    int bound=bind_imports(dst,image_size,import_rva,import_size,resolver,&count);
    if(bound!=Q_PE_OK)return bound;
    out->base=dst;out->size=image_size;out->entry_rva=entry;out->imports=count;
    return Q_PE_OK;
}
void *q_pe64_entry(const QPe64Image *image){
    if(!image||!image->base||image->entry_rva>=image->size)return 0;
    return image->base+image->entry_rva;
}
void *q_pe64_export(const QPe64Image *image,const char *name){
    if(!image||!name||!image->base||image->size<0x100)return 0;
    uint8_t *m=image->base;
    if(get16(m)!=0x5a4d)return 0;
    uint32_t pe=get32(m+60);
    if(!span(pe,24,image->size)||get32(m+pe)!=0x4550)return 0;
    size_t opt=(size_t)pe+24;
    if(!span(opt,120,image->size)||get32(m+opt+108)<1)return 0;
    uint32_t rva=get32(m+opt+112),length=get32(m+opt+116);
    if(!rva||!span(rva,length,image->size)||length<40)return 0;
    uint8_t *dir=m+rva;
    uint32_t funcs=get32(dir+28),names=get32(dir+32),ordinals=get32(dir+36);
    uint32_t nfunc=get32(dir+20),nname=get32(dir+24);
    if(!span(funcs,(size_t)nfunc*4,image->size)||!span(names,(size_t)nname*4,image->size)||!span(ordinals,(size_t)nname*2,image->size))return 0;
    for(uint32_t i=0;i<nname;i++){
        uint32_t at=get32(m+names+i*4);
        if(!string_ok(m,image->size,at)||!name_equals((const char *)(m+at),name))continue;
        uint16_t ord=get16(m+ordinals+i*2);
        if(ord>=nfunc)return 0;
        uint32_t address=get32(m+funcs+(size_t)ord*4);
        if(address>=image->size||(address>=rva&&address<(uint64_t)rva+length))return 0;
        return m+address;
    }
    return 0;
}

#include "trash.h"

#define TRASH_DEPTH 8
#define META_BYTES (QFS_PATH+8)
static QFsEntry trash_walk[TRASH_DEPTH][QFS_ENTRIES];
static QFsEntry trash_list_buffer[QFS_ENTRIES];

static int same(const char *a,const char *b){
    for(;*a&&*b;a++,b++){
        char x=*a,y=*b;
        if(x>='A'&&x<='Z')x+=32;
        if(y>='A'&&y<='Z')y+=32;
        if(x!=y)return 0;
    }
    return *a==*b;
}
static int prefix(const char *path,const char *root){
    int i=0;
    while(root[i]&&path[i]==root[i])i++;
    return !root[i]&&(!path[i]||path[i]=='\\');
}
static int make_path(char *out,const char *base,const char *id){
    return qfs_join(out,QFS_PATH,base,id);
}
static const char *directory(QStorage *s){
    return s->home_index==s->ram_index?"\\Recycle":"\\Users\\Default\\.Trash";
}
static int prepare(QStorage *s){
    if(!s||s->home_index<0||s->home_index>=s->count)return -1;
    QFsEntry entry;
    const char *root=directory(s);
    if(qfs_list(s,s->home_index,root,&entry,1)>=0)return 0;
    if(qfs_mkdir(s,s->home_index,root)!=0)return -1;
    return qfs_list(s,s->home_index,root,&entry,1)>=0?0:-1;
}
static int valid_id(const char *id){
    if(!id||id[0]!='T')return 0;
    for(int i=1;i<8;i++)if(id[i]<'0'||id[i]>'9')return 0;
    return !id[8];
}
static int info_path(QStorage *s,const char *id,char *out){
    char name[13];
    for(int i=0;i<8;i++)name[i]=id[i];
    name[8]='.';name[9]='I';name[10]='N';name[11]='F';name[12]=0;
    return make_path(out,directory(s),name);
}
static int read_info(QStorage *s,const char *id,QTrashEntry *out){
    if(!valid_id(id)||!s||!out)return -1;
    char info[QFS_PATH],payload[QFS_PATH],bytes[META_BYTES];
    size_t length=0;
    if(info_path(s,id,info)!=0||make_path(payload,directory(s),id)!=0||
       qfs_read(s,s->home_index,info,bytes,sizeof(bytes),&length)!=0||
       length<8||bytes[0]!='Q'||bytes[1]!='T'||bytes[2]!='R'||bytes[3]!='2'||(bytes[4]!='F'&&bytes[4]!='D'))return -1;
    size_t path_length=(unsigned char)bytes[5];
    path_length|=(size_t)(unsigned char)bytes[6]<<8;
    if(path_length<2||path_length>=QFS_PATH||length!=path_length+7||bytes[7]!='\\')return -1;
    for(size_t i=0;i<path_length;i++)if(bytes[7+i]==0)return -1;
    QFsEntry entries[1];
    int last=0;
    for(size_t i=0;i<path_length;i++)if(bytes[7+i]=='\\')last=(int)i;
    if(path_length==(size_t)last+1)return -1;
    char restored[QFS_PATH];
    for(size_t i=0;i<path_length;i++)restored[i]=bytes[7+i];
    restored[path_length]=0;
    if(prefix(restored,directory(s)))return -1;
    if(qfs_list(s,s->home_index,directory(s),entries,1)<0)return -1;
    int count=qfs_list(s,s->home_index,directory(s),trash_list_buffer,QFS_ENTRIES);
    if(count<0)return -1;
    int matched=0;
    for(int i=0;i<count;i++)if(same(trash_list_buffer[i].name,id)){
        if((trash_list_buffer[i].directory!=0)!=(bytes[4]=='D'))return -1;
        out->size=trash_list_buffer[i].size;
        matched=1;break;
    }
    if(!matched)return -1;
    for(int i=0;i<8;i++)out->id[i]=id[i];
    out->id[8]=0;
    for(size_t i=0;i<path_length;i++)out->path[i]=bytes[7+i];
    out->path[path_length]=0;
    int n=0;
    for(size_t i=(size_t)last+1;i<path_length&&n<71;i++)out->name[n++]=bytes[7+i];
    out->name[n]=0;out->directory=bytes[4]=='D';
    return 0;
}
static int tree(QStorage *s,const char *source,const char *dest,int is_dir,int depth,int remove){
    if(depth>=TRASH_DEPTH)return -1;
    if(!remove){
        if(!is_dir)return qfs_copy(s,s->home_index,source,s->home_index,dest);
        if(qfs_mkdir(s,s->home_index,dest)!=0)return -1;
    }
    if(is_dir){
        int count=qfs_list(s,s->home_index,source,trash_walk[depth],QFS_ENTRIES);
        if(count<0)return -1;
        for(int i=0;i<count;i++){
            char from[QFS_PATH],to[QFS_PATH];
            QFsEntry *entry=&trash_walk[depth][i];
            if(make_path(from,source,entry->name)!=0)return -1;
            if(!remove&&make_path(to,dest,entry->name)!=0)return -1;
            if(tree(s,from,remove?0:to,entry->directory,depth+1,remove)!=0)return -1;
        }
    }
    return remove?qfs_delete(s,s->home_index,source):0;
}
static void cleanup(QStorage *s,const char *path,int is_dir){
    tree(s,path,0,is_dir,0,1);
}
int qtrash_put(QStorage *s,int volume,const char *path,int is_dir){
    if(!s||!path||path[0]!='\\'||!path[1]||volume!=s->home_index||
       prefix(path,directory(s))||prefix(path,"\\Users\\Default\\Applications")||
       same(path,"\\QUSER.DAT")||prepare(s)!=0)return -1;
    const char *root=directory(s);
    int listing=qfs_list(s,volume,root,trash_list_buffer,QFS_ENTRIES);
    if(listing<0||listing>QFS_ENTRIES-2)return -1;
    for(int number=1;number<=9999999;number++){
        char id[]="T0000000";
        int value=number;
        for(int j=7;j>=1;j--){id[j]=(char)('0'+value%10);value/=10;}
        char meta[QFS_PATH],payload[QFS_PATH];
        if(info_path(s,id,meta)!=0||make_path(payload,root,id)!=0)return -1;
        int occupied=0;
        for(int i=0;i<listing;i++){
            if(same(trash_list_buffer[i].name,id)){occupied=1;break;}
            char inf[13];
            for(int j=0;j<8;j++)inf[j]=id[j];
            inf[8]='.';inf[9]='I';inf[10]='N';inf[11]='F';inf[12]=0;
            if(same(trash_list_buffer[i].name,inf)){occupied=1;break;}
        }
        if(occupied)continue;
        if(tree(s,path,payload,is_dir,0,0)!=0){cleanup(s,payload,is_dir);return -1;}
        char data[META_BYTES];
        size_t length=0;while(path[length]&&length<QFS_PATH)length++;
        if(length>=QFS_PATH){cleanup(s,payload,is_dir);return -1;}
        data[0]='Q';data[1]='T';data[2]='R';data[3]='2';
        data[4]=is_dir?'D':'F';data[5]=(char)(length&255);data[6]=(char)(length>>8);
        for(size_t k=0;k<length;k++)data[7+k]=path[k];
        if(qfs_write(s,volume,meta,data,7+length)!=0){
            cleanup(s,payload,is_dir);return -1;
        }
        if(tree(s,path,0,is_dir,0,1)!=0)return -1;
        return 0;
    }
    return -1;
}
int qtrash_list(QStorage *s,QTrashEntry *out,int capacity){
    if(!s||!out||capacity<1)return -1;
    if(prepare(s)!=0)return -1;
    int count=qfs_list(s,s->home_index,directory(s),trash_list_buffer,QFS_ENTRIES);
    if(count<0)return -1;
    int n=0;
    char ids[QTRASH_LIMIT][9];
    for(int i=0;i<count&&n<capacity&&n<QTRASH_LIMIT;i++){
        const char *name=trash_list_buffer[i].name;
        if(!valid_id(name))continue;
        for(int j=0;j<9;j++)ids[n][j]=name[j];
        n++;
    }
    int written=0;
    for(int i=0;i<n;i++)if(read_info(s,ids[i],&out[written])==0)written++;
    return written;
}
int qtrash_restore(QStorage *s,const char *id){
    QTrashEntry item={0};
    if(read_info(s,id,&item)!=0)return -1;
    char payload[QFS_PATH];
    if(make_path(payload,directory(s),id)!=0)return -1;
    char parent[QFS_PATH];
    int last=0;
    for(int i=0;item.path[i];i++)if(item.path[i]=='\\')last=i;
    if(last>0){for(int i=0;i<last;i++)parent[i]=item.path[i];parent[last]=0;}
    else {parent[0]='\\';parent[1]=0;}
    QFsEntry probe[1];
    if(qfs_list(s,s->home_index,parent,probe,1)<0)return -1;
    if(item.directory){
        if(qfs_list(s,s->home_index,item.path,probe,1)>=0)return -1;
    }else{
        char bytes[1];size_t len=0;
        if(qfs_read(s,s->home_index,item.path,bytes,sizeof(bytes),&len)==0)return -1;
        int count=qfs_list(s,s->home_index,parent,trash_list_buffer,QFS_ENTRIES);
        for(int i=0;i<count;i++)if(same(trash_list_buffer[i].name,item.name))return -1;
    }
    if(tree(s,payload,item.path,item.directory,0,0)!=0){cleanup(s,item.path,item.directory);return -1;}
    if(tree(s,payload,0,item.directory,0,1)!=0)return -1;
    char info[QFS_PATH];
    return info_path(s,id,info)==0?qfs_delete(s,s->home_index,info):-1;
}
int qtrash_remove(QStorage *s,const char *id){
    QTrashEntry item={0};
    if(read_info(s,id,&item)!=0)return -1;
    char path[QFS_PATH],info[QFS_PATH];
    if(make_path(path,directory(s),id)!=0||info_path(s,id,info)!=0)return -1;
    if(tree(s,path,0,item.directory,0,1)!=0)return -1;
    return qfs_delete(s,s->home_index,info);
}
int qtrash_empty(QStorage *s){
    static QTrashEntry items[QTRASH_LIMIT];
    int count=qtrash_list(s,items,QTRASH_LIMIT),failed=0;
    if(count<0)return -1;
    for(int i=0;i<count;i++)if(qtrash_remove(s,items[i].id)!=0)failed++;
    return failed?-1:0;
}

#include "trash.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL trash at line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static QStorage storage;
static QTrashEntry items[QTRASH_LIMIT];
static char data[96];
static size_t len;
int main(void){
    qfs_init(&storage,0,0);
    CHECK(qfs_mkdir(&storage,0,"\\Documents")==0);
    CHECK(qfs_write(&storage,0,"\\Documents\\hello.txt","first",5)==0);
    CHECK(qtrash_put(&storage,0,"\\Documents\\hello.txt",0)==0);
    CHECK(qfs_read(&storage,0,"\\Documents\\hello.txt",data,sizeof(data),&len)!=0);
    CHECK(qtrash_list(&storage,items,QTRASH_LIMIT)==1);
    CHECK(!strcmp(items[0].path,"\\Documents\\hello.txt"));
    CHECK(!strcmp(items[0].name,"hello.txt")&&!items[0].directory);
    char id[9];strcpy(id,items[0].id);
    CHECK(qfs_write(&storage,0,"\\Documents\\hello.txt","new",3)==0);
    CHECK(qtrash_restore(&storage,id)!=0);
    CHECK(qfs_read(&storage,0,"\\Documents\\hello.txt",data,sizeof(data),&len)==0);
    CHECK(len==3&&!memcmp(data,"new",3));
    CHECK(qfs_delete(&storage,0,"\\Documents\\hello.txt")==0);
    CHECK(qtrash_restore(&storage,id)==0);
    CHECK(qfs_read(&storage,0,"\\Documents\\hello.txt",data,sizeof(data),&len)==0);
    CHECK(len==5&&!memcmp(data,"first",5));
    CHECK(qtrash_list(&storage,items,QTRASH_LIMIT)==0);
    CHECK(qfs_mkdir(&storage,0,"\\Documents\\Album")==0);
    CHECK(qfs_write(&storage,0,"\\Documents\\Album\\a.txt","alpha",5)==0);
    CHECK(qfs_mkdir(&storage,0,"\\Documents\\Album\\Nested")==0);
    CHECK(qfs_write(&storage,0,"\\Documents\\Album\\Nested\\b.txt","beta",4)==0);
    CHECK(qtrash_put(&storage,0,"\\Documents\\Album",1)==0);
    CHECK(qfs_list(&storage,0,"\\Documents\\Album",items? (QFsEntry *)data:0,1)<0);
    CHECK(qtrash_list(&storage,items,QTRASH_LIMIT)==1&&items[0].directory);
    strcpy(id,items[0].id);
    CHECK(qtrash_restore(&storage,id)==0);
    CHECK(qfs_read(&storage,0,"\\Documents\\Album\\Nested\\b.txt",data,sizeof(data),&len)==0);
    CHECK(len==4&&!memcmp(data,"beta",4));
    CHECK(qtrash_put(&storage,0,"\\Documents\\Album",1)==0);
    CHECK(qtrash_list(&storage,items,QTRASH_LIMIT)==1);
    CHECK(qtrash_remove(&storage,items[0].id)==0);
    CHECK(qtrash_list(&storage,items,QTRASH_LIMIT)==0);
    CHECK(qfs_write(&storage,0,"\\Documents\\temp.txt","one",3)==0);
    CHECK(qtrash_put(&storage,0,"\\Documents\\temp.txt",0)==0);
    CHECK(qtrash_empty(&storage)==0);
    CHECK(qtrash_list(&storage,items,QTRASH_LIMIT)==0);
    CHECK(qtrash_put(&storage,0,"\\Recycle",1)!=0);
    CHECK(qtrash_put(&storage,0,"\\Documents",1)==0);
    CHECK(qtrash_list(&storage,items,QTRASH_LIMIT)==1);
    CHECK(qtrash_empty(&storage)==0);
    puts("PASS: Recycle Bin, collision-safe restore, nested folders, permanent removal");
    return 0;
}

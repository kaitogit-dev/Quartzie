#include "media.h"
#include "storage.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#define CHECK(x) do{if(!(x)){fprintf(stderr,"FAIL media line %d: %s\n",__LINE__,#x);exit(1);}}while(0)
static void u16(uint8_t *b,int off,int v){b[off]=(uint8_t)v;b[off+1]=(uint8_t)(v>>8);}
static void u32(uint8_t *b,int off,unsigned v){u16(b,off,v&65535);u16(b,off+2,v>>16);}
static QStorage storage;
int main(void){
    qfs_init(&storage,0,0);
    CHECK(storage.count==1&&storage.ram_index==0);
    CHECK(qfs_mkdir(&storage,0,"\\\\Pictures")==0);
    CHECK(qfs_write(&storage,0,"\\\\Pictures\\\\test.txt","hello",5)==0);
    QFsEntry entries[QFS_ENTRIES];
    CHECK(qfs_list(&storage,0,"\\\\",entries,QFS_ENTRIES)==1);
    CHECK(entries[0].directory&&strcmp(entries[0].name,"Pictures")==0);
    CHECK(qfs_list(&storage,0,"\\\\Pictures",entries,QFS_ENTRIES)==1);
    CHECK(strcmp(entries[0].name,"test.txt")==0);
    char text_data[32];size_t text_len=0;
    CHECK(qfs_read(&storage,0,"\\\\Pictures\\\\test.txt",text_data,sizeof(text_data),&text_len)==0&&text_len==5);
    CHECK(memcmp(text_data,"hello",5)==0);
    CHECK(qfs_delete(&storage,0,"\\\\Pictures")!=0);
    CHECK(qfs_delete(&storage,0,"\\\\Pictures\\\\test.txt")==0);
    CHECK(qfs_delete(&storage,0,"\\\\Pictures")==0);

    char path[QFS_PATH];
    CHECK(qfs_join(path,sizeof(path),"\\","PHOTO.BMP")==0);
    CHECK(strcmp(path,"\\PHOTO.BMP")==0);
    CHECK(qfs_join(path,sizeof(path),"\\Folder","clip.avi")==0);
    CHECK(strcmp(path,"\\Folder\\clip.avi")==0);
    CHECK(qfs_join(path,sizeof(path),"\\","../wrong")==-1);
    QMedia m;
    uint8_t bmp[70]={0};
    bmp[0]='B';bmp[1]='M';
    u32(bmp,10,54);u32(bmp,14,40);u32(bmp,18,2);u32(bmp,22,2);u16(bmp,26,1);u16(bmp,28,24);
    bmp[54]=0;bmp[55]=0;bmp[56]=255;
    bmp[62]=255;bmp[63]=0;bmp[64]=0;
    CHECK(qm_open(&m,bmp,sizeof(bmp))==0&&m.kind==QM_IMAGE);
    CHECK(m.width==2&&m.height==2);
    CHECK(qm_pixel(&m,0,1)==0xff0000);
    CHECK(qm_pixel(&m,0,0)==0x0000ff);
    bmp[30]=1;CHECK(qm_open(&m,bmp,sizeof(bmp))!=0);
    uint8_t wav[52]={0};
    memcpy(wav,"RIFF",4);memcpy(wav+8,"WAVEfmt ",8);
    u32(wav,16,16);u16(wav,20,1);u16(wav,22,1);u32(wav,24,8000);u32(wav,28,8000);u16(wav,32,1);u16(wav,34,8);
    memcpy(wav+36,"data",4);u32(wav,40,8);
    for(int i=0;i<8;i++)wav[44+i]=(uint8_t)(i*32);
    CHECK(qm_open(&m,wav,sizeof(wav))==0&&m.kind==QM_AUDIO);
    CHECK(qm_audio_count(&m)==8&&qm_audio_sample(&m,0)==0&&qm_audio_sample(&m,7)==224);
    m.playing=1;qm_advance(&m,2000);CHECK(!m.playing);
    puts("PASS: path validation, BMP pixels, PCM WAV samples and playback timing");
    return 0;
}

#include "media.h"
#include "codecs.h"
static uint16_t u16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t u32(const uint8_t *p){return (uint32_t)p[0]|((uint32_t)p[1]<<8)|((uint32_t)p[2]<<16)|((uint32_t)p[3]<<24);}
static int32_t s32(const uint8_t *p){return (int32_t)u32(p);}
static int four(const uint8_t *p,const char *id){return p[0]==(uint8_t)id[0]&&p[1]==(uint8_t)id[1]&&p[2]==(uint8_t)id[2]&&p[3]==(uint8_t)id[3];}
static void clear(QMedia *m){unsigned char *p=(unsigned char *)m;for(size_t i=0;i<sizeof(*m);i++)p[i]=0;}
static int image_info(QMedia *m,const uint8_t *data,size_t length){
    if(length<54||data[0]!='B'||data[1]!='M'||u32(data+14)<40)return -1;
    int32_t width=s32(data+18),height=s32(data+22);
    unsigned bpp=u16(data+28);
    uint32_t offset=u32(data+10);
    if(width<1||width>4096||height==0||height< -4096||height>4096||u16(data+26)!=1||(bpp!=24&&bpp!=32)||u32(data+30)!=0)return -1;
    int rows=height<0?-height:height;
    uint64_t stride=(((uint64_t)width*bpp+31)/32)*4;
    if(offset>length||stride*(uint64_t)rows>length-offset)return -1;
    m->kind=QM_IMAGE;m->width=width;m->height=rows;m->bpp=(int)bpp;m->topdown=height<0;m->pixels_offset=offset;
    m->frame_count=1;return 0;
}
static int avi_chunks(QMedia *m,size_t begin,size_t end,int level){
    if(level>5||end>m->length)return -1;
    const uint8_t *d=m->data;
    for(size_t at=begin;at+8<=end;){
        uint32_t length=u32(d+at+4);
        size_t payload=at+8;
        if(length>end-payload)break;
        if(four(d+at,"LIST")&&length>=4){
            if(avi_chunks(m,payload+4,payload+length,level+1)!=0)return -1;
        }else if(four(d+at,"avih")&&length>=40){
            uint32_t rate=u32(d+payload);
            if(rate) m->fps=(int)(1000000u/rate);
            m->width=(int)u32(d+payload+32);
            m->height=(int)u32(d+payload+36);
        }else if(four(d+at,"strf")&&length>=40){
            if(u32(d+payload)==40&&u32(d+payload+16)==0&&(u16(d+payload+14)==24||u16(d+payload+14)==32)){
                m->bpp=u16(d+payload+14);
                if(s32(d+payload+8)<0)m->topdown=1;
            }
        }else if(d[at]=='0'&&d[at+1]=='0'&&d[at+2]=='d'&&d[at+3]=='b'){
            if(m->frame_count<QM_FRAMES){
                m->frame_start[m->frame_count]=(uint32_t)payload;
                m->frame_size[m->frame_count]=length;
                m->frame_count++;
            }
        }
        size_t next=payload+length+(length&1u);
        if(next<=at)break;
        at=next;
    }
    return 0;
}
static int wav_info(QMedia *m){
    const uint8_t *d=m->data;
    int fmt=0,have_data=0;
    for(size_t at=12;at+8<=m->length;){
        uint32_t size=u32(d+at+4);
        size_t body=at+8;
        if(size>m->length-body)break;
        if(four(d+at,"fmt ")&&size>=16&&u16(d+body)==1){
            m->channels=u16(d+body+2);
            m->sample_rate=u32(d+body+4);
            m->sample_bits=u16(d+body+14);
            fmt=1;
        }else if(four(d+at,"data")){
            m->audio_start=(uint32_t)body;
            m->audio_size=size;
            have_data=1;
        }
        at=body+size+(size&1u);
    }
    if(!fmt||!have_data||(m->channels!=1&&m->channels!=2)||(m->sample_bits!=8&&m->sample_bits!=16)||m->sample_rate<4000||m->sample_rate>96000)return -1;
    m->kind=QM_AUDIO;return 0;
}
int qm_open(QMedia *m,const void *bytes,size_t length){
    if(!m||!bytes||length<12)return -1;
    clear(m);m->data=(const uint8_t *)bytes;m->length=length;
    const uint8_t *d=m->data;
    if(d[0]=='B'&&d[1]=='M'&&image_info(m,d,length)==0)return 0;
    if(!four(d,"RIFF")){
        uint32_t *pixels=0;int width=0,height=0;
        if(qc_image(d,length,&pixels,&width,&height)==0){
            m->kind=QM_IMAGE;m->width=width;m->height=height;
            m->rgba=(const uint8_t *)(const void *)pixels;m->decoded_image=1;m->frame_count=1;
            return 0;
        }
        if((four(d,"ID3")||(d[0]==0xff&&(d[1]&0xe0)==0xe0))&&
            qc_mp3(d,length,(uint8_t **)&m->pcm8,&m->pcm_count,&m->sample_rate)==0){
            m->kind=QM_AUDIO;m->channels=1;m->sample_bits=8;
            m->audio_size=(uint32_t)m->pcm_count;
            return 0;
        }
        return -1;
    }
    if(four(d+8,"WAVE"))return wav_info(m);
    if(!four(d+8,"AVI "))return -1;
    if(avi_chunks(m,12,length,0)!=0||m->width<1||m->height<1||m->width>1920||m->height>1080||(m->bpp!=24&&m->bpp!=32)||m->frame_count<1)return -1;
    uint64_t stride=(((uint64_t)m->width*m->bpp+31)/32)*4;
    for(int i=0;i<m->frame_count;i++)if(stride*(uint64_t)m->height>m->frame_size[i])return -1;
    if(m->fps<1||m->fps>60)m->fps=24;
    m->kind=QM_VIDEO;return 0;
}
uint32_t qm_pixel(const QMedia *m,int x,int y){
    if(!m||(m->kind!=QM_IMAGE&&m->kind!=QM_VIDEO)||x<0||y<0||x>=m->width||y>=m->height)return 0;
    if(m->decoded_image&&m->rgba){
        size_t p=((size_t)y*(size_t)m->width+(size_t)x)*4;
        uint32_t alpha=m->rgba[p+3];
        uint32_t red=(m->rgba[p]*alpha+16u*(255u-alpha))/255u;
        uint32_t green=(m->rgba[p+1]*alpha+24u*(255u-alpha))/255u;
        uint32_t blue=(m->rgba[p+2]*alpha+39u*(255u-alpha))/255u;
        return (red<<16)|(green<<8)|blue;
    }
    int stride=(int)((((uint64_t)m->width*m->bpp+31)/32)*4);
    int row=m->topdown?y:m->height-y-1;
    size_t base=m->kind==QM_VIDEO?m->frame_start[m->frame]:m->pixels_offset;
    size_t index=base+(size_t)row*(size_t)stride+(size_t)x*(size_t)(m->bpp/8);
    if(index+3>m->length)return 0;
    const uint8_t *p=m->data+index;
    return ((uint32_t)p[2]<<16)|((uint32_t)p[1]<<8)|p[0];
}
uint64_t qm_audio_count(const QMedia *m){
    if(m&&m->pcm8)return m->pcm_count;
    return m&&m->kind==QM_AUDIO?m->audio_size/(m->channels*(m->sample_bits/8)):0;
}
uint8_t qm_audio_sample(const QMedia *m,uint64_t n){
    if(!m||m->kind!=QM_AUDIO||n>=qm_audio_count(m))return 128;
    if(m->pcm8)return m->pcm8[n];
    size_t offset=m->audio_start+(size_t)(n*m->channels*(m->sample_bits/8));
    int value=0;
    for(uint32_t c=0;c<m->channels;c++){
        if(m->sample_bits==8)value+=((int)m->data[offset+c]-128)*256;
        else value+=(int16_t)u16(m->data+offset+c*2);
    }
    value=value/(int)m->channels/256+128;
    return (uint8_t)(value<0?0:value>255?255:value);
}
int qm_advance(QMedia *m,uint32_t elapsed){
    if(!m||!m->playing)return 0;
    m->elapsed_us+=elapsed;
    if(m->kind==QM_VIDEO){
        uint64_t frame=(m->elapsed_us*(uint64_t)m->fps)/1000000u;
        if(frame>=(uint64_t)m->frame_count){
            m->frame=m->frame_count-1;m->playing=0;return 1;
        }
        if(m->frame!=(int)frame){m->frame=(int)frame;return 1;}
    }else if(m->kind==QM_AUDIO){
        if(m->elapsed_us*m->sample_rate/1000000u>=qm_audio_count(m)){m->playing=0;return 1;}
        return 1;
    }
    return 0;
}

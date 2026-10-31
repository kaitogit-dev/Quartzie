#include "package.h"

static uint16_t u16(const uint8_t *p){return (uint16_t)p[0]|((uint16_t)p[1]<<8);}
static uint32_t u32(const uint8_t *p){return (uint32_t)u16(p)|((uint32_t)u16(p+2)<<16);}
static uint32_t rotate(uint32_t x,int n){return (x>>n)|(x<<(32-n));}
static const uint32_t constants[64]={
    0x428a2f98,0x71374491,0xb5c0fbcf,0xe9b5dba5,0x3956c25b,0x59f111f1,0x923f82a4,0xab1c5ed5,
    0xd807aa98,0x12835b01,0x243185be,0x550c7dc3,0x72be5d74,0x80deb1fe,0x9bdc06a7,0xc19bf174,
    0xe49b69c1,0xefbe4786,0x0fc19dc6,0x240ca1cc,0x2de92c6f,0x4a7484aa,0x5cb0a9dc,0x76f988da,
    0x983e5152,0xa831c66d,0xb00327c8,0xbf597fc7,0xc6e00bf3,0xd5a79147,0x06ca6351,0x14292967,
    0x27b70a85,0x2e1b2138,0x4d2c6dfc,0x53380d13,0x650a7354,0x766a0abb,0x81c2c92e,0x92722c85,
    0xa2bfe8a1,0xa81a664b,0xc24b8b70,0xc76c51a3,0xd192e819,0xd6990624,0xf40e3585,0x106aa070,
    0x19a4c116,0x1e376c08,0x2748774c,0x34b0bcb5,0x391c0cb3,0x4ed8aa4a,0x5b9cca4f,0x682e6ff3,
    0x748f82ee,0x78a5636f,0x84c87814,0x8cc70208,0x90befffa,0xa4506ceb,0xbef9a3f7,0xc67178f2
};
static void digest(const uint8_t *bytes,size_t length,uint8_t out[32]){
    uint32_t state[8]={0x6a09e667,0xbb67ae85,0x3c6ef372,0xa54ff53a,0x510e527f,0x9b05688c,0x1f83d9ab,0x5be0cd19};
    size_t blocks=(length/64)+1+((length%64)>=56);
    for(size_t block=0;block<blocks;block++){
        uint8_t chunk[64]={0};uint32_t words[64];
        for(int j=0;j<64;j++){
            size_t n=block*64u+(size_t)j;
            if(n<length)chunk[j]=bytes[n];
            else if(n==length)chunk[j]=0x80;
            else if(block==blocks-1&&j>=56)chunk[j]=(uint8_t)(((uint64_t)length*8u)>>(8*(63-j)));
        }
        for(int j=0;j<16;j++){
            int n=j*4;
            words[j]=((uint32_t)chunk[n]<<24)|((uint32_t)chunk[n+1]<<16)|((uint32_t)chunk[n+2]<<8)|chunk[n+3];
        }
        for(int j=16;j<64;j++){
            uint32_t a=words[j-15],b=words[j-2];
            words[j]=(rotate(b,17)^rotate(b,19)^(b>>10))+words[j-7]+(rotate(a,7)^rotate(a,18)^(a>>3))+words[j-16];
        }
        uint32_t a=state[0],b=state[1],c=state[2],d=state[3],e=state[4],f=state[5],g=state[6],h=state[7];
        for(int j=0;j<64;j++){
            uint32_t first=h+(rotate(e,6)^rotate(e,11)^rotate(e,25))+((e&f)^(~e&g))+constants[j]+words[j];
            uint32_t second=(rotate(a,2)^rotate(a,13)^rotate(a,22))+((a&b)^(a&c)^(b&c));
            h=g;g=f;f=e;e=d+first;d=c;c=b;b=a;a=first+second;
        }
        state[0]+=a;state[1]+=b;state[2]+=c;state[3]+=d;
        state[4]+=e;state[5]+=f;state[6]+=g;state[7]+=h;
    }
    for(int i=0;i<8;i++)for(int j=0;j<4;j++)out[i*4+j]=(uint8_t)(state[i]>>(24-8*j));
}
static uint32_t crc(const uint8_t *data,size_t size){
    uint32_t sum=~(uint32_t)0;
    for(size_t i=0;i<size;i++){
        sum^=data[i];
        for(int b=0;b<8;b++)sum=(sum>>1)^(0xedb88320u&(0-(sum&1u)));
    }
    return ~sum;
}
static int same(const char *a,const char *b){
    while(*a&&*b){if(*a++!=*b++)return 0;}
    return *a==*b;
}
static int field(const uint8_t *data,size_t size,const char *key,char *output,size_t capacity){
    size_t keylen=0;while(key[keylen])keylen++;
    for(size_t i=0;i+keylen+4<size;i++){
        if(data[i]!='"')continue;
        size_t j=0;while(j<keylen&&data[i+1+j]==(uint8_t)key[j])j++;
        if(j!=keylen||data[i+1+j]!='"'||data[i+2+j]!=':'||data[i+3+j]!='"')continue;
        size_t start=i+4+j,count=0;
        for(size_t n=start;n<size;n++){
            if(data[n]=='"'){
                if(!count)return -1;
                output[count]=0;
                return 0;
            }
            if(data[n]=='\\'||data[n]<32||count+1>=capacity)return -1;
            output[count++]=(char)data[n];
        }
    }
    return -1;
}
static int number(const uint8_t *data,size_t size,const char *key,uint64_t *value){
    size_t length=0;while(key[length])length++;
    for(size_t i=0;i+length+3<size;i++){
        if(data[i]!='"')continue;
        size_t j=0;while(j<length&&data[i+1+j]==(uint8_t)key[j])j++;
        if(j!=length||data[i+j+1]!='"'||data[i+j+2]!=':')continue;
        i+=j+3;uint64_t n=0;int digits=0;
        while(i<size&&data[i]>='0'&&data[i]<='9'){
            if(n>1000000000u)return -1;
            n=n*10u+(data[i++]-'0');digits++;
        }
        if(!digits)return -1;
        *value=n;return 0;
    }
    return -1;
}
typedef struct {
    char name[40];
    const uint8_t *bytes;
    size_t length;
} Entry;
int qpkg_verify(const uint8_t *bytes,size_t length,QPackage *info){
    if(!bytes||!info||length<54||length>8u*1024u*1024u)return -1;
    size_t end=length-22,minimum=length>65557?length-65557:0;
    while(end>minimum&&u32(bytes+end)!=0x06054b50u)end--;
    if(u32(bytes+end)!=0x06054b50u||end+22u+u16(bytes+end+20)!=length)return -1;
    if(u16(bytes+end+4)||u16(bytes+end+6)||u16(bytes+end+8)!=u16(bytes+end+10))return -1;
    int count=u16(bytes+end+10);
    if(count<2||count>3)return -1;
    size_t position=u32(bytes+end+16),central_size=u32(bytes+end+12);
    if(position>length||central_size>end-position||position+central_size!=end)return -1;
    Entry entries[3]={0};
    for(int index=0;index<count;index++){
        if(position+46>end||u32(bytes+position)!=0x02014b50u)return -1;
        uint16_t flags=u16(bytes+position+8),method=u16(bytes+position+10);
        uint32_t checksum=u32(bytes+position+16),packed=u32(bytes+position+20),raw=u32(bytes+position+24);
        uint16_t n=u16(bytes+position+28),extra=u16(bytes+position+30),comment=u16(bytes+position+32);
        size_t off=u32(bytes+position+42),total=46u+n+extra+comment;
        if(flags||method||raw!=packed||!n||n>=40||position+total>end||off+30u+n>length)return -1;
        if(u32(bytes+off)!=0x04034b50u||u16(bytes+off+6)!=flags||u16(bytes+off+8)!=method)return -1;
        if(u32(bytes+off+14)!=checksum||u32(bytes+off+18)!=packed||u32(bytes+off+22)!=raw)return -1;
        if(u16(bytes+off+26)!=n)return -1;
        size_t data_off=off+30u+u16(bytes+off+26)+u16(bytes+off+28);
        if(data_off>length||packed>length-data_off||data_off+packed>u32(bytes+end+16))return -1;
        for(int j=0;j<n;j++){
            char c=(char)bytes[position+46+j];
            if(c!=bytes[off+30+j]||c<' '||c=='\\')return -1;
            entries[index].name[j]=c;
        }
        entries[index].name[n]=0;
        for(int k=0;k<index;k++)if(same(entries[k].name,entries[index].name))return -1;
        entries[index].bytes=bytes+data_off;entries[index].length=raw;
        if(crc(entries[index].bytes,raw)!=checksum)return -1;
        position+=total;
    }
    if(position!=end)return -1;
    const Entry *manifest=0,*payload=0,*icon=0;
    for(int i=0;i<count;i++){
        if(same(entries[i].name,"manifest.json"))manifest=&entries[i];
        else if(same(entries[i].name,"payload/application.exe")||same(entries[i].name,"payload/application.apk"))payload=&entries[i];
        else if(same(entries[i].name,"assets/icon.png"))icon=&entries[i];
        else return -1;
    }
    if(!manifest||!payload||(icon?count!=3:count!=2)||manifest->length>16384)return -1;
    char schema[40],platform[16],path[40],sha[65],name[81],app_id[80],version[65];
    uint64_t schema_version=0,payload_length=0;
    if(field(manifest->bytes,manifest->length,"schema",schema,sizeof(schema))!=0||
       field(manifest->bytes,manifest->length,"platform",platform,sizeof(platform))!=0||
       field(manifest->bytes,manifest->length,"payload",path,sizeof(path))!=0||
       field(manifest->bytes,manifest->length,"sha256",sha,sizeof(sha))!=0||
       field(manifest->bytes,manifest->length,"name",name,sizeof(name))!=0||
       field(manifest->bytes,manifest->length,"id",app_id,sizeof(app_id))!=0||
       field(manifest->bytes,manifest->length,"version",version,sizeof(version))!=0||
       number(manifest->bytes,manifest->length,"schema_version",&schema_version)!=0||
       number(manifest->bytes,manifest->length,"size",&payload_length)!=0)return -1;
    if(!same(schema,"quartzie.app.package")||schema_version!=1||payload_length!=payload->length||
       !same(path,payload->name)||(!same(platform,"windows")&&!same(platform,"android"))||
       (same(platform,"windows")!=same(payload->name,"payload/application.exe")))return -1;
    if(!app_id[0]||!name[0]||!version[0]||!same(app_id,app_id)||!same(path,payload->name))return -1;
    for(int i=0;app_id[i];i++){
        char c=app_id[i];
        if(!((c>='a'&&c<='z')||(c>='0'&&c<='9')||c=='_'||c=='.'))return -1;
    }
    uint8_t computed[32];digest(payload->bytes,payload->length,computed);
    const char *hex="0123456789abcdef";
    for(int i=0;i<32;i++)if(sha[2*i]!=hex[computed[i]>>4]||sha[2*i+1]!=hex[computed[i]&15])return -1;
    if(sha[64])return -1;
    if(same(platform,"windows")&&(payload->length<64||payload->bytes[0]!='M'||payload->bytes[1]!='Z'))return -1;
    if(same(platform,"android")&&(payload->length<4||u32(payload->bytes)!=0x04034b50u))return -1;
    for(int i=0;app_id[i]||i==0;i++)info->id[i]=app_id[i];
    for(int i=0;name[i]||i==0;i++)info->name[i]=name[i];
    for(int i=0;version[i]||i==0;i++)info->version[i]=version[i];
    for(int i=0;platform[i]||i==0;i++)info->platform[i]=platform[i];
    info->payload=payload->bytes;
    info->payload_size=payload->length;
    return 0;
}

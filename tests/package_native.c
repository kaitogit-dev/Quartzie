#include "package.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
static uint8_t package[8*1024*1024];
#define CHECK(c) do{if(!(c)){fprintf(stderr,"FAIL native package line %d\n",__LINE__);return 1;}}while(0)
int main(int argc,char **argv){
    CHECK(argc==2);
    FILE *source=fopen(argv[1],"rb");
    CHECK(source);
    size_t n=fread(package,1,sizeof(package),source);
    fclose(source);
    CHECK(n>100);
    QPackage info={0};
    CHECK(qpkg_verify(package,n,&info)==0);
    CHECK(!strcmp(info.id,"quartzie.testapp"));
    CHECK(!strcmp(info.version,"1.0"));
    CHECK(!strcmp(info.platform,"windows"));
    CHECK(info.payload_size>128&&info.payload[0]=='M'&&info.payload[1]=='Z');
    package[n/2]^=0x5a;
    CHECK(qpkg_verify(package,n,&info)!=0);
    package[n/2]^=0x5a;
    CHECK(qpkg_verify(package,n-1,&info)!=0);
    CHECK(qpkg_verify(package,12,&info)!=0);
    puts("PASS: native ZIP package SHA256, CRC32, metadata and tamper validation");
    return 0;
}

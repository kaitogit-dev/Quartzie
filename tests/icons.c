#include "icons.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(value) do{if(!(value)){fprintf(stderr,"FAIL icon test at %d\n",__LINE__);exit(1);}}while(0)
int main(void){
    CHECK(QI_COUNT==21);
    CHECK(q_icon_pixels(QI_COUNT)==0);
    for(int i=0;i<QI_COUNT;i++){
        const uint32_t *icon=q_icon_pixels((unsigned int)i);
        CHECK(icon!=0);
        uint32_t visible=0,transparent=0,colorful=0;
        for(int n=0;n<Q_ICON_SIZE*Q_ICON_SIZE;n++){
            uint32_t sample=icon[n];
            uint32_t alpha=sample>>24;
            if(alpha>200)visible++;
            if(alpha<20)transparent++;
            if(alpha>200&&(sample&0xffffffu)!=0)colorful++;
        }
        CHECK(visible>20&&transparent>20&&colorful>20);
    }
    CHECK(q_icon_pixels(QI_FILE_EXPLORER)[24*Q_ICON_SIZE+24]!=q_icon_pixels(QI_TEXT)[24*Q_ICON_SIZE+24]);
    CHECK(q_icon_pixels(QI_START)[5*Q_ICON_SIZE+24]!=0);
    CHECK((q_icon_pixels(QI_START)[0]>>24)==0);
    CHECK((q_icon_pixels(QI_START)[20*Q_ICON_SIZE+24]>>24)>200);
    puts("PASS: 20 application icons and the transparent Quartzie Start logo");
    return 0;
}

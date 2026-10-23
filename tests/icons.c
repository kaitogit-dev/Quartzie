#include "icons.h"
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>

#define CHECK(value) do{if(!(value)){fprintf(stderr,"FAIL icon test at %d\n",__LINE__);exit(1);}}while(0)
int main(void){
    CHECK(QI_COUNT==25);
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
    CHECK(q_icon_pixels(QI_FILE_EXPLORER)[32*Q_ICON_SIZE+32]!=q_icon_pixels(QI_TEXT)[32*Q_ICON_SIZE+32]);
    CHECK(q_icon_pixels(QI_START)[16*Q_ICON_SIZE+32]!=0);
    CHECK((q_icon_pixels(QI_START)[0]>>24)==0);
    CHECK((q_icon_pixels(QI_START)[32*Q_ICON_SIZE+32]>>24)>200);
    CHECK(Q_ICON_SIZE==64);
    for(int id=0;id<QI_COUNT;id++){
        for(int size_index=0;size_index<3;size_index++){
            const int size=size_index==0?40:size_index==1?43:58;
            const uint32_t *bitmap=q_icon_pixels_sized((unsigned int)id,size);
            CHECK(bitmap!=0);
            int visible=0,transparent=0;
            for(int n=0;n<size*size;n++){
                if((bitmap[n]>>24)>200)visible++;
                if((bitmap[n]>>24)<20)transparent++;
            }
            CHECK(visible>12&&transparent>12);
        }
    }
    CHECK(q_icon_pixels(QI_ABOUT)[32*Q_ICON_SIZE+32]!=q_icon_pixels(QI_CALCULATOR)[32*Q_ICON_SIZE+32]);
    CHECK(q_icon_pixels(QI_NOTES)[32*Q_ICON_SIZE+32]!=q_icon_pixels(QI_TEXT)[32*Q_ICON_SIZE+32]);
    CHECK(q_icon_pixels(QI_STORE)[32*Q_ICON_SIZE+32]!=q_icon_pixels(QI_FILE_EXPLORER)[32*Q_ICON_SIZE+32]);
    CHECK((q_icon_pixels(QI_ABOUT)[32*Q_ICON_SIZE+32]>>24)>128);
    CHECK((q_icon_pixels(QI_CALCULATOR)[20*Q_ICON_SIZE+32]>>24)>128);
    CHECK(q_icon_pixels_sized(QI_COUNT,40)==0);
    CHECK(q_icon_pixels_sized(QI_COUNT,25)==0);
    puts("PASS: native 64px source and transparent 40/43/58px app icons");
    return 0;
}

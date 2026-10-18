#ifndef QUARTZIE_PS2_H
#define QUARTZIE_PS2_H
#include <stdint.h>
typedef struct {uint8_t bytes[3];int index;} QPs2Mouse;
int q_ps2_feed(QPs2Mouse *mouse,uint8_t data,int *dx,int *dy,int *buttons);
int q_ps2_enable(void);
int q_ps2_poll(QPs2Mouse *mouse,int *dx,int *dy,int *buttons);
#endif

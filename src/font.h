#ifndef QUARTZIE_FONT_H
#define QUARTZIE_FONT_H
#include <stdint.h>
typedef struct { uint32_t offset; uint8_t width,height,advance; int8_t left,top; } QGlyph;
const uint8_t *q_font_data(void);
const QGlyph *q_font_glyph(int style,int code);
#endif

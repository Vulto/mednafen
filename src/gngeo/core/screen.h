#include "gngeo_compat.h"
#ifndef GNGEO_SCREEN_H
#define GNGEO_SCREEN_H
#include "gngeo_compat.h"
typedef struct { int x,y,w,h; } GNG_Rect;
typedef struct { int w,h,pitch; void *pixels; } GNG_Surface;
extern GNG_Surface *buffer; extern GNG_Rect visible_area; extern int yscreenpadding;
extern Uint8 interpolation,nblitter,neffect,scale,fullscreen;
void screen_update(void);
#endif

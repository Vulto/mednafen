#ifndef GNGEO_COMPAT_H
#define GNGEO_COMPAT_H
#include <stdint.h>
#include <stddef.h>
#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <stdio.h>
#include <stdarg.h>
typedef int64_t off64_t;
typedef uint8_t Uint8; typedef uint16_t Uint16; typedef uint32_t Uint32; typedef int8_t Sint8; typedef int16_t Sint16; typedef int32_t Sint32; typedef uintptr_t Uintptr;
typedef uint8_t UINT8; typedef uint16_t UINT16; typedef uint32_t UINT32; typedef int8_t INT8; typedef int16_t INT16; typedef int32_t INT32;
static inline Uint16 GnGeoSwap16(Uint16 x){return (Uint16)((x>>8)|(x<<8));}
static inline Uint32 GnGeoSwap32(Uint32 x){return ((x>>24)&0xff)|((x>>8)&0xff00)|((x<<8)&0xff0000)|((x<<24)&0xff000000);}
#define GN_TRUE 1
#define GN_FALSE 0
typedef struct GnGeoStateIO { uint8_t *data; size_t pos, size; int load; } *GnGeoStateFile;
static inline int mkstate_data(GnGeoStateFile g, void *data, int size, int mode) { if(!g || size<0) return -1; if(g->pos+(size_t)size>g->size) return -1; if(g->data){ if(mode==0) memcpy(data,g->data+g->pos,size); else memcpy(g->data+g->pos,data,size); } g->pos += (size_t)size; return size; }
extern Uint8 state_version;
#endif

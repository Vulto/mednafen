#ifndef GNGEO_RUNTIME_H
#define GNGEO_RUNTIME_H
#include <stddef.h>
#include <stdint.h>
typedef struct GAME_ROMS GAME_ROMS;
void init_neo(void); void GnGeoCoreShutdown(void); int GnGeoCoreInitRoms(void); void setup_misc_patch(char*); void neogeo_reset(void); void GnGeoCoreSetRoms(GAME_ROMS*,uint8_t*,SYSTEM,COUNTRY); void GnGeoCoreSetInput(uint8_t,uint8_t,uint8_t,uint8_t,uint8_t); void GnGeoCoreGetFrame(uint16_t*,unsigned); void GnGeoCoreSetAudioRate(int); unsigned GnGeoCoreGenerateAudio(int16_t*,unsigned); int GnGeoRunFrame(void); size_t GnGeoCoreStateSize(void); int GnGeoCoreStateSaveLoad(void*,int);
#endif

#ifndef MDFN_GNGEO_MEMORY_H
#define MDFN_GNGEO_MEMORY_H
#include "rom_types.h"
bool GnGeoMemoryInit(GAME_ROMS *roms);
void GnGeoMemoryClose(void);
void GnGeoMemoryReset(void);
void GnGeoMemoryRun(int32_t cycles);
#endif

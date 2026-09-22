#ifndef MDFN_GNGEO_ROM_LOADER_H
#define MDFN_GNGEO_ROM_LOADER_H
#include "rom_types.h"
void GnGeoFreeRoms(GAME_ROMS* roms);
namespace Mednafen { struct GameFile; bool GnGeoHasDriver(GameFile*); bool GnGeoLoadRomSet(GameFile*, GAME_ROMS*, SYSTEM, COUNTRY); }
#endif

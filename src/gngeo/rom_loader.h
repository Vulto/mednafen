#ifndef MDFN_GNGEO_ROM_LOADER_H
#define MDFN_GNGEO_ROM_LOADER_H

#include "rom_types.h"

namespace Mednafen
{

struct GameFile;

bool GnGeoLoadRomSet(
    GameFile* gf,
    GAME_ROMS* roms,
    SYSTEM system,
    COUNTRY country);

}

#endif

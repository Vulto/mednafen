#ifndef MDFN_GNGEO_BIOS_H
#define MDFN_GNGEO_BIOS_H

namespace Mednafen
{

struct GameFile;

bool GnGeoLoadBiosLo(GameFile *gf);
void GnGeoFreeBiosLo(void);

}

#endif

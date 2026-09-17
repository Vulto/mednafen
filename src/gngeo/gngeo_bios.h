#ifndef MDFN_GNGEO_BIOS_H
#define MDFN_GNGEO_BIOS_H
#include <stddef.h>
namespace Mednafen
{
struct GameFile;
bool GnGeoLoadBiosLo(GameFile *gf);
void GnGeoFreeBiosLo(void);
uint8 *GnGeoGetBiosLo(size_t *size);
}
#endif

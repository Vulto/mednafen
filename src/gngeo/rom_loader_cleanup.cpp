#include <mednafen/mednafen.h>
#include <stdlib.h>

#include "rom_loader.h"

namespace Mednafen
{

static void freeRegion(ROM_REGION* region)
{
    if(!region)
        return;

    free(region->p);
    region->p = nullptr;
    region->size = 0;
}

void GnGeoFreeRomSet(GAME_ROMS* roms)
{
    if(!roms)
        return;

    freeRegion(&roms->cpu_m68k);
    freeRegion(&roms->cpu_z80c);
    freeRegion(&roms->tiles);
    freeRegion(&roms->game_sfix);
    freeRegion(&roms->bios_sfix);
    freeRegion(&roms->bios_audio);
    freeRegion(&roms->zoom_table);
    freeRegion(&roms->bios_m68k);
    freeRegion(&roms->spr_usage);
    freeRegion(&roms->gfix_usage);
    freeRegion(&roms->bfix_usage);

    if(roms->adpcmb.p == roms->adpcma.p)
    {
        roms->adpcmb.p = nullptr;
        roms->adpcmb.size = 0;
    }
    else
        freeRegion(&roms->adpcmb);

    freeRegion(&roms->adpcma);

    free(roms->info.name);
    free(roms->info.longname);

    memset(roms, 0, sizeof(*roms));
}

}

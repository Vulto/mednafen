#include <mednafen/mednafen.h>
#include "../compress/ArchiveReader.h"
#include "rom_loader.h"

namespace Mednafen
{

bool GnGeoTestRomSet(GameFile* gf)
{
    if(!gf || !gf->vfs)
        return false;

    if(gf->ext != "zip")
        return false;

    std::unique_ptr<ArchiveReader> dataArchive;

    try
    {
        dataArchive.reset(ArchiveReader::Open(
            &NVFS,
            MDFN_MakeFName(MDFNMKF_FIRMWARE, 0, "gngeo_data.zip")));
    }
    catch(const MDFN_Error&)
    {
        return false;
    }

    if(!dataArchive)
        return false;

    const std::string drvPath = "/rom/" + gf->outside.fbase + ".drv";

    try
    {
        std::unique_ptr<Stream> drv(
            dataArchive->open(drvPath, VirtualFS::MODE_READ));

        return drv != nullptr;
    }
    catch(const MDFN_Error&)
    {
        return false;
    }
}

}

#include <mednafen/mednafen.h>
#include "../src/git.h"
#include "../general.h"
#include "../compress/ArchiveReader.h"
#include "gngeo_bios.h"

namespace Mednafen
{

static uint8 *GnGeoLoBios = nullptr;
static size_t GnGeoLoBiosSize = 0;

bool GnGeoLoadBiosLo(GameFile *gf)
{
    GnGeoFreeBiosLo();

    std::unique_ptr<ArchiveReader> archive(
        ArchiveReader::Open(
            &NVFS,
            MDFN_MakeFName(MDFNMKF_FIRMWARE, 0, "neogeo.zip")));

    if(!archive && gf && gf->outside.vfs)
    {
        std::string path = gf->outside.dir.empty() ? "neogeo.zip" : gf->outside.dir + "/neogeo.zip";
        archive.reset(ArchiveReader::Open(gf->outside.vfs, path));
    }

    if(!archive)
        return false;

    std::unique_ptr<Stream> stream;

    try
    {
        stream.reset(archive->open("000-lo.lo", VirtualFS::MODE_READ));
    }
    catch(const MDFN_Error&)
    {
        stream.reset();
    }

    if(!stream)
    {
        try
        {
            stream.reset(archive->open("/000-lo.lo", VirtualFS::MODE_READ));
        }
        catch(const MDFN_Error&)
        {
            stream.reset();
        }
    }

    if(!stream)
    {
        for(size_t i = 0; i < archive->num_files(); i++)
        {
            try
            {
                if(archive->get_file_size(i) == 0)
                    continue;
                stream.reset(archive->open(i));
                if(stream)
                    break;
            }
            catch(const MDFN_Error&)
            {
                stream.reset();
            }
        }
    }

    if(!stream)
        return false;

    GnGeoLoBiosSize = (size_t)stream->size();
    GnGeoLoBios = (uint8 *)malloc(GnGeoLoBiosSize);

    if(!GnGeoLoBios)
    {
        GnGeoLoBiosSize = 0;
        return false;
    }

    if(stream->read(GnGeoLoBios, GnGeoLoBiosSize) != GnGeoLoBiosSize)
    {
        GnGeoFreeBiosLo();
        return false;
    }

    return true;
}

void GnGeoFreeBiosLo(void)
{
    free(GnGeoLoBios);
    GnGeoLoBios = nullptr;
    GnGeoLoBiosSize = 0;
}

uint8 *GnGeoGetBiosLo(size_t *size) { if(size) *size=GnGeoLoBiosSize; return GnGeoLoBios; }
}

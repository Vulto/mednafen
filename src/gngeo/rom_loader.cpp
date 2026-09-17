#include <mednafen/mednafen.h>
#include "../src/git.h"
#include "../general.h"
#include "../compress/ArchiveReader.h"

#ifdef HAVE_CONFIG_H
#include "config.h"
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <zlib.h>

#include "rom_loader.h"

#define TILE_INVISIBLE 1
#define LOAD_BUF_SIZE (128 * 1024)

static int GnGeoAllocateRegion(ROM_REGION *r, Uint32 size, int region)
{
    (void)region;
    r->p = NULL;
    r->size = size;
    if(size == 0) return 0;
    r->p = (Uint8 *)malloc(size);
    if(r->p == NULL) { r->size = 0; return -1; }
    memset(r->p, 0, size);
    return 0;
}

static void GnGeoFreeRegion(ROM_REGION *r)
{
    if(r == NULL) return;
    free(r->p);
    r->p = NULL;
    r->size = 0;
}

static void GnGeoFreeRoms(GAME_ROMS *roms)
{
    if(roms == nullptr) return;
    GnGeoFreeRegion(&roms->cpu_m68k);
    GnGeoFreeRegion(&roms->cpu_z80c);
    GnGeoFreeRegion(&roms->tiles);
    GnGeoFreeRegion(&roms->game_sfix);
    GnGeoFreeRegion(&roms->cpu_z80);
    GnGeoFreeRegion(&roms->bios_audio);
    if(roms->adpcmb.p != roms->adpcma.p) GnGeoFreeRegion(&roms->adpcmb);
    else { roms->adpcmb.p = nullptr; roms->adpcmb.size = 0; }
    GnGeoFreeRegion(&roms->adpcma);
    GnGeoFreeRegion(&roms->bios_m68k);
    GnGeoFreeRegion(&roms->bios_sfix);
    GnGeoFreeRegion(&roms->spr_usage);
    GnGeoFreeRegion(&roms->gfix_usage);
    free(roms->info.name);
    free(roms->info.longname);
    roms->info.name = nullptr;
    roms->info.longname = nullptr;
}

static Uint32 GnGeoConvertRomTile(Uint8 *g, Uint32 tileno)
{
    Uint8 swap[128];
    Uint32 *gfxdata = (Uint32 *)&g[tileno << 7];
    Uint32 usage = 0;
    memcpy(swap, gfxdata, 128);
    for(int y = 0; y < 16; y++)
    {
        Uint32 dw = 0;
        for(int x = 0; x < 8; x++)
        {
            Uint32 pen = ((swap[64 + (y << 2) + 3] >> x) & 1) << 3;
            pen |= ((swap[64 + (y << 2) + 1] >> x) & 1) << 2;
            pen |= ((swap[64 + (y << 2) + 2] >> x) & 1) << 1;
            pen |= (swap[64 + (y << 2)] >> x) & 1;
            dw |= pen << ((7 - x) << 2);
            usage |= 1U << pen;
        }
        *gfxdata++ = dw;
        dw = 0;
        for(int x = 0; x < 8; x++)
        {
            Uint32 pen = ((swap[(y << 2) + 3] >> x) & 1) << 3;
            pen |= ((swap[(y << 2) + 1] >> x) & 1) << 2;
            pen |= ((swap[(y << 2) + 2] >> x) & 1) << 1;
            pen |= (swap[y << 2] >> x) & 1;
            dw |= pen << ((7 - x) << 2);
            usage |= 1U << pen;
        }
        *gfxdata++ = dw;
    }
    return (usage & ~1U) == 0 ? TILE_INVISIBLE << ((tileno & 0xF) * 2) : 0;
}

static bool GnGeoConvertAllTile(GAME_ROMS *roms)
{
    if(roms == nullptr || roms->tiles.p == nullptr) return false;
    Uint32 usage_size = (roms->tiles.size >> 11) * sizeof(Uint32);
    if(GnGeoAllocateRegion(&roms->spr_usage, usage_size, REGION_SPR_USAGE) != 0) return false;
    memset(roms->spr_usage.p, 0, roms->spr_usage.size);
    for(Uint32 i = 0; i < (roms->tiles.size >> 7); i++)
        ((Uint32 *)roms->spr_usage.p)[i >> 4] |= GnGeoConvertRomTile(roms->tiles.p, i);
    return true;
}

static bool GnGeoConvertAllChar(GAME_ROMS *roms)
{
    if(roms == nullptr || roms->game_sfix.p == nullptr || roms->gfix_usage.p == nullptr) return false;
    Uint32 size = roms->game_sfix.size;
    if((size & 31) != 0) return false;
    Uint8 *src = (Uint8 *)malloc(size);
    if(src == nullptr) return false;
    memcpy(src, roms->game_sfix.p, size);
    Uint8 *ptr = roms->game_sfix.p;
    Uint8 *source = src;
    Uint8 *usage_ptr = roms->gfix_usage.p;
    for(Uint32 remaining = size; remaining > 0; remaining -= 32)
    {
        Uint8 usage = 0;
        for(int j = 0; j < 8; j++)
        {
            *ptr++ = source[16]; usage |= source[16];
            *ptr++ = source[24]; usage |= source[24];
            *ptr++ = source[0]; usage |= source[0];
            *ptr++ = source[8]; usage |= source[8];
            source++;
        }
        source += 24;
        *usage_ptr++ = usage;
    }
    free(src);
    return true;
}

static bool GnGeoCheckCrc(Mednafen::Stream *stream, Uint32 expected)
{
    Uint8 buffer[LOAD_BUF_SIZE];
    uLong crc = crc32(0L, Z_NULL, 0);
    uint64 remaining = stream->size();
    try
    {
        stream->seek(0, SEEK_SET);
        while(remaining != 0)
        {
            uint64 chunk = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
            if(stream->read(buffer, chunk) != chunk) return false;
            crc = crc32(crc, buffer, (uInt)chunk);
            remaining -= chunk;
        }
        stream->seek(0, SEEK_SET);
    }
    catch(const Mednafen::MDFN_Error&) { return false; }
    return (Uint32)crc == expected;
}

static std::unique_ptr<Mednafen::Stream> GnGeoOpenRom(Mednafen::ArchiveReader *archive, Uint32 expected_crc, const char *filename)
{
    if(archive == nullptr) return nullptr;
    if(filename != nullptr)
    {
        try
        {
            std::unique_ptr<Mednafen::Stream> stream(archive->open(filename, Mednafen::VirtualFS::MODE_READ));
            if(stream && GnGeoCheckCrc(stream.get(), expected_crc)) return stream;
        }
        catch(const Mednafen::MDFN_Error&) {}
    }
    for(size_t i = 0; i < archive->num_files(); i++)
    {
        if(archive->get_file_size(i) == 0) continue;
        try
        {
            std::unique_ptr<Mednafen::Stream> stream(archive->open(i));
            if(stream && GnGeoCheckCrc(stream.get(), expected_crc)) return stream;
        }
        catch(const Mednafen::MDFN_Error&) {}
    }
    return nullptr;
}

static ROM_REGION *GnGeoRegion(GAME_ROMS *roms, int region)
{
    switch(region)
    {
        case REGION_SPRITES: return &roms->tiles;
        case REGION_AUDIO_CPU_CARTRIDGE: return &roms->cpu_z80;
        case REGION_AUDIO_CPU_ENCRYPTED: return &roms->cpu_z80c;
        case REGION_MAIN_CPU_CARTRIDGE: return &roms->cpu_m68k;
        case REGION_FIXED_LAYER_CARTRIDGE: return &roms->game_sfix;
        case REGION_AUDIO_DATA_1: return &roms->adpcma;
        case REGION_AUDIO_DATA_2: return &roms->adpcmb;
        case REGION_MAIN_CPU_BIOS: return &roms->bios_m68k;
        case REGION_AUDIO_CPU_BIOS: return &roms->bios_audio;
        case REGION_FIXED_LAYER_BIOS: return &roms->bios_sfix;
        default: return nullptr;
    }
}

static bool GnGeoLoadRegion(Mednafen::ArchiveReader *archive, GAME_ROMS *roms, int region, Uint32 src, Uint32 dest, Uint32 size, Uint32 crc, const char *filename)
{
    std::unique_ptr<Mednafen::Stream> stream = GnGeoOpenRom(archive, crc, filename);
    ROM_REGION *target = GnGeoRegion(roms, region);
    if(!stream || target == nullptr || target->p == nullptr) return false;
    try { if(src != 0) stream->seek(region == REGION_SPRITES ? src / 2 : src, SEEK_SET); }
    catch(const Mednafen::MDFN_Error&) { return false; }
    if(region == REGION_SPRITES)
    {
        if(target->size < (dest & ~1U) + size * 2U) return false;
        Uint8 buffer[LOAD_BUF_SIZE];
        Uint8 *p = target->p + dest;
        while(size != 0)
        {
            Uint32 chunk = size > sizeof(buffer) ? sizeof(buffer) : size;
            if(stream->read(buffer, chunk) != chunk) return false;
            for(Uint32 i = 0; i < chunk; i++) { *p = buffer[i]; p += 2; }
            size -= chunk;
        }
    }
    else
    {
        if(target->size < dest + size) return false;
        Uint8 *p = target->p + dest;
        while(size != 0)
        {
            Uint32 chunk = size > LOAD_BUF_SIZE ? LOAD_BUF_SIZE : size;
            if(stream->read(p, chunk) != chunk) return false;
            p += chunk;
            size -= chunk;
        }
    }
    return true;
}

static std::unique_ptr<Mednafen::Stream> GnGeoOpenBiosFile(Mednafen::ArchiveReader *archive, const char *name)
{
    if(!archive) return nullptr;
    try { return std::unique_ptr<Mednafen::Stream>(archive->open(name, Mednafen::VirtualFS::MODE_READ)); }
    catch(const Mednafen::MDFN_Error&) {}
    for(size_t i = 0; i < archive->num_files(); i++)
    {
        const std::string *path = archive->get_file_path(i);
        if(path && (path->size() == strlen(name) || (path->size() == strlen(name) + 1 && (*path)[0] == '/')))
        {
            const char *p = path->c_str() + (path->at(0) == '/' ? 1 : 0);
            if(!strcmp(p, name))
            {
                try { return std::unique_ptr<Mednafen::Stream>(archive->open(i)); }
                catch(const Mednafen::MDFN_Error&) { return nullptr; }
            }
        }
    }
    return nullptr;
}

static bool GnGeoLoadBios(Mednafen::GameFile *gf, GAME_ROMS *roms, SYSTEM system, COUNTRY country)
{
    std::unique_ptr<Mednafen::ArchiveReader> bios_archive(
        Mednafen::ArchiveReader::Open(&Mednafen::NVFS,
            Mednafen::MDFN_MakeFName(Mednafen::MDFNMKF_FIRMWARE, 0, "neogeo.zip")));
    if(!bios_archive && gf && gf->outside.vfs)
    {
        std::string path = gf->outside.dir.empty() ? "neogeo.zip" : gf->outside.dir + "/neogeo.zip";
        bios_archive.reset(Mednafen::ArchiveReader::Open(gf->outside.vfs, path));
    }
    if(!bios_archive) return false;

    if(roms->bios_sfix.p == nullptr)
    {
        std::unique_ptr<Mednafen::Stream> stream = GnGeoOpenBiosFile(bios_archive.get(), "sfix.sfx");
        if(!stream) stream = GnGeoOpenBiosFile(bios_archive.get(), "sfix.sfix");
        if(!stream) return false;
        Uint32 size = (Uint32)stream->size();
        if(GnGeoAllocateRegion(&roms->bios_sfix, size, REGION_FIXED_LAYER_BIOS) != 0) return false;
        if(stream->read(roms->bios_sfix.p, size) != size) return false;
    }

    if(roms->bios_m68k.p == nullptr)
    {
        const char *romfile = "sp-s2.sp1";
        if(system == SYS_UNIBIOS) romfile = "uni-bios.rom";
        else if(system == SYS_HOME) romfile = "aes-bios.bin";
        else if(country == CTY_JAPAN) romfile = "vs-bios.rom";
        else if(country == CTY_USA) romfile = "usa_2slt.bin";
        else if(country == CTY_ASIA) romfile = "asia-s3.rom";
        std::unique_ptr<Mednafen::Stream> stream = GnGeoOpenBiosFile(bios_archive.get(), romfile);
        if(!stream) return false;
        Uint32 size = (Uint32)stream->size();
        if(GnGeoAllocateRegion(&roms->bios_m68k, size, REGION_MAIN_CPU_BIOS) != 0) return false;
        if(stream->read(roms->bios_m68k.p, size) != size) return false;
    }
    return true;
}

bool Mednafen::GnGeoLoadRomSet(Mednafen::GameFile *gf, GAME_ROMS *roms, SYSTEM system, COUNTRY country)
{
    memset(roms, 0, sizeof(*roms));
    if(!gf || !gf->vfs || !gf->outside.vfs) return false;

    std::unique_ptr<Mednafen::ArchiveReader> data_archive(
        Mednafen::ArchiveReader::Open(&Mednafen::NVFS,
            MDFN_MakeFName(MDFNMKF_FIRMWARE, 0, "gngeo_data.zip")));
    if(!data_archive) return false;

    std::string drv_path = "/rom/" + gf->outside.fbase + ".drv";
    std::unique_ptr<Mednafen::Stream> drv(data_archive->open(drv_path, Mednafen::VirtualFS::MODE_READ));
    if(!drv) return false;

    ROM_DEF drv_def;
    memset(&drv_def, 0, sizeof(drv_def));
    if(drv->read(drv_def.name, sizeof(drv_def.name)) != sizeof(drv_def.name) ||
       drv->read(drv_def.parent, sizeof(drv_def.parent)) != sizeof(drv_def.parent) ||
       drv->read(drv_def.longname, sizeof(drv_def.longname)) != sizeof(drv_def.longname) ||
       drv->read(&drv_def.year, sizeof(drv_def.year)) != sizeof(drv_def.year)) return false;
    for(unsigned i = 0; i < 10; i++)
        if(drv->read(&drv_def.romsize[i], sizeof(drv_def.romsize[i])) != sizeof(drv_def.romsize[i])) return false;
    if(drv->read(&drv_def.nb_romfile, sizeof(drv_def.nb_romfile)) != sizeof(drv_def.nb_romfile) || drv_def.nb_romfile > 32) return false;
    for(unsigned i = 0; i < drv_def.nb_romfile; i++)
    {
        auto &r = drv_def.rom[i];
        if(drv->read(r.filename, sizeof(r.filename)) != sizeof(r.filename) ||
           drv->read(&r.region, sizeof(r.region)) != sizeof(r.region) ||
           drv->read(&r.src, sizeof(r.src)) != sizeof(r.src) ||
           drv->read(&r.dest, sizeof(r.dest)) != sizeof(r.dest) ||
           drv->read(&r.size, sizeof(r.size)) != sizeof(r.size) ||
           drv->read(&r.crc, sizeof(r.crc)) != sizeof(r.crc)) return false;
    }

    roms->info.name = strdup(drv_def.name);
    roms->info.longname = strdup(drv_def.longname);
    roms->info.year = (int)drv_def.year;
    roms->info.flags = 0;
    if(!roms->info.name || !roms->info.longname) { GnGeoFreeRoms(roms); return false; }

    if(GnGeoAllocateRegion(&roms->cpu_m68k, drv_def.romsize[REGION_MAIN_CPU_CARTRIDGE], REGION_MAIN_CPU_CARTRIDGE) != 0) goto fail;
    if(drv_def.romsize[REGION_AUDIO_CPU_CARTRIDGE] == 0 && drv_def.romsize[REGION_AUDIO_CPU_ENCRYPTED] != 0)
    {
        if(GnGeoAllocateRegion(&roms->cpu_z80c, 0x80000, REGION_AUDIO_CPU_ENCRYPTED) != 0 ||
           GnGeoAllocateRegion(&roms->cpu_z80, 0x90000, REGION_AUDIO_CPU_CARTRIDGE) != 0) goto fail;
    }
    else if(GnGeoAllocateRegion(&roms->cpu_z80, drv_def.romsize[REGION_AUDIO_CPU_CARTRIDGE], REGION_AUDIO_CPU_CARTRIDGE) != 0) goto fail;
    if(GnGeoAllocateRegion(&roms->tiles, drv_def.romsize[REGION_SPRITES], REGION_SPRITES) != 0) goto fail;
    if(GnGeoAllocateRegion(&roms->game_sfix, drv_def.romsize[REGION_FIXED_LAYER_CARTRIDGE], REGION_FIXED_LAYER_CARTRIDGE) != 0) goto fail;
    if(GnGeoAllocateRegion(&roms->gfix_usage, roms->game_sfix.size >> 5, REGION_GAME_FIX_USAGE) != 0) goto fail;
    if(GnGeoAllocateRegion(&roms->adpcma, drv_def.romsize[REGION_AUDIO_DATA_1], REGION_AUDIO_DATA_1) != 0) goto fail;
    if(GnGeoAllocateRegion(&roms->adpcmb, drv_def.romsize[REGION_AUDIO_DATA_2], REGION_AUDIO_DATA_2) != 0) goto fail;
    if(drv_def.romsize[REGION_MAIN_CPU_BIOS]) { roms->info.flags |= HAS_CUSTOM_CPU_BIOS; if(GnGeoAllocateRegion(&roms->bios_m68k, drv_def.romsize[REGION_MAIN_CPU_BIOS], REGION_MAIN_CPU_BIOS) != 0) goto fail; }
    if(drv_def.romsize[REGION_AUDIO_CPU_BIOS]) { roms->info.flags |= HAS_CUSTOM_AUDIO_BIOS; if(GnGeoAllocateRegion(&roms->bios_audio, drv_def.romsize[REGION_AUDIO_CPU_BIOS], REGION_AUDIO_CPU_BIOS) != 0) goto fail; }
    if(drv_def.romsize[REGION_FIXED_LAYER_BIOS]) { roms->info.flags |= HAS_CUSTOM_SFIX_BIOS; if(GnGeoAllocateRegion(&roms->bios_sfix, drv_def.romsize[REGION_FIXED_LAYER_BIOS], REGION_FIXED_LAYER_BIOS) != 0) goto fail; }

    std::string game_path = gf->outside.dir.empty() ? gf->outside.fbase + ".zip" : gf->outside.dir + "/" + gf->outside.fbase + ".zip";
    std::unique_ptr<Mednafen::ArchiveReader> game_archive(Mednafen::ArchiveReader::Open(gf->outside.vfs, game_path));
    if(!game_archive) goto fail;

    std::unique_ptr<Mednafen::ArchiveReader> parent_archive;
    if(drv_def.parent[0])
    {
        std::string parent_path = gf->outside.dir.empty() ? std::string(drv_def.parent) + ".zip" : gf->outside.dir + "/" + std::string(drv_def.parent) + ".zip";
        parent_archive.reset(Mednafen::ArchiveReader::Open(gf->outside.vfs, parent_path));
    }

    for(unsigned i = 0; i < drv_def.nb_romfile; i++)
    {
        const auto &r = drv_def.rom[i];
        if(GnGeoLoadRegion(game_archive.get(), roms, r.region, r.src, r.dest, r.size, r.crc, r.filename)) continue;
        if(parent_archive && GnGeoLoadRegion(parent_archive.get(), roms, r.region, r.src, r.dest, r.size, r.crc, r.filename)) continue;
        if(r.region != REGION_FIXED_LAYER_BIOS && r.region != REGION_AUDIO_CPU_BIOS && r.region != REGION_MAIN_CPU_BIOS)
        {
            MDFN_printf("GnGeo loader: missing ROM %s (region %u, size %u, CRC %08x)\n", r.filename, r.region, r.size, r.crc);
            goto fail;
        }
    }

    if(roms->adpcmb.size == 0) { roms->adpcmb.p = roms->adpcma.p; roms->adpcmb.size = roms->adpcma.size; }

    if(!GnGeoLoadBios(gf, roms, system, country)) goto fail;
    if(!GnGeoConvertAllTile(roms)) goto fail;
    if(!GnGeoConvertAllChar(roms)) goto fail;
    return true;

fail:
    GnGeoFreeRoms(roms);
    return false;
}

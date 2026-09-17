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
    if(roms->adpcmb.p != roms->adpcma.p)
        GnGeoFreeRegion(&roms->adpcmb);
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
    Uint32 *gfxdata;
    Uint32 usage = 0;
    gfxdata = (Uint32 *)&g[tileno << 7];
    memcpy(swap, gfxdata, 128);
    for(int y = 0; y < 16; y++)
    {
        Uint32 dw = 0;
        for(int x = 0; x < 8; x++)
        {
            Uint32 pen;
            pen  = ((swap[64 + (y << 2) + 3] >> x) & 1) << 3;
            pen |= ((swap[64 + (y << 2) + 1] >> x) & 1) << 2;
            pen |= ((swap[64 + (y << 2) + 2] >> x) & 1) << 1;
            pen |=  (swap[64 + (y << 2)] >> x) & 1;
            dw |= pen << ((7 - x) << 2);
            usage |= 1U << pen;
        }
        *gfxdata++ = dw;
        dw = 0;
        for(int x = 0; x < 8; x++)
        {
            Uint32 pen;
            pen  = ((swap[(y << 2) + 3] >> x) & 1) << 3;
            pen |= ((swap[(y << 2) + 1] >> x) & 1) << 2;
            pen |= ((swap[(y << 2) + 2] >> x) & 1) << 1;
            pen |=  (swap[(y << 2)] >> x) & 1;
            dw |= pen << ((7 - x) << 2);
            usage |= 1U << pen;
        }
        *gfxdata++ = dw;
    }
    if((usage & ~1U) == 0) return TILE_INVISIBLE << ((tileno & 0xF) * 2);
    return 0;
}

static bool GnGeoConvertAllTile(GAME_ROMS *roms)
{
    if(roms == nullptr || roms->tiles.p == nullptr) return false;
    Uint32 usage_size = (roms->tiles.size >> 11) * sizeof(Uint32);
    if(GnGeoAllocateRegion(&roms->spr_usage, usage_size, REGION_SPR_USAGE) != 0)
        { GnGeoFreeRoms(roms); return false; }
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

static std::unique_ptr<Mednafen::Stream> GnGeoOpenRom(Mednafen::ArchiveReader *archive, Uint32 expected_crc, const char *filename)
{
    if(archive == nullptr) return nullptr;
    auto CheckCrc = [](Mednafen::Stream *stream, Uint32 expected) -> bool
    {
        Uint8 buffer[LOAD_BUF_SIZE];
        uLong crc = crc32(0L, Z_NULL, 0);
        uint64 remaining = stream->size();
        try
        {
            stream->seek(0, SEEK_SET);
            while(remaining != 0)
            {
                uint64 chunk = remaining;
                if(chunk > sizeof(buffer)) chunk = sizeof(buffer);
                if(stream->read(buffer, chunk) != chunk) return false;
                crc = crc32(crc, buffer, (uInt)chunk);
                remaining -= chunk;
            }
            stream->seek(0, SEEK_SET);
        }
        catch(const Mednafen::MDFN_Error&) { return false; }
        return (Uint32)crc == expected;
    };
    if(filename != nullptr)
    {
        try
        {
            std::unique_ptr<Mednafen::Stream> stream(archive->open(filename, Mednafen::VirtualFS::MODE_READ));
            if(stream && CheckCrc(stream.get(), expected_crc)) return stream;
        }
        catch(const Mednafen::MDFN_Error&) {}
    }
    for(size_t i = 0; i < archive->num_files(); i++)
    {
        if(archive->get_file_size(i) == 0) continue;
        try
        {
            std::unique_ptr<Mednafen::Stream> stream(archive->open(i));
            if(stream && CheckCrc(stream.get(), expected_crc)) return stream;
        }
        catch(const Mednafen::MDFN_Error&) {}
    }
    return nullptr;
}

static bool GnGeoLoadRegion(Mednafen::ArchiveReader *archive, GAME_ROMS *roms, int region, Uint32 src, Uint32 dest, Uint32 size, Uint32 crc, const char *filename)
{
    if(archive == nullptr || filename == nullptr) return false;
    std::unique_ptr<Mednafen::Stream> stream = GnGeoOpenRom(archive, crc, filename);
    if(!stream) return false;
    if(src != 0)
    {
        try { stream->seek(region == REGION_SPRITES ? src / 2 : src, SEEK_SET); }
        catch(const Mednafen::MDFN_Error&) { return false; }
    }
    ROM_REGION *target = nullptr;
    switch(region)
    {
        case REGION_SPRITES: target = &roms->tiles; break;
        case REGION_AUDIO_CPU_CARTRIDGE: target = &roms->cpu_z80; break;
        case REGION_AUDIO_CPU_ENCRYPTED: target = &roms->cpu_z80c; break;
        case REGION_MAIN_CPU_CARTRIDGE: target = &roms->cpu_m68k; break;
        case REGION_FIXED_LAYER_CARTRIDGE: target = &roms->game_sfix; break;
        case REGION_AUDIO_DATA_1: target = &roms->adpcma; break;
        case REGION_AUDIO_DATA_2: target = &roms->adpcmb; break;
        case REGION_MAIN_CPU_BIOS: target = &roms->bios_m68k; break;
        case REGION_AUDIO_CPU_BIOS: target = &roms->bios_audio; break;
        case REGION_FIXED_LAYER_BIOS: target = &roms->bios_sfix; break;
        default: return false;
    }
    if(target->p == nullptr) return false;
    if(region == REGION_SPRITES)
    {
        if(target->size < (dest & ~1U) + size * 2U) return false;
        Uint8 *buffer = (Uint8 *)malloc(LOAD_BUF_SIZE);
        if(buffer == nullptr) return false;
        Uint8 *p = target->p + dest;
        while(size != 0)
        {
            Uint32 chunk = size > LOAD_BUF_SIZE ? LOAD_BUF_SIZE : size;
            if(stream->read(buffer, chunk) != chunk) { free(buffer); return false; }
            for(Uint32 i = 0; i < chunk; i++) { *p = buffer[i]; p += 2; }
            size -= chunk;
        }
        free(buffer);
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

static bool GnGeoLoadBios(Mednafen::GameFile *gf, GAME_ROMS *roms, SYSTEM system, COUNTRY country)
{
    std::unique_ptr<Mednafen::ArchiveReader> bios_archive(
        Mednafen::ArchiveReader::Open(&Mednafen::NVFS,
            Mednafen::MDFN_MakeFName(Mednafen::MDFNMKF_FIRMWARE, 0, "neogeo.zip")));

    if(!bios_archive && gf != nullptr && gf->outside.vfs != nullptr)
    {
        std::string bios_path;
        if(gf->outside.dir.empty())
            bios_path = "neogeo.zip";
        else
            bios_path = gf->outside.dir + "/neogeo.zip";
        bios_archive.reset(Mednafen::ArchiveReader::Open(gf->outside.vfs, bios_path));
    }

    if(!bios_archive) return false;

    if(roms->bios_sfix.p == nullptr)
    {
        std::unique_ptr<Mednafen::Stream> stream;
        try { stream.reset(bios_archive->open("sfix.sfx", Mednafen::VirtualFS::MODE_READ)); }
        catch(const Mednafen::MDFN_Error&) {}
        if(!stream)
        {
            try { stream.reset(bios_archive->open("sfix.sfix", Mednafen::VirtualFS::MODE_READ)); }
            catch(const Mednafen::MDFN_Error&) {}
        }
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
        else
        {
            switch(country)
            {
                case CTY_JAPAN: romfile = "vs-bios.rom"; break;
                case CTY_USA: romfile = "usa_2slt.bin"; break;
                case CTY_ASIA: romfile = "asia-s3.rom"; break;
                case CTY_EUROPE: default: romfile = "sp-s2.sp1"; break;
            }
        }
        std::unique_ptr<Mednafen::Stream> stream;
        try { stream.reset(bios_archive->open(romfile, Mednafen::VirtualFS::MODE_READ)); }
        catch(const Mednafen::MDFN_Error&) { return false; }
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
    if(gf == nullptr || gf->vfs == nullptr) return false;
    std::unique_ptr<Mednafen::ArchiveReader> data_archive(
        Mednafen::ArchiveReader::Open(&Mednafen::NVFS,
            MDFN_MakeFName(MDFNMKF_FIRMWARE, 0, "gngeo_data.zip")));
    if(!data_archive) return false;
    std::string drv_path = "/rom/" + gf->outside.fbase + ".drv";
    std::unique_ptr<Mednafen::Stream> drv(data_archive->open(drv_path, Mednafen::VirtualFS::MODE_READ));
    if(!drv) return false;
    ROM_DEF drv_def;
    memset(&drv_def, 0, sizeof(drv_def));
    drv->read(drv_def.name, sizeof(drv_def.name));
    drv->read(drv_def.parent, sizeof(drv_def.parent));
    drv->read(drv_def.longname, sizeof(drv_def.longname));
    drv->read(&drv_def.year, sizeof(drv_def.year));
    for(unsigned i = 0; i < 10; i++) drv->read(&drv_def.romsize[i], sizeof(drv_def.romsize[i]));
    drv->read(&drv_def.nb_romfile, sizeof(drv_def.nb_romfile));
    if(drv_def.nb_romfile > 32) return false;
    for(unsigned i = 0; i < drv_def.nb_romfile; i++)
    {
        drv->read(drv_def.rom[i].filename, sizeof(drv_def.rom[i].filename));
        drv->read(&drv_def.rom[i].region, sizeof(drv_def.rom[i].region));
        drv->read(&drv_def.rom[i].src, sizeof(drv_def.rom[i].src));
        drv->read(&drv_def.rom[i].dest, sizeof(drv_def.rom[i].dest));
        drv->read(&drv_def.rom[i].size, sizeof(drv_def.rom[i].size));
        drv->read(&drv_def.rom[i].crc, sizeof(drv_def.rom[i].crc));
    }
    roms->info.name = strdup(drv_def.name);
    roms->info.longname = strdup(drv_def.longname);
    roms->info.year = (int)drv_def.year;
    roms->info.flags = 0;
    if(GnGeoAllocateRegion(&roms->cpu_m68k, drv_def.romsize[REGION_MAIN_CPU_CARTRIDGE], REGION_MAIN_CPU_CARTRIDGE) != 0) { GnGeoFreeRoms(roms); return false; }
    if(drv_def.romsize[REGION_AUDIO_CPU_CARTRIDGE] == 0 && drv_def.romsize[REGION_AUDIO_CPU_ENCRYPTED] != 0)
    {
        if(GnGeoAllocateRegion(&roms->cpu_z80c, 0x80000, REGION_AUDIO_CPU_ENCRYPTED) != 0 || GnGeoAllocateRegion(&roms->cpu_z80, 0x90000, REGION_AUDIO_CPU_CARTRIDGE) != 0) { GnGeoFreeRoms(roms); return false; }
    }
    else if(GnGeoAllocateRegion(&roms->cpu_z80, drv_def.romsize[REGION_AUDIO_CPU_CARTRIDGE], REGION_AUDIO_CPU_CARTRIDGE) != 0) { GnGeoFreeRoms(roms); return false; }
    if(GnGeoAllocateRegion(&roms->tiles, drv_def.romsize[REGION_SPRITES], REGION_SPRITES) != 0) { GnGeoFreeRoms(roms); return false; }
    if(GnGeoAllocateRegion(&roms->game_sfix, drv_def.romsize[REGION_FIXED_LAYER_CARTRIDGE], REGION_FIXED_LAYER_CARTRIDGE) != 0) { GnGeoFreeRoms(roms); return false; }
    if(GnGeoAllocateRegion(&roms->gfix_usage, roms->game_sfix.size >> 5, REGION_GAME_FIX_USAGE) != 0) { GnGeoFreeRoms(roms); return false; }
    if(GnGeoAllocateRegion(&roms->adpcma, drv_def.romsize[REGION_AUDIO_DATA_1], REGION_AUDIO_DATA_1) != 0) { GnGeoFreeRoms(roms); return false; }
    if(GnGeoAllocateRegion(&roms->adpcmb, drv_def.romsize[REGION_AUDIO_DATA_2], REGION_AUDIO_DATA_2) != 0) { GnGeoFreeRoms(roms); return false; }
    if(drv_def.romsize[REGION_MAIN_CPU_BIOS] != 0) { roms->info.flags |= HAS_CUSTOM_CPU_BIOS; if(GnGeoAllocateRegion(&roms->bios_m68k, drv_def.romsize[REGION_MAIN_CPU_BIOS], REGION_MAIN_CPU_BIOS) != 0) { GnGeoFreeRoms(roms); return false; } }
    if(drv_def.romsize[REGION_AUDIO_CPU_BIOS] != 0) { roms->info.flags |= HAS_CUSTOM_AUDIO_BIOS; if(GnGeoAllocateRegion(&roms->bios_audio, drv_def.romsize[REGION_AUDIO_CPU_BIOS], REGION_AUDIO_CPU_BIOS) != 0) { GnGeoFreeRoms(roms); return false; } }
    if(drv_def.romsize[REGION_FIXED_LAYER_BIOS] != 0) { roms->info.flags |= HAS_CUSTOM_SFIX_BIOS; if(GnGeoAllocateRegion(&roms->bios_sfix, drv_def.romsize[REGION_FIXED_LAYER_BIOS], REGION_FIXED_LAYER_BIOS) != 0) { GnGeoFreeRoms(roms); return false; } }
    if(!GnGeoLoadBios(gf, roms, system, country)) { GnGeoFreeRoms(roms); return false; }
    std::string game_path;
    if(gf->outside.dir.empty()) game_path = gf->outside.fbase + ".zip";
    else game_path = gf->outside.dir + "/" + gf->outside.fbase + ".zip";
    std::unique_ptr<Mednafen::ArchiveReader> game_archive(Mednafen::ArchiveReader::Open(gf->outside.vfs, game_path));
    if(!game_archive) { GnGeoFreeRoms(roms); return false; }
    std::unique_ptr<Mednafen::ArchiveReader> parent_archive;
    if(drv_def.parent[0] != 0)
    {
        std::string parent_path;
        if(gf->outside.dir.empty()) parent_path = std::string(drv_def.parent) + ".zip";
        else parent_path = gf->outside.dir + "/" + std::string(drv_def.parent) + ".zip";
        parent_archive.reset(Mednafen::ArchiveReader::Open(gf->outside.vfs, parent_path));
    }
    for(unsigned i = 0; i < drv_def.nb_romfile; i++)
    {
        const ROM_DEF::romfile& rom = drv_def.rom[i];
        if(GnGeoLoadRegion(game_archive.get(), roms, rom.region, rom.src, rom.dest, rom.size, rom.crc, rom.filename)) continue;
        if(parent_archive && GnGeoLoadRegion(parent_archive.get(), roms, rom.region, rom.src, rom.dest, rom.size, rom.crc, rom.filename)) continue;
        if(rom.region != REGION_FIXED_LAYER_BIOS && rom.region != REGION_AUDIO_CPU_BIOS && rom.region != REGION_MAIN_CPU_BIOS) { GnGeoFreeRoms(roms); return false; }
    }
    if(roms->adpcmb.size == 0) { roms->adpcmb.p = roms->adpcma.p; roms->adpcmb.size = roms->adpcma.size; }
    if(!GnGeoConvertAllTile(roms)) { GnGeoFreeRoms(roms); return false; }
    if(!GnGeoConvertAllChar(roms)) { GnGeoFreeRoms(roms); return false; }
    return true;
}

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
#include <cctype>

#include "rom_loader.h"
#include "core/gngeo_drivers.h"
extern "C" int GnGeoInitRoms(GAME_ROMS *);
extern "C" void convert_all_tile(GAME_ROMS *);
extern "C" void convert_all_char(Uint8 *, int, Uint8 *);

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

void GnGeoFreeRoms(GAME_ROMS *roms)
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
    GnGeoFreeRegion(&roms->zoom_table);
    GnGeoFreeRegion(&roms->bfix_usage);
    free(roms->info.name);
    free(roms->info.longname);
    roms->info.name = nullptr;
    roms->info.longname = nullptr;
}

static bool GnGeoCheckCrc(Mednafen::Stream *stream, Uint32 expected)
{
    std::vector<Uint8> buffer(LOAD_BUF_SIZE);
    uLong crc = crc32(0L, Z_NULL, 0);
    uint64 remaining = stream->size();
    try
    {
        stream->seek(0, SEEK_SET);
        while(remaining != 0)
        {
            uint64 chunk = remaining > sizeof(buffer) ? sizeof(buffer) : remaining;
            if(stream->read(buffer.data(), chunk) != chunk) return false;
            crc = crc32(crc, buffer.data(), (uInt)chunk);
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
        std::vector<Uint8> buffer(LOAD_BUF_SIZE);
        Uint8 *p = target->p + dest;
        while(size != 0)
        {
            Uint32 chunk = size > buffer.size() ? buffer.size() : size;
            if(stream->read(buffer.data(), chunk) != chunk) return false;
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

static std::unique_ptr<Mednafen::Stream> GnGeoOpenBiosFile(Mednafen::ArchiveReader *archive, const char *name, Uint32 expected_size, Uint32 expected_crc)
{
    if(!archive) return nullptr;
    try
    {
        std::unique_ptr<Mednafen::Stream> stream(archive->open(name, Mednafen::VirtualFS::MODE_READ));
        if(stream && (expected_size == 0 || stream->size() == expected_size) &&
           (expected_crc == 0 || GnGeoCheckCrc(stream.get(), expected_crc))) return stream;
    }
    catch(const Mednafen::MDFN_Error&) {}
    const std::string slash_name = std::string("/") + name;
    for(size_t i = 0; i < archive->num_files(); i++)
    {
        const std::string *path = archive->get_file_path(i);
        if(path == nullptr || (*path != name && *path != slash_name)) continue;
        if(expected_size != 0 && archive->get_file_size(i) != expected_size) return nullptr;
        try
        {
            std::unique_ptr<Mednafen::Stream> stream(archive->open(i));
            if(stream && (expected_crc == 0 || GnGeoCheckCrc(stream.get(), expected_crc))) return stream;
        }
        catch(const Mednafen::MDFN_Error&) {}
        return nullptr;
    }

    return nullptr;
}

static bool GnGeoLoadBios(Mednafen::GameFile *gf, GAME_ROMS *roms, SYSTEM system, COUNTRY country)
{
    const std::string bios_archive_name =
        Mednafen::MDFN_GetSettingS("gngeo.bios").empty() ? "neogeo.zip" : Mednafen::MDFN_GetSettingS("gngeo.bios");

    std::unique_ptr<Mednafen::ArchiveReader> bios_archive(
        Mednafen::ArchiveReader::Open(&Mednafen::NVFS,
            Mednafen::MDFN_MakeFName(Mednafen::MDFNMKF_FIRMWARE, 0, "neogeo.zip")));
    if(!bios_archive && gf && gf->outside.vfs)
    {
        std::string path = gf->outside.dir.empty() ? "neogeo.zip" : gf->outside.dir + "/neogeo.zip";
        bios_archive.reset(Mednafen::ArchiveReader::Open(gf->outside.vfs, path));
    }
    if(!bios_archive) return false;

    std::unique_ptr<Mednafen::ArchiveReader> main_bios_archive;
    if(system == SYS_UNIBIOS)
    {
        main_bios_archive.reset(Mednafen::ArchiveReader::Open(
            &Mednafen::NVFS,
            Mednafen::MDFN_MakeFName(Mednafen::MDFNMKF_FIRMWARE, 0, bios_archive_name)));
        if(!main_bios_archive && gf && gf->outside.vfs)
        {
            std::string path = gf->outside.dir.empty() ? bios_archive_name : gf->outside.dir + "/" + bios_archive_name;
            main_bios_archive.reset(Mednafen::ArchiveReader::Open(gf->outside.vfs, path));
        }
        if(!main_bios_archive) return false;
    }
    else
        main_bios_archive.reset(nullptr);

    if(roms->bios_sfix.p == nullptr)
    {
        std::unique_ptr<Mednafen::Stream> stream = GnGeoOpenBiosFile(bios_archive.get(), "sfix.sfx", 0x20000, 0xc2ea0cfd);
        if(!stream) stream = GnGeoOpenBiosFile(bios_archive.get(), "sfix.sfix", 0x20000, 0xc2ea0cfd);
        if(!stream && system == SYS_ARCADE && country == CTY_EUROPE)
        {
            stream = GnGeoOpenBiosFile(bios_archive.get(), "sfix.sfx", 0x20000, 0);
            if(!stream) stream = GnGeoOpenBiosFile(bios_archive.get(), "sfix.sfix", 0x20000, 0);
        }
        if(!stream) {
        return false;
    }
        Uint32 size = (Uint32)stream->size();
        if(GnGeoAllocateRegion(&roms->bios_sfix, size, REGION_FIXED_LAYER_BIOS) != 0) return false;
        if(stream->read(roms->bios_sfix.p, size) != size) return false;
    }

    if(roms->bios_m68k.p == nullptr)
    {
            const char *romfile = "sp-s2.sp1";
        Uint32 bios_crc = 0x9036d879;
        if(system == SYS_UNIBIOS) { romfile = "uni-bios.rom"; bios_crc = 0; }
        else if(system == SYS_HOME) { romfile = "aes-bios.bin"; bios_crc = 0; }
        else if(country == CTY_JAPAN) { romfile = "vs-bios.rom"; bios_crc = 0xf0e8f27d; }
        else if(country == CTY_USA) { romfile = "usa_2slt.bin"; bios_crc = 0xe72943de; }
        else if(country == CTY_ASIA) { romfile = "asia-s3.rom"; bios_crc = 0x91b64be3; }
        Mednafen::ArchiveReader *main_archive = system == SYS_UNIBIOS ? main_bios_archive.get() : bios_archive.get();
        std::unique_ptr<Mednafen::Stream> stream = GnGeoOpenBiosFile(main_archive, romfile, 0x20000, bios_crc);
        if(!stream && system == SYS_UNIBIOS)
        {
            romfile = "sp-s2.sp1";
            bios_crc = 0;
            stream = GnGeoOpenBiosFile(bios_archive.get(), romfile, 0x20000, bios_crc);
        }
        if(!stream && system == SYS_ARCADE && country == CTY_EUROPE)
            stream = GnGeoOpenBiosFile(bios_archive.get(), romfile, 0x20000, 0);
        if(!stream && system == SYS_ARCADE)
        {
            romfile = "uni-bios.rom";
            bios_crc = 0;
            stream = GnGeoOpenBiosFile(bios_archive.get(), romfile, 0x20000, bios_crc);
        }
        if(!stream) return false;
        Uint32 size = (Uint32)stream->size();
        if(GnGeoAllocateRegion(&roms->bios_m68k, size, REGION_MAIN_CPU_BIOS) != 0) return false;
        if(stream->read(roms->bios_m68k.p, size) != size) return false;
    }
    return true;
}

static std::string GnGeoDriverBase(const std::string &base)
{
    std::string normalized = base;
    size_t suffix = normalized.rfind('(');
    if(suffix != std::string::npos && !normalized.empty() && normalized.back() == ')')
        normalized.resize(suffix);
    return normalized;
}

static bool GnGeoLoadExternalDriver(Mednafen::GameFile *gf, std::vector<Uint8> &data)
{
    if(!gf || !gf->outside.vfs || gf->outside.fbase.empty()) return false;

    std::string path = gf->outside.dir.empty() ? "gngeo_data.zip" :
        gf->outside.dir + "/gngeo_data.zip";
    std::unique_ptr<Mednafen::ArchiveReader> archive(
        Mednafen::ArchiveReader::Open(gf->outside.vfs, path));
    if(!archive)
        archive.reset(Mednafen::ArchiveReader::Open(gf->outside.vfs, "gngeo_data.zip"));
    if(!archive) return false;

    std::string driver_base = GnGeoDriverBase(gf->outside.fbase);
    std::string name = "rom/" + driver_base + ".drv";
    std::unique_ptr<Mednafen::Stream> stream;
    try {
        stream.reset(archive->open(name, Mednafen::VirtualFS::MODE_READ));
    } catch(const Mednafen::MDFN_Error&) {}

    if(!stream) {
        try {
            stream.reset(archive->open("/" + name, Mednafen::VirtualFS::MODE_READ));
        } catch(const Mednafen::MDFN_Error&) {}
    }

    if(!stream) {
        std::string lower = gf->outside.fbase;
        for(char &ch : lower)
            ch = (char)std::tolower((unsigned char)ch);
        if(lower != gf->outside.fbase) {
            try {
                stream.reset(archive->open("rom/" + lower + ".drv",
                    Mednafen::VirtualFS::MODE_READ));
            } catch(const Mednafen::MDFN_Error&) {}
        }
    }

    if(!stream) {
        std::string wanted = name;
        for(char &ch : wanted)
            ch = (char)std::tolower((unsigned char)ch);
        for(size_t i = 0; i < archive->num_files(); i++) {
            const std::string *entry = archive->get_file_path(i);
            if(!entry) continue;
            std::string candidate = *entry;
            for(char &ch : candidate)
                ch = (char)std::tolower((unsigned char)ch);
            if(candidate == wanted) {
                try {
                    stream.reset(archive->open(i));
                } catch(const Mednafen::MDFN_Error&) {}
                if(stream) break;
            }
        }
    }
    if(!stream && driver_base != gf->outside.fbase)
    {
        try { stream.reset(archive->open("rom/" + driver_base + ".drv", Mednafen::VirtualFS::MODE_READ)); }
        catch(const Mednafen::MDFN_Error&) {}
        if(!stream)
        {
            try { stream.reset(archive->open("/rom/" + driver_base + ".drv", Mednafen::VirtualFS::MODE_READ)); }
            catch(const Mednafen::MDFN_Error&) {}
        }
    }

    if(!stream) return false;

    uint64 size = stream->size();
    if(size == 0 || size > 16384) return false;
    data.resize((size_t)size);
    try {
        if(stream->read(data.data(), size) != size) return false;
    } catch(const Mednafen::MDFN_Error&) {
        return false;
    }
    return true;
}

bool Mednafen::GnGeoHasDriver(Mednafen::GameFile *gf)
{
    if(!gf || !gf->outside.vfs || gf->outside.fbase.empty()) return false;

    Uint32 size = 0;
    if(GnGeoFindDriver(gf->outside.fbase.c_str(), &size) != nullptr && size != 0)
        return true;

    std::string driver_base = GnGeoDriverBase(gf->outside.fbase);
    if(driver_base != gf->outside.fbase &&
       GnGeoFindDriver(driver_base.c_str(), &size) != nullptr && size != 0)
        return true;

    std::string lower = driver_base;
    for(char &ch : lower)
        ch = (char)std::tolower((unsigned char)ch);
    if(lower != driver_base &&
       GnGeoFindDriver(lower.c_str(), &size) != nullptr && size != 0)
        return true;

    std::vector<Uint8> driver;
    return GnGeoLoadExternalDriver(gf, driver);
}

bool Mednafen::GnGeoLoadRomSet(Mednafen::GameFile *gf, GAME_ROMS *roms, SYSTEM system, COUNTRY country)
{
    memset(roms, 0, sizeof(*roms));
    if(!gf || !gf->outside.vfs) return false;

    Uint32 drv_size=0;
    std::vector<Uint8> external_driver;
    std::string driver_base = GnGeoDriverBase(gf->outside.fbase);
    const Uint8 *drv_data=(const Uint8*)GnGeoFindDriver(gf->outside.fbase.c_str(),&drv_size);
    if(!drv_data) {
        std::string lower = driver_base;
        for(char &ch : lower)
            ch = (char)std::tolower((unsigned char)ch);
        if(lower != driver_base)
            drv_data=(const Uint8*)GnGeoFindDriver(lower.c_str(),&drv_size);
    }
    if(!drv_data) {
        if(!GnGeoLoadExternalDriver(gf, external_driver)) { MDFN_printf("GnGeo loader: no driver for %s\\n", gf->outside.fbase.c_str()); return false; }
        drv_data=external_driver.data();
        drv_size=(Uint32)external_driver.size();
    }

    const Uint8 *dp=drv_data, *de=drv_data+drv_size;
    auto read_drv=[&](void *dst,size_t n)->bool { if((size_t)(de-dp)<n)return false; memcpy(dst,dp,n);dp+=n;return true; };
    ROM_DEF drv_def; memset(&drv_def,0,sizeof(drv_def));
    if(!read_drv(drv_def.name,sizeof(drv_def.name)) || !read_drv(drv_def.parent,sizeof(drv_def.parent)) || !read_drv(drv_def.longname,sizeof(drv_def.longname)) || !read_drv(&drv_def.year,sizeof(drv_def.year))) {
        return false;
    }
    for(unsigned i=0;i<10;i++) if(!read_drv(&drv_def.romsize[i],sizeof(drv_def.romsize[i]))) {
        return false;
    }
    if(!read_drv(&drv_def.nb_romfile,sizeof(drv_def.nb_romfile)) || drv_def.nb_romfile>32) {
        return false;
    }
    for(unsigned i=0;i<drv_def.nb_romfile;i++) {
        auto &r=drv_def.rom[i];
        if(!read_drv(r.filename,sizeof(r.filename)) ||
           !read_drv(&r.region,sizeof(r.region)) ||
           !read_drv(&r.src,sizeof(r.src)) ||
           !read_drv(&r.dest,sizeof(r.dest)) ||
           !read_drv(&r.size,sizeof(r.size)) ||
           !read_drv(&r.crc,sizeof(r.crc))) {
            return false;
        }
    }

    roms->info.name = strdup(drv_def.name);
    roms->info.longname = strdup(drv_def.longname);
    roms->info.year = (int)drv_def.year;
    roms->info.flags = 0;
    if(!roms->info.name || !roms->info.longname) { GnGeoFreeRoms(roms); return false; }

    auto Fail = [&]() -> bool { fprintf(stderr, "GnGeo loader: generic failure\n"); GnGeoFreeRoms(roms); return false; };
    if(GnGeoAllocateRegion(&roms->cpu_m68k, drv_def.romsize[REGION_MAIN_CPU_CARTRIDGE], REGION_MAIN_CPU_CARTRIDGE) != 0) return Fail();
    if(drv_def.romsize[REGION_AUDIO_CPU_CARTRIDGE] == 0 && drv_def.romsize[REGION_AUDIO_CPU_ENCRYPTED] != 0)
    {
        if(GnGeoAllocateRegion(&roms->cpu_z80c, 0x80000, REGION_AUDIO_CPU_ENCRYPTED) != 0 ||
           GnGeoAllocateRegion(&roms->cpu_z80, 0x90000, REGION_AUDIO_CPU_CARTRIDGE) != 0) return Fail();
    }
    else if(GnGeoAllocateRegion(&roms->cpu_z80, drv_def.romsize[REGION_AUDIO_CPU_CARTRIDGE], REGION_AUDIO_CPU_CARTRIDGE) != 0) return Fail();
    if(GnGeoAllocateRegion(&roms->tiles, drv_def.romsize[REGION_SPRITES], REGION_SPRITES) != 0) return Fail();
    if(GnGeoAllocateRegion(&roms->game_sfix, drv_def.romsize[REGION_FIXED_LAYER_CARTRIDGE], REGION_FIXED_LAYER_CARTRIDGE) != 0) return Fail();
    if(GnGeoAllocateRegion(&roms->gfix_usage, roms->game_sfix.size >> 5, REGION_GAME_FIX_USAGE) != 0) return Fail();
    if(GnGeoAllocateRegion(&roms->adpcma, drv_def.romsize[REGION_AUDIO_DATA_1], REGION_AUDIO_DATA_1) != 0) return Fail();
    if(GnGeoAllocateRegion(&roms->adpcmb, drv_def.romsize[REGION_AUDIO_DATA_2], REGION_AUDIO_DATA_2) != 0) return Fail();
    if(drv_def.romsize[REGION_MAIN_CPU_BIOS]) { roms->info.flags |= HAS_CUSTOM_CPU_BIOS; if(GnGeoAllocateRegion(&roms->bios_m68k, drv_def.romsize[REGION_MAIN_CPU_BIOS], REGION_MAIN_CPU_BIOS) != 0) return Fail(); }
    if(drv_def.romsize[REGION_AUDIO_CPU_BIOS]) { roms->info.flags |= HAS_CUSTOM_AUDIO_BIOS; if(GnGeoAllocateRegion(&roms->bios_audio, drv_def.romsize[REGION_AUDIO_CPU_BIOS], REGION_AUDIO_CPU_BIOS) != 0) return Fail(); }
    if(drv_def.romsize[REGION_FIXED_LAYER_BIOS]) { roms->info.flags |= HAS_CUSTOM_SFIX_BIOS; if(GnGeoAllocateRegion(&roms->bios_sfix, drv_def.romsize[REGION_FIXED_LAYER_BIOS], REGION_FIXED_LAYER_BIOS) != 0) return Fail(); }
    std::unique_ptr<Mednafen::ArchiveReader> game_archive;
    if(gf->outside.vfs) {
        std::string game_path = gf->outside.dir.empty() ? gf->outside.fbase + ".zip" : gf->outside.dir + "/" + gf->outside.fbase + ".zip";
        try {
            game_archive.reset(Mednafen::ArchiveReader::Open(gf->outside.vfs, game_path));
        } catch(const Mednafen::MDFN_Error&) {}
    }
    if(!game_archive) { MDFN_printf("GnGeo loader: cannot open game archive for %s\\n", gf->outside.fbase.c_str()); return Fail(); }

    std::unique_ptr<Mednafen::ArchiveReader> parent_archive;
    if(drv_def.parent[0])
    {
        std::string parent_path = gf->outside.dir.empty() ? std::string(drv_def.parent) + ".zip" : gf->outside.dir + "/" + std::string(drv_def.parent) + ".zip";
        try { parent_archive.reset(Mednafen::ArchiveReader::Open(gf->outside.vfs, parent_path)); } catch(const Mednafen::MDFN_Error&) {}
        if(!parent_archive) {
            try { parent_archive.reset(Mednafen::ArchiveReader::Open(&Mednafen::NVFS, parent_path)); } catch(const Mednafen::MDFN_Error&) {}
        }
    }

    for(unsigned i = 0; i < drv_def.nb_romfile; i++)
    {
        const auto &r = drv_def.rom[i];
        if(GnGeoLoadRegion(game_archive.get(), roms, r.region, r.src, r.dest, r.size, r.crc, r.filename)) continue;
        if(parent_archive && GnGeoLoadRegion(parent_archive.get(), roms, r.region, r.src, r.dest, r.size, r.crc, r.filename)) continue;
        if(r.region != REGION_FIXED_LAYER_BIOS && r.region != REGION_AUDIO_CPU_BIOS && r.region != REGION_MAIN_CPU_BIOS)
        {
            MDFN_printf("GnGeo loader: missing ROM %s (region %u, size %u, CRC %08x)\n", r.filename, r.region, r.size, r.crc);
            return Fail();
        }
    }
    if(roms->adpcmb.size == 0) { roms->adpcmb.p = roms->adpcma.p; roms->adpcmb.size = roms->adpcma.size; }
    if(!GnGeoLoadBios(gf, roms, system, country)) { MDFN_printf("GnGeo loader: BIOS load failed\\n"); return Fail(); }
    return true;
}

#include "gngeo.h"
#include "rom_loader.h"
#include "gngeo_memory.h"
#include "gngeo_bios.h"
#include "../compress/ArchiveReader.h"
#include "../general.h"

namespace Mednafen
{
	namespace MDFN_IEN_GNGEO
	{
		static const std::vector<Mednafen::InputDeviceInfoStruct> InputDeviceInfo = {};
		static const std::vector<Mednafen::InputPortInfoStruct> PortInfo =
		{
			{ "builtin", "Built-In", InputDeviceInfo, "" }
		};
		static const Mednafen::FileExtensionSpecStruct KnownExtensions[] =
		{
			{ ".zip", 0, gettext_noop("Neo Geo arcade ROM archive") },
			{ NULL, 0, NULL }
		};
		static GAME_ROMS GameRoms;

		static void Load(Mednafen::GameFile *gf)
		{
			memset(&GameRoms, 0, sizeof(GameRoms));
			if(!Mednafen::GnGeoLoadRomSet(gf, &GameRoms, SYS_ARCADE, CTY_EUROPE))
				throw MDFN_Error(0, gettext_noop("Unable to load GnGeo ROM set."));
			if(!Mednafen::GnGeoLoadBiosLo(gf))
				throw MDFN_Error(0, gettext_noop("Unable to load GnGeo 000-lo.lo BIOS."));
			if(!GnGeoMemoryInit(&GameRoms))
				throw MDFN_Error(0, gettext_noop("Unable to initialize GnGeo 68000 memory."));
		}

		static bool TestMagic(Mednafen::GameFile *gf)
		{
			if(gf == nullptr || gf->outside.vfs == nullptr || gf->outside.fbase.empty())
				return false;

			std::unique_ptr<Mednafen::ArchiveReader> data_archive(
				Mednafen::ArchiveReader::Open(
					&Mednafen::NVFS,
					MDFN_MakeFName(MDFNMKF_FIRMWARE, 0, "gngeo_data.zip")));

			if(!data_archive)
				return false;

			const std::string drv_path = "/rom/" + gf->outside.fbase + ".drv";

			try
			{
				std::unique_ptr<Mednafen::Stream> stream(
					data_archive->open(drv_path, Mednafen::VirtualFS::MODE_READ));
				return stream != nullptr;
			}
			catch(const Mednafen::MDFN_Error&)
			{
				return false;
			}
		}

		static void CloseGame(void) { GnGeoMemoryClose(); GnGeoFreeBiosLo(); }
		static void StateAction(Mednafen::StateMem *sm, const unsigned load, const bool data_only)
		{ (void)sm; (void)load; (void)data_only; }
		static void Emulate(Mednafen::EmulateSpecStruct *espec)
		{
			GnGeoMemoryRun(200000);
			if(espec)
				espec->MasterCycles = 200000;
		}
		static void SetInput(unsigned port, const char *type, uint8 *data)
		{ (void)port; (void)type; (void)data; }
		static void DoSimpleCommand(int cmd) { (void)cmd; }
		static const Mednafen::MDFNSetting GnGeoSettings[] = { { NULL } };
	}
}

MDFN_HIDE extern const Mednafen::MDFNGI EmulatedGnGeo =
{
	"gngeo", "Neo Geo (GnGeo)", Mednafen::MDFN_IEN_GNGEO::KnownExtensions,
	Mednafen::MODPRIO_INTERNAL_HIGH, NULL, Mednafen::MDFN_IEN_GNGEO::PortInfo, NULL,
	Mednafen::MDFN_IEN_GNGEO::Load, Mednafen::MDFN_IEN_GNGEO::TestMagic,
	NULL, NULL, Mednafen::MDFN_IEN_GNGEO::CloseGame, NULL, NULL, NULL, NULL,
	NULL, 0, Mednafen::CheatInfo_Empty, false, Mednafen::MDFN_IEN_GNGEO::StateAction,
	Mednafen::MDFN_IEN_GNGEO::Emulate, NULL, Mednafen::MDFN_IEN_GNGEO::SetInput, NULL,
	Mednafen::MDFN_IEN_GNGEO::DoSimpleCommand, NULL, Mednafen::MDFN_IEN_GNGEO::GnGeoSettings,
	MDFN_MASTERCLOCK_FIXED(6144000), 0, Mednafen::EVFSUPPORT_NONE, false,
	320, 224, NULL, 320, 224, 320, 224, 2,
};

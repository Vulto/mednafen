#include <mednafen/mednafen.h>
#include <mednafen/state.h>
#include <mednafen/general.h>
#include "gngeo.h"
#include "rom_loader.h"
#include "gngeo_bios.h"
extern "C" {
#include "core/gngeo_runtime.h"
#include "core/gngeo_drivers.h"
}
#include <cstring>
#include <memory>

namespace Mednafen { namespace MDFN_IEN_GNGEO {
static GAME_ROMS GameRoms;
static const IDIISG IDII = {
 IDIIS_Button("up","UP",0,"down"), IDIIS_Button("down","DOWN",1,"up"), IDIIS_Button("left","LEFT",2,"right"), IDIIS_Button("right","RIGHT",3,"left"),
 IDIIS_ButtonCR("a","A",4,nullptr), IDIIS_ButtonCR("b","B",5,nullptr), IDIIS_ButtonCR("c","C",6,nullptr), IDIIS_ButtonCR("d","D",7,nullptr), IDIIS_ButtonCR("start","START",8,nullptr), IDIIS_ButtonCR("select","SELECT",9,nullptr), IDIIS_ButtonCR("coin","COIN",10,nullptr)
};
static const std::vector<InputDeviceInfoStruct> InputDeviceInfo = {{ "gamepad", "Neo Geo Controller", "", IDII, 0 }};
static const std::vector<InputPortInfoStruct> PortInfo = {{"p1","Player 1",InputDeviceInfo,"gamepad"},{"p2","Player 2",InputDeviceInfo,"gamepad"}};
static const FileExtensionSpecStruct KnownExtensions[]={{".zip",0,gettext_noop("Neo Geo arcade ROM archive")},{NULL,0,NULL}};
static const MDFNSetting GnGeoSettings[]={ { NULL } };
static uint8 *InputP1,*InputP2;
static std::vector<uint8> StateBlob;
static unsigned CoinPulseFrames;

static void SetInput(unsigned port,const char *type,uint8 *ptr){ if(!strcmp(type,"gamepad")){ if(port==0) InputP1=ptr; else if(port==1) InputP2=ptr; } }

static void ApplyInput(){
 uint8 p1=0xff,p2=0xff,s=0x8f,c=7;
 auto apply=[](uint8 *p,uint8 &v){if(!p)return; if(p[0]&&!p[1])v&=0xfe; if(p[1]&&!p[0])v&=0xfd; if(p[2]&&!p[3])v&=0xfb; if(p[3]&&!p[2])v&=0xf7; if(p[4])v&=0xef;if(p[5])v&=0xdf;if(p[6])v&=0xbf;if(p[7])v&=0x7f;};
 apply(InputP1,p1); apply(InputP2,p2);
 if(InputP1&&InputP1[8])s&=0xfd;
 if(InputP2&&InputP2[8])s&=0xf7;
 if(InputP1&&InputP1[9])s&=0xfe;
 if(InputP2&&InputP2[9])s&=0xfb;
 if(InputP1&&InputP1[10])c&=0x6;
 if(InputP2&&InputP2[10])c&=0x5;
 if(CoinPulseFrames) { c&=0x6; CoinPulseFrames--; }
#ifdef GNGEO_CI_INPUT_TRACE
 static uint8 lastP1=0xff,lastP2=0xff,lastS=0x8f,lastC=7;
 if(p1!=lastP1 || p2!=lastP2 || s!=lastS || c!=lastC) {
  fprintf(stderr,"GNGEO_INPUT p1=%02x p2=%02x start=%02x coin=%02x\n",p1,p2,s,c);
  lastP1=p1; lastP2=p2; lastS=s; lastC=c;
 }
#endif
 GnGeoCoreSetInput(p1,p2,s,c,0);
}

static void Load(GameFile *gf)
{
 memset(&GameRoms,0,sizeof(GameRoms));
 try
 {
  if(!GnGeoLoadRomSet(gf,&GameRoms,SYS_ARCADE,CTY_EUROPE)) throw MDFN_Error(ENOENT,_("Unable to load Neo Geo ROM set."));
  if(!GnGeoLoadBiosLo(gf)) throw MDFN_Error(ENOENT,_("Unable to load 000-lo.lo BIOS."));
  size_t lo_size=0; uint8 *lo=GnGeoGetBiosLo(&lo_size);
  if(!lo || lo_size<0x10000) throw MDFN_Error(ENOENT,_("Unable to load 000-lo.lo BIOS."));
  GnGeoCoreSetRoms(&GameRoms,lo);
  if(GnGeoCoreInitRoms()!=0) throw MDFN_Error(EINVAL,_("Unable to initialize Neo Geo ROM set."));
  init_neo(); setup_misc_patch(GameRoms.info.name);
  StateBlob.resize(GnGeoCoreStateSize());
 }
 catch(...)
 {
  GnGeoFreeRoms(&GameRoms); GnGeoFreeBiosLo(); StateBlob.clear(); throw;
 }
}

static bool TestMagic(GameFile *gf)
{
 if(!gf || gf->outside.vfs == nullptr || gf->outside.fbase.empty()) return false;
 Uint32 size = 0;
 return GnGeoFindDriver(gf->outside.fbase.c_str(), &size) != nullptr && size != 0;
}

static void CloseGame(){ GnGeoCoreShutdown(); StateBlob.clear(); GnGeoFreeRoms(&GameRoms); GnGeoFreeBiosLo(); memset(&GameRoms,0,sizeof(GameRoms)); }

static void Emulate(EmulateSpecStruct *espec){
 ApplyInput();
 if(espec->SoundFormatChanged) GnGeoCoreSetAudioRate(espec->SoundRate);
 espec->DisplayRect={0,0,320,224};
 for(unsigned y=0;y<224;y++) espec->LineWidths[y]=320;
 GnGeoRunFrame();
 if(espec->surface->format.opp==2)
  GnGeoCoreGetFrame(espec->surface->pix<uint16>(),espec->surface->pitchinpix);
 else
 {
  static std::vector<uint16> temp(320*224);
  GnGeoCoreGetFrame(temp.data(),320);
  for(unsigned y=0;y<224;y++) {
   uint32 *dst=espec->surface->pix<uint32>()+y*espec->surface->pitchinpix;
   for(unsigned x=0;x<320;x++){
    uint16 p=temp[y*320+x];
    uint8 r=(uint8)((p>>11)*255/31),g=(uint8)(((p>>5)&63)*255/63),b=(uint8)((p&31)*255/31);
    dst[x]=espec->surface->format.MakeColor(r,g,b,255);
   }
  }
 }
 if(espec->SoundBuf && espec->SoundBufMaxSize > 0 && espec->SoundRate > 0) {
  unsigned frames=(unsigned)((espec->SoundRate+30)/60);
  if(frames>(unsigned)espec->SoundBufMaxSize) frames=(unsigned)espec->SoundBufMaxSize;
  espec->SoundBufSize=GnGeoCoreGenerateAudio(espec->SoundBuf,frames);
 } else espec->SoundBufSize=0;
 espec->MasterCycles=200000;
}

#ifdef GNGEO_CI_STATE_TRACE
static unsigned GnGeoStateSaveCount;
static unsigned GnGeoStateLoadCount;
#endif

static void StateAction(StateMem *sm,const unsigned load,const bool data_only){
#ifdef GNGEO_CI_STATE_TRACE
 printf("GNGEO_STATE %s\n",load?"load":"save");
#endif
 if(StateBlob.empty()) StateBlob.resize(GnGeoCoreStateSize());
 SFORMAT sf[]={{"GNCORE",StateBlob.data(),(uint32)StateBlob.size(),1,SFORMAT::FORM::GENERIC,0,0},{NULL,NULL,0,0,SFORMAT::FORM::GENERIC,0,0}};
 if(!load) GnGeoCoreStateSaveLoad(StateBlob.data(),0);
 MDFNSS_StateAction(sm,load,data_only,sf,"GNGEO");
 if(load) GnGeoCoreStateSaveLoad(StateBlob.data(),1);
#ifdef GNGEO_CI_STATE_TRACE
 if(load) GnGeoStateLoadCount++; else GnGeoStateSaveCount++;
#endif
}

static void DoSimpleCommand(int cmd){
 if(cmd==MDFN_MSC_POWER||cmd==MDFN_MSC_RESET) neogeo_reset();
 else if(cmd==MDFN_MSC_INSERT_COIN) CoinPulseFrames=2;
}

}}

using namespace Mednafen::MDFN_IEN_GNGEO;
MDFN_HIDE extern const Mednafen::MDFNGI EmulatedGnGeo={"gngeo","Neo Geo (GnGeo)",KnownExtensions,Mednafen::MODPRIO_INTERNAL_HIGH,NULL,PortInfo,NULL,Load,TestMagic,NULL,NULL,CloseGame,NULL,NULL,NULL,NULL,NULL,0,Mednafen::CheatInfo_Empty,false,StateAction,Emulate,NULL,SetInput,NULL,DoSimpleCommand,NULL,GnGeoSettings,MDFN_MASTERCLOCK_FIXED(12000000),60 * 65536 * 256,Mednafen::EVFSUPPORT_RGB565,false,320,224,NULL,320,224,320,224,2};

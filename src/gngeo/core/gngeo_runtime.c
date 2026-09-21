#include "gngeo_compat.h"
#include "emu.h"
#include "memory.h"
#include "video.h"
#include "screen.h"
#include "timer.h"
#include "pd4990a.h"
#include "ym2610/2610intf.h"
#include "ym2610/ym2610.h"
#include "state.h"
#include "roms.h"

CONF conf;
int frame;
int nb_interlace=256;
int current_line;
Uint8 state_version=ST_VER3;
Uint16 play_buffer[16384];

static GNG_Surface FrameSurface;
static Uint16 FramePixels[352*256];
static int AudioRate=44100;
static int CoreInitialized=0;

#ifdef GNGEO_CI_FRAME_TRACE
static uint32_t GnGeoFrameNumber;

static uint32_t GnGeoFrameHash(void)
{
 uint32_t h=2166136261u;
 for(unsigned y=0;y<224;y++) for(unsigned x=0;x<320;x++) { h^=FramePixels[(y+16)*352+(x+16)]; h*=16777619u; }
 return h;
}
#endif
int Fc=0, LastLine=0, SkipFrame=0, SkipNext=0;

static void GnGeoSoundIrq(int irq)
{
 if(irq) cpu_z80_raise_irq(0); else cpu_z80_lower_irq();
}


void neogeo_reset(void)
{
 sram_lock=0; sound_code=0; pending_command=0; result_code=0;
 if(memory.rom.cpu_m68k.size>0x100000) cpu_68k_bankswitch(0x100000); else cpu_68k_bankswitch(0);
 cpu_68k_reset();
 YM2610_sh_reset();
}

void setup_misc_patch(char *name)
{
 if(!name) return;
 if(!strcmp(name,"ssideki")) WRITE_WORD_ROM(&memory.rom.cpu_m68k.p[0x2240],0x4e71);
 if(!strcmp(name,"mslugx")) {
  Uint8 *RAM=memory.rom.cpu_m68k.p;
  for(int i=0;i<(int)memory.rom.cpu_m68k.size-8;i+=2)
   if(READ_WORD_ROM(&RAM[i])==0x0243 && READ_WORD_ROM(&RAM[i+2])==0x0001 && READ_WORD_ROM(&RAM[i+4])==0x6600) { WRITE_WORD_ROM(&RAM[i+4],0x4e71); WRITE_WORD_ROM(&RAM[i+6],0x4e71); }
  if(memory.rom.cpu_m68k.size>0x3c10){ WRITE_WORD_ROM(&RAM[0x3bdc],0x4e71); WRITE_WORD_ROM(&RAM[0x3bde],0x4e71); WRITE_WORD_ROM(&RAM[0x3be0],0x4e71); WRITE_WORD_ROM(&RAM[0x3c0c],0x4e71); WRITE_WORD_ROM(&RAM[0x3c0e],0x4e71); WRITE_WORD_ROM(&RAM[0x3c10],0x4e71); WRITE_WORD_ROM(&RAM[0x3c36],0x4e71); WRITE_WORD_ROM(&RAM[0x3c38],0x4e71); }
 }
}

void init_neo(void)
{
 FrameSurface.w=352; FrameSurface.h=256; FrameSurface.pitch=352*2; FrameSurface.pixels=FramePixels;
 buffer=&FrameSurface; visible_area.x=16; visible_area.y=16; visible_area.w=320; visible_area.h=224;
 memset(FramePixels,0,sizeof(FramePixels));
#ifdef GNGEO_CI_FRAME_TRACE
 GnGeoFrameNumber=0;
#endif
 memory.vid.modulo=1;
 cpu_68k_init(); pd4990a_init();
 cpu_z80_init();
 conf.sound=1;
 YM2610_sh_start();
 init_video();
 neogeo_reset();
 update_all_pal();
 current_pal=memory.vid.pal_neo[0]; current_pc_pal=(Uint32*)memory.vid.pal_host[0];
 current_fix=memory.rom.bios_sfix.p; fix_usage=memory.fix_board_usage;
 CoreInitialized=1;
 memory.vid.currentpal=0; memory.vid.currentfix=0;
}

void GnGeoCoreShutdown(void)
{
 if(CoreInitialized){ YM2610_sh_stop(); CoreInitialized=0; }
}

void GnGeoCoreSetRoms(GAME_ROMS *r, Uint8 *lo)
{
 memset(&memory,0,sizeof(memory));
 memcpy(&memory.rom,r,sizeof(*r));
 memory.ng_lo=lo;
 memory.fix_game_usage=memory.rom.gfix_usage.p;
 memory.nb_of_tiles=memory.rom.tiles.size>>7;
 memcpy(memory.game_vector,memory.rom.cpu_m68k.p,0x80);
 memcpy(memory.rom.cpu_m68k.p,memory.rom.bios_m68k.p,0x80);
 conf.system=SYS_ARCADE; conf.country=CTY_EUROPE; conf.pal=0; conf.raster=1; conf.sample_rate=AudioRate;
}

int GnGeoCoreInitRoms(void) { if(GnGeoInitRoms(&memory.rom)!=0) return -1; convert_all_tile(&memory.rom); convert_all_char(memory.rom.game_sfix.p,memory.rom.game_sfix.size,memory.rom.gfix_usage.p); if(memory.rom.bios_sfix.p && memory.rom.bios_sfix.size) convert_all_char(memory.rom.bios_sfix.p,memory.rom.bios_sfix.size,memory.fix_board_usage); return 0; }

void GnGeoCoreSetInput(uint8_t p1,uint8_t p2,uint8_t start,uint8_t coin,uint8_t unused) { (void)unused; memory.intern_p1=p1; memory.intern_p2=p2; memory.intern_start=start; memory.intern_coin=coin; }

void GnGeoCoreGetFrame(uint16_t *dst, unsigned pitch_pixels)
{
 for(unsigned y=0;y<224;y++) memcpy(dst+y*pitch_pixels,FramePixels+(y+16)*352+16,320*2);
}

void GnGeoCoreSetAudioRate(int rate)
{
 if(rate<=0) return;
 if(rate!=AudioRate){ AudioRate=rate; conf.sample_rate=(Uint16)rate; YM2610ChangeSamplerate(rate); }
}

unsigned GnGeoCoreGenerateAudio(int16_t *out,unsigned frames)
{
 unsigned n=frames; if(n>8192)n=8192;
 YM2610Update_stream((int)n);
 memcpy(out,play_buffer,n*2*sizeof(int16_t));
 return n;
}




   

static int UpdateScanline(void)
{
 memory.vid.irq2taken=0;
 if(memory.vid.irq2control&0x10 && current_line==memory.vid.irq2start){ if(memory.vid.irq2control&0x80) memory.vid.irq2start+=(memory.vid.irq2pos+3)/0x180; memory.vid.irq2taken=1; }
 if(memory.vid.irq2taken){ if(!SkipFrame){if(LastLine<21)LastLine=21;if(current_line<20)current_line=20;draw_screen_scanline(LastLine-21,current_line-20,0);} LastLine=current_line; }
 current_line++; return memory.vid.irq2taken;
}

int GnGeoRunFrame(void)
{
 Uint32 tm=0; const Uint32 slice=200000/264; const Uint32 zslice=73333/256;
 memory.vid.irq2start=(memory.vid.irq2control&0x40)?(memory.vid.irq2pos+3)/0x180:1000;
 SkipFrame=0; current_line=0; LastLine=0;
 for(int i=0;i<256;i++){ cpu_z80_run(zslice); my_timer(); }
 for(int i=0;i<264;i++){ tm=cpu_68k_run(slice-tm); if(UpdateScanline()) cpu_68k_interrupt(2); }
 tm=cpu_68k_run(200000-tm);
 if(LastLine<21) draw_screen(); else draw_screen_scanline(LastLine-21,262,1);
 memory.watchdog++; if(memory.watchdog>7){cpu_68k_reset();memory.watchdog=0;} cpu_68k_interrupt(1);
 pd4990a_addretrace();
 if(Fc>=neogeo_frame_counter_speed){Fc=0;neogeo_frame_counter++;} Fc++;
#ifdef GNGEO_CI_FRAME_TRACE
 GnGeoFrameNumber++;
 if((GnGeoFrameNumber%30)==0) printf("GNGEO_FRAME frame=%u hash=%08x\n",GnGeoFrameNumber,GnGeoFrameHash());
#endif
 return 1;
}

size_t GnGeoCoreStateSize(void);
int GnGeoCoreStateSaveLoad(void *data,int load);

#include "gngeo_compat.h"
#include "emu.h"
#include "memory.h"
#include "video.h"
#include "timer.h"
#include "pd4990a.h"
#include "ym2610/ym2610.h"
extern size_t GnGeo68kStateSize(void); extern void GnGeo68kStateSaveLoad(void*,int);
extern size_t GnGeoZ80StateSize(void); extern void GnGeoZ80StateSaveLoad(void*,int);
extern size_t ym2610_state_size(void); extern size_t GnGeoYM2610TimerStateSize(void); extern void GnGeoYM2610TimerStateSaveLoad(void*,int);
extern int Fc,LastLine,SkipFrame,SkipNext;

static size_t S=0;
#define CPY(q,p,n,load) do{ if(load) memcpy((p),(q),(n)); else memcpy((q),(p),(n)); (q)+=(n); }while(0)

size_t GnGeoCoreStateSize(void)
{
 if(!S) S=0x10000+sizeof(memory.vid.ram)+sizeof(memory.vid.pal_neo)+sizeof(memory.vid.pal_host)+sizeof(memory.vid.currentpal)+sizeof(memory.vid.currentfix)+sizeof(memory.vid.rbuf)+sizeof(memory.vid.fc)+sizeof(memory.vid.fc_speed)+sizeof(memory.vid.vptr)+sizeof(memory.vid.modulo)+sizeof(memory.vid.current_line)+sizeof(memory.vid.irq2control)+sizeof(memory.vid.irq2taken)+sizeof(memory.vid.irq2start)+sizeof(memory.vid.irq2pos)+0x10000+0x800+0x80+sizeof(memory.current_vector)+4+4+4+4+sizeof(memory.sma_rng_addr)+0x800+sizeof(memory.watchdog)+sizeof(bankaddress)+sizeof(sram_lock)+sizeof(sound_code)+sizeof(pending_command)+sizeof(result_code)+sizeof(z80_bank)+sizeof(neogeo_frame_counter)+sizeof(current_line)+sizeof(Fc)+sizeof(LastLine)+sizeof(SkipFrame)+sizeof(SkipNext)+GnGeo68kStateSize()+GnGeoZ80StateSize()+GnGeoTimerStateSize()+GnGeoYM2610TimerStateSize()+GnGeoPdStateSize()+ym2610_state_size();
 return S;
}

int GnGeoCoreStateSaveLoad(void *data,int load)
{
 unsigned char *q=(unsigned char*)data;
 if(!q) return 0;
 CPY(q,memory.ram,0x10000,load);
 CPY(q,memory.vid.ram,sizeof(memory.vid.ram),load);
 CPY(q,memory.vid.pal_neo,sizeof(memory.vid.pal_neo),load);
 CPY(q,memory.vid.pal_host,sizeof(memory.vid.pal_host),load);
 CPY(q,&memory.vid.currentpal,sizeof(memory.vid.currentpal),load); CPY(q,&memory.vid.currentfix,sizeof(memory.vid.currentfix),load); CPY(q,&memory.vid.rbuf,sizeof(memory.vid.rbuf),load); CPY(q,&memory.vid.fc,sizeof(memory.vid.fc),load); CPY(q,&memory.vid.fc_speed,sizeof(memory.vid.fc_speed),load); CPY(q,&memory.vid.vptr,sizeof(memory.vid.vptr),load); CPY(q,&memory.vid.modulo,sizeof(memory.vid.modulo),load); CPY(q,&memory.vid.current_line,sizeof(memory.vid.current_line),load); CPY(q,&memory.vid.irq2control,sizeof(memory.vid.irq2control),load); CPY(q,&memory.vid.irq2taken,sizeof(memory.vid.irq2taken),load); CPY(q,&memory.vid.irq2start,sizeof(memory.vid.irq2start),load); CPY(q,&memory.vid.irq2pos,sizeof(memory.vid.irq2pos),load);
 CPY(q,memory.sram,0x10000,load); CPY(q,memory.z80_ram,0x800,load); CPY(q,memory.game_vector,0x80,load); CPY(q,&memory.current_vector,sizeof(memory.current_vector),load); CPY(q,&memory.intern_p1,1,load); CPY(q,&memory.intern_p2,1,load); CPY(q,&memory.intern_coin,1,load); CPY(q,&memory.intern_start,1,load); CPY(q,&memory.bksw_handler,sizeof(memory.bksw_handler),load); CPY(q,&memory.sma_rng_addr,sizeof(memory.sma_rng_addr),load); CPY(q,memory.memcard,0x800,load); CPY(q,&memory.watchdog,sizeof(memory.watchdog),load);
 CPY(q,&bankaddress,sizeof(bankaddress),load); CPY(q,&sram_lock,1,load); CPY(q,&sound_code,1,load); CPY(q,&pending_command,1,load); CPY(q,&result_code,1,load); CPY(q,z80_bank,sizeof(z80_bank),load); CPY(q,&neogeo_frame_counter,sizeof(neogeo_frame_counter),load); CPY(q,&current_line,sizeof(current_line),load); CPY(q,&Fc,sizeof(Fc),load); CPY(q,&LastLine,sizeof(LastLine),load); CPY(q,&SkipFrame,sizeof(SkipFrame),load); CPY(q,&SkipNext,sizeof(SkipNext),load);
 GnGeo68kStateSaveLoad(q,load); q+=GnGeo68kStateSize(); GnGeoZ80StateSaveLoad(q,load); q+=GnGeoZ80StateSize(); GnGeoTimerStateSaveLoad(q,load); q+=GnGeoTimerStateSize(); GnGeoYM2610TimerStateSaveLoad(q,load); q+=GnGeoYM2610TimerStateSize(); GnGeoPdStateSaveLoad(q,load); q+=GnGeoPdStateSize();
 GnGeoStateFile io=(GnGeoStateFile)malloc(sizeof(*io)); if(!io)return 0; io->data=q;io->pos=0;io->size=ym2610_state_size();io->load=load; ym2610_mkstate(io,load?STREAD:STWRITE); free(io); q+=ym2610_state_size();
 if(load){ cpu_68k_bankswitch(bankaddress); if(memory.current_vector==0) memcpy(memory.rom.cpu_m68k.p,memory.rom.bios_m68k.p,0x80); else memcpy(memory.rom.cpu_m68k.p,memory.game_vector,0x80); if(memory.vid.currentpal){current_pal=memory.vid.pal_neo[1];current_pc_pal=(Uint32*)memory.vid.pal_host[1];}else{current_pal=memory.vid.pal_neo[0];current_pc_pal=(Uint32*)memory.vid.pal_host[0];} if(memory.vid.currentfix){current_fix=memory.rom.game_sfix.p;fix_usage=memory.fix_game_usage;}else{current_fix=memory.rom.bios_sfix.p;fix_usage=memory.fix_board_usage;} }
 return (int)(q-(unsigned char*)data);
}

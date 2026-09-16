#include "gngeo_memory.h"
#include "../hw_cpu/m68k/m68k.h"
#include <cstring>
using namespace Mednafen;
namespace {
GAME_ROMS *R; M68K CPU; uint8_t Ram[0x10000], Sram[0x10000], Card[0x800], GameVector[0x80];
uint32_t Bank;
uint8 MDFN_FASTCALL Read8(uint32 a) {
 if(a<0x100000) return R->cpu_m68k.p[a%R->cpu_m68k.size];
 if(a<0x200000) return Ram[a&0xffff];
 if(a<0x300000 && R->cpu_m68k.size>0x100000) { uint32 o=Bank+(a&0xfffff); if(o<R->cpu_m68k.size)return R->cpu_m68k.p[o]; }
 if((a&0xfff000)==0x300000) return 0xff;
 if((a&0xfff000)==0x320000) return 0xff;
 if((a&0xfff000)==0x340000) return 0xff;
 if((a&0xfff000)==0x380000) return 0;
 if((a&0xfff000)==0x3a0000) return 0xf0;
 if((a&0xfff000)==0x3c0000) return 0;
 if((a&0xffe000)==0x400000) return 0;
 if((a&0xfff000)==0x800000) return (a&1) ? 0xff : Card[(a&0xfff)>>1];
 if((a&0xff0000)==0xc00000) return R->bios_m68k.p[a&0x1ffff];
 if((a&0xff0000)==0xd00000) return Sram[a&0xffff];
 return 0xf0;
}
uint16 MDFN_FASTCALL Read16(uint32 a) { return (uint16(Read8(a))<<8)|Read8(a+1); }
uint16 MDFN_FASTCALL ReadOp(uint32 a) { return Read16(a); }
void MDFN_FASTCALL Write8(uint32 a,uint8 v) {
 if(a>=0x100000&&a<0x200000){Ram[a&0xffff]=v;return;}
 if((a&0xff0000)==0xd00000){Sram[a&0xffff]=v;return;}
 if((a&0xfff000)==0x320000)return;
 if((a&0xfff000)==0x3a0000){uint32 o=a&0xff; if(o==3)memcpy(R->cpu_m68k.p,R->bios_m68k.p,0x80); if(o==0x13)memcpy(R->cpu_m68k.p,GameVector,0x80); return;}
 if((a&0xfff000)==0x800000){Card[(a&0xfff)>>1]=v;return;}
 if(a>=0x2ffff0&&a<0x300000&&R->cpu_m68k.size>0x100000){Bank=(uint32(v&7)+1)*0x100000;if(Bank>=R->cpu_m68k.size)Bank=0x100000;}
}
void MDFN_FASTCALL Write16(uint32 a,uint16 v){Write8(a,v>>8);Write8(a+1,v);}
}
bool GnGeoMemoryInit(GAME_ROMS *roms){ if(!roms||!roms->cpu_m68k.p||!roms->bios_m68k.p)return false; R=roms;memcpy(GameVector,R->cpu_m68k.p,0x80);memcpy(R->cpu_m68k.p,R->bios_m68k.p,0x80);Bank=0x100000;CPU.BusReadInstr=ReadOp;CPU.BusRead8=Read8;CPU.BusRead16=Read16;CPU.BusWrite8=Write8;CPU.BusWrite16=Write16;CPU.Reset(true);return true; }
void GnGeoMemoryClose(void){R=nullptr;}
void GnGeoMemoryReset(void){if(R){memset(Ram,0,sizeof(Ram));memset(Sram,0,sizeof(Sram));Bank=0x100000;CPU.Reset(false);}}
void GnGeoMemoryRun(int32_t cycles){CPU.Run(CPU.timestamp+cycles);}

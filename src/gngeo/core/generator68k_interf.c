#include "gngeo_compat.h"
#include <stdio.h>
#include <string.h>
/*  gngeo a neogeo emulator
 *  Copyright (C) 2001 Peponas Mathieu
 * 
 *  This program is free software; you can redistribute it and/or modify  
 *  it under the terms of the GNU General Public License as published by   
 *  the Free Software Foundation; either version 2 of the License, or    
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU Library General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. 
 */

/* genrator68k interface */



#ifdef HAVE_CONFIG_H
#include <config.h>
#endif

#ifdef USE_GENERATOR68K
#include <stdlib.h>

#include "generator68k/generator.h"
#include "generator68k/cpu68k.h"
#include "generator68k/reg68k.h"
#include "generator68k/mem68k.h"
#include "memory.h"
#include "emu.h"
#include "gnutil.h"

extern unsigned int cpu68k_clocks;
extern Uint8 *cpu68k_rom;
extern unsigned int cpu68k_romlen;
extern Uint8 *cpu68k_ram;

extern Uint32 reg68k_pc;
extern t_sr reg68k_sr;

Uint8 *mem68k_memptr_bad(Uint32 addr);
Uint8 *mem68k_memptr_cpu(Uint32 addr);
Uint8 *mem68k_memptr_bios(Uint32 addr);
Uint8 *mem68k_memptr_cpu_bk(Uint32 addr);
Uint8 *mem68k_memptr_ram(Uint32 addr);

int diss68k_getdumpline(uint32 addr68k, uint8 *addr, char *dumpline);

t_mem68k_def mem68k_def[] = {
    {0x000, 0x0fff, mem68k_memptr_bad,
     mem68k_fetch_invalid_byte, mem68k_fetch_invalid_word,
     mem68k_fetch_invalid_long,
     mem68k_store_invalid_byte, mem68k_store_invalid_word,
     mem68k_store_invalid_long},

    /* RAM */
    {0x100, 0x1FF, mem68k_memptr_ram,
     mem68k_fetch_ram_byte, mem68k_fetch_ram_word, mem68k_fetch_ram_long,
     mem68k_store_ram_byte, mem68k_store_ram_word, mem68k_store_ram_long},

    /* BANKED CPU */
    {0x200, 0x2ff, mem68k_memptr_cpu_bk,
     NULL, NULL, NULL, NULL, NULL, NULL},

    /* CPU BANK 0 */
    {0x000, 0x0ff, mem68k_memptr_cpu,
     mem68k_fetch_cpu_byte, mem68k_fetch_cpu_word, mem68k_fetch_cpu_long,
     mem68k_store_invalid_byte, mem68k_store_invalid_word,
     mem68k_store_invalid_long},

    /* BIOS */
    {0xc00, 0xcFf, mem68k_memptr_bios,
     mem68k_fetch_bios_byte, mem68k_fetch_bios_word,
     mem68k_fetch_bios_long,
     mem68k_store_invalid_byte, mem68k_store_invalid_word,
     mem68k_store_invalid_long},

    /* SRAM */
    {0xd00, 0xdff, mem68k_memptr_bad,
     mem68k_fetch_sram_byte, mem68k_fetch_sram_word,
     mem68k_fetch_sram_long,
     mem68k_store_sram_byte, mem68k_store_sram_word,
     mem68k_store_sram_long},

    /* PAL */
    {0x400, 0x401, mem68k_memptr_bad,
     mem68k_fetch_pal_byte, mem68k_fetch_pal_word, mem68k_fetch_pal_long,
     mem68k_store_pal_byte, mem68k_store_pal_word, mem68k_store_pal_long},

    /* VIDEO */
    {0x3c0, 0x3c0, mem68k_memptr_bad,
     mem68k_fetch_video_byte, mem68k_fetch_video_word,
     mem68k_fetch_video_long,
     mem68k_store_video_byte, mem68k_store_video_word,
     mem68k_store_video_long},

    /* CONTRLOER 1 */
    {0x300, 0x300, mem68k_memptr_bad,
     mem68k_fetch_ctl1_byte, mem68k_fetch_ctl1_word,
     mem68k_fetch_ctl1_long,
     mem68k_store_invalid_byte, mem68k_store_invalid_word,
     mem68k_store_invalid_long},

    /* CONTRLOER 2 */
    {0x340, 0x340, mem68k_memptr_bad,
     mem68k_fetch_ctl2_byte, mem68k_fetch_ctl2_word,
     mem68k_fetch_ctl2_long,
     mem68k_store_invalid_byte, mem68k_store_invalid_word,
     mem68k_store_invalid_long},

    /* CONTROLER 3 + PD4990 */
    {0x380, 0x380, mem68k_memptr_bad,
     mem68k_fetch_ctl3_byte, mem68k_fetch_ctl3_word,
     mem68k_fetch_ctl3_long,
     mem68k_store_pd4990_byte, mem68k_store_pd4990_word,
     mem68k_store_pd4990_long},

    /* COIN + Z80 */
    {0x320, 0x320, mem68k_memptr_bad,
     mem68k_fetch_coin_byte, mem68k_fetch_coin_word,
     mem68k_fetch_coin_long,
     mem68k_store_z80_byte, mem68k_store_z80_word, mem68k_store_z80_long},

    /* MEMCARD */
    {0x800, 0x800, mem68k_memptr_bad,
     mem68k_fetch_memcrd_byte, mem68k_fetch_memcrd_word,
     mem68k_fetch_memcrd_long,
     mem68k_store_memcrd_byte, mem68k_store_memcrd_word,
     mem68k_store_memcrd_long},

    /* SETTING DIVER */
    {0x3A0, 0x3a0, mem68k_memptr_bad,
     mem68k_fetch_invalid_byte, mem68k_fetch_invalid_word,
     mem68k_fetch_invalid_long,
     mem68k_store_setting_byte, mem68k_store_setting_word,
     mem68k_store_setting_long},

    {0, 0, NULL, NULL, NULL, NULL, NULL, NULL, NULL}

};

Uint8 *(*mem68k_memptr[0x1000]) (Uint32 addr);
Uint8(*mem68k_fetch_byte[0x1000]) (Uint32 addr);
Uint16(*mem68k_fetch_word[0x1000]) (Uint32 addr);
Uint32(*mem68k_fetch_long[0x1000]) (Uint32 addr);
void (*mem68k_store_byte[0x1000]) (Uint32 addr, Uint8 data);
void (*mem68k_store_word[0x1000]) (Uint32 addr, Uint16 data);
void (*mem68k_store_long[0x1000]) (Uint32 addr, Uint32 data);


void bankswitcher_init(void)
{
    mem68k_def[2].fetch_byte = mem68k_fetch_bk_normal_byte;
    mem68k_def[2].fetch_word = mem68k_fetch_bk_normal_word;
    mem68k_def[2].fetch_long = mem68k_fetch_bk_normal_long;
    mem68k_def[2].store_byte = mem68k_store_bk_normal_byte;
    mem68k_def[2].store_word = mem68k_store_bk_normal_word;
    mem68k_def[2].store_long = mem68k_store_bk_normal_long;
}

int mem68k_init(void)
{
    int i = 0;
    int j;

    bankswitcher_init();

    memset(mem68k_memptr, 0, sizeof(mem68k_memptr));
    memset(mem68k_fetch_byte, 0, sizeof(mem68k_fetch_byte));
    memset(mem68k_fetch_word, 0, sizeof(mem68k_fetch_word));
    memset(mem68k_fetch_long, 0, sizeof(mem68k_fetch_long));
    memset(mem68k_store_byte, 0, sizeof(mem68k_store_byte));
    memset(mem68k_store_word, 0, sizeof(mem68k_store_word));
    memset(mem68k_store_long, 0, sizeof(mem68k_store_long));

    do
    {
        for(j = mem68k_def[i].start; j <= mem68k_def[i].end && j < 0x1000; j++)
        {
            mem68k_memptr[j] = mem68k_def[i].memptr;
            mem68k_fetch_byte[j] = mem68k_def[i].fetch_byte;
            mem68k_fetch_word[j] = mem68k_def[i].fetch_word;
            mem68k_fetch_long[j] = mem68k_def[i].fetch_long;
            mem68k_store_byte[j] = mem68k_def[i].store_byte;
            mem68k_store_word[j] = mem68k_def[i].store_word;
            mem68k_store_long[j] = mem68k_def[i].store_long;
        }
        i++;
    } while(mem68k_def[i].start != 0 || mem68k_def[i].end != 0);

    return 0;
}

Uint8 *mem68k_memptr_bad(Uint32 addr)
{
    return memory.rom.cpu_m68k.p;
}

Uint8 *mem68k_memptr_cpu(Uint32 addr)
{
    if(addr < cpu68k_romlen)
        return memory.rom.cpu_m68k.p + addr;
    return memory.rom.cpu_m68k.p;
}

Uint8 *mem68k_memptr_bios(Uint32 addr)
{
    return memory.rom.bios_m68k.p + (addr & 0x1FFFF);
}

Uint8 *mem68k_memptr_cpu_bk(Uint32 addr)
{
    return memory.rom.cpu_m68k.p + (addr & 0xFFFFF) + bankaddress;
}

Uint8 *mem68k_memptr_ram(Uint32 addr)
{
    return memory.ram + (addr & 0xFFFF);
}

void cpu_68k_bankswitch(Uint32 address)
{
    bankaddress = address;
}

void cpu_68k_reset(void)
{
    cpu68k_reset();
}

void cpu_68k_init(void)
{

    cpu68k_clearcache();

    cpu68k_ram = memory.ram;
    cpu68k_rom = memory.rom.cpu_m68k.p;
    cpu68k_romlen = memory.rom.cpu_m68k.size < 0x100000 ?
        memory.rom.cpu_m68k.size : 0x100000;

    mem68k_init();
    cpu68k_init();

    if(memory.rom.cpu_m68k.size > 0x100000)
        cpu_68k_bankswitch(0);
}

int cpu_68k_run(Uint32 nb_cycle)
{
    static int n;
    n = reg68k_external_execute(nb_cycle);
    /*
    pc=regs.pc;
    sr=regs.sr.sr_int;
    asp=regs.sp;
    memcpy(mregs,regs.regs,16*sizeof(Uint32));
    */
    cpu68k_endfield();
    return n;
}

/* Debuger interface */
static Uint32 gen68k_disassemble(int pc, int nb_instr)
{
    int i;
    char buf[512];
    Uint8 *addr;
    //printf("%x\n",pc);
    for (i = 0; i < nb_instr; i++) {
	addr = mem68k_memptr[pc >> 12] (pc);
	pc += diss68k_getdumpline(pc, addr, buf)*2;
	printf("%s", buf);
    }
    return pc;
}

static void gen68k_dumpreg(void)
{
    int i;
    printf("d0=%08x   d4=%08x   a0=%08x   a4=%08x   %c%c%c%c%c %04x\n",
	   regs.regs[0],regs.regs[4],regs.regs[8],regs.regs[12],
	   ((regs.sr.sr_int >> 4) & 1 ? 'X' : '-'),
	   ((regs.sr.sr_int >> 3) & 1 ? 'N' : '-'),
	   ((regs.sr.sr_int >> 2) & 1 ? 'Z' : '-'),
	   ((regs.sr.sr_int >> 1) & 1 ? 'V' : '-'),
	   ((regs.sr.sr_int     ) & 1 ? 'C' : '-'),regs.sr.sr_int);
    printf("d1=%08x   d5=%08x   a1=%08x   a5=%08x\n",
	   regs.regs[1],regs.regs[5],regs.regs[9],regs.regs[13]);
    printf("d2=%08x   d6=%08x   a2=%08x   a6=%08x\n",
	   regs.regs[2],regs.regs[6],regs.regs[10],regs.regs[14]);
    printf("d3=%08x   d7=%08x   a3=%08x   a7=%08x   usp=%08x\n",
	   regs.regs[3],regs.regs[7],regs.regs[11],regs.regs[15],regs.sp);
    
}

static void hexdump(Uint32 addr) {
    Uint8 c, tmpchar[16];
    Uint32 tmpaddr;
    int i, j, k;
    tmpaddr = addr & 0xFFFFFFF0;
    for(i = 0; i < 8; i++) {
	printf("%08X: %c", tmpaddr,(addr == tmpaddr) ? '>' : ' ' );
	for(j = 0; j < 16; j += 2) {
	    k = fetchword(tmpaddr) & 0xFFFF;
#ifdef WORDS_BIGENDIAN
	    tmpchar[j + 1] = k >> 8;
	    tmpchar[j    ] = k & 0xFF;
#else
	    tmpchar[j    ] = k >> 8;
	    tmpchar[j + 1] = k & 0xFF;
#endif
	    tmpaddr += 2;
	    printf("%02X%02X%c",
		   tmpchar[j], tmpchar[j + 1],
		   (( addr            == tmpaddr   )&&(j!=14))?'>':
		   (((addr&0xFFFFFFFE)==(tmpaddr-2))?'<':' ')
		);
	}
	printf("  ");
	for(j = 0; j < 16; j++) {
	    c = tmpchar[j];
	    if((c<32)||(c>126))c='.';
	    printf("%c", c);
	}
	printf("\n");
    }
    //addr += 0x80;
}

Uint32 cpu_68k_getpc(void)
{
    return regs.pc;
}

int cpu_68k_run_step(void)
{
    return reg68k_external_step();
}





void cpu_68k_interrupt(int a)
{
    //  printf("Interrupt %d\n",a);
    reg68k_external_autovector(a);
}

int cpu_68k_getcycle(void)
{
    return cpu68k_clocks;
}

#endif
#include "generator68k/cpu68k.h"
size_t GnGeo68kStateSize(void) { return sizeof(regs); }
void GnGeo68kStateSaveLoad(void *p, int load) { if(load) memcpy(&regs,p,sizeof(regs)); else memcpy(p,&regs,sizeof(regs)); }

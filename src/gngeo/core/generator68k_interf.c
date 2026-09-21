void cpu_68k_init(void)
{
    printf("GEN68k CPU INIT\n");
    cpu68k_clearcache();
    cpu68k_ram = memory.ram;
    cpu68k_rom = memory.rom.cpu_m68k.p;
    if (memory.rom.cpu_m68k.size < 0x100000)
        cpu68k_romlen = memory.rom.cpu_m68k.size;
    else
        cpu68k_romlen = 0x100000;
    mem68k_init();
    cpu68k_init();
    if (memory.rom.cpu_m68k.size > 0x100000)
        cpu_68k_bankswitch(0);
}

int cpu_68k_run(Uint32 nb_cycle)
{
    static int n;
    n = reg68k_external_execute(nb_cycle);
    //printf("pc=%x\n",regs.pc);
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

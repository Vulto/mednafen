#include "gngeo_compat.h"
#ifndef GNGEO_EMU_H
#define GNGEO_EMU_H
#include "gngeo_compat.h"
#include "roms.h"
typedef enum SYSTEM { SYS_ARCADE=0, SYS_HOME, SYS_UNIBIOS, SYS_MAX } SYSTEM;
typedef enum COUNTRY { CTY_JAPAN=0, CTY_EUROPE, CTY_USA, CTY_ASIA, CTY_MAX } COUNTRY;
typedef struct { char *game; Uint16 x_start,y_start,res_x,res_y,sample_rate,test_switch; Uint8 sound,vsync,do_message,nb_joy,raster,debug,rom_type,special_bios,extra_xor,pal,accurate940; SYSTEM system; COUNTRY country; Uint8 autoframeskip,show_fps,sleep_idle,screen320; char message[128]; char fps[4]; } CONF;
extern CONF conf; extern int frame,nb_interlace,current_line;
void neogeo_reset(void); void init_neo(void); void setup_misc_patch(char *name);
int cpu_68k_run(Uint32 cycles); void cpu_68k_interrupt(int level); int cpu_68k_getcycle(void);
void cpu_z80_run(int cycles); void cpu_z80_raise_irq(int l); void cpu_z80_lower_irq(void);
void pd4990a_init(void); void pd4990a_addretrace(void);
void init_video(void); void draw_screen(void); void draw_screen_scanline(int start,int end,int refresh);
#endif

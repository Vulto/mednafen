if WANT_GNGEO_EMU

mednafen_SOURCES += \
        gngeo/gngeo.cpp \
        gngeo/gngeo_bios.cpp \
        gngeo/rom_loader.cpp \
        gngeo/gngeo.h \
        gngeo/gngeo_bios.h \
        gngeo/rom_loader.h \
        gngeo/rom_types.h \
        gngeo/core/gngeo_runtime.c \
        gngeo/core/gngeo_runtime.h \
        gngeo/core/gngeo_state.c \
        gngeo/core/gngeo_compat.h \
        gngeo/core/gngeo_data.c \
        gngeo/core/gngeo_drivers.c \
        gngeo/core/gngeo_drivers.h \
        gngeo/core/emu.h \
        gngeo/core/conf.h \
        gngeo/core/screen.h \
        gngeo/core/state.h \
        gngeo/core/sound.h \
        gngeo/core/roms.h \
        gngeo/core/roms_core.c \
        gngeo/core/memory.c \
        gngeo/core/memory.h \
        gngeo/core/video.c \
        gngeo/core/video.h \
        gngeo/core/video_template.h \
        gngeo/core/pd4990a.c \
        gngeo/core/pd4990a.h \
        gngeo/core/timer.c \
        gngeo/core/timer.h \
        gngeo/core/transpack.c \
        gngeo/core/transpack.h \
        gngeo/core/gnutil.c \
        gngeo/core/gnutil.h \
        gngeo/core/neocrypt.c \
        gngeo/core/neocrypt.h \
        gngeo/core/neoboot.c \
        gngeo/core/mame_layer.c \
        gngeo/core/mame_layer.h \
        gngeo/core/generator68k_interf.c \
        gngeo/core/mamez80_interf.c \
        gngeo/core/generator68k/cpu68k.c \
        gngeo/core/generator68k/reg68k.c \
        gngeo/core/generator68k/diss68k.c \
        gngeo/core/generator68k/tab68k.c \
        gngeo/core/generator68k/cpu68k-0.c \
        gngeo/core/generator68k/cpu68k-1.c \
        gngeo/core/generator68k/cpu68k-2.c \
        gngeo/core/generator68k/cpu68k-3.c \
        gngeo/core/generator68k/cpu68k-4.c \
        gngeo/core/generator68k/cpu68k-5.c \
        gngeo/core/generator68k/cpu68k-6.c \
        gngeo/core/generator68k/cpu68k-7.c \
        gngeo/core/generator68k/cpu68k-8.c \
        gngeo/core/generator68k/cpu68k-9.c \
        gngeo/core/generator68k/cpu68k-a.c \
        gngeo/core/generator68k/cpu68k-b.c \
        gngeo/core/generator68k/cpu68k-c.c \
        gngeo/core/generator68k/cpu68k-d.c \
        gngeo/core/generator68k/cpu68k-e.c \
        gngeo/core/generator68k/cpu68k-f.c \
        gngeo/core/generator68k/cpu68k.h \
        gngeo/core/generator68k/generator.h \
        gngeo/core/generator68k/reg68k.h \
        gngeo/core/generator68k/registers.h \
        gngeo/core/generator68k/mem68k.h \
        gngeo/core/generator68k/compile.h \
        gngeo/core/generator68k/cpu68k-inline.h \
        gngeo/core/generator68k/def68k-iibs.h \
        gngeo/core/generator68k/def68k-funcs.h \
        gngeo/core/generator68k/def68k-proto.h \
        gngeo/core/mamez80/z80.c \
        gngeo/core/mamez80/z80.h \
        gngeo/core/mamez80/mamez80.h \
        gngeo/core/ym2610/2610intf.c \
        gngeo/core/ym2610/ym2610.c \
        gngeo/core/ym2610/2610intf.h \
        gngeo/core/ym2610/ym2610.h \
        gngeo/core/ym2610/mvs.h

mednafen_CPPFLAGS = \
        -I$(srcdir)/gngeo/core \
        -I$(srcdir)/gngeo/core/generator68k \
        -I$(srcdir)/gngeo/core/mamez80 \
        -I$(srcdir)/gngeo/core/ym2610 \
        -DUSE_GENERATOR68K -DUSE_MAMEZ80

mednafen_CFLAGS = -fcommon

endif

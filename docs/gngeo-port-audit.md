# GnGeo Core Port Audit

## Objective

The Mednafen GnGeo module must reuse GnGeo's Neo Geo emulation logic and discard GnGeo's standalone frontend/control layer. Mednafen owns the module lifecycle, filesystem, settings, input, video, audio, timing boundary, reset, and state interface.

## Reuse classification

### Reuse directly/adapt minimally

- `memory.c`: Neo Geo 68000 address map and hardware register behavior.
- `roms.c`: ROM driver interpretation, region allocation, parent fallback, ROM interleaving, FIX conversion, sprite conversion, BIOS selection, and game-specific initialization/decryption dispatch.
- `video.c` / renderer code: Neo Geo sprite/FIX/palette/VRAM rendering logic. Only the final output target changes from SDL surfaces to Mednafen's framebuffer.
- `interrupt.c` / interrupt logic: Neo Geo IRQ behavior.
- `timer.c`: Neo Geo timing/timer behavior.
- `pd4990a.c`: RTC behavior.
- `ym2610/*`: YM2610 emulation and register behavior, adapted to Mednafen audio/timing callbacks.
- Z80 interface logic: Neo Geo Z80 memory/port map and communication protocol. The CPU execution engine may use Mednafen's existing Z80 implementation where its interface is suitable.
- game-specific decrypt/protection routines from `neocrypt`/original GnGeo sources.

### Replace with Mednafen infrastructure

- GnGeo `main_loop()`.
- SDL window/surfaces/input/audio/timing.
- GnGeo configuration parser and command-line frontend.
- GnGeo menus/events/messages.
- GnGeo save-state implementation.
- GnGeo standalone filesystem/ZIP frontend layer.
- GnGeo frame pacing and frontend frame skipping.
- Standalone screen/blitter/effect pipeline.

### Existing Mednafen facilities to use

- `MDFNGI` lifecycle.
- `GameFile` and `VirtualFS`/`ArchiveReader`.
- `MDFNSetting`/`Settings`.
- `M68K` CPU core.
- Existing Z80 CPU infrastructure when compatible.
- `EmulateSpecStruct` for framebuffer, audio and cycle accounting.
- `StateAction`/`MDFNSS_StateAction`.
- `SetInput()` and Mednafen input descriptors.
- `DoSimpleCommand()` for reset/power.

## Current code assessment

The current `gngeo_memory.cpp` is an adapter around Mednafen's `M68K`, not a replacement 68000 emulator. It already contains substantial Neo Geo bus behavior, including ROM/RAM banking, SRAM, card, palette, VRAM registers, Z80 command registers and vector switching.

The current ROM loader is structurally based on GnGeo's original `dr_load_roms()`/`load_region()` flow, but it still needs to be aligned more closely with the original ordering and initialization sequence. In particular, the original flow is:

1. parse ROM driver;
2. open game and parent archives;
3. allocate regions;
4. load all game/parent regions;
5. alias ADPCM-B when absent;
6. initialize game-specific ROM/decryption state;
7. convert graphics;
8. load global/custom BIOS;
9. copy vectors and initialize video/hardware.

The Mednafen port must preserve that sequence while replacing only the I/O/frontend mechanisms.

## Correct implementation priority

1. Finish ROM/BIOS loading using the original GnGeo sequence and Mednafen VFS.
2. Port the original game-specific initialization/decryption dispatch.
3. Complete the Neo Geo hardware state around the existing 68000 bus adapter.
4. Port the original video renderer to the Mednafen framebuffer.
5. Integrate the original Z80 communication and use the existing Mednafen Z80 engine where appropriate.
6. Integrate the original YM2610 and ADPCM code with Mednafen audio/timing.
7. Port RTC and remaining I/O behavior.
8. Port the original frame scheduler, including `my_timer()`, scanlines, IRQ1/IRQ2, YM2610 timers/IRQ, Z80 timing/NMI/IRQ and watchdog.
9. Implement Mednafen input, reset, settings and save-state integration.
10. Validate KOF98, then parent/clone, BIOS variants, graphics, audio and synchronization.
11. Remove every remaining SDL 1.2/frontend dependency from the core.

## Completion criterion

The core is not complete when it merely compiles or loads a ROM database. Completion requires a real Neo Geo game to boot and run inside Mednafen using Mednafen's own lifecycle and interfaces, with video, audio, input, interrupts, timing and save states working.

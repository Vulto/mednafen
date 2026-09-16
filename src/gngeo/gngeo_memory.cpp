#include "gngeo_memory.h"
#include "../hw_cpu/m68k/m68k.h"
#include <cstring>

using namespace Mednafen;

namespace
{
GAME_ROMS *R = nullptr;
M68K CPU;

uint8 Ram[0x10000];
uint8 Sram[0x10000];
uint8 Card[0x800];
uint8 Palette[0x2000];
uint8 VideoRam[0x10000];
uint8 GameVector[0x80];

uint32 Bank = 0x100000;
uint16 VideoPointer = 0;
uint16 VideoModulo = 0;
uint16 VideoReadBuffer = 0;
uint32 Irq2Position = 0;
uint8 Irq2Control = 0;
uint8 SoundCode = 0;
uint8 PendingCommand = 0;
uint8 ResultCode = 0;
uint8 CurrentVector = 0;
uint8 SramLocked = 0;
uint8 Player1 = 0xff;
uint8 Player2 = 0xff;
uint8 Start = 0xff;
uint8 Coin = 0xff;

static uint8 ReadRom8(const ROM_REGION &region, uint32 offset)
{
	if(!region.p || offset >= region.size)
		return 0xff;
	return region.p[offset];
}

static uint16 ReadRom16(const ROM_REGION &region, uint32 offset)
{
	return (uint16(ReadRom8(region, offset)) << 8) | ReadRom8(region, offset + 1);
}

static uint16 ReadVideoRamWord(uint16 pointer)
{
	uint32 offset = (uint32(pointer & 0x7fff) << 1);
	return (uint16(VideoRam[offset]) << 8) | VideoRam[offset + 1];
}

static void WriteRam16(uint8 *ram, uint32 offset, uint16 value)
{
	ram[offset & 0xffff] = value >> 8;
	ram[(offset + 1) & 0xffff] = value;
}

static uint16 ReadVideoRegister(uint32 address)
{
	switch(address & 0x0f)
	{
		case 0x00:
		case 0x02:
		case 0x0a:
			return VideoReadBuffer;
		case 0x04:
			return VideoModulo;
		case 0x06:
			return 0;
		default:
			return 0;
	}
}

static void WriteVideoWord(uint32 address, uint16 value)
{
	switch(address & 0x0f)
	{
		case 0x00:
			VideoPointer = value;
			VideoReadBuffer = ReadVideoRamWord(VideoPointer);
			break;

		case 0x02:
			WriteRam16(VideoRam, uint32(VideoPointer & 0x7fff) << 1, value);
			VideoPointer = (VideoPointer & 0x8000) |
				((VideoPointer + VideoModulo) & 0x7fff);
			VideoReadBuffer = ReadVideoRamWord(VideoPointer);
			break;

		case 0x04:
			VideoModulo = (value & 0x4000) ? (value | 0x8000) : (value & 0x7fff);
			break;

		case 0x06:
			Irq2Control = value & 0xff;
			break;

		case 0x08:
			Irq2Position = (Irq2Position & 0x0000ffff) | (uint32(value) << 16);
			break;

		case 0x0a:
			Irq2Position = (Irq2Position & 0xffff0000) | value;
			break;

		case 0x0c:
			break;
	}
}

static uint8 Read8(uint32 address)
{
	address &= 0x00ffffff;

	if(address < 0x100000)
		return ReadRom8(R->cpu_m68k, address);

	if(address < 0x200000)
		return Ram[address & 0xffff];

	if(address < 0x300000)
	{
		uint32 offset = Bank + (address & 0xfffff);
		return ReadRom8(R->cpu_m68k, offset);
	}

	switch(address & 0xfff000)
	{
		case 0x300000:
			return 0xff;

		case 0x320000:
			if((address & 0xffff) == 0x0000)
				return (PendingCommand ? 0x00 : 0x80) | ResultCode;
			return 0xff;

		case 0x340000:
			if((address & 0xffff) == 0x0000)
				return Player2;
			return 0xff;

		case 0x380000:
			if((address & 0xffff) == 0x0000)
				return Start;
			if((address & 0xffff) == 0x0001)
				return Coin;
			return 0;

		case 0x3a0000:
			return 0;

		case 0x3c0000:
		{
			uint16 value = ReadVideoRegister(address & ~1);
			return (address & 1) ? uint8(value) : uint8(value >> 8);
		}
	}

	if((address & 0xffe000) == 0x400000)
		return Palette[address & 0x1fff];

	if((address & 0xfff000) == 0x800000)
		return (address & 1) ? 0xff : Card[(address & 0xfff) >> 1];

	if((address & 0xff0000) == 0xc00000)
		return ReadRom8(R->bios_m68k, address & 0x1ffff);

	if((address & 0xff0000) == 0xd00000)
		return Sram[address & 0xffff];

	return 0xf0;
}

static uint16 Read16(uint32 address)
{
	if((address & 0x00f00000) == 0x003c0000)
		return ReadVideoRegister(address);
	return (uint16(Read8(address)) << 8) | Read8(address + 1);
}

static uint16 ReadOp(uint32 address)
{
	return Read16(address);
}

static void Write8(uint32 address, uint8 value)
{
	address &= 0x00ffffff;

	if(address >= 0x100000 && address < 0x200000)
	{
		Ram[address & 0xffff] = value;
		return;
	}

	if(address >= 0x2ffff0 && address < 0x300000 && R->cpu_m68k.size > 0x100000)
	{
		Bank = (uint32(value & 7) + 1) * 0x100000;
		if(Bank >= R->cpu_m68k.size)
			Bank = 0x100000;
		return;
	}

	if((address & 0xfff000) == 0x320000)
	{
		if((address & 0xffff) == 0)
		{
			SoundCode = value;
			PendingCommand = 1;
		}
		return;
	}

	if((address & 0xfff000) == 0x3a0000)
	{
		uint32 offset = address & 0xff;
		if(offset == 0x03)
		{
			memcpy(R->cpu_m68k.p, R->bios_m68k.p, 0x80);
			CurrentVector = 0;
		}
		else if(offset == 0x13)
		{
			memcpy(R->cpu_m68k.p, GameVector, 0x80);
			CurrentVector = 1;
		}
		else if(offset == 0x0d)
			SramLocked = 1;
		else if(offset == 0x1d)
			SramLocked = 0;
		return;
	}

	if((address & 0xfff000) == 0x3c0000)
	{
		uint16 old = ReadVideoRegister(address & ~1);
		uint16 value16 = (address & 1) ? uint16((old & 0xff00) | value) : uint16((uint16(value) << 8) | (old & 0xff));
		WriteVideoWord(address & ~1, value16);
		return;
	}

	if((address & 0xffe000) == 0x400000)
	{
		Palette[address & 0x1fff] = value;
		return;
	}

	if((address & 0xfff000) == 0x800000)
	{
		if(!(address & 1))
			Card[(address & 0xfff) >> 1] = value;
		return;
	}

	if((address & 0xff0000) == 0xd00000)
	{
		if(!SramLocked)
			Sram[address & 0xffff] = value;
		return;
	}
}

static void Write16(uint32 address, uint16 value)
{
	if((address & 0xfff000) == 0x3c0000)
	{
		WriteVideoWord(address, value);
		return;
	}

	if((address & 0xffe000) == 0x400000)
	{
		WriteRam16(Palette, address & 0x1fff, value);
		return;
	}

	Write8(address, value >> 8);
	Write8(address + 1, value);
}
}

bool GnGeoMemoryInit(GAME_ROMS *roms)
{
	if(!roms || !roms->cpu_m68k.p || !roms->bios_m68k.p)
		return false;

	R = roms;
	memcpy(GameVector, R->cpu_m68k.p, sizeof(GameVector));
	memcpy(R->cpu_m68k.p, R->bios_m68k.p, sizeof(GameVector));

	std::memset(Ram, 0, sizeof(Ram));
	std::memset(Sram, 0, sizeof(Sram));
	std::memset(Card, 0, sizeof(Card));
	std::memset(Palette, 0, sizeof(Palette));
	std::memset(VideoRam, 0, sizeof(VideoRam));

	Bank = 0x100000;
	VideoPointer = 0;
	VideoModulo = 0;
	VideoReadBuffer = 0;
	Irq2Position = 0;
	Irq2Control = 0;
	SoundCode = 0;
	PendingCommand = 0;
	ResultCode = 0;
	CurrentVector = 0;
	SramLocked = 0;

	CPU.BusReadInstr = ReadOp;
	CPU.BusRead8 = Read8;
	CPU.BusRead16 = Read16;
	CPU.BusWrite8 = Write8;
	CPU.BusWrite16 = Write16;
	CPU.Reset(true);
	return true;
}

void GnGeoMemoryClose(void)
{
	R = nullptr;
}

void GnGeoMemoryReset(void)
{
	if(!R)
		return;

	std::memset(Ram, 0, sizeof(Ram));
	std::memset(Palette, 0, sizeof(Palette));
	std::memset(VideoRam, 0, sizeof(VideoRam));
	Bank = 0x100000;
	VideoPointer = 0;
	VideoModulo = 0;
	VideoReadBuffer = 0;
	Irq2Position = 0;
	Irq2Control = 0;
	SoundCode = 0;
	PendingCommand = 0;
	ResultCode = 0;
	SramLocked = 0;
	CPU.Reset(false);
}

void GnGeoMemoryRun(int32_t cycles)
{
	CPU.Run(CPU.timestamp + cycles);
}

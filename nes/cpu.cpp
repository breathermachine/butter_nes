#define _CRT_SECURE_NO_WARNINGS

#include "cpu.h"
//#include "graphics_system.h"
#include <string.h>
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#ifdef _WIN32
#include <crtdbg.h>
#include <windows.h>
#endif

char _log_opname[255];
char _log_operands[255];
char _reg_values[256];

bool cpu_log = true;

void decodeLatch(unsigned short value);

// 9 - supported
// 0 - unsupported: 0 operands
// 1 - unsupported: 1 operands
// 2 - unsupported: 2 operands
int unsupportedOpcodes[] =
{
//  0  1  2  3  4  5  6  7  8  9  A  B  C  D  E  F
    9, 9, 1, 1, 1, 9, 9, 1, 9, 9, 9, 1, 2, 9, 9, 1,  //0
    9, 9, 1, 1, 1, 9, 9, 1, 9, 9, 0, 1, 2, 9, 9, 1,  //1
    9, 9, 1, 1, 9, 9, 9, 1, 9, 9, 9, 1, 9, 9, 9, 1,  //2
    9, 9, 1, 1, 1, 9, 9, 1, 9, 9, 0, 1, 2, 9, 9, 1,  //3
    9, 9, 1, 1, 1, 9, 9, 1, 9, 9, 9, 1, 9, 9, 9, 1,  //4
    9, 9, 1, 1, 1, 9, 9, 1, 9, 9, 0, 1, 2, 9, 9, 1,  //5
    9, 9, 1, 1, 1, 9, 9, 1, 9, 9, 9, 1, 9, 9, 9, 1,  //6
    9, 9, 1, 1, 1, 9, 9, 1, 9, 9, 0, 1, 2, 9, 9, 1,  //7
    1, 9, 1, 1, 9, 9, 9, 1, 9, 1, 9, 1, 9, 9, 9, 1,  //8
    9, 9, 1, 1, 9, 9, 9, 1, 9, 9, 9, 1, 1, 9, 1, 1,  //9
    9, 9, 9, 1, 9, 9, 9, 1, 9, 9, 9, 1, 9, 9, 9, 1,  //A
    9, 9, 1, 1, 9, 9, 9, 1, 9, 9, 9, 1, 9, 9, 9, 1,  //B
    9, 9, 1, 1, 9, 9, 9, 1, 9, 9, 9, 1, 9, 9, 9, 1,  //C
    9, 9, 1, 1, 1, 9, 9, 1, 9, 9, 0, 1, 2, 9, 9, 1,  //D
    9, 9, 1, 1, 9, 9, 9, 1, 9, 9, 9, 1, 9, 9, 9, 1,  //E
    9, 9, 1, 1, 1, 9, 9, 1, 9, 9, 0, 1, 2, 9, 9, 1,  //F
};

extern unsigned char lengthCounterPeriodTable[];

template<typename T>
void set_bit(T& var, int bit, int value)
{
	if (!!value)
		var |= (value << bit);
	else
		var &= ~(value << bit);
}

CPU::CPU():
	resetCPU(false)
{
	cpuMemVec.resize(MEMORY_SIZE);
	ppuMemVec.resize(MEMORY_SIZE);
	sprMemVec.resize(0x100);
	memory = &cpuMemVec[0];
	ppuMemory = &ppuMemVec[0];
	sprMemory = &sprMemVec[0];
	romPRGROM = 0;
	romCHRROM = 0;
}

CPU::~CPU()
{
}

void CPU::push_byte(unsigned char byte)
{
	memory[0x100 + SP] = byte;
	--SP;
}

unsigned char CPU::pull_byte()
{
	return memory[0x100 + SP];
}

void CPU::shift(LOCATION reg, SHIFT_DIRECTION dir, bool rotate)
{
	unsigned short temp, carry;

	if (reg == REG_A)
		temp = A;
	else if (reg == REG_MEMORY)
		temp = cpuValue;

	if (dir == BIT_SHIFT_LEFT)
	{
		carry = !!(temp & 0x80);
		temp <<= 1;

		//Bit 0 is filled with the current value of the
		//carry flag whilst the old bit 7 becomes the new
		//carry flag value.
		if (rotate)
		{
			set_bit(temp, 0, get_flag(FLAG_CARRY));
		}
	}
	else if (dir == BIT_SHIFT_RIGHT)
	{
		carry = (temp & 1);
		temp >>= 1;

		//Bit 7 is filled with the current value of the
		//carry flag whilst the old bit 0 becomes the new
		//carry flag value.
		if (rotate)
		{
			set_bit(temp, 7, get_flag(FLAG_CARRY));
		}
	}

	set_flag(FLAG_CARRY, carry);
	set_flag(FLAG_ZERO, (char)temp == 0);
	set_flag(FLAG_NEGATIVE, ((char) temp) < 0);

	if (reg == REG_A)
		A = static_cast<unsigned char>(temp);
	else
		cpuValue = temp;
}

void CPU::bitwise(BITWISE_OP operation, int value)
{
	switch (operation)
	{
	case BITWISE_AND:
		A &= value;
		break;
	case BITWISE_OR:
		A |= value;
		break;
	case BITWISE_XOR:
		A ^= value;
		break;
	default:
		assert(0);
	}

	set_flag(FLAG_ZERO, A == 0);
	set_flag(FLAG_NEGATIVE, (char)A < 0);
}

void CPU::set_flag(FLAG flag, int value)
{
	if (value)
		P |= flag;
	else
		P &= ~flag;
}

int CPU::get_flag(FLAG flag)
{
	return !!(P & flag);
}

void CPU::load(LOCATION reg, int value)
{
	switch(reg)
	{
	case REG_A:
		A = value;
		break;
	case REG_X:
		X = value;
		break;
	case REG_Y:
		Y = value;
		break;
	}

	set_flag(FLAG_NEGATIVE, (char)value < 0);
	set_flag(FLAG_ZERO, value == 0);
}

bool CPU::IsSupportedMapper(int mapper)
{
	bool isSupported = false;

	int supportedMappers[] = { MAPPER_NROM, MAPPER_UNROM, MAPPER_CNROM, MAPPER_87, MAPPER_185, MAPPER_MMC1, MAPPER_MMC3, MAPPER_GNROM, MAPPER_AXROM, MAPPER_MMC2 };

	for (int i = 0; i < sizeof(supportedMappers)/sizeof(supportedMappers[0]); ++i)
		if (supportedMappers[i] == mapper)
			return true;

	return false;
}

void CPU::load_rom(char *filename)
{
	if (!filename)
		return;

	printf("\nLoading %s\n", filename);
	FILE *f = fopen(filename, "rb");
	if (!f)
    {
        printf("Cannot find file: %s\n", filename);
        return;
    }

	INESHeader header;
	fread(&header, sizeof(header), 1, f);

	int mapper = ((header.flag6 & 0xF0) >> 4) | (header.flag7 & 0xF0);

	if (!IsSupportedMapper(mapper))
	{
		fprintf(stderr, "Unsupported mapper: %d\n", mapper);
		fclose(f);
		return;
	}

	romHeader = header;

	romPRGROM = 0;
	romCHRROM = 0;

	romMapperType = mapper;

	if ((header.flag7 & 0xC) == 0x8)
	{
		nes2_0Header = true;
		printf("NES 2.0 header\n");
	}
	else
	{
		nes2_0Header = false;
		printf("iNES header\n");
	}
		
	printf("Mapper: %d\n", romMapperType);
	printf("16kb PRGROM: %d\n", header.prgRomSize);
	printf(" 8kb CHRROM: %d\n", header.chrRomSize);
	printf("     PRGRAM: %d\n", header.prgRamSize);

	// Initialize WRAM / battery backed RAM
	wramSize = 0;
	batterySize = 0;
	chrramsize = 0;

	if (nes2_0Header)
	{
		if (header.flag10 & 0xF0)
			batterySize = 128 << (((header.flag10 & 0xF0) >> 4) - 1);
		else
			batterySize = 0;

		if (header.flag10 & 0xF)
			wramSize = 128 << ((header.flag10 & 0xF) - 1);
		else
			wramSize = 0;

		if (header.flag11 & 0xF)
			chrramsize = 128 << ((header.flag11 & 0xF) - 1);
		else
			chrramsize = 0;
	}
	else
	{
		if (header.flag6 & 2)
			batterySize = SIZE_8K;

		if (header.prgRamSize == 0) // 8kb by default if 0
			wramSize = SIZE_8K;

		if (header.chrRomSize == 0) // 8kb CHR-RAM
			chrramsize = SIZE_8K;
	}

	// Battery backed ram
	if (batterySize > 0)
	{
		battPRGRAMVec.resize(batterySize);
		battPRGRAM = &battPRGRAMVec[0];
	}
	else
	{
		battPRGRAM = NULL;
	}

	// PRGRAM
	if (wramSize > 0)
	{
		WRAMVec.resize(wramSize);
		WRAM = &WRAMVec[0];
	}
	else
	{
		WRAM = NULL;
	}

	// CHR-RAM
	if (chrramsize) // CHRRAM. Assume 8k for now
	{
		chrRamVec.resize(chrramsize);
		chrram = &chrRamVec[0];
	}
	else
	{
		chrram = NULL;
	}

	printf("CHR-RAM %d\n", chrramsize);
	printf("WRAM %d\n", wramSize);
	printf("SRAM %d\n", batterySize);

	int size = header.prgRomSize * SIZE_16K;
	if (size != 0)
	{
		romPRGMROMVec.resize(size);
		romPRGROM = &romPRGMROMVec[0];
		fread(romPRGROM, 1, size, f);
	}

	size = header.chrRomSize * SIZE_8K;
	if (size != 0)
	{
		romCHRROMVec.resize(size);
		romCHRROM = &romCHRROMVec[0];
		fread(romCHRROM, 1, size, f);
	}

	fclose(f);

	if (header.flag9)
		printf("WARNING: PAL rom\n");

	romFilename = filename;
}

void CPU::reset()
{
	if (romHeader.flag6 & 1)
		ppuNTMirroring = MIRRORING_VERTICAL; // Vertical mirroring
	else
		ppuNTMirroring = MIRRORING_HORIZONTAL;

	if (romHeader.flag6 & 8)
		printf("4 SCREEN!\n");

	memset(memory, 0, 0x10000);
	memset(ppuMemory, 0, 0x10000);
	memset(sprMemory, 0, 0x100);
	if (WRAM)
		memset(WRAM, 0, wramSize);

	if (romMapperType == MAPPER_NROM || romMapperType == MAPPER_185)
	{
		memcpy(&memory[0x8000], romPRGROM, romHeader.prgRomSize * SIZE_16K);

		if (romHeader.prgRomSize == 1)
			memcpy(&memory[0xC000], romPRGROM, 0x4000);

		memcpy(&ppuMemory[0], romCHRROM, SIZE_8K * romHeader.chrRomSize);
	}
	else if (romMapperType == MAPPER_UNROM)
	{
		memcpy(&memory[0x8000], romPRGROM, SIZE_16K);
		memcpy(&memory[0xC000], &romPRGROM[(romHeader.prgRomSize - 1) * SIZE_16K], SIZE_16K);
	}
	else if (romMapperType == MAPPER_CNROM)
	{
		memcpy(&memory[0x8000], romPRGROM, SIZE_16K * romHeader.prgRomSize);
		if (romHeader.prgRomSize == 1)
			memcpy(&memory[0xC000], romPRGROM, SIZE_16K);
		memcpy(ppuMemory, romCHRROM, SIZE_8K);
	}
	else if (romMapperType == 87) // CNROM but bit reversed
	{
		memcpy(&memory[0x8000], romPRGROM, SIZE_16K * romHeader.prgRomSize);
		if (romHeader.prgRomSize == 1)
			memcpy(&memory[0xC000], romPRGROM, SIZE_16K);
	}
	else if (romMapperType == MAPPER_GNROM)
	{
		ChangePRGBank(0x8000, 0, SIZE_32K);
		ChangeCHRBank(0, 0, SIZE_8K);
	}
	else if (romMapperType == MAPPER_MMC1)
	{
		// First 16kb at $8000, last 16kb at $C000
		ChangePRGBank(0x8000, 0, SIZE_16K);
		ChangePRGBank(0xC000, romHeader.prgRomSize - 1, SIZE_16K);

		ChangeCHRBank(0, 0, SIZE_8K);
		// First 8kb of pattern?
		mmc1WriteCount = 0;
		mmc1ShiftRegister = 0;
		mmc1PROGROMBankMode = 3;
		mmc1CHRROMBankMode = 0;
		mmc1Mirroring = 0;
	}
	else if (romMapperType == MAPPER_MMC3)
	{
		//CPU $C000 - $DFFF(or $8000 - $9FFF) : 8 KB PRG ROM bank, fixed to the second - last bank
		//CPU $E000 - $FFFF : 8 KB PRG ROM bank, fixed to the last bank
		ChangePRGBank(0xC000, romHeader.prgRomSize - 1, SIZE_16K);

		mmc3IRQEnabled = false;
		mmc3IRQCounterReload = false;
	}
	else if (romMapperType == MAPPER_AXROM)
	{
		ChangePRGBank(0x8000, 0, SIZE_32K);
		ppuNTMirroring = MIRRORING_1SCREEN_000;
	}
	else if (romMapperType == MAPPER_MMC2)
	{
		ChangePRGBank(0x8000, 0, SIZE_8K);
		ChangePRGBank(0xA000, romHeader.prgRomSize * 2 - 3, SIZE_8K, 3);
	}
	else
	{
		assert(0 && "Unsupported mapper type");
	}

#if CPU_TEST
	PC = 0xC000;
	P = 0x24;
	SP = 0xFD;
	A = 0;
	X = 0;
	Y = 0;
#else
    P = 0x34;
	SP = 0xFD;
	A = 0;
	X = 0;
	Y = 0;
	PC = MAKE_SHORT(memory[0xFFFC], memory[0xFFFD]);
#endif
	printf("Starting at %X\n", PC);

    _log_opname[0] = '\0';
    _log_operands[0] = '\0';

    cpuClock = 0;
	cpuOperation = OP_NOP;
	cpuState = INST_FETCH_OPCODE;
	cpuSMFunc = 0;
	cpuNMI = false;
	cpuIRQ = false;
	cpuDMARequested = false;

	//Set PPU values
    ppu2005_2006FlipFlop = 0;
	ppuAddressMemory = 0;
	ppuScanline = 0;
	ppuRenderX = 0;
	ppuRenderY = -1;
	ppuScanlineClock = 0;
	ppuClock = 0;
	ppuSprite0InRangeInEval = false;

	apuTriangleChannel.enabled = 0;
	apuTriangleChannel.lengthCounter= 0;

	for (int i = 0; i < 2; ++i)
	{
		apuRectChannels[i].enabled = false;
		apuRectChannels[i].lengthCounter = 0;
	}

	apuNoiseChannel.enabled = false;
	apuNoiseChannel.lengthCounter = 0;
	apuFrameCounterFrame = 0;
}

unsigned char CPU::FetchOpcode()
{
	unsigned char opcode = get_byte(PC);

#if LOG_CPU
	if (cpu_log)
	{
		_log_operands[0] = '\0';
		_log_opname[0] = '\0';
		sprintf(_reg_values, "A:%02X X:%02X Y:%02X P:%02X SP:%02X", A, X, Y, P, SP);
		sprintf(_log_operands, "%04X  %02X ", PC, opcode);
	}
#endif

	++PC;

	if (unsupportedOpcodes[opcode] != 9)
	{
		char buf[128];
		sprintf(buf, "Unsupported opcode encountered $%X, PC=$%04X\n", opcode, PC);
		printf(buf);
		assert(0 && "Unsupported opcode");    // Crash on unsupported opcodes for now
	}

	return opcode;
}

unsigned char CPU::FetchOperand(bool incrementPC)
{
	unsigned char ret = get_byte(PC);
	log_operand_byte(ret);
	if (incrementPC)
		++PC;

	return ret;
}

void CPU::DecodeOpcode()
{
	DecodeOpcodeUtil(cpuOpcode, cpuAddressingMode, cpuOperation, cpuInstType);
	cpuOpArgCt = cpuAddressingMode & 3;
	cpuOperand1 = 0;
	cpuOperand2 = 0;

	// Specific state machines for special operations
	if (cpuOperation == OP_JSR)
	{
		SetSM(&CPU::SM_jsr);
	}
	else if (cpuOperation == OP_PHA || cpuOperation == OP_PHP)
	{
		PrintLogLine();
		SetSM(&CPU::SM_php_pha);
	}
	else if (cpuOperation == OP_PLA || cpuOperation == OP_PLP)
	{
		PrintLogLine();
		SetSM(&CPU::SM_plp_pla);
	}
	else if (cpuOperation == OP_JMP)
	{
		SetSM(&CPU::SM_jmp);
	}
	else if (cpuOperation == OP_JMP_IND)
	{
		SetSM(&CPU::SM_jmp_ind);
	}
	else if (cpuOperation == OP_BRK)
	{
		SetSM(&CPU::SM_brk);
	}
	else if (cpuOperation == OP_RTS)
	{
		PrintLogLine();
		SetSM(&CPU::SM_rts);
	}
	else if (cpuOperation == OP_RTI)
	{
		PrintLogLine();
		SetSM(&CPU::SM_rti);
	}
	else if ((cpuOpcode & 0x1F) == 0x10)
	{
		// Branch instructions
		SetSM(&CPU::SM_branch);
	}
	else if (cpuAddressingMode == AM_ZERO_PAGE_X_INDIRECT)
	{
		SetSM(&CPU::SM_zp_x_ind);
	}
	else if (cpuAddressingMode == AM_ZERO_PAGE_INDIRECT_Y)
	{
		SetSM(&CPU::SM_zp_ind_y);
	}
	else if (cpuAddressingMode == AM_ZERO_PAGE)
	{
		SetSM(&CPU::SM_zp);
	}
	else if (cpuAddressingMode == AM_ABSOLUTE)
	{
		SetSM(&CPU::SM_abs);
	}
	else if (cpuAddressingMode == AM_ABSOLUTE_X || cpuAddressingMode == AM_ABSOLUTE_Y)
	{
		SetSM(&CPU::SM_abs_indexed);
	}
	else if (cpuAddressingMode == AM_ZERO_PAGE_X || cpuAddressingMode == AM_ZERO_PAGE_Y)
	{
		SetSM(&CPU::SM_zp_indexed);
	}
	else
	{
		if (cpuOpArgCt == 0)
		{
			cpuState = INST_FETCH_OPCODE;
			PrintLogLine();
		}
		else if (cpuAddressingMode == AM_IMMEDIATE)
		{
			// Get first operand
			cpuOperand1 = FetchOperand(true);
			PrintLogLine();
			cpuValue = cpuOperand1;
			cpuState = INST_FETCH_OPCODE;
		}
	}
}

// Run a single CPU cycle
void CPU::clock()
{
	++cpuClock;

	switch (cpuState)
	{
	case INST_FETCH_OPCODE:
		// Execute buffered operation
		switch (cpuOperation)
		{
		case OP_LDX:
			load(REG_X, cpuValue);
			break;
		case OP_LDA:
			load(REG_A, cpuValue);
			break;
		case OP_LDY:
			load(REG_Y, cpuValue);
			break;
		case OP_CLV:
			set_flag(FLAG_OVERFLOW, 0);
			break;
		case OP_CLD:
			set_flag(FLAG_DECIMAL, 0);
			break;
		case OP_SED:
			set_flag(FLAG_DECIMAL, 1);
			break;
		case OP_SEC:
			set_flag(FLAG_CARRY, 1);
			break;
		case OP_CLC:
			set_flag(FLAG_CARRY, 0);
			break;
		case OP_SEI:
			set_flag(FLAG_INTERRUPT, 1);
			break;
		case OP_CLI:
			set_flag(FLAG_INTERRUPT, 0);
			break;
		case OP_BIT:
			bit(cpuValue);
			break;

		// bitwise
		case OP_AND:
			bitwise(BITWISE_AND, cpuValue);
			break;
		case OP_ORA:
			bitwise(BITWISE_OR, cpuValue);
			break;
		case OP_EOR:
			bitwise(BITWISE_XOR, cpuValue);
			break;

		case OP_CMP:
			compare(A, cpuValue);
			break;
		case OP_CPX:
			compare(X, cpuValue);
			break;
		case OP_CPY:
			compare(Y, cpuValue);
			break;

		case OP_ADC:
			add_subtract(ARITHMETIC_ADD, cpuValue);
			break;
		case OP_SBC:
			add_subtract(ARITHMETIC_SUBTRACT, cpuValue);
			break;
		case OP_INY:
			inc_decrement(REG_Y, 1);
			break;
		case OP_INX:
			inc_decrement(REG_X, 1);
			break;
		case OP_DEY:
			inc_decrement(REG_Y, -1);
			break;
		case OP_DEX:
			inc_decrement(REG_X, -1);
			break;
		case OP_TAY:
			Y = A;
			set_flag(FLAG_ZERO, Y == 0);
			set_flag(FLAG_NEGATIVE, static_cast<char>(Y) < 0);
			break;
		case OP_TAX:
			X = A;
			set_flag(FLAG_ZERO, X == 0);
			set_flag(FLAG_NEGATIVE, static_cast<char>(X) < 0);
			break;
		case OP_TYA:
			A = Y;
			set_flag(FLAG_ZERO, A == 0);
			set_flag(FLAG_NEGATIVE, static_cast<char>(A) < 0);
			break;
		case OP_TXA:
			A = X;
			set_flag(FLAG_ZERO, A == 0);
			set_flag(FLAG_NEGATIVE, static_cast<char>(A) < 0);
			break;
		case OP_TXS:
			SP = X;
			break;
		case OP_TSX:
			X = SP;
			set_flag(FLAG_ZERO, X == 0);
			set_flag(FLAG_NEGATIVE, static_cast<char>(X) < 0);
			break;
		case OP_LSR:
			if (cpuAddressingMode == AM_ACCUMULATOR)
				shift(REG_A, BIT_SHIFT_RIGHT, false);
			break;
		case OP_ASL:
			if (cpuAddressingMode == AM_ACCUMULATOR)
				shift(REG_A, BIT_SHIFT_LEFT, false);
			break;
		case OP_ROR:
			if (cpuAddressingMode == AM_ACCUMULATOR)
				shift(REG_A, BIT_SHIFT_RIGHT, true);
			break;
		case OP_ROL:
			if (cpuAddressingMode == AM_ACCUMULATOR)
				shift(REG_A, BIT_SHIFT_LEFT, true);
			break;
		}

		if (cpuNMI)
		{
			SetSM(&CPU::SM_NMI);
		}
		else if (cpuIRQ && !get_flag(FLAG_INTERRUPT))
		{
			SetSM(&CPU::SM_IRQ);
		}
		else if (cpuDMARequested)
			cpuState = INST_DMA_READ;
		else
		{
			cpuOpcode = FetchOpcode();
			cpuState = INST_DECODE_OPCODE;
		}
		break;

	case INST_DECODE_OPCODE:
		DecodeOpcode();
		break;

	case INST_SM_SUB:
		CALL_MEMBER_FN(*this, cpuSMFunc)();
		break;

	case INST_DMA_READ:
		cpuDMAValue = memory[cpuDMACPUAddress + cpuDMACount];
		cpuState = INST_DMA_WRITE;
		break;

	case INST_DMA_WRITE:
		sprMemory[cpuDMASprAddress++] = cpuDMAValue;
		if (++cpuDMACount == 256)
		{
			cpuDMARequested = false;
			cpuState = INST_FETCH_OPCODE;
			cpuOperation = OP_NOP;
		}
		else
			cpuState = INST_DMA_READ;
		break;
	}
}

void CPU::add_subtract(ARITH_MODE operation, char value)
{
    short sresult;
	unsigned short uresult;

    switch (operation)
    {
	case ARITHMETIC_ADD:
		sresult = (char)A + (char)value + get_flag(FLAG_CARRY);
		uresult = A + (unsigned char)value + get_flag(FLAG_CARRY);
        break;

    case ARITHMETIC_SUBTRACT:
        sresult = (char) A - (char) value - !get_flag(FLAG_CARRY);
		uresult = A + (unsigned char)(value ^ 0xFF) + get_flag(FLAG_CARRY);
        break;
    }

	A = static_cast<unsigned char>(sresult);

	set_flag(FLAG_CARRY, uresult & 0x100);
    set_flag(FLAG_OVERFLOW, sresult < -128 || sresult > 127);
	set_flag(FLAG_NEGATIVE, A & 0x80);
	set_flag(FLAG_ZERO, A == 0);
}

void CPU::compare(char left, char right)
{
	set_flag(FLAG_CARRY, (unsigned char)left >= (unsigned char)right);
	set_flag(FLAG_ZERO, left == right);
	set_flag(FLAG_NEGATIVE, (char)(left - right) < 0);
}

void CPU::bit(char value)
{
	set_flag(FLAG_ZERO, (A & value) == 0);
	set_flag(FLAG_OVERFLOW, value & 0x40);
	set_flag(FLAG_NEGATIVE, value & 0x80);
}

void CPU::interrupt()
{
}

void CPU::inc_decrement(LOCATION reg, int dir)
{
	char result = 0;
	switch (reg)
	{
	case REG_Y:
		Y += dir;
		result = Y;
		break;
	case REG_X:
		X += dir;
		result = X;
		break;
	case REG_MEMORY:
		cpuValue += dir;
		result = cpuValue;
		break;
	default:
		assert(0);
	}

	set_flag(FLAG_ZERO, result == 0);
	set_flag(FLAG_NEGATIVE, result < 0);
}

void CPU::test()
{
	load(REG_A, 65);

	set_flag(FLAG_CARRY, 1);
	assert(get_flag(FLAG_CARRY));

	shift(REG_A, BIT_SHIFT_LEFT, 1);
	assert(A == 131);
	assert(get_flag(FLAG_NEGATIVE));

	load(REG_A, 30);
	set_flag(FLAG_CARRY, 1);
	shift(REG_A, BIT_SHIFT_RIGHT, 1);
	assert(A == 143);

	set_flag(FLAG_CARRY, 1);

	load(REG_A , 1);
	shift(REG_A, BIT_SHIFT_RIGHT, 0);
	assert(get_flag(FLAG_ZERO));

	load(REG_A, 10);
	set_flag(FLAG_CARRY, 0);
	add_subtract(ARITHMETIC_ADD, 1);
	assert(A == 11);
	assert(!get_flag(FLAG_NEGATIVE));

	load(REG_A, 10);
	set_flag(FLAG_CARRY, 1);
	add_subtract(ARITHMETIC_SUBTRACT, 1);
	assert(A == 9);
	assert(!get_flag(FLAG_NEGATIVE));

	// -1 - (-1) == 0
	load(REG_A, -1);
	set_flag(FLAG_CARRY, 1);
	add_subtract(ARITHMETIC_SUBTRACT, 255);
	assert(static_cast<char>(A) == 0);
	assert(!get_flag(FLAG_OVERFLOW));

	// -1 - 1 == -2
	load(REG_A, -1);
	set_flag(FLAG_CARRY, 1);
	add_subtract(ARITHMETIC_SUBTRACT, 1);
	assert(static_cast<char>(A) == -2);
	assert(get_flag(FLAG_NEGATIVE));
	assert(!get_flag(FLAG_OVERFLOW));

	// -128 -1 == -129 (overflow) -> 127
	load(REG_A, -128);
	set_flag(FLAG_CARRY, 1);
	add_subtract(ARITHMETIC_SUBTRACT, 1);
	assert(get_flag(FLAG_OVERFLOW));
	assert(!get_flag(FLAG_NEGATIVE));
	assert(A == 127);

	// 64 + 64 == 128 (overflow) -> -128
	load(REG_A, 64);
	set_flag(FLAG_CARRY, 0);
	add_subtract(ARITHMETIC_ADD, 64);
	assert(get_flag(FLAG_OVERFLOW));
	assert(get_flag(FLAG_NEGATIVE));

	// Check carry for LDA #7F (127) ,ADC #80 (-128)
	load(REG_A, 0x7F);
	set_flag(FLAG_CARRY, 0);
	add_subtract(ARITHMETIC_ADD, 0x80);
	assert(!get_flag(FLAG_CARRY));
}

//Logging
void CPU::log_opname(const char *op)
{
#if LOG_CPU
    strcpy(_log_opname, op);
#endif
}

void CPU::log_operand_byte(unsigned char value)
{
#if LOG_CPU
	char buf[256];
	sprintf(buf, "%02X ", value);
	strcat(_log_operands, buf);
#endif
}

//Register functions. Handle mirroring
inline unsigned char CPU::get_byte(unsigned short initialAddress)
{
	unsigned short address = initialAddress;

#if !CPU_TEST
	// System memory mirroring
	if (address < 0x2000)
		return memory[address & 0xE7FF];

	// PPU register mirroring
	if (address >= 0x2000 && address <= 0x3FFF)
		address &= 0xE007;

    switch (address)
    {
//PPU
	case PPUSTATUS:
        {
            unsigned char status =
                ((ppuInVblank) ? 128 : 0) |
                ((sprSprite0Hit) ? 64 : 0) |
				((sprSpriteOverflow) ? 32 : 0);

            ppuInVblank = false;
			ppu2005_2006FlipFlop = 0;
            return status;
        }
        break;

	case OAMDATA:
        return sprMemory[sprAddress];

	case PPUDATA:
		return get_ppu_byte();

//APU status register
	case 0x4015:
		{
			//if-d nt21
			unsigned char status =
				((apuNoiseChannel.lengthCounter > 0) ? 8 : 0) |
				((apuTriangleChannel.lengthCounter > 0) ? 4 : 0) |
				((apuRectChannels[0].lengthCounter > 0) ? 1 : 0) |
				((apuRectChannels[1].lengthCounter > 0) ? 2 : 0);
			cpuIRQ = false;
			return status;
		}

//JOYSTICK
    case 0x4016:    //joypad #1
        return get_key_status(joyStrobeIndex++);
    }
#endif

	// WRAM/battery backed ram
	if (address >= 0x6000 && address <= 0x7FFF)
	{
		if (battPRGRAM)
			return battPRGRAM[address & 0x1FFF];
		if (WRAM)
			return WRAM[address & 0x1FFF];
		return 0xFF;
	}

    return memory[address];
}

int CPU::get_key_status(int index)
{
    // A, B, Select, Start, Up, Down, Left, Right.
    if (index >= 0 && index < 8)
        return joyStickStatus[index];
    return 0;
}

void CPU::put_byte(unsigned short initialAddress, unsigned char value)
{
	unsigned short address = initialAddress;

#if !CPU_TEST
//ROM Mappers
	// TODO: Use function pointer to avoid switch for different mapper types
    switch (romMapperType)
    {
	case MAPPER_UNROM:
        if (address & 0x8000) // $8000-$FFFF
			ChangePRGBank(0x8000, value, SIZE_16K);
        break;
	case MAPPER_CNROM:
        if (address & 0x8000)
			ChangeCHRBank(0, value, SIZE_8K);
        break;
    case MAPPER_87:
        if (address >= 0x6000 && address <= 0x7FFF)
        {
            int which = ((value & 2) >> 1) | ((value & 1) << 1);
			ChangeCHRBank(0, which, SIZE_8K);
        }
        break;
	case MAPPER_GNROM:
		if (address & 0x8000)
		{
			int chrRomBank = value & 3;
			int progRomBank = (value >> 4) & 3;

			ChangeCHRBank(0, chrRomBank, SIZE_8K);
			ChangePRGBank(0x8000, progRomBank, SIZE_32K);
		}

		break;
	case MAPPER_MMC1:
		if (address >= 0x8000 && address <= 0x9FFF)
		{
			// Control
			mmc1UpdateShiftRegister(value);
			if (mmc1WriteCount == 5)
			{
				mmc1PROGROMBankMode = (mmc1ShiftRegister & 12) >> 2;
				mmc1CHRROMBankMode = (mmc1ShiftRegister & 16) >> 4;
				mmc1Mirroring = (mmc1ShiftRegister & 3);

				if (mmc1Mirroring == 2)
					ppuNTMirroring = MIRRORING_VERTICAL;
				else if (mmc1Mirroring == 3)
					ppuNTMirroring = MIRRORING_HORIZONTAL;
				else if (mmc1Mirroring == 0)
					ppuNTMirroring = MIRRORING_1SCREEN_000;
				else if (mmc1Mirroring == 1)
					ppuNTMirroring = MIRRORING_1SCREEN_400;

				mmc1WriteCount = 0;
			}
		}
		else if (address >= 0xA000 && address <= 0xBFFF)
		{
			// CHR bank0
			mmc1UpdateShiftRegister(value);
			if (mmc1WriteCount == 5)
			{
				// (0: switch 8 KB at a time; 1: switch two separate 4 KB banks)
				if (mmc1CHRROMBankMode == 1)
					ChangeCHRBank(0, mmc1ShiftRegister, SIZE_4K);
				else
					ChangeCHRBank(0, (mmc1ShiftRegister & 0x1E) >> 1, SIZE_8K);
				mmc1WriteCount = 0;
			}
		}
		else if (address >= 0xC000 && address <= 0xDFFF)
		{
			// CHR bank1
			mmc1UpdateShiftRegister(value);
			if (mmc1WriteCount == 5)
			{
				if (mmc1CHRROMBankMode == 1)
					ChangeCHRBank(0x1000, mmc1ShiftRegister, SIZE_4K);
				mmc1WriteCount = 0;
			}
		}
		else if (address >= 0xE000 && address <= 0xFFFF)
		{
			// PRG bank select
			mmc1UpdateShiftRegister(value);
			if (mmc1WriteCount == 5)
			{
				unsigned char progROMBank = mmc1ShiftRegister & 0xF;

				if (mmc1PROGROMBankMode == 3)
				{
					// Fix last bank at $C000, modify $8000
					ChangePRGBank(0x8000, progROMBank, SIZE_16K);
					ChangePRGBank(0xC000, romHeader.prgRomSize - 1, SIZE_16K);
				}
				else if (mmc1PROGROMBankMode == 0 || mmc1PROGROMBankMode == 1)
				{
					// 32PRG ROM mode
					ChangePRGBank(0x8000, (progROMBank & 0xE) >> 1, SIZE_32K);
				}
				else if (mmc1PROGROMBankMode == 2)
				{
					// Fix first bank at $8000, modify $C000
					ChangePRGBank(0x8000, 0, SIZE_16K);
					ChangePRGBank(0xC000, progROMBank, SIZE_16K);
				}

				ramChipDisable = (mmc1ShiftRegister & 0x10) != 0;
				mmc1WriteCount = 0;
			}
		}
		break;

	case MAPPER_MMC3:
		if (address >= 0x8000 && address <= 0x9FFF)
		{
			if (!(address & 1))
			{
				mmc3BankRegister = value & 7;
				mmc3PRGROmBankMode = (value & 0x40) != 0;
				mmc3A12Inversion = (value & 0x80) ? 0x1000 : 0;
			}
			else
			{
				unsigned char bank = value;
				switch (mmc3BankRegister)
				{
				case 0:
					ChangeCHRBank(0x0000 ^ mmc3A12Inversion, (bank & 0xFE) >> 1, SIZE_2K);
					break;
				case 1:
					ChangeCHRBank(0x0800 ^ mmc3A12Inversion, (bank & 0xFE) >> 1, SIZE_2K);
					break;
				case 2:
					ChangeCHRBank(0x1000 ^ mmc3A12Inversion, bank, SIZE_1K);
					break;
				case 3:
					ChangeCHRBank(0x1400 ^ mmc3A12Inversion, bank, SIZE_1K);
					break;
				case 4:
					ChangeCHRBank(0x1800 ^ mmc3A12Inversion, bank, SIZE_1K);
					break;
				case 5:
					ChangeCHRBank(0x1C00 ^ mmc3A12Inversion, bank, SIZE_1K);
					break;
				case 6:
					if (mmc3PRGROmBankMode == 0) // change $8000, fixed $C000, 8KB PRGROM
					{
						ChangePRGBank(0x8000, bank, SIZE_8K);
						ChangePRGBank(0xC000, 2 * romHeader.prgRomSize - 2, SIZE_8K);
					}
					else // change $C000, fixed $8000, 8KB PRGROM
					{
						ChangePRGBank(0xC000, bank, SIZE_8K);
						ChangePRGBank(0x8000, 2 * romHeader.prgRomSize - 2, SIZE_8K);
					}
					break;
				case 7:
					ChangePRGBank(0xA000, bank, SIZE_8K);
					break;
				}
			}
		}
		else if (address >= 0xA000 && address <= 0xBFFF)
		{
			if (!(address & 1))
			{
				if (value & 1)
					ppuNTMirroring = MIRRORING_HORIZONTAL;
				else
					ppuNTMirroring = MIRRORING_VERTICAL;
			}
		}
		else if (address >= 0xC000 && address <= 0xDFFF)
		{
			if (address & 1)
			{
				// IRQ reload
				mmc3IRQCounterReload = true;
				mmc3IRQCounter = 0;
			}
			else
			{
				// IRQ latch
				mmc3IRQReloadValue = value;
				//printf("Reload Value %d\n", value);
			}
		}
		else if (address >= 0xE000 && address <= 0xFFFF)
		{
			if (address & 1) // MMC3 IRQ Enable
			{
				//if (!mmc3IRQEnabled)
				//	printf("Enable MMC3 IRQs\n");
				mmc3IRQEnabled = true;
			}
			else // IRQ Disable
			{
				//if (mmc3IRQEnabled)
				//	printf("Disabling MMC3 IRQs\n");
				mmc3IRQEnabled = false;
				cpuIRQ = false;
				mmc3IRQCounter = mmc3IRQCounterReload;
			}
		}
		break;
	case MAPPER_AXROM:
		if (address & 0x8000)
		{
			unsigned char prgBank = value & 7;
			ChangePRGBank(0x8000, prgBank, SIZE_32K);
			if (value & 0x10)
				ppuNTMirroring = MIRRORING_1SCREEN_400;
			else
				ppuNTMirroring = MIRRORING_1SCREEN_000;
		}
		break;
	case MAPPER_MMC2:
		if (address >= 0xA000 && address <= 0xAFFF)
		{
			unsigned char bank = value & 0xF;
			ChangePRGBank(0x8000, bank, SIZE_8K);
		}
		else if (address >= 0xB000 && address <= 0xBFFF && mmc2Latch == 0xFD)
		{
			ChangeCHRBank(0, value & 0x1F, SIZE_4K);
		}
		else if (address >= 0xC000 && address <= 0xCFFF && mmc2Latch == 0xFE)
		{
			ChangeCHRBank(0, value & 0x1F, SIZE_4K);
		}
		else if (address >= 0xD000 && address <= 0xDFFF && mmc2Latch == 0xFD)
		{
			ChangeCHRBank(0x1000, value & 0x1F, SIZE_4K);
		}
		else if (address >= 0xE000 && address <= 0xEFFF && mmc2Latch == 0xFE)
		{
			ChangeCHRBank(0x1000, value & 0x1F, SIZE_4K);
		}
		break;
    }

	// PPU register mirroring
	if (address >= 0x2000 && address <= 0x3FFF)
		address &= 0xE007;

	//CPU memory mirroring
    if (address < 0x2000)
		memory[address & 0xE7FF] = value;

	// REGISTERS
	// TODO: Move to PPU/APU
    switch(address)
    {
	case PPUCTRL:
		ppuAddressLatch &= ~(0x3 << 10);
		ppuAddressLatch |= (value & 3) << 10;
        break;

	case PPUSCROLL:
        if (ppu2005_2006FlipFlop == 0)
		{
			// fine x
			ppuFineX = value & 0x7;

			// coarse x
			ppuAddressLatch &= ~0x1F;
			ppuAddressLatch |= (value & 0xF8) >> 3;
		}
        else
		{
			// coarse y
			ppuAddressLatch &= ~0x3E0;
			ppuAddressLatch |= (value & 0xF8) << 2;

			// fine y
			ppuAddressLatch &= ~0x7000;
			ppuAddressLatch |= (value & 0x7) << 12;
		}

        ppu2005_2006FlipFlop = !ppu2005_2006FlipFlop;
        break;

	// PPU address register
	case PPUADDR:
        if (ppu2005_2006FlipFlop == 0)
		{
			ppuAddressLatch &= ~0x3F00;
			ppuAddressLatch |= (value & 0x3F) << 8;
			ppuAddressLatch &= ~(3 << 14);
		}
        else
		{
			//coarse x
			ppuAddressLatch &= ~0xFF;
			ppuAddressLatch |= value;
			if (!(ppuAddressMemory & 0x1000) && (ppuAddressLatch & 0x1000))
				mmc3ClockIRQCounter();
			ppuAddressMemory = ppuAddressLatch;
		}
		ppu2005_2006FlipFlop = !ppu2005_2006FlipFlop;
        break;

	case PPUDATA:
		put_ppu_byte(ppuAddressMemory, value);
		ppuUpdateAddress2007();
		break;

//SPRITE
	case OAMADDR:
        sprAddress = value;
        break;
	case OAMDATA:
        sprMemory[sprAddress++] = value;
        break;
	case OAMDMA:
		cpuDMARequested = true;
		cpuDMACPUAddress = value << 8;
		cpuDMACount = 0;
		cpuDMASprAddress = sprAddress;
        break;

//APU
	//Triangle registers
	case 0x4008:
		apuTriangleChannel.linearCounterReloadValue = 0;
		apuTriangleChannel.linearCounterReloadValue |= (value & 0x7F);
		apuTriangleChannel.lengthCounterHalt = (value & 0x80) != 0;
		apuTriangleChannel.controlFlag = apuTriangleChannel.lengthCounterHalt;
		//printf("4008 = %d %d\n", apuTriangleChannel.lengthCounterHalt, apuTriangleChannel.linearCounterReloadValue);
		break;

	case 0x400A:
		apuTriangleChannel.timerPeriod &= ~0xFF;
		apuTriangleChannel.timerPeriod |= value;
		//printf("400A = %d\n", apuTriangleChannel.timerPeriod);
		break;

	case 0x400B:
		apuTriangleChannel.timerPeriod &= ~0x700;
		apuTriangleChannel.timerPeriod |= (value & 7) << 8;
		if (apuTriangleChannel.enabled)
			apuTriangleChannel.lengthCounter = lengthCounterPeriodTable[(value & 0xF8) >> 3];
		apuTriangleChannel.linearCounterReload = true;
		//printf("400B = %d %d\n", apuTriangleChannel.timerPeriod, apuTriangleChannel.lengthCounter);
		break;

	//Square 1 registers
	case 0x4000: //Envelope Unit
		apuSetRectChannelEnvelope(0, value);
		break;
	case 0x4001: // Sweep Unit
		apuSetRectChannelSweep(0, value);
		break;
	case 0x4002: // Timer Low
		apuSetRectChannelTimerLow(0, value);
		break;
	case 0x4003: // Timer high, length counter
		apuSetRectChannelLengthCtr(0, value);
		break;

	//Square 2 registers
	case 0x4004:
		apuSetRectChannelEnvelope(1, value);
		break;
	case 0x4005:
		apuSetRectChannelSweep(1, value);
		break;
	case 0x4006:
		apuSetRectChannelTimerLow(1, value);
		break;
	case 0x4007:
		apuSetRectChannelLengthCtr(1, value);
		break;

	// Noise registers
	case 0x400C:
		apuNoiseChannel.lengthCounterHalt = (value & 0x20) != 0;
		apuNoiseChannel.envelope.loop = (value & 0x20) != 0;
		apuNoiseChannel.envelope.constantVol = (value & 0x10) != 0;
		apuNoiseChannel.envelope.volume = (value & 0xF);
		apuNoiseChannel.envelope.dividerPeriodReload = (value & 0xF);
		break;
	case 0x400E:
		apuNoiseChannelSetPeriod(value);
		break;
	case 0x400F:
		if (apuNoiseChannel.enabled)
			apuNoiseChannel.lengthCounter = lengthCounterPeriodTable[(value & 0xF8) >> 3];
		apuNoiseChannel.envelope.startFlag = true;
		break;

	case 0x4015: // APUSTATUS
		//printf("4015 = %X\n", value);
		apuRectChannels[0].enabled = (value & 1) != 0;
		apuRectChannels[1].enabled = (value & 2) != 0;
		apuTriangleChannel.enabled = (value & 4) != 0;
		apuNoiseChannel.enabled = (value & 8) != 0;

		if (!apuRectChannels[0].enabled)
			apuRectChannels[0].lengthCounter = 0;
		if (!apuRectChannels[1].enabled)
			apuRectChannels[1].lengthCounter = 0;
		if (!apuTriangleChannel.enabled)
			apuTriangleChannel.lengthCounter = 0;
		if (!apuNoiseChannel.enabled)
			apuNoiseChannel.lengthCounter = 0;
		break;

	case 0x4017:
		apuFrameCounterFrame = 0;
		if ((value & 0x80) != 0)
		{
			apuClockLengthCounters();
			apuClockFrameCounter();
		}
		break;

//JOYSTICK
    case 0x4016:    //joypad 1
        {
            //Writing 1 to this bit will record the state of each button
            //on the controller.
            //Writing 0 afterwards will allow the buttons to be
            //read back, one at a time.
            if (value & 1)
                joyStrobeIndex = 0;
        }
        break;
    }
#endif

	// TODO: Check whether we can really update the value at PRGROM or not
	if (address < 0x8000)
	{
		// WRAM/Battery backed ram write
		if (address >= 0x6000 && address <= 0x7FFF)
		{
			if (ramChipDisable)
				printf("CHIP DISABLED!\n");
			if (battPRGRAM && !ramChipDisable)
				battPRGRAM[address & 0x1FFF] = value;
			if (WRAM && !ramChipDisable)
				WRAM[address & 0x1FFF] = value;
			//memory[address] = value;
		}
		else
			memory[address] = value;
	}
}

static int clock_ctr = 0;

// Runs at PPU clock
void CPU::update()
{
	++clock_ctr;

	if (clock_ctr % 22325 == 0) //240 Hz
		apuClockFrameCounter();

	if (clock_ctr % 122 == 0) // 44.1 kHz
	{
		apuPutSoundSample();
	}

	if (resetCPU)
	{
		if (loadROM)
			load_rom(romFilename);
		reset();
		resetCPU = false;
		loadROM = false;
		return;
	}

	//assert(_CrtCheckMemory());
	ppuClockPPU();

	if (clock_ctr % 3 == 0)
	{
		clock();
		apuClockTriangleTimer();
	}

	if (clock_ctr % 6 == 0)
	{
		apuClockRectangleTimer();
		apuClockNoiseTimer();
	}
}

void decodeLatch(unsigned short value)
{
	printf("Coarse X: %d\n", (value & 0x1F));
	printf("Coarse Y: %d\n", (value & 0x3E0) >> 5);
	printf("Horiz NT: %d\n", (value & 0x400) >> 10);
	printf("Vert NT: %d\n",  (value & 0x800) >> 11);
	printf("Fine Y: %d\n",   (value & 0x7000) >> 12);
}

void CPU::PrintLogLine()
{
	if (!cpu_log) return;

#if LOG_CPU	
	// 28 char string for operands
	char buf[128];
	buf[0] = '\0';

	if (cpuAddressingMode == AM_ABSOLUTE)
	{
		unsigned short addr = MAKEWORD(cpuOperand1, cpuOperand2);

		if (cpuOperation == OP_JMP_IND)
		{
			unsigned char lowByte = memory[addr];
			unsigned char highByte = memory[MAKEWORD((addr + 1) & 0xFF, cpuOperand2)];

			sprintf(buf, "($%04X) = %04X", addr, MAKEWORD(lowByte, highByte));
		}
		else
		{
			sprintf(buf, "$%04X", addr, memory[addr]);

			if (cpuOperation != OP_JMP && cpuOperation != OP_JSR)
			{
				char buf2[128];
				sprintf(buf2, " = %02X", memory[addr]);
				strcat(buf, buf2);
			}
		}
	}
	else if (cpuAddressingMode == AM_IMMEDIATE)
	{
		sprintf(buf, "#$%02X", cpuOperand1);
	}
	else if (cpuAddressingMode == AM_ZERO_PAGE)
	{
		sprintf(buf, "$%02X = %02X", cpuOperand1, memory[cpuOperand1]);
	}
	else if (cpuAddressingMode == AM_RELATIVE)
	{
		sprintf(buf, "$%02X", PC + (char) cpuOperand1);
	}
	else if (cpuAddressingMode == AM_ACCUMULATOR)
	{
		sprintf(buf, "A");
	}
	else if (cpuAddressingMode == AM_ZERO_PAGE_X_INDIRECT)
	{
		unsigned char c = cpuOperand1 + X;
		unsigned short addr = (memory[(c + 1) &0xFF] << 8) | memory[c];
		unsigned char val = memory[addr];
		sprintf(buf, "($%02X,X) @ %02X = %04X = %02X", cpuOperand1, c, addr, val);
	}
	else if (cpuAddressingMode == AM_ZERO_PAGE_INDIRECT_Y)
	{
		unsigned short addr = memory[cpuOperand1] | memory[(cpuOperand1 + 1) & 0xFF] << 8;
		sprintf(buf, "($%02X),Y = %04X @ %04X = %02X", cpuOperand1, addr, (addr + Y) & 0xFFFF, memory[(addr + Y) & 0xFFFF]);
	}
	else if (cpuAddressingMode == AM_ABSOLUTE_X)
	{
		unsigned short addr = MAKEWORD(cpuOperand1, cpuOperand2);
		sprintf(buf, "$%04X,X @ %04X = %02X", addr, (addr + X) & 0xFFFF, memory[(addr + X) & 0xFFFF]);
	}
	else if (cpuAddressingMode == AM_ABSOLUTE_Y)
	{
		unsigned short addr = MAKEWORD(cpuOperand1, cpuOperand2);
		sprintf(buf, "$%04X,Y @ %04X = %02X", addr, (addr + Y) & 0xFFFF, memory[(addr + Y) & 0xFFFF]);
	}
	else if (cpuAddressingMode == AM_ZERO_PAGE_X)
	{
		unsigned short addr = MAKEWORD(cpuOperand1, cpuOperand2);
		sprintf(buf, "$%02X,X @ %02X = %02X", cpuOperand1, (addr + X) & 0xFF, memory[(addr + X) & 0xFF]);
	}
	else if (cpuAddressingMode == AM_ZERO_PAGE_Y)
	{
		unsigned short addr = MAKEWORD(cpuOperand1, cpuOperand2);
		sprintf(buf, "$%02X,Y @ %02X = %02X", cpuOperand1, (addr + Y) & 0xFF, memory[(addr + Y) & 0xFF]);
	}

	printf("%-16s", _log_operands);
	printf("%-4s", _log_opname);
	printf("%-28s", buf);
	printf("%s\n", _reg_values);
#endif
}

void CPU::SetSM(SM_Func func)
{
	cpuSMClock = 1;
	cpuState = INST_SM_SUB;
	cpuSMFunc = func;
	CALL_MEMBER_FN(*this, cpuSMFunc)();
}

void CPU::StoreRegister()
{
	if (cpuOperation == OP_STA)
		put_byte(cpuMemAddress, A);
	else if (cpuOperation == OP_STX)
		put_byte(cpuMemAddress, X);
	else if (cpuOperation == OP_STY)
		put_byte(cpuMemAddress, Y);
}

void CPU::PerformRMWOp()
{
	if (cpuOperation == OP_LSR)
		shift(REG_MEMORY, BIT_SHIFT_RIGHT, false);
	else if (cpuOperation == OP_ASL)
		shift(REG_MEMORY, BIT_SHIFT_LEFT, false);
	else if (cpuOperation == OP_ROR)
		shift(REG_MEMORY, BIT_SHIFT_RIGHT, true);
	else if (cpuOperation == OP_ROL)
		shift(REG_MEMORY, BIT_SHIFT_LEFT, true);
	else if (cpuOperation == OP_INC)
		inc_decrement(REG_MEMORY, 1);
	else if (cpuOperation == OP_DEC)
		inc_decrement(REG_MEMORY, -1);
}

void CPU::NMI()
{
	cpuNMI = true;
}

void CPU::mmc1UpdateShiftRegister(unsigned char value)
{
	if (value & 128)
	{
		mmc1WriteCount = 0;
		mmc1ShiftRegister = 0;
		mmc1PROGROMBankMode = 3;
		ChangePRGBank(0xC000, romHeader.prgRomSize - 1, SIZE_16K);
	}
	else
	{
		if (value & 1)
			mmc1ShiftRegister |= 1 << mmc1WriteCount;
		else
			mmc1ShiftRegister &= ~(1 << mmc1WriteCount);
		++mmc1WriteCount;
	}
}

void CPU::ChangePRGBank(unsigned short addr, unsigned int bank, unsigned int bankSize, unsigned int numBanks)
{
	if (!romPRGROM) return;
	int max = romHeader.prgRomSize;
	switch (bankSize)
	{
	case SIZE_1K:
		max <<= 4;
		break;
	case SIZE_2K:
		max <<= 3;
		break;
	case SIZE_4K:
		max <<= 2;
		break;
	case SIZE_8K:
		max <<= 1;
		break;
	case SIZE_16K:
		break;
	case SIZE_32K:
		max >>= 1;
		break;
	default:
		assert(0 && "Invalid bank size");
	}
	//printf("PRGCHANGE Bank Addr=%X, Size=%d, Num=%d, Bank=%d, Raw Bank=%d\n", addr, bankSize, numBanks, (bank & (max - 1)), bank);
	memcpy(memory + addr, romPRGROM + (bank & (max - 1)) * bankSize, bankSize * numBanks);
}

void CPU::ChangeCHRBank(unsigned short addr, unsigned int bank, unsigned int bankSize, unsigned int numBanks)
{	
	if (romCHRROM)
	{
		int max = romHeader.chrRomSize;
		switch (bankSize)
		{
		case SIZE_1K:
			max <<= 3;
			break;
		case SIZE_2K:
			max <<= 2;
			break;
		case SIZE_4K:
			max <<= 1;
			break;
		case SIZE_8K:
			break;
		default:
			assert(0 && "Invalid bank size");
		}
		memcpy(ppuMemory + addr, romCHRROM + (bank & (max - 1)) * bankSize, bankSize * numBanks);
	}
	else if (chrram)
	{
		int max = 1;
		switch (bankSize)
		{
		case SIZE_1K:
			max <<= 3;
			break;
		case SIZE_2K:
			max <<= 2;
			break;
		case SIZE_4K:
			max <<= 1;
			break;
		case SIZE_8K:
			break;
		default:
			assert(0 && "Invalid bank size");
		}
		memcpy(ppuMemory + addr, chrram + (bank & (max - 1)) * bankSize, bankSize * numBanks);
	}
}

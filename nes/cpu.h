#ifndef CPU_H_INCLUDED
#define CPU_H_INCLUDED

#include "config.h"
#include <vector>

#define MAKE_SHORT(lo, hi) ((lo) | ((hi) << 8))
#define GET_LOBYTE(s) ((s) & 0xFF)
#define GET_HIBYTE(s) (((s) & 0xFF00) >> 8)

struct INESHeader
{
	char signature[4];
	unsigned char prgRomSize;
	unsigned char chrRomSize;
	char flag6;
	char flag7;
	unsigned char prgRamSize;
	char flag9;
	char flag10;
	char flag11;
	char flag12;
	char flag13;
	char unused[2];
};

struct SpriteEval
{
    unsigned char tile;
    unsigned char x;
    unsigned char attribute;
    unsigned char rangeResult;
};

struct Divider
{
	unsigned int count;
	unsigned int period;
};

struct Envelope
{
	Divider divider;
	unsigned char counter;
	bool startFlag;
	bool constantVol;
	bool loop;
	unsigned char volume;
	unsigned char dividerPeriodReload;

	void Clock();
};

struct SpriteRender
{
	//Shift registers
    unsigned char tile0;	//contains the bitmap tile data for plane 0
    unsigned char tile1;	//contains the bitmap tile data for plane 1
    unsigned char x;
	bool		  background;
	unsigned char palette;	// already shifted by 2 bits
};

struct TriangleChannel
{
	// clocked by a 1.79MHz signal
	int timerCounter;
	int timerPeriod;

	//For now, consider linear counter to be the same as length counter, only more accurate.
	int linearCounter;
	int linearCounterReloadValue;
	bool linearCounterReload;

	//Triangle waveform, clocked by timer and controlled by length and linear counters
	int sequencerIndex;	// 0 to 31 then wraps around

	//If enabled, decrements lengthCounter by 1. Stays at 0 until reloaded.
	//If 0, silence channel
	int lengthCounter;

	bool enabled;
	bool lengthCounterHalt; // also controls linear counter
	bool controlFlag;
};

struct RectangleChannel
{
	bool enabled;
	Envelope envelope;

	// clocked by a 895kHz signal
	unsigned int timerCounter;
	unsigned int timerPeriod;

	bool lengthCounterHalt;
	unsigned int lengthCounter;

	Divider sweepUnitDivider;
	bool sweepUnitEnable;
	bool sweepUnitReload;
	bool sweepUnitNegate;
	unsigned char sweepUnitShiftCount;

	unsigned char sequencerPos;
	unsigned char duty;
};

struct NoiseChannel
{
	bool enabled;
	Envelope envelope;
	// clocked by a 895kHz signal
	unsigned int timerCounter;
	unsigned int timerPeriod;
	unsigned int lengthCounter;
	bool lengthCounterHalt;
	bool mode;
	unsigned short feedbackShiftReg = 1;
};

const int SIZE_1K = 1024;
const int SIZE_2K = 2048;
const int SIZE_4K = 4096;
const int SIZE_8K = 8192;
const int SIZE_16K = 16384;
const int SIZE_32K = 32768;

const int PPUCTRL	= 0x2000;  //0x2000
const int PPUMASK	= 0x2001;  //0x2001
const int PPUSTATUS	= 0x2002;  //0x2002
const int OAMADDR	= 0x2003;  //0x2003
const int OAMDATA	= 0x2004;  //0x2004
const int PPUSCROLL	= 0x2005;  //0x2005
const int PPUADDR	= 0x2006;  //0x2006
const int PPUDATA	= 0x2007;  //0x2007
const int OAMDMA	= 0x4014;  //0x4014

const int PRERENDER_SCANLINE = 20;
const int POSTRENDER_SCANLINE = 261;

enum CPU_STATE
{
	INST_FETCH_OPCODE,
	INST_DECODE_OPCODE,
	INST_DMA_READ,
	INST_DMA_WRITE,
	INST_SM_SUB,	// sub state machine
};

enum CPU_OPERATION
{
	OP_ADC = 500, // give a value higher than 0xFF so they wont clash with ops only having single opcodes
	OP_SBC,
	OP_ORA,
	OP_AND,
	OP_EOR,

	OP_STX,
	OP_STY,
	OP_STA,

	OP_LDX,
	OP_LDY,
	OP_LDA,

	OP_CMP,
	OP_ASL,
	OP_ROL,
	OP_LSR,
	OP_ROR,
	OP_DEC,
	OP_INC,

	OP_BIT,
	OP_JMP,
	OP_JMP_IND,
	OP_CPY,
	OP_CPX,

	OP_JSR,

	OP_BPL = 0x10,
	OP_BMI = 0x30,
	OP_BVC = 0x50,
	OP_BVS = 0x70,
	OP_BCC = 0x90,
	OP_BCS = 0xB0,
	OP_BNE = 0xD0,
	OP_BEQ = 0xF0,

	OP_PHP = 0x08,
	OP_PLP = 0x28,
	OP_PHA = 0x48,
	OP_PLA = 0x68,
	OP_DEY = 0x88,
	OP_TAY = 0xA8,
	OP_INY = 0xC8,
	OP_INX = 0xE8,
	OP_CLC = 0x18,
	OP_SEC = 0x38,
	OP_CLI = 0x58,
	OP_SEI = 0x78,
	OP_TYA = 0x98,
	OP_CLV = 0xB8,
	OP_CLD = 0xD8,
	OP_SED = 0xF8,
	OP_TXA = 0x8A,
	OP_TXS = 0x9A,
	OP_TAX = 0xAA,
	OP_TSX = 0xBA,
	OP_DEX = 0xCA,
	OP_NOP = 0xEA,
	OP_RTS = 0x60,
	OP_BRK = 0x00,
	OP_RTI = 0x40,
};

enum ADDRESSING_MODE
{
	AM_IMMEDIATE = 0x01,				// 1 arg
	AM_ZERO_PAGE = 0x11,				// 1 arg
	AM_ZERO_PAGE_X = 0x21,				// 1 arg
	AM_ZERO_PAGE_Y = 0x31,				// 1 arg
	AM_RELATIVE = 0x41,					// 1 arg
	AM_ABSOLUTE = 0x52,					// 2 args
	AM_ZERO_PAGE_X_INDIRECT = 0x61,		// 1 arg
	AM_ZERO_PAGE_INDIRECT_Y = 0x71,		// 1 arg
	AM_ABSOLUTE_X = 0x82,				// 2 args
	AM_ABSOLUTE_Y = 0x92,				// 2 args
	AM_IMPLIED = 0xA0,					// 0 arg
	AM_ACCUMULATOR = 0xB0				// 0 arg
};

enum LOCATION
{
	REG_A = 0,
	REG_X = 1,
	REG_Y = 2,
	REG_MEMORY = 3
};

enum ARITH_MODE
{
	ARITHMETIC_ADD = 0,
	ARITHMETIC_SUBTRACT = 1
};

enum BITWISE_OP
{
	BITWISE_OR = 0,
	BITWISE_AND = 1,
	BITWISE_XOR = 2
};

enum SHIFT_DIRECTION
{
	BIT_SHIFT_LEFT = 0,
	BIT_SHIFT_RIGHT = 1
};

enum INSTRUCTION_TYPE
{
	IT_READ = 1,
	IT_WRITE = 2,
	IT_RMW = IT_READ | IT_WRITE,
};

enum FLAG
{
	FLAG_NEGATIVE = 1 << 7,
	FLAG_OVERFLOW = 1 << 6,
	FLAG_BREAK = 1 << 4,
	FLAG_DECIMAL = 1 << 3,
	FLAG_INTERRUPT = 1 << 2,
	FLAG_ZERO = 1 << 1,
	FLAG_CARRY = 1 << 0
};

const int MAPPER_NROM = 0;
const int MAPPER_MMC1 = 1;
const int MAPPER_MMC2 = 9;
const int MAPPER_MMC3 = 4;
const int MAPPER_UNROM = 2;
const int MAPPER_CNROM = 3;
const int MAPPER_AXROM = 7;
const int MAPPER_87 = 87;
const int MAPPER_GNROM = 66;
const int MAPPER_185 = 185;

const int MIRRORING_VERTICAL = 0;
const int MIRRORING_HORIZONTAL = 1;
const int MIRRORING_1SCREEN_000 = 2;
const int MIRRORING_1SCREEN_400 = 3;
const int MIRRORING_4SCREEN = 4;

#if LOG_CPU
#define LOG(fmt, ...) printf(fmt, __VA_ARGS__)
#else
#define LOG(fmt, ...) ((void)0)
#endif

#define CALL_MEMBER_FN(object,ptrToMember)  ((object).*(ptrToMember))

struct CPU
{
	static const int MEMORY_SIZE = 0x10000;

	unsigned short PC;					// Program counter
	unsigned char  A;					// Accumulator
	unsigned char  X;					// X index register
	unsigned char  Y;					// Y index register
	unsigned char  P;					// Status flags
	unsigned char  SP;					// Stack pointer
	unsigned char  *memory;
	std::vector<unsigned char> cpuMemVec;

	CPU();
	~CPU();

    void update();

	void clock();

	void DecodeOpcodeUtil(unsigned char opcode, ADDRESSING_MODE &addrMode, CPU_OPERATION &baseOp, INSTRUCTION_TYPE &instType);

	void set_flag(FLAG flag, int value);
	int  get_flag(FLAG flag);

	void shift(LOCATION reg, SHIFT_DIRECTION dir, bool rotate);
	void inc_decrement(LOCATION reg, int dir);

	void load(LOCATION reg, int value);
	void bitwise(BITWISE_OP operation, int value);
	void add_subtract(ARITH_MODE operation, char value);
	void compare(char left, char right);
	void bit(char value);

	void test();
	void interrupt();

	void load_rom(char *filename);
	unsigned char FetchOpcode();
	unsigned char FetchOperand(bool incrementPC);
	void DecodeOpcode();

	// Always decrements SP after push
	void push_byte(unsigned char byte);

	// Does not increment SP before pull
	unsigned char pull_byte();

    // CPU memory access functions
    unsigned char get_byte(unsigned short address);
    void put_byte(unsigned short address, unsigned char value);

	bool			resetCPU;
	bool			loadROM;
	char			*romFilename;

    //cpu
	bool			cpuNMI;
	bool			cpuIRQ;
	unsigned int	cpuClock;
	unsigned char	cpuOpcode;
	unsigned char	cpuOperand1;
	unsigned char	cpuOperand2;

	unsigned char	cpuOpArgCt;
	ADDRESSING_MODE cpuAddressingMode;
	INSTRUCTION_TYPE cpuInstType;
	CPU_OPERATION	cpuOperation;
	CPU_STATE		cpuState;
	unsigned short  cpuNewPC;

	bool			ramChipDisable;
	unsigned short  cpuMemAddress;
	unsigned short	cpuNewMemAddress; // for checking whether to call extra cycle
	unsigned char   cpuValue;

	// state machine functions
	typedef void (CPU::*SM_Func)();

	SM_Func			cpuSMFunc;
	void StoreRegister();
	void PerformRMWOp();

	void SetSM(SM_Func func);
	void SM_NMI();
	void SM_IRQ();
	void SM_brk();
	void SM_abs();
	void SM_abs_indexed();
	void SM_zp_indexed();
	void SM_zp_x_ind();
	void SM_zp_ind_y();
	void SM_zp();
	void SM_jmp();
	void SM_jmp_ind();
	void SM_jsr();
	void SM_rts();
	void SM_rti();
	void SM_php_pha();
	void SM_plp_pla();
	void SM_branch();

	unsigned char   cpuSMClock;	// instruction clock (1-based, starts with opcode fetch)

	// Assume 8k for now
	unsigned char *battPRGRAM;
	std::vector<unsigned char> battPRGRAMVec;
	unsigned char *WRAM;
	std::vector<unsigned char> WRAMVec;

////////////////////////////////////////////// DMA //////////////////////////////////////////////
    bool           cpuDMARequested;
    unsigned short cpuDMACPUAddress;
	unsigned char  cpuDMASprAddress;
	unsigned short cpuDMACount;
	unsigned char  cpuDMAValue;

////////////////////////////////////////////// PPU //////////////////////////////////////////////
    unsigned int ppuClock;					//total clock count (resets per frame)
	unsigned int ppuScanlineClock;			//clock count (resets per scanline)

	// 0-19   : VINT (v-blank)
	// 20     : DUMMY scanline (pre-render scanline)
	// 21-260 : RENDER
	// 261    : BLANK (post-render scanline, does nothing)
    unsigned short ppuScanline;
	unsigned char *chrram;
	unsigned int chrramsize;
	std::vector<unsigned char> chrRamVec;
    unsigned char *ppuMemory;
	std::vector<unsigned char> ppuMemVec;
	unsigned int   ppuUnivBGColor;

    //PPUSCROLL/PPUADDR
    unsigned short ppu2005_2006FlipFlop; // 0 = MSB, 1 = LSB
    bool		   ppuInVblank;
    unsigned short ppuNTMirroring;
    unsigned char  ppuByteBuffer;

	unsigned char  ppuObjColor;
	unsigned char  ppuObjIdx;
	bool		   ppuObjFG;
    //Scroll registers
	unsigned short ppuAddressMemory;		// register (loopy_v)
	unsigned short ppuAddressLatch;			// latch	(loopy_t)
    unsigned char  ppuFineX;

	//Latches for NT fetching
    unsigned char  ppuRenderNTTile;
    unsigned char  ppuRenderAttributeLatch;
	unsigned char  ppuPatternLatch;

    unsigned short ppuRenderX;
    unsigned short ppuRenderY;

	unsigned short ppuNTShiftRegister[2];
	unsigned short ppuRenderAttributeShiftRegister[2];

    unsigned char  ppuBackgroundPaletteIndex;
	unsigned char  ppuSpritePaletteIndex;

	// sprite 0 flag
	bool		   ppuSprite0InRangeInEval;
	bool		   ppuSprite0InRangeInRender;

    void ppuIncrementCoarseX();
    void ppuIncrementFineY();
	void ppuClockPPU();
	void ppuDoPerScanlineClock();

	unsigned char get_ppu_byte();
	unsigned short get_ciram_address(unsigned short ppuaddress);
    void put_ppu_byte(unsigned short address, unsigned char value);
	void ppuUpdateAddress2007();

	void ppuFetchBackground();
	void ppuComputeWinningPixel();
	void ppuRenderPixel();
	void NMI();

////////////////////////////////////////////// JOYSTICK ////////////////////////////////////////////
    unsigned char joyStrobeIndex;
    unsigned char joyStrobe;
    int get_key_status(int index);

////////////////////////////////////////////// SPRITE //////////////////////////////////////////////
    unsigned char *sprMemory;
	std::vector<unsigned char> sprMemVec;
    SpriteEval sprEvalMemory[8];
    SpriteRender sprRenderMemory[8];
    unsigned char sprAddress;
    unsigned char sprEvalIndex;				// Contains the number of in-range sprites
    unsigned char sprEvalToRenderIndex;
    bool sprSprite0Hit;
	bool sprSpriteOverflow;

    void sprEvalSprites();
	void sprFetchSprites();

////////////////////////////////////////////// APU /////////////////////////////////////////////////
	void apuClockFrameCounter();	//runs at a fairly low frequency (240Hz)
	unsigned char apuFrameCounterFrame;
	void apuClockLengthCounters();
	void apuClockSweepUnits();
	void apuClockEnvelopesAndLinearCounter();
	void apuClockTriangleTimer();
	void apuClockRectangleTimer();
	void apuClockNoiseTimer();
	void apuClockRectangleChannels();
	void apuPutSoundSample();

	void apuSetRectChannelEnvelope(int index, unsigned char value);
	void apuSetRectChannelSweep(int index, unsigned char value);
	void apuSetRectChannelTimerLow(int index, unsigned char value);
	void apuSetRectChannelLengthCtr(int index, unsigned char value);
	void apuNoiseChannelSetPeriod(unsigned char value);

	TriangleChannel		apuTriangleChannel;
	RectangleChannel	apuRectChannels[2];
	NoiseChannel		apuNoiseChannel;
////////////////////////////////////////////// ROM /////////////////////////////////////////////////
    unsigned char romMapperType;
	unsigned char *romPRGROM;
	std::vector<unsigned char> romPRGMROMVec;
	unsigned char *romCHRROM;
	std::vector<unsigned char> romCHRROMVec;
	INESHeader romHeader;
	bool	nes2_0Header;
	unsigned int wramSize;
	unsigned int batterySize;

	void ChangePRGBank(unsigned short addr, unsigned int bank, unsigned int bankSize, unsigned int numBanks = 1);
	void ChangeCHRBank(unsigned short addr, unsigned int bank, unsigned int bankSize, unsigned int numBanks = 1);

////////////////////////////////////////////// MISC ////////////////////////////////////////////////
	int ppuMemAccessCountFromScanlineClock();
    //logging
    void log_opname(const char *name);
    void log_operand_byte(unsigned char value);
	bool IsSupportedMapper(int mapper);
	void reset();
	void DrawNameTable(int x, int y, unsigned short addr);
	bool debugDrawNT;

////////////////////////////////////////////// MAPPER-SPECIFIC ////////////////////////////////////
	unsigned char mmc1WriteCount;
	unsigned char mmc1ShiftRegister;
	unsigned char mmc1PROGROMBankMode;
	unsigned char mmc1CHRROMBankMode;
	unsigned char mmc1Mirroring;
	void mmc1UpdateShiftRegister(unsigned char value);

	unsigned char mmc3BankRegister;
	unsigned char mmc3PRGROmBankMode;
	unsigned short mmc3A12Inversion;
	unsigned char mmc3IRQReloadValue;
	unsigned char mmc3IRQCounter;
	bool mmc3IRQCounterReload;
	bool mmc3IRQEnabled;
	void mmc3ClockIRQCounter();

	unsigned char mmc2Latch = 0xFE;

	void PrintLogLine();
};

void put_sound_sample(int val);
extern bool joyStickStatus[];

#endif

// Disassembler.cpp : Defines the entry point for the console application.
//

#include "stdafx.h"
#include <vector>

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

ADDRESSING_MODE cpuAddressingMode;
char _log_operands[1024];
char _log_opname[1024];
unsigned char cpuOperand1;
unsigned char cpuOperand2;
unsigned short startAddress = 0;

static const char *opcodeNames[] = {
	"BRK", "ORA", "", "", "", "ORA", "ASL", "", "PHP", "ORA", "ASL", "", "", "ORA", "ASL", "",
	"BPL", "ORA", "", "", "", "ORA", "ASL", "", "CLC", "ORA", "", "", "", "ORA", "ASL", "",
	"JSR", "AND", "", "", "BIT", "AND", "ROL", "", "PLP", "AND", "ROL", "", "BIT", "AND", "ROL", "",
	"BMI", "AND", "", "", "", "AND", "ROL", "", "SEC", "AND", "", "", "", "AND", "ROL", "",
	"RTI", "EOR", "", "", "", "EOR", "LSR", "", "PHA", "EOR", "LSR", "", "JMP", "EOR", "LSR", "",
	"BVC", "EOR", "", "", "", "EOR", "LSR", "", "CLI", "EOR", "", "", "", "EOR", "LSR", "",
	"RTS", "ADC", "", "", "", "ADC", "ROR", "", "PLA", "ADC", "ROR", "", "JMP", "ADC", "ROR", "",
	"BVS", "ADC", "", "", "", "ADC", "ROR", "", "SEI", "ADC", "", "", "", "ADC", "ROR", "",
	"", "STA", "", "", "STY", "STA", "STX", "", "DEY", "", "TXA", "", "STY", "STA", "STX", "",
	"BCC", "STA", "", "", "STY", "STA", "STX", "", "TYA", "STA", "TXS", "", "", "STA", "", "",
	"LDY", "LDA", "LDX", "", "LDY", "LDA", "LDX", "", "TAY", "LDA", "TAX", "", "LDY", "LDA", "LDX", "",
	"BCS", "LDA", "", "", "LDY", "LDA", "LDX", "", "CLV", "LDA", "TSX", "", "LDY", "LDA", "LDX", "",
	"CPY", "CMP", "", "", "CPY", "CMP", "DEC", "", "INY", "CMP", "DEX", "", "CPY", "CMP", "DEC", "",
	"BNE", "CMP", "", "", "", "CMP", "DEC", "", "CLD", "CMP", "", "", "", "CMP", "DEC", "",
	"CPX", "SBC", "", "", "CPX", "SBC", "INC", "", "INX", "SBC", "NOP", "", "CPX", "SBC", "INC", "",
	"BEQ", "SBC", "", "", "", "SBC", "INC", "", "SED", "SBC", "", "", "", "SBC", "INC", "",
};

void log_opname(const char *op)
{
	strcpy(_log_opname, op);
}

void log_operand_byte(unsigned char value)
{
	char buf[256];
	sprintf(buf, "%02X ", value);
	strcat(_log_operands, buf);
}

// Decodes the addressing mode and the base function of that opcode
// readOp are opcodes that will be executed on next instruction fetch
// ADC, AND, CMP, EOR, LDA, LDX, LDY, ORA, SBC, NOP, BIT
void DecodeOpcode(unsigned char opcode, ADDRESSING_MODE &addrMode, CPU_OPERATION &baseOp)
{
	// 0xaaabbbcc
	int cc = opcode & 3;
	int aaa = (opcode & 0xE0) >> 5;
	int bbb = (opcode & 0x1C) >> 2;

	if (cc == 1)
	{
		switch (bbb)
		{
		case 0: // (zp,x)
			addrMode = AM_ZERO_PAGE_X_INDIRECT;
			break;
		case 1: //zp
			addrMode = AM_ZERO_PAGE;
			break;
		case 2: //#
			addrMode = AM_IMMEDIATE;
			break;
		case 3: //abs
			addrMode = AM_ABSOLUTE;
			break;
		case 4: //(zp),y
			addrMode = AM_ZERO_PAGE_INDIRECT_Y;
			break;
		case 5: //zp,x
			addrMode = AM_ZERO_PAGE_X;
			break;
		case 6: //abs,y
			addrMode = AM_ABSOLUTE_Y;
			break;
		case 7: //abs,x
			addrMode = AM_ABSOLUTE_X;
			break;
		}

		switch (aaa)
		{
		case 0:	//ORA
			log_opname("ORA");
			baseOp = OP_ORA;
			break;
		case 1:	 //AND
			log_opname("AND");
			baseOp = OP_AND;
			break;
		case 2:	//EOR
			log_opname("EOR");
			baseOp = OP_EOR;
			break;
		case 3:	//ADC
			log_opname("ADC");
			baseOp = OP_ADC;
			break;
		case 4:	//STA
			log_opname("STA");
			baseOp = OP_STA;
			break;
		case 5:	//LDA
			log_opname("LDA");
			baseOp = OP_LDA;
			break;
		case 6:	//CMP
			log_opname("CMP");
			baseOp = OP_CMP;
			break;
		case 7:	//SBC
			log_opname("SBC");
			baseOp = OP_SBC;
			break;
		}
	}
	else if (cc == 2)
	{
		switch (bbb)
		{
		case 0: //#
			addrMode = AM_IMMEDIATE;
			break;
		case 1: //zp
			addrMode = AM_ZERO_PAGE;
			break;
		case 2: //accumulator
			addrMode = AM_ACCUMULATOR;
			break;
		case 3: //abs
			addrMode = AM_ABSOLUTE;
			break;
		case 5: //zp,x
			//LDX zp,x becomes LDX zp,y
			//STX zp,x becomes STX zp, y
			if (aaa == 5 || aaa == 4)
				addrMode = AM_ZERO_PAGE_Y;
			else
				addrMode = AM_ZERO_PAGE_X;
			break;
		case 7: //abs,x
			//LDX a,x becomes LDX a,y
			//STX a,x becomes STX a,y
			if (aaa == 5 || aaa == 4)
				addrMode = AM_ABSOLUTE_Y;
			else
				addrMode = AM_ABSOLUTE_X;
			break;
		}

		switch (aaa)
		{
		case 0: //ASL
			log_opname("ASL");
			baseOp = OP_ASL;
			break;
		case 1: //ROL
			log_opname("ROL");
			baseOp = OP_ROL;
			break;
		case 2: //LSR
			log_opname("LSR");
			baseOp = OP_LSR;
			break;
		case 3: //ROR
			log_opname("ROR");
			baseOp = OP_ROR;
			break;
		case 4: //STX
			log_opname("STX");
			baseOp = OP_STX;
			break;
		case 5: //LDX
			log_opname("LDX");
			baseOp = OP_LDX;
			break;
		case 6: //DEC
			log_opname("DEC");
			baseOp = OP_DEC;
			break;
		case 7:	//INC
			log_opname("INC");
			baseOp = OP_INC;
			break;
		}
	}
	else if (cc == 0)
	{
		switch (bbb)
		{
		case 0: //#
			addrMode = AM_IMMEDIATE;
			break;
		case 1: //zp
			addrMode = AM_ZERO_PAGE;
			break;
		case 3: //abs
			addrMode = AM_ABSOLUTE;
			break;
		case 5: //zp,x
			addrMode = AM_ZERO_PAGE_X;
			break;
		case 7: //abs,x
			addrMode = AM_ABSOLUTE_X;
			break;
		}

		switch (aaa)
		{
		case 1: //BIT
			log_opname("BIT");
			baseOp = OP_BIT;
			break;
		case 2: //JMP abs
			log_opname("JMP");
			baseOp = OP_JMP;
			break;
		case 3: //JMP(abs)
			log_opname("JMP");
			baseOp = OP_JMP_IND;
			break;
		case 4: //STY
			log_opname("STY");
			baseOp = OP_STY;
			break;
		case 5: //LDY
			log_opname("LDY");
			baseOp = OP_LDY;
			break;
		case 6: //CPY
			log_opname("CPY");
			baseOp = OP_CPY;
			break;
		case 7: //CPX
			log_opname("CPX");
			baseOp = OP_CPX;
			break;
		}
	}

	if ((opcode & 0x1F) == 0x10)
	{
		// branch ops
		log_opname(opcodeNames[opcode]);
		baseOp = static_cast<CPU_OPERATION>(opcode);
		addrMode = AM_RELATIVE;
	}

	// Single addressing mode ops
	switch (opcode)
	{
	case OP_PHP:
	case OP_PLP:
	case OP_PHA:
	case OP_PLA:
	case OP_DEY:
	case OP_TAY:
	case OP_INY:
	case OP_INX:
	case OP_CLC:
	case OP_SEC:
	case OP_CLI:
	case OP_SEI:
	case OP_TYA:
	case OP_CLV:
	case OP_CLD:
	case OP_SED:
	case OP_TXA:
	case OP_TXS:
	case OP_TAX:
	case OP_TSX:
	case OP_DEX:
	case OP_NOP:
	case OP_RTS:
	case OP_BRK:
	case OP_RTI:
		log_opname(opcodeNames[opcode]);
		baseOp = static_cast<CPU_OPERATION>(opcode);
		addrMode = AM_IMPLIED;
		break;

	case 0x20: //JSR abs
		log_opname(opcodeNames[opcode]);
		baseOp = OP_JSR;
		addrMode = AM_ABSOLUTE;
		break;
	}
}

unsigned short MakeWORD(unsigned char hi, unsigned char lo)
{
	return (hi << 8) | lo;
}

void PrintLogLine(int pos, CPU_OPERATION operation)
{
	printf("%-7s", _log_operands);
	printf("%-4s", _log_opname);

	// 28 char string for operands
	char buf[128];
	buf[0] = '\0';

	if (cpuAddressingMode == AM_ABSOLUTE)
	{
		unsigned short addr = MakeWORD(cpuOperand2, cpuOperand1);

		if (operation == OP_JMP_IND)
		{
			sprintf(buf, "($%04X)", addr);
		}
		else
		{
			sprintf(buf, "$%04X", addr);
		}
	}
	else if (cpuAddressingMode == AM_IMMEDIATE)
	{
		sprintf(buf, "#$%02X", cpuOperand1);
	}
	else if (cpuAddressingMode == AM_ZERO_PAGE)
	{
		sprintf(buf, "$%02X", cpuOperand1);
	}
	else if (cpuAddressingMode == AM_RELATIVE)
	{
		sprintf(buf, "%04X", pos + (char)cpuOperand1 + startAddress);
	}
	else if (cpuAddressingMode == AM_ACCUMULATOR)
	{
		sprintf(buf, "A");
	}
	else if (cpuAddressingMode == AM_ZERO_PAGE_X_INDIRECT)
	{
		sprintf(buf, "($%02X,X)", cpuOperand1);
	}
	else if (cpuAddressingMode == AM_ZERO_PAGE_INDIRECT_Y)
	{
		sprintf(buf, "($%02X),Y", cpuOperand1);
	}
	else if (cpuAddressingMode == AM_ABSOLUTE_X)
	{
		unsigned short addr = MakeWORD(cpuOperand2, cpuOperand1);
		sprintf(buf, "$%04X,X", addr);
	}
	else if (cpuAddressingMode == AM_ABSOLUTE_Y)
	{
		unsigned short addr = MakeWORD(cpuOperand2, cpuOperand1);
		sprintf(buf, "$%04X,Y", addr);
	}
	else if (cpuAddressingMode == AM_ZERO_PAGE_X)
	{
		sprintf(buf, "$%02X,X", cpuOperand1);
	}
	else if (cpuAddressingMode == AM_ZERO_PAGE_Y)
	{
		sprintf(buf, "$%02X,Y", cpuOperand1);
	}

	printf("%s\n", buf);
}

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
	char unused[5];
};

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

#define FILENAME _T("C:\\Users\\jay\\Desktop\\testroms\\make\\a.nes")
//#define FILENAME "C:\\Users\\jay\\Desktop\\testroms\\Metroid (USA).nes"

int _tmain(int argc, _TCHAR* argv[])
{
	if (argc < 2)
		return 1;
	
	FILE *f = _tfopen(argv[1], _T("rb"));
	//FILE *f = _tfopen(FILENAME, _T("rb"));

	if (!f)
		return 1;

	INESHeader header;
	fread(&header, sizeof(header), 1, f);

	if (header.prgRomSize == 0)
		return 1;

	std::vector<unsigned char> code;
	code.resize(header.prgRomSize * 16384);
	fread(&code[0], 1, code.size(), f);
	fclose(f);

	unsigned short *s = reinterpret_cast<unsigned short*>(&code[code.size() - 6]);

	if (header.prgRomSize == 1)
		startAddress = 0xC000;
	else if (header.prgRomSize == 2)
		startAddress = 0x8000;

	printf("Code size %X\n", code.size());
	printf("NMI   %04X\n", *s);
	++s;

	printf("RESET %04X\n", *s);
	++s;

	printf("IRQ   %04X\n", *s);

	unsigned int pos = 0;

	for (;;)
	{
		if (pos == code.size())
			break;

		unsigned char opcode = code[pos];

		if (unsupportedOpcodes[opcode] != 9)
		{
			//printf("%04X ", pos);
			//printf("%02X ???\n", opcode);
			++pos;
			continue;
		}

		CPU_OPERATION baseOp;

		_log_operands[0] = '\0';
		_log_opname[0] = '\0';

		DecodeOpcode(opcode, cpuAddressingMode, baseOp);
		printf("%04X ", pos + startAddress);
		printf("%02X ", opcode);

		++pos;
		if (pos == code.size())
			break;

		int operandCt = cpuAddressingMode & 3;

		if (operandCt >= 1)
		{
			cpuOperand1 = code[pos++];
			log_operand_byte(cpuOperand1);
		}

		if (operandCt == 2)
		{
			cpuOperand2 = code[pos++];
			log_operand_byte(cpuOperand2);
		}

		PrintLogLine(pos, baseOp);
	}

	

	return 0;
}


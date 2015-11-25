#include "cpu.h"

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

// Decodes the addressing mode and the base function of that opcode
// readOp are opcodes that will be executed on next instruction fetch
// ADC, AND, CMP, EOR, LDA, LDX, LDY, ORA, SBC, NOP, BIT
void CPU::DecodeOpcodeUtil(unsigned char opcode, ADDRESSING_MODE &addrMode, CPU_OPERATION &baseOp, INSTRUCTION_TYPE &instType)
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
			instType = IT_READ;
			break;
		case 1:	 //AND
			log_opname("AND");
			baseOp = OP_AND;
			instType = IT_READ;
			break;
		case 2:	//EOR
			log_opname("EOR");
			baseOp = OP_EOR;
			instType = IT_READ;
			break;
		case 3:	//ADC
			log_opname("ADC");
			baseOp = OP_ADC;
			instType = IT_READ;
			break;
		case 4:	//STA
			log_opname("STA");
			baseOp = OP_STA;
			instType = IT_WRITE;
			break;
		case 5:	//LDA
			log_opname("LDA");
			baseOp = OP_LDA;
			instType = IT_READ;
			break;
		case 6:	//CMP
			log_opname("CMP");
			baseOp = OP_CMP;
			instType = IT_READ;
			break;
		case 7:	//SBC
			log_opname("SBC");
			baseOp = OP_SBC;
			instType = IT_READ;
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
			if (addrMode == AM_ACCUMULATOR)
				instType = IT_READ;
			else
				instType = IT_RMW;
			break;
		case 1: //ROL
			log_opname("ROL");
			baseOp = OP_ROL;
			if (addrMode == AM_ACCUMULATOR)
				instType = IT_READ;
			else
				instType = IT_RMW;
			break;
		case 2: //LSR
			log_opname("LSR");
			baseOp = OP_LSR;
			if (addrMode == AM_ACCUMULATOR)
				instType = IT_READ;
			else
				instType = IT_RMW;
			break;
		case 3: //ROR
			log_opname("ROR");
			baseOp = OP_ROR;
			if (addrMode == AM_ACCUMULATOR)
				instType = IT_READ;
			else
				instType = IT_RMW;
			break;
		case 4: //STX
			log_opname("STX");
			baseOp = OP_STX;
			instType = IT_WRITE;
			break;
		case 5: //LDX
			log_opname("LDX");
			baseOp = OP_LDX;
			instType = IT_READ;
			break;
		case 6: //DEC
			log_opname("DEC");
			baseOp = OP_DEC;
			instType = IT_RMW;
			break;
		case 7:	//INC
			log_opname("INC");
			baseOp = OP_INC;
			instType = IT_RMW;
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
			instType = IT_READ;
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
			instType = IT_WRITE;
			break;
		case 5: //LDY
			log_opname("LDY");
			baseOp = OP_LDY;
			instType = IT_READ;
			break;
		case 6: //CPY
			log_opname("CPY");
			baseOp = OP_CPY;
			instType = IT_READ;
			break;
		case 7: //CPX
			log_opname("CPX");
			baseOp = OP_CPX;
			instType = IT_READ;
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

#include "cpu.h"
#include <Windows.h>

void CPU::SM_rti()
{
	++cpuSMClock;

	switch (cpuSMClock)
	{
	case 2:
		break;
	case 3:
		++SP;
		break;
	case 4:
		P = pull_byte() | (1 << 5);
		++SP;
		break;
	case 5:
		PC &= 0xFF00;
		PC |= pull_byte();
		++SP;
		break;
	case 6:
		PC &= 0xFF;
		PC |= (pull_byte() << 8);
		cpuState = INST_FETCH_OPCODE;
		cpuOperation = OP_NOP;
		break;
	}
}

void CPU::SM_jmp()
{
	++cpuSMClock;

	switch (cpuSMClock)
	{
	case 2:
		cpuOperand1 = FetchOperand(true);
		break;
	case 3:
		cpuOperand2 = FetchOperand(false);
		PrintLogLine();

		PC &= 0xFF00;
		PC |= cpuOperand1;

		PC &= 0xFF;
		PC |= cpuOperand2 << 8;
		cpuState = INST_FETCH_OPCODE;
		cpuOperation = OP_NOP;
		break;
	}
}

void CPU::SM_jmp_ind()
{
	++cpuSMClock;

	switch (cpuSMClock)
	{
	case 2:
		cpuOperand1 = FetchOperand(true);
		break;
	case 3:
		cpuOperand2 = FetchOperand(true);
		PrintLogLine();
		break;
	case 4:
		cpuMemAddress = MAKEWORD(cpuOperand1, cpuOperand2);
		PC &= 0xFF00;
		PC |= get_byte(cpuMemAddress);
		break;

	case 5:
		PC &= 0xFF;
		PC |= get_byte(MAKEWORD((cpuMemAddress + 1) & 0xFF, cpuOperand2)) << 8;
		cpuState = INST_FETCH_OPCODE;
		cpuOperation = OP_NOP;
		break;
	}
}

void CPU::SM_zp_x_ind()
{
	++cpuSMClock;

	switch (cpuSMClock)
	{
	case 2:
		cpuOperand1 = FetchOperand(true);
		PrintLogLine();
		break;
	case 3:
		break;
	case 4:
		cpuMemAddress = get_byte((unsigned char)(cpuOperand1 + X));
		break;
	case 5:
		cpuMemAddress |= get_byte((unsigned char)(cpuOperand1 + X + 1)) << 8;
		break;
	case 6:
		if (cpuInstType & IT_READ)
			cpuValue = get_byte(cpuMemAddress);

		StoreRegister();

		if (cpuInstType == IT_WRITE)
			cpuOpcode = OP_NOP;

		cpuState = INST_FETCH_OPCODE;
		break;
	}
}

void CPU::SM_zp()
{
	++cpuSMClock;

	switch (cpuSMClock)
	{
	case 2:
		cpuOperand1 = FetchOperand(true);
		cpuMemAddress = cpuOperand1;
		PrintLogLine();
		break;

	case 3:
		if (cpuInstType & IT_READ)
			cpuValue = get_byte(cpuMemAddress);

		StoreRegister();

		if (cpuInstType == IT_READ || cpuInstType == IT_WRITE)
			cpuState = INST_FETCH_OPCODE;
		break;

	case 4:
		put_byte(cpuMemAddress, cpuValue);
		PerformRMWOp();
		break;

	case 5:
		put_byte(cpuMemAddress, cpuValue);
		cpuState = INST_FETCH_OPCODE;
		break;
	}
}

void CPU::SM_abs()
{
	++cpuSMClock;

	switch (cpuSMClock)
	{
	case 2:
		cpuOperand1 = FetchOperand(true);
		break;
	case 3:
		cpuOperand2 = FetchOperand(true);
		PrintLogLine();
		break;
	case 4:
		cpuMemAddress = MAKEWORD(cpuOperand1, cpuOperand2);

		if (cpuInstType & IT_READ)
			cpuValue = get_byte(cpuMemAddress);

		StoreRegister();

		if (cpuInstType == IT_READ || cpuInstType == IT_WRITE)
			cpuState = INST_FETCH_OPCODE;
		break;
	case 5:
		put_byte(cpuMemAddress, cpuValue);
		PerformRMWOp();
		break;
	case 6:
		put_byte(cpuMemAddress, cpuValue);
		cpuState = INST_FETCH_OPCODE;
		break;
	}

}

void CPU::SM_zp_indexed()
{
	++cpuSMClock;
	unsigned char offset = 0;

	switch (cpuSMClock)
	{
	case 2:
		cpuOperand1 = FetchOperand(true);
		PrintLogLine();
		break;

	case 3:
		if (cpuAddressingMode == AM_ZERO_PAGE_X)
			offset = X;
		else if (cpuAddressingMode == AM_ZERO_PAGE_Y)
			offset = Y;

		cpuMemAddress = (cpuOperand1 + offset) & 0xFF;
		break;
	case 4:
		if (cpuInstType & IT_READ)
			cpuValue = get_byte(cpuMemAddress);
		StoreRegister();
		if (cpuInstType == IT_READ || cpuInstType == IT_WRITE)
			cpuState = INST_FETCH_OPCODE;
		break;
	case 5:
		put_byte(cpuMemAddress, cpuValue);
		PerformRMWOp();
		break;
	case 6:
		put_byte(cpuMemAddress, cpuValue);
		cpuState = INST_FETCH_OPCODE;
		break;
	}
}

// TODO: Share code with IRQ
void CPU::SM_NMI()
{
	++cpuSMClock;

	switch (cpuSMClock)
	{
	case 2:
		break;
	case 3:
		break;
	case 4:
		push_byte((PC & 0xFF00) >> 8);
		break;
	case 5:
		push_byte(PC & 0x00FF);
		break;
	case 6:
		push_byte(P & ~FLAG_BREAK);
		break;
	case 7:
		PC &= 0xFF00;
		PC |= get_byte(0xFFFA);
		set_flag(FLAG_INTERRUPT, 1);
		break;
	case 8:
		PC &= 0xFF;
		PC |= get_byte(0xFFFB) << 8;
		cpuState = INST_FETCH_OPCODE;
		cpuOperation = OP_NOP;
		cpuNMI = false;
		break;
	}
}

void CPU::SM_IRQ()
{
	++cpuSMClock;

	switch (cpuSMClock)
	{
	case 2:
		break;
	case 3:
		break;
	case 4:
		push_byte((PC & 0xFF00) >> 8);
		break;
	case 5:
		push_byte(PC & 0x00FF);
		break;
	case 6:
		push_byte(P & ~FLAG_BREAK);
		break;
	case 7:
		PC &= 0xFF00;
		PC |= get_byte(0xFFFE);
		set_flag(FLAG_INTERRUPT, 1);
		break;
	case 8:
		PC &= 0xFF;
		PC |= get_byte(0xFFFF) << 8;
		cpuState = INST_FETCH_OPCODE;
		cpuOperation = OP_NOP;
		break;
	}
}

void CPU::SM_brk()
{
	++cpuSMClock;

	switch (cpuSMClock)
	{
	case 2:
		++PC;
		break;
	case 3:
		push_byte(HIBYTE(PC));
		set_flag(FLAG_BREAK, 1);
		break;
	case 4:
		push_byte(LOBYTE(PC));
		break;
	case 5:
		push_byte(P | FLAG_BREAK);
		break;
	case 6:
		PC &= 0xFF00;
		PC |= get_byte(0xFFFE);
		set_flag(FLAG_INTERRUPT, 1);
		break;
	case 7:
		PC &= 0xFF;
		PC |= get_byte(0xFFFF) << 8;
		cpuState = INST_FETCH_OPCODE;
		cpuOperation = OP_NOP;
		break;
	}
}

static FLAG flags[] = { FLAG_NEGATIVE, FLAG_OVERFLOW, FLAG_CARRY, FLAG_ZERO };

void CPU::SM_branch()
{
	++cpuSMClock;

	switch (cpuSMClock)
	{
	case 2: // fetch operand, increment PC
		cpuOperand1 = FetchOperand(true);
		PrintLogLine();
		break;

	case 3:
		//xxy10000
		if (get_flag(flags[(cpuOpcode & 0xC0) >> 6]) == (cpuOpcode & 0x20) >> 5)
		{
			// Increment PCL only (check later if we crossed a branch)
			cpuNewPC = PC + (char)cpuOperand1;
			unsigned char pcl = PC + (char)cpuOperand1;
			PC &= 0xFF00;
			PC |= pcl;
		}
		else
		{
			//branch failed
			cpuOpcode = FetchOpcode();
			cpuState = INST_DECODE_OPCODE;
		}
		break;

	case 4: // This cycle is executed when branch is taken

		if (PC == cpuNewPC)
		{
			// No need to fix PC..
			cpuOpcode = FetchOpcode();
			cpuState = INST_DECODE_OPCODE;
		}
		else
		{
			PC = cpuNewPC;
		}
		break;

	case 5: // This cycle is executed when page boundary is crossed
		cpuOpcode = FetchOpcode();
		cpuState = INST_DECODE_OPCODE;
		break;
	}
}

void CPU::SM_jsr()
{
	++cpuSMClock;

	switch (cpuSMClock)
	{
	case 2:
		cpuOperand1 = FetchOperand(true);
		break;
	case 3:
		break;
	case 4:
		push_byte((PC & 0xFF00) >> 8);
		break;
	case 5:
		push_byte(PC & 0xFF);
		break;
	case 6:
		cpuOperand2 = FetchOperand(false);
		PrintLogLine();
		PC = (cpuOperand2 << 8) | cpuOperand1;
		cpuState = INST_FETCH_OPCODE;
		cpuOperation = OP_NOP;
		break;
	}
}

void CPU::SM_plp_pla()
{
	++cpuSMClock;

	switch (cpuSMClock)
	{
	case 2:
		break;
	case 3:
		++SP;
		break;
	case 4:
		if (cpuOperation == OP_PLA)
		{
			A = pull_byte();
			set_flag(FLAG_ZERO, A == 0);
			set_flag(FLAG_NEGATIVE, static_cast<char>(A) < 0);
		}
		else if (cpuOperation == OP_PLP)
		{
			P = pull_byte() | (1 << 5);
			set_flag(FLAG_BREAK, 0);
		}
		cpuState = INST_FETCH_OPCODE;
		cpuOperation = OP_NOP;
		break;
	}

}

void CPU::SM_php_pha()
{
	++cpuSMClock;

	switch (cpuSMClock)
	{
	case 2:
		break;
	case 3:
		if (cpuOperation == OP_PHP)
			push_byte(P | FLAG_BREAK);
		else if (cpuOperation == OP_PHA)
			push_byte(A);
		cpuState = INST_FETCH_OPCODE;
		cpuOperation = OP_NOP;
		break;
	}
}

void CPU::SM_rts()
{
	++cpuSMClock;

	switch (cpuSMClock)
	{
	case 2:
		break;
	case 3:
		++SP;
		break;
	case 4: // pull PCL
		PC &= 0xFF00;
		PC |= pull_byte();
		++SP;
		break;
	case 5: // pull PCH
		PC &= 0xFF;
		PC |= (pull_byte() << 8);
		break;
	case 6:
		++PC;
		cpuState = INST_FETCH_OPCODE;
		break;
	}
}

void CPU::SM_zp_ind_y()
{
	++cpuSMClock;

	switch (cpuSMClock)
	{
	case 2:
		cpuOperand1 = FetchOperand(true);
		PrintLogLine();
		break;
	case 3:
		// fetch low byte of effective address
		cpuMemAddress = get_byte(cpuOperand1);
		break;
	case 4:
	{
		// fetch high byte of effective address, add Y to PCL
		cpuMemAddress |= get_byte((cpuOperand1 + 1) & 0xFF) << 8;
		cpuNewMemAddress = cpuMemAddress + Y;
		unsigned char lowByte = cpuMemAddress + Y;
		cpuMemAddress &= 0xFF00;
		cpuMemAddress |= lowByte;
	}
	break;
	case 5:
		if (cpuMemAddress == cpuNewMemAddress && cpuInstType == IT_READ) // no need to fix
		{
			cpuValue = get_byte(cpuMemAddress);
			cpuState = INST_FETCH_OPCODE;
		}
		else
		{
			get_byte(cpuMemAddress);
			cpuMemAddress = cpuNewMemAddress;
		}
		break;
	case 6:
		cpuValue = get_byte(cpuMemAddress);
		StoreRegister();
		cpuState = INST_FETCH_OPCODE;
		break;
	}
}

void CPU::SM_abs_indexed()
{
	++cpuSMClock;
	unsigned char offset = 0;

	switch (cpuSMClock)
	{
	case 2:
		cpuOperand1 = FetchOperand(true);
		break;
	case 3:
		cpuOperand2 = FetchOperand(true);
		PrintLogLine();

		if (cpuAddressingMode == AM_ABSOLUTE_X)
			offset = X;
		else if (cpuAddressingMode == AM_ABSOLUTE_Y)
			offset = Y;

		cpuMemAddress = MAKEWORD(cpuOperand1 + offset, cpuOperand2);
		cpuNewMemAddress = MAKEWORD(cpuOperand1, cpuOperand2) + offset;
		break;
	case 4:
		if (cpuNewMemAddress == cpuMemAddress && cpuInstType == IT_READ)
		{
			cpuValue = get_byte(cpuMemAddress);
			cpuState = INST_FETCH_OPCODE;
		}
		else
		{
			get_byte(cpuMemAddress); // Dummy read
			cpuMemAddress = cpuNewMemAddress;
		}
		break;
	case 5:
		cpuValue = get_byte(cpuMemAddress);
		StoreRegister();
		if (cpuInstType == IT_READ || cpuInstType == IT_WRITE)
			cpuState = INST_FETCH_OPCODE;
		break;
	case 6:
		put_byte(cpuMemAddress, cpuValue);
		PerformRMWOp();
		break;
	case 7:
		cpuState = INST_FETCH_OPCODE;
		put_byte(cpuMemAddress, cpuValue);
		break;
	}
}


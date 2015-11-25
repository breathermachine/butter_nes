#include "cpu.h"
#include <stdio.h>

static double square_wave[] = { 
	0, 0.01160914, 0.022939481, 0.034000949, 0.044803002, 0.055354659, 0.065664528, 0.075740825,
	0.085591398, 0.095223748, 0.104645048, 0.113862159, 0.122881647, 0.131709801, 0.140352645, 0.148815953, 0.157105263,
	0.165225885, 0.173182917, 0.180981252, 0.188625592, 0.196120454, 0.203470178, 0.210678941, 0.21775076, 0.224689499,
	0.231498881, 0.23818249, 0.244743777, 0.251186072, 0.257512581
};

static unsigned char dutyCycleSequence[] = { 0x40, 0x60, 0x78, 0x9F };

unsigned char lengthCounterPeriodTable[] = {
	0x0A, 0xFE, 0x14, 0x02, 0x28, 0x04, 0x50, 0x06, 0xA0, 0x08, 0x3C, 0x0A, 0x0E, 0x0C, 0x1A, 0x0E,
	0x0C, 0x10, 0x18, 0x12, 0x30, 0x14, 0x60, 0x16, 0xC0, 0x18, 0x48, 0x1A, 0x10, 0x1C, 0x20, 0x1E,
};

static unsigned char triangleSequence[] =
{
	0xF, 0xE, 0xD, 0xC, 0xB, 0xA, 0x9, 0x8, 0x7, 0x6, 0x5, 0x4, 0x3, 0x2, 0x1, 0x0,
	0x0, 0x1, 0x2, 0x3, 0x4, 0x5, 0x6, 0x7, 0x8, 0x9, 0xA, 0xB, 0xC, 0xD, 0xE, 0xF,
};

static unsigned int noisePeriodTable[] = {
	4, 8, 16, 32, 64, 96, 128, 160, 202, 254, 380, 508, 762, 1016, 2034, 4068
};

void CPU::apuClockLengthCounters()
{
	// Clock length counter
	if (!apuTriangleChannel.enabled)
		apuTriangleChannel.lengthCounter = 0;
	else if (apuTriangleChannel.lengthCounter > 0 && !apuTriangleChannel.lengthCounterHalt)
		--apuTriangleChannel.lengthCounter;

	for (int i = 0; i < 2; ++i)
	{
		RectangleChannel *r = apuRectChannels + i;
		if (r->lengthCounter != 0 && !r->lengthCounterHalt)
			--r->lengthCounter;
	}

	if (!apuNoiseChannel.enabled)
		apuNoiseChannel.lengthCounter = 0;
	else if (apuNoiseChannel.lengthCounter > 0 && !apuNoiseChannel.lengthCounterHalt)
		--apuNoiseChannel.lengthCounter;
}

void CPU::apuClockSweepUnits()
{
	// Clock sweep unit
	for (int i = 0; i < 2; ++i)
	{
		RectangleChannel *r = &apuRectChannels[i];

		if (r->sweepUnitReload)
		{
			r->sweepUnitDivider.count = r->sweepUnitDivider.period;
			r->sweepUnitReload = false;
		}
		else
		{
			if (r->sweepUnitDivider.count != 0)
				--r->sweepUnitDivider.count;
			else if (r->sweepUnitEnable)
			{
				r->sweepUnitDivider.count = r->sweepUnitDivider.period;
				int shiftedPeriod = r->timerPeriod >> r->sweepUnitShiftCount;
				int targetPeriod = 0;
				if (r->sweepUnitNegate)
					targetPeriod = r->timerPeriod - shiftedPeriod;
				else
					targetPeriod = r->timerPeriod + shiftedPeriod;

				r->timerPeriod = (targetPeriod & 0x7FF);
				//printf("new period %d\n", r->timerPeriod);
			}
		}
	}
}

void CPU::apuClockEnvelopesAndLinearCounter()
{
	// Clock linear counter
	if (apuTriangleChannel.linearCounterReload)
		apuTriangleChannel.linearCounter = apuTriangleChannel.linearCounterReloadValue;
	else if (apuTriangleChannel.linearCounter != 0)
		--apuTriangleChannel.linearCounter;

	if (!apuTriangleChannel.controlFlag)
		apuTriangleChannel.linearCounterReload = false;

	// Clock envelopes
	for (int i = 0; i < 2; ++i)
		apuRectChannels[i].envelope.Clock();

	// Clock noise envelope
	apuNoiseChannel.envelope.Clock();
}

// Call frequency: 1.79MHz
void CPU::apuClockTriangleTimer()
{
	if (apuTriangleChannel.timerCounter == 0)
	{
		//Clock triangle sequencer if length and linear are both nonzero
		if (apuTriangleChannel.lengthCounter > 0 && apuTriangleChannel.linearCounter > 0)
			apuTriangleChannel.sequencerIndex = (apuTriangleChannel.sequencerIndex + 1) & 0x1F;

		apuTriangleChannel.timerCounter = apuTriangleChannel.timerPeriod;
	}
	else
		--apuTriangleChannel.timerCounter;
}

// Call frequency: 895kHz
void CPU::apuClockRectangleTimer()
{
	for (int i = 0; i < 2; ++i)
	{
		RectangleChannel *r = apuRectChannels + i;
		if (r->timerCounter == 0)
		{
			r->sequencerPos = (r->sequencerPos + 1) & 7;
			r->timerCounter = r->timerPeriod;
		}
		else
			--r->timerCounter;
	}
}

// Call frequency: 895kHz
void CPU::apuClockNoiseTimer()
{
	//When the timer clocks the shift register, the following actions occur in order :
	//1. Feedback is calculated as the exclusive - OR of bit 0 
	//     and one other bit : bit 6 if Mode flag is set, otherwise bit 1.
	//2. The shift register is shifted right by one bit.
	//3. Bit 14, the leftmost bit, is set to the feedback calculated earlier.
	if (apuNoiseChannel.timerCounter == 0)
	{
		int bit = 0;
		if (apuNoiseChannel.mode)
			bit = !!(apuNoiseChannel.feedbackShiftReg & 0x40);
		else
			bit = !!(apuNoiseChannel.feedbackShiftReg & 2);

		int xorValue = (apuNoiseChannel.feedbackShiftReg & 1) ^ (bit);
		apuNoiseChannel.feedbackShiftReg >>= 1;
		apuNoiseChannel.feedbackShiftReg &= ~0x4000;
		apuNoiseChannel.feedbackShiftReg |= (xorValue << 13);

		apuNoiseChannel.timerCounter = apuNoiseChannel.timerPeriod;
	}
	else
		--apuNoiseChannel.timerCounter;
}

// Call frequency: 240Hz
void CPU::apuClockFrameCounter()
{
	// Get 0x4017
	unsigned char flags = memory[0x4017];

	if (flags & 0x80)
	{
//		cpuIRQ = false;

		// Mode 1: 5-step
		if (apuFrameCounterFrame == 0 || apuFrameCounterFrame == 2)
		{
			apuClockLengthCounters();
			apuClockSweepUnits();
		}

		if (apuFrameCounterFrame >= 0 && apuFrameCounterFrame <= 3)
			apuClockEnvelopesAndLinearCounter();

		apuFrameCounterFrame = (apuFrameCounterFrame + 1) % 5;
	}
	else
	{
		// Mode 0: 4-step
//		if (apuFrameCounterFrame == 3 && !(flags & 0x40))
//			cpuIRQ = true;

		if (apuFrameCounterFrame == 1 || apuFrameCounterFrame == 3)
		{
			apuClockLengthCounters();
			apuClockSweepUnits();
		}

		apuClockEnvelopesAndLinearCounter();
		apuFrameCounterFrame = (apuFrameCounterFrame + 1) & 3;
	}
}

void CPU::apuSetRectChannelEnvelope(int index, unsigned char value)
{
	apuRectChannels[index].duty = (value & 0xC0) >> 6;
	apuRectChannels[index].lengthCounterHalt = (value & 0x20) != 0;
	apuRectChannels[index].envelope.loop = (value & 0x20) != 0;
	apuRectChannels[index].envelope.dividerPeriodReload = value & 0xF;
	apuRectChannels[index].envelope.volume = value & 0xF;
	apuRectChannels[index].envelope.constantVol = (value & 0x10) != 0;
	//printf("%X\n", value);
	//printf("duty %d, halt %d, reload value %d, volume %d, constantVol %d\n", apuRectChannels[index].duty, apuRectChannels[index].lengthCounterHalt,
	//	apuRectChannels[index].envelope.dividerPeriodReload, apuRectChannels[index].envelope.volume, apuRectChannels[index].envelope.constantVol);
}

void CPU::apuSetRectChannelSweep(int index, unsigned char value)
{
	apuRectChannels[index].sweepUnitEnable = (value & 0x80) != 0;
	apuRectChannels[index].sweepUnitDivider.period = ((value & 0x70) >> 4);
	apuRectChannels[index].sweepUnitNegate = (value & 0x8) != 0;
	apuRectChannels[index].sweepUnitShiftCount = value & 7;
	apuRectChannels[index].sweepUnitReload = true;

	//printf("sweep %d %d\n", apuRectChannels[index].sweepUnitEnable, apuRectChannels[index].sweepUnitShiftCount);
}

void CPU::apuSetRectChannelTimerLow(int index, unsigned char value)
{
	apuRectChannels[index].timerPeriod &= ~0xFF;
	apuRectChannels[index].timerPeriod |= value;

	//printf("4002 %X\n", value);
	//printf("4002 period %X\n", apuRectChannels[index].timerPeriod);
}

void CPU::apuSetRectChannelLengthCtr(int index, unsigned char value)
{
	apuRectChannels[index].timerPeriod &= ~0x700;
	apuRectChannels[index].timerPeriod |= (value & 7) << 8;
	if (apuRectChannels[index].enabled)
		apuRectChannels[index].lengthCounter = lengthCounterPeriodTable[(value & 0xF8) >> 3];
	apuRectChannels[index].envelope.startFlag = true;

	//printf("4003 %X\n", value);
	//printf("4003 period %X\n", apuRectChannels[index].timerPeriod);
	//printf("4003 lenctr %X\n", apuRectChannels[index].lengthCounter);
}

void CPU::apuNoiseChannelSetPeriod(unsigned char value)
{
	apuNoiseChannel.mode = (value & 0x80) != 0;
	apuNoiseChannel.timerPeriod = noisePeriodTable[value & 0xF];
}

static double tndValue(int n)
{
	if (n == 0)
		return 0;
	return 163.67 / (24329.0 / n + 100);
}

void CPU::apuPutSoundSample()
{
	double finalValue = 0;
	unsigned char triangleValue = 0;
	unsigned char noiseValue = 0;
	unsigned char dmcValue = 0;
	unsigned char squareValue = 0;

	if (apuTriangleChannel.linearCounter > 0 && apuTriangleChannel.lengthCounter > 0)
		triangleValue = triangleSequence[apuTriangleChannel.sequencerIndex];

	for (int i = 0; i < 2; ++i)
	{
		if (apuRectChannels[i].lengthCounter > 0 && (dutyCycleSequence[apuRectChannels[i].duty] & 1 << apuRectChannels[i].sequencerPos) && apuRectChannels[i].timerPeriod >= 8)
		{
			if (apuRectChannels[i].envelope.constantVol)
				squareValue += apuRectChannels[i].envelope.volume;
			else
				squareValue += apuRectChannels[i].envelope.counter;
		}
	}

	if (apuNoiseChannel.lengthCounter > 0 && (apuNoiseChannel.feedbackShiftReg & 1))
	{
		if (apuNoiseChannel.envelope.constantVol)
			noiseValue = apuNoiseChannel.envelope.volume;
		else
			noiseValue = apuNoiseChannel.envelope.counter;
	}
		
	finalValue = square_wave[squareValue] + tndValue(3 * triangleValue + 2 * noiseValue + dmcValue);
	put_sound_sample(finalValue * 65535 - 0x8000);
}

void Envelope::Clock()
{
	// When clocked by the frame counter, one of two actions occurs
	// 1. if the start flag is clear, the divider is clocked, 
	// 2. otherwise the start flag is cleared, the counter is loaded with 15, 
	//    and the divider's period is immediately reloaded.
	if (startFlag)
	{
		startFlag = false;
		counter = 15;
		divider.period = dividerPeriodReload;
	}
	else
	{
		// clock divider
		if (divider.count == 0)
		{
			divider.count = divider.period;
			if (loop)
				counter = 15;
			else if (counter != 0)
				--counter;
		}
		else
			--divider.count;
	}
}


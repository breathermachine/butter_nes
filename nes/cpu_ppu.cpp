#include "cpu.h"
#include "utils.h"
#include "SDL2/SDL.h"
#include <assert.h>
#include "sdl_graphics_system.h"

bool drawNT = false;

static unsigned int nesPalette[] = {
	0x757575, 0x271B8F, 0x0000AB, 0x47009F, 0x8F0077, 0xAB0013, 0xA70000, 0x7F0B00,
	0x432F00, 0x004700, 0x005100, 0x003F17, 0x1B3F5F, 0x000000, 0x000000, 0x000000,
	0xBCBCBC, 0x0073EF, 0x233BEF, 0x8300F3, 0xBF00BF, 0xE7005B, 0xDB2B00, 0xCB4F0F,
	0x8B7300, 0x009700, 0x00AB00, 0x00933B, 0x00838B, 0x000000, 0x000000, 0x000000,
	0xFFFFFF, 0x3FBFFF, 0x5F97FF, 0xA78BFD, 0xF77BFF, 0xFF77B7, 0xFF7763, 0xFF9B3B,
	0xF3BF3F, 0x83D313, 0x4FDF4B, 0x58F898, 0x00EBDB, 0x000000, 0x000000, 0x000000,
	0xFFFFFF, 0xABE7FF, 0xC7D7FF, 0xD7CBFF, 0xFFC7FF, 0xFFC7DB, 0xFFBFB3, 0xFFDBAB,
	0xFFE7A3, 0xE3FFA3, 0xABF3BF, 0xB3FFCF, 0x9FFFF3, 0x000000, 0x000000, 0x000000
};

static bool isTransparent(int color)
{
	return (color & 3) == 0;
}

static int frame = 1;

void CPU::ppuClockPPU()
{
	//Check for NMI
	if (ppuClock == 0)
	{
		ppuInVblank = true;

		bool executeNMIonVBlank = (memory[PPUCTRL] & 0x80) != 0;
		if (executeNMIonVBlank)
			NMI();
	}

	ppuDoPerScanlineClock();

	++ppuClock;
	++ppuScanlineClock;

	//Increment scanline count after 341 PPU clocks.
	if (ppuScanlineClock == 341)
	{
		if (ppuScanline >= PRERENDER_SCANLINE)
			++ppuRenderY;

		++ppuScanline;

		ppuRenderX = 0;
		ppuScanlineClock = 0;

		if (ppuScanline == 262)
		{
			++frame;
			//prepare for next frame
			ppuScanline = 0;
			ppuRenderX = 0;
			ppuRenderY = -1;
			ppuClock = 0;

#if DRAW_NAMETABLES
			if (drawNT)
			{
				DrawNameTable(0, 0, 0x2000);
				DrawNameTable(256, 0, 0x2400);
				DrawNameTable(0, 240, 0x2800);
				DrawNameTable(256, 240, 0x2C00);
			}
#endif

			static Uint32 lastFrameTime = SDL_GetTicks();
			int remaining = FRAME_TIME - (SDL_GetTicks() - lastFrameTime);

			if (remaining > 0)
				SDL_Delay(remaining);

			lastFrameTime = SDL_GetTicks();
		}
	}
}

void CPU::ppuIncrementCoarseX()
{
	if ((ppuAddressMemory & 0x001F) == 31)
	{   // if coarse X == 31
		ppuAddressMemory &= ~0x001F; // coarse X = 0
		ppuAddressMemory ^= 0x0400;	// switch horizontal nametable
	}
	else
		ppuAddressMemory += 1; // increment coarse X
}

void CPU::ppuIncrementFineY()
{
	unsigned short oldAddr = ppuAddressMemory;

	if ((ppuAddressMemory & 0x7000) != 0x7000)          // if fine Y < 7
		ppuAddressMemory += 0x1000;                     // increment fine Y
	else
	{
		ppuAddressMemory &= 0x0FFF;                      // fine Y = 0
		int y = (ppuAddressMemory & 0x03E0) >> 5;        // let y = coarse Y
		if (y == 29)
		{
			y = 0;                          // coarse Y = 0
			ppuAddressMemory ^= 0x0800;                    // switch vertical nametable
		}
		else if (y == 31)
			y = 0;                          // coarse Y = 0, nametable not switched
		else
			y += 1;                         // increment coarse Y
		ppuAddressMemory = (ppuAddressMemory & ~0x03E0) | (y << 5);     // put coarse Y back into v
	}

	if (!(oldAddr & 0x1000) && (ppuAddressMemory & 0x1000))
		mmc3ClockIRQCounter();
}

void CPU::ppuUpdateAddress2007()
{
	/*
	If the VRAM address increment bit (2000.2) is clear (inc. amt. = 1), all the
	scroll counters are daisy-chained (in the order of HT, VT, H, V, FV) so that
	the carry out of each counter controls the next counter's clock rate. The
	result is that all 5 counters function as a single 15-bit one. Any access to
	2007 clocks the HT counter here.

	If the VRAM address increment bit is set (inc. amt. = 32), the only
	difference is that the HT counter is no longer being clocked, and the VT
	counter is now being clocked by access to 2007.
	*/
	unsigned short oldAddr = ppuAddressMemory;

	ppuAddressMemory += (memory[PPUCTRL] & 4) ? 32 : 1;
	ppuAddressMemory &= 0x3FFF;

	if (!(oldAddr & 0x1000) && (ppuAddressMemory & 0x1000))
		mmc3ClockIRQCounter();
}

void CPU::sprEvalSprites()
{
	/*
	������������������������������������������������������������������������������������������������������������������������������
	��   0    �� YYYYYYYY �� Sprite Y coordinate - 1. Consider the   ��
	��        ��          �� coordinate the upper-left corner of     ��
	��        ��          �� the sprite itself.                      ��
	��   1    �� IIIIIIII �� Sprite Tile Index #                     ��
	��   2    �� vhp???cc �� Colour/Attributes                       ��
	��        ��          ��  v = Vertical Flip   (1=Flip)           ��
	��        ��          ��  h = Horizontal Flip (1=Flip)           ��
	��        ��          ��  p = Sprite Priority Bit                ��
	��        ��          ��         0 = Sprite on top of background ��
	��        ��          ��         1 = Sprite behind background    ��
	��        ��          ��  c = Upper two (2) bits of colour       ��
	��   3    �� XXXXXXXX �� Sprite X coordinate (upper-left corner) ��
	������������������������������������������������������������������������������������������������������������������������������
	*/
	unsigned char addr = memory[OAMADDR];

	int attribute = sprMemory[addr + 2];
	int y = sprMemory[addr];
	int rangeResult = (ppuScanline - 21) - y;

	int spriteSize = (memory[PPUCTRL] & 32) ? 16 : 8;

	if (rangeResult >= 0 && rangeResult < spriteSize && y < 240)
	{
		if (sprEvalIndex < 8)
		{
			//Sprite is in range, copy attributes..
			sprEvalMemory[sprEvalIndex].x = sprMemory[addr + 3];
			sprEvalMemory[sprEvalIndex].tile = sprMemory[addr + 1];

			if (attribute & 128) //vertical flip
				rangeResult = ~rangeResult;

			sprEvalMemory[sprEvalIndex].rangeResult = rangeResult & 0x0F;
			sprEvalMemory[sprEvalIndex].attribute = attribute;

			if (ppuScanlineClock == 64)
				ppuSprite0InRangeInEval = true;

			++sprEvalIndex;
		}
		else
			sprSpriteOverflow = true;
	}

	memory[OAMADDR] += 4;
}

void CPU::ppuComputeWinningPixel()
{
	ppuObjColor = 0;
	ppuObjIdx = 8;

	bool showBG = (memory[PPUMASK] & 8) != 0;
	bool showSprites = (memory[PPUMASK] & 16) != 0;
	bool showLeftSprites = (memory[PPUMASK] & 4) != 0;
	bool showLeftBG = (memory[PPUMASK] & 2) != 0;

	// Evaluate winning sprite pixel.
	if (showSprites)
	{
		for (int i = 0; i < 8; ++i)
		{
			if (sprRenderMemory[i].x != 0) continue;

			unsigned char objColor = (sprRenderMemory[i].tile1 & 128) >> 6 |
					(sprRenderMemory[i].tile0 & 128) >> 7;

			if (objColor)
			{
				//Found a winner!
				ppuObjColor = objColor;
				ppuObjFG = !sprRenderMemory[i].background;
				ppuObjIdx = i;
				break;
			}
		}

		// Give each sprites a tick.
		for (int i = 0; i < 8; ++i)
		{
			if (sprRenderMemory[i].x > 0)
				--sprRenderMemory[i].x;
			else
			{
				sprRenderMemory[i].tile0 <<= 1;
				sprRenderMemory[i].tile1 <<= 1;
			}
		}
	}

	// Computation of sprite #0 hit
	if (ppuObjColor &&
		ppuSprite0InRangeInRender &&
		ppuObjIdx == 0 &&
		!isTransparent(ppuBackgroundPaletteIndex) && showBG && showSprites)
	{
		if (ppuRenderX < 8)
		{
			if (showLeftSprites && showLeftBG)
				sprSprite0Hit = true;
		}
		else if (ppuRenderX < 255)
		{
			sprSprite0Hit = true;
		}
	}
}

void CPU::ppuRenderPixel()
{
	// Winning objectPixel vs pfPixel
	//IF (OBJpri=foreground OR PFpixel=xparent) AND OBJpixel<>xparent is TRUE
	//   Render using Obj Pixel
	//Else
	//	 Render using PlayField Pixel

#if DRAW_NORMALLY
	if (!drawNT)
	{
		if (ppuObjColor && (ppuObjFG || isTransparent(ppuBackgroundPaletteIndex)))
			SDLGraphicsSystem::GetSystem()->PutPixel(ppuRenderX, ppuRenderY, nesPalette[ppuMemory[0x3F10 | ppuObjColor | sprRenderMemory[ppuObjIdx].palette] & 0x3F]);
		else if (!isTransparent(ppuBackgroundPaletteIndex))
			SDLGraphicsSystem::GetSystem()->PutPixel(ppuRenderX, ppuRenderY, nesPalette[ppuMemory[0x3F00 | ppuBackgroundPaletteIndex] & 0x3F]);
		else
			SDLGraphicsSystem::GetSystem()->PutPixel(ppuRenderX, ppuRenderY, ppuUnivBGColor);
	}
#elif DRAW_SPRITE0
	if (winningObjectColor)
	{
		if (ppuSprite0InRangeInRender && winningObjectIndex == 0)
			SDLGraphicsSystem::GetSystem()->PutPixel(ppuRenderX, ppuRenderY, 0x00FF0000);
		else
			SDLGraphicsSystem::GetSystem()->PutPixel(ppuRenderX, ppuRenderY, 0x00007700);
	}
	else if (!isTransparent(ppuBackgroundPaletteIndex))
		SDLGraphicsSystem::GetSystem()->PutPixel(ppuRenderX, ppuRenderY, 0xFFFFFFFF);
	else
		SDLGraphicsSystem::GetSystem()->PutPixel(ppuRenderX, ppuRenderY, 0x00000000);
#endif
}

void CPU::sprFetchSprites()
{
	/*
	The fetched pattern table data (which is 2 bytes), plus the associated
	3 attribute bits (palette select & priority), and the x coordinate byte
	in sprite temporary memory are then loaded into a part of the PPU called
	the "sprite buffer memory" (the primary object present bit is also copied).
	This memory area again, is large enough to hold the contents for 8 sprites.

	The composition of one sprite buffer element here is: 2 8-bit shift
	registers (the fetched pattern table data is loaded in here, where it will
	be serialized at the appropriate time), a 3-bit latch (which holds the
	color & priority data for an object), and an 8-bit down counter
	(this is where the x coordinate is loaded).
	*/
	unsigned short memStart;

	SpriteEval *pSpriteEval = &sprEvalMemory[sprEvalToRenderIndex];

	int spriteSize = (memory[PPUCTRL] & 32) ? 16 : 8;
	if (spriteSize == 8)
		memStart = ((memory[PPUCTRL] & 8) << 9) + (pSpriteEval->tile << 4);
	else if (pSpriteEval->tile & 1)
		//use 0x1000
		memStart = 0x1000 + ((pSpriteEval->tile & 0xFE) << 4);
	else
		//use 0x0000
		memStart = (pSpriteEval->tile & 0xFE) << 4;

	int maxOffset = spriteSize - 1;
	int offset = 0;
	if (pSpriteEval->rangeResult >= 8 && spriteSize == 16)
		offset = 8;

	SpriteRender *pSpriteRender = &sprRenderMemory[sprEvalToRenderIndex];

	//Horizontal flipping of bytes occur here in the fetching of sprite data.
	switch (ppuScanlineClock % 8)
	{
	case 5:
		//tile 0 fetch
		if (sprEvalToRenderIndex < sprEvalIndex)
		{
			pSpriteRender->tile0 = ppuMemory[memStart + (pSpriteEval->rangeResult & maxOffset) + offset];
			if (pSpriteEval->attribute & 64)
				pSpriteRender->tile0 = FlipByte(pSpriteRender->tile0);
		}
		else
		{
			pSpriteRender->tile0 = 0;
		}
		break;

	case 7:
		//tile 1 fetch
		if (sprEvalToRenderIndex < sprEvalIndex)
		{
			pSpriteRender->tile1 = ppuMemory[memStart + (pSpriteEval->rangeResult & maxOffset) + 8 + offset];
			if (pSpriteEval->attribute & 64)
				pSpriteRender->tile1 = FlipByte(pSpriteRender->tile1);
		}
		else
		{
			pSpriteRender->tile1 = 0;
		}

		pSpriteRender->x = pSpriteEval->x;
		pSpriteRender->palette = (pSpriteEval->attribute & 3) << 2;
		pSpriteRender->background = (pSpriteEval->attribute & 32) != 0;

		sprEvalToRenderIndex = (sprEvalToRenderIndex + 1) % 8;
		ppuSprite0InRangeInRender = ppuSprite0InRangeInEval;
		break;
	}
}

unsigned short CPU::get_ciram_address(unsigned short ppuaddress)
{
	unsigned short ret = 0;
	if (ppuNTMirroring == MIRRORING_VERTICAL) // PPU A10 to CIRAM A10
		ret = ppuaddress & 0x7FF;
	else if (ppuNTMirroring == MIRRORING_HORIZONTAL) // PPU A11 to CIRAM A10
		ret = (ppuaddress & 0x3FF) | ((ppuaddress >> 1) & 0x400);
	else if (ppuNTMirroring == MIRRORING_1SCREEN_000)
		ret = (ppuaddress & 0x3FF);
	else if (ppuNTMirroring == MIRRORING_1SCREEN_400)
		ret = (ppuaddress & 0x3FF) | 0x400;

	return 0x2000 | ret;
}

//Add mirroring here..
unsigned char CPU::get_ppu_byte()
{
	unsigned char returnValue = 0;

	// Palette access is instant
	if ((ppuAddressMemory & 0xFF00) == 0x3F00)
	{
		returnValue = ppuMemory[ppuAddressMemory & 0xFF1F]; //$FF1F -> palette mirroring
		ppuByteBuffer = ppuMemory[get_ciram_address(ppuAddressMemory & 0xEFFF)];
	}
	else if (ppuAddressMemory & 0x2000)
	{
		returnValue = ppuByteBuffer;
		ppuByteBuffer = ppuMemory[get_ciram_address(ppuAddressMemory & 0xEFFF)];
	}
	else
	{
		// CHR access
		returnValue = ppuByteBuffer;
		ppuByteBuffer = ppuMemory[ppuAddressMemory];
	}

	ppuUpdateAddress2007();
	return returnValue;
}

void CPU::put_ppu_byte(unsigned short address, unsigned char value)
{
	//Nametable mirroring
	if ((address & 0xFF00) == 0x3F00)
	{
		// Palette mirroring
		unsigned short baseAddr = address & 0xFF1F;
		if (baseAddr == 0x3F10)
		{
			ppuMemory[0x3F00] = value;
			ppuUnivBGColor = nesPalette[value];
		}
		else if (baseAddr == 0x3F00)
		{
			ppuMemory[0x3F10] = value;
			ppuUnivBGColor = nesPalette[value];
		}
		ppuMemory[baseAddr] = value;
	}
	else if (address & 0x2000)
	{
		ppuMemory[get_ciram_address(address)] = value;
	}
	else if (chrram)
	{
		chrram[address & 0x1FFF] = value;
		ppuMemory[address] = value;
	}
}

void CPU::ppuFetchBackground()
{
	int accessCount = ppuMemAccessCountFromScanlineClock();

#if 1
	if (!(
		accessCount <= 128
		||
		(accessCount >= 161 && accessCount <= 170)
		)
		) return;
#endif

	//  NT, Attrib, Pattern Fetch
	switch (ppuScanlineClock % 8)
	{
	case 1:
	{
		// Nametable tile
		ppuRenderNTTile = ppuMemory[get_ciram_address(0x2000 | (ppuAddressMemory & 0xFFF))];
	}
	break;

	case 3:
		// Attribute byte
		ppuRenderAttributeLatch =
			ppuMemory[get_ciram_address(0x23C0 | // start of memory for attribute table
			(ppuAddressMemory & 0xC00) |	// nametable bit
			((ppuAddressMemory >> 4) & 0x38) | // upper 3 bits of Tile Y
			((ppuAddressMemory >> 2) & 0x07))]; // upper 3 bits of TileX
		break;

	case 5:
		// Pattern 0
		ppuPatternLatch = ppuMemory[
			((memory[PPUCTRL] & 16) << 8) + (ppuRenderNTTile << 4) + ((ppuAddressMemory & 0x7000) >> 12)
		];
		break;

	case 7:
		// Pattern 1 (+8 bytes)
	{
		unsigned char patternValue = ppuMemory[
			((memory[PPUCTRL] & 16) << 8) + (ppuRenderNTTile << 4) + 8 + ((ppuAddressMemory & 0x7000) >> 12)
		];

		if (ppuScanlineClock == 335)
		{
			ppuNTShiftRegister[1] <<= 8;
			ppuNTShiftRegister[0] <<= 8;

			ppuRenderAttributeShiftRegister[1] <<= 8;
			ppuRenderAttributeShiftRegister[0] <<= 8;
		}

		// populate the shift reigsters from the latches.
		ppuNTShiftRegister[1] &= 0xFF00;
		ppuNTShiftRegister[1] |= patternValue;

		ppuNTShiftRegister[0] &= 0xFF00;
		ppuNTShiftRegister[0] |= ppuPatternLatch;

		int tileX = ppuAddressMemory & 0x1F;
		int tileY = (ppuAddressMemory >> 5) & 0x1F;
		unsigned char ppuAttribShift = ((tileY & 2) << 1) | (tileX & 2);

		unsigned char attribVal = (ppuRenderAttributeLatch & (3 << ppuAttribShift)) >> ppuAttribShift;

		ppuRenderAttributeShiftRegister[0] &= 0xFF00;
		ppuRenderAttributeShiftRegister[1] &= 0xFF00;
		ppuRenderAttributeShiftRegister[0] |= (attribVal & 1) ? 0xFF : 0;
		ppuRenderAttributeShiftRegister[1] |= (attribVal & 2) ? 0xFF : 0;
	}
	break;
	}
}

int CPU::ppuMemAccessCountFromScanlineClock()
{
	return ppuScanlineClock / 2 + 1;
}

void CPU::ppuDoPerScanlineClock()
{
	// Core PPU function
	// 0-19   : VINT (v-blank)
	// 20     : DUMMY scanline (pre-render scanline / does not perform secondary OAM clear and sprite evaluation)
	// 21-260 : RENDER
	// 261    : BLANK (post-render scanline, does nothing)

	// 341 PPU cycles / scanline
	// 1 unused cycle (cycle# 340)

	// Memory accesses (170 total 1access=2ppu clocks)
	//   0 - 127: Fetch NT data, sprite evaluation, rendering
	// 128 - 159: Fetch sprite data
	// 160 - 167: Fetch NT data for next scanline
	// 168 - 169: 2 nametable bytes (unknown)

	bool showBG = (memory[PPUMASK] & 8) != 0;
	bool showSprites = (memory[PPUMASK] & 16) != 0;
	bool isRenderingEnabled = showBG || showSprites;

	if (ppuScanline == PRERENDER_SCANLINE && ppuScanlineClock == 0)
	{
		sprSprite0Hit = false;
		sprSpriteOverflow = false;
		ppuInVblank = false;
	}

	if (showBG)
	{
		if (ppuScanlineClock <= 255)
		{
			unsigned short pos = 1 << (15 - (ppuFineX & 7)); // ppuFineX [0-7], shift [15-8]

			ppuBackgroundPaletteIndex =
				((ppuNTShiftRegister[0] & pos) ? 1 : 0) |
				((ppuNTShiftRegister[1] & pos) ? 2 : 0) |
				((ppuRenderAttributeShiftRegister[0] & pos) ? 4 : 0) |
				((ppuRenderAttributeShiftRegister[1] & pos) ? 8 : 0);

			//Shift the shift registers
			ppuNTShiftRegister[0] <<= 1;
			ppuNTShiftRegister[1] <<= 1;

			//Also need to shift the attribute registers
			ppuRenderAttributeShiftRegister[0] <<= 1;
			ppuRenderAttributeShiftRegister[1] <<= 1;
		}

		ppuFetchBackground();
	}
	else
	{
		ppuBackgroundPaletteIndex = 0;
	}

	if (isRenderingEnabled)
	{
		if (ppuScanlineClock == 0 && ppuScanline != PRERENDER_SCANLINE)
		{
			sprEvalIndex = 0;
			sprEvalToRenderIndex = 0;
			ppuSprite0InRangeInEval = false;
		}
	}

	if (showSprites)
	{
		if (ppuScanline >= PRERENDER_SCANLINE && ppuScanline <= 260)
		{
			if (ppuScanline != PRERENDER_SCANLINE && ppuScanlineClock < 64)
				// Secondary OAM clear
				memset(&sprEvalMemory[0], 0xFF, 8 * sizeof(SpriteEval));

			// Sprite Pattern Fetch
			if (ppuScanlineClock >= 256 && ppuScanlineClock < 320)
			{
				memory[OAMADDR] = 0;
				sprFetchSprites();
			}
		}
	}

	if (isRenderingEnabled && ppuScanlineClock >= 64 && ppuScanlineClock <= 255 && ppuScanline != PRERENDER_SCANLINE && (ppuScanlineClock - 64) % 3 == 0)
		sprEvalSprites();

	if (isRenderingEnabled && ppuScanlineClock < 256)
		ppuComputeWinningPixel();

	ppuRenderPixel();

	if (ppuScanline == PRERENDER_SCANLINE && ppuScanlineClock >= 280 && ppuScanlineClock <= 304 && isRenderingEnabled)
	{
		// Copy fine Y, vert NT and coarse Y
		//v: IHGF.ED CBA..... = t : IHGF.ED CBA.....
		ppuAddressMemory &= ~0x7BE0;
		ppuAddressMemory |= ppuAddressLatch & 0x7BE0;
	}

	// Update memory addresses (scroll registers)
	if (ppuScanline >= PRERENDER_SCANLINE && ppuScanline <= 260 && showBG)
	{
		// Increment Fine Y on start of HBLANK.
		if (ppuScanlineClock == 256)
			ppuIncrementFineY();

		if (ppuScanlineClock == 257)
		{
			//(D10 and D4-D0) into Loopy_V (coarse X and horizontal NT)
			// Coarse X
			ppuAddressMemory &= ~0x1F;
			ppuAddressMemory |= (ppuAddressLatch & 0x1F);
			// Horizontal NT
			ppuAddressMemory &= ~0x400;
			ppuAddressMemory |= (ppuAddressLatch & 0x400);
		}

		// Increment Coarse X from 328 to 336, and every 8 clocks
		// 8, 16, 24, etc..
		if (ppuScanlineClock % 8 == 7)
		{
			if ((ppuScanlineClock <= 255) || (ppuScanlineClock == 327) || (ppuScanlineClock == 335))
				ppuIncrementCoarseX();
		}
	}

	++ppuRenderX;
}

void CPU::DrawNameTable(int sx, int sy, unsigned short addr)
{
	for (int row = 0; row < 30; ++row)
		for (int col = 0; col < 32; ++col)
		{
			int y = row * 8;

			unsigned char tile = ppuMemory[get_ciram_address(addr + 32 * row + col)];
			unsigned char attrib =
				ppuMemory[get_ciram_address(addr | 0x3C0 |
				((row << 1) & 0x38) |
				((col >> 2) & 0x07))]; // upper 3 bits of TileX
			unsigned char shift = ((row & 2) << 1) | (col & 2);

			for (int scanline = 0; scanline < 8; ++ scanline)
			{
				int x = col * 8;

				unsigned char p0 = ppuMemory[((memory[PPUCTRL] & 16) << 8) + (tile << 4) + scanline];
				unsigned char p1 = ppuMemory[((memory[PPUCTRL] & 16) << 8) + (tile << 4) + 8 + scanline];

				for (int c = 0; c < 8; ++c)
				{
					unsigned char a = attrib & (3 << shift);
					if (shift == 0)
						a <<= 2;
					else if (shift == 4)
						a >>= 2;
					else if (shift == 6)
						a >>= 4;

					unsigned char color = ((p1 & 128) >> 6) | ((p0 & 128) >> 7) | a;
					p0 <<= 1;
					p1 <<= 1;

					SDLGraphicsSystem::GetSystem()->PutPixel(x + sx, y + sy, nesPalette[ppuMemory[0x3F00 | color]]);
					++x;
				}

				++y;
			}
		}
}

void CPU::mmc3ClockIRQCounter()
{
	//printf("CLOCK IRQ COUNTER %d, %d\n", ppuScanline, ppuScanlineClock);

#if 0
	if (mmc3IRQCounterReload || mmc3IRQCounter == 0)
	{
		mmc3IRQCounter = mmc3IRQReloadValue;
		mmc3IRQCounterReload = false;
	}
	else
		--mmc3IRQCounter;

	if (mmc3IRQCounter == 0)
	{
		if (mmc3IRQEnabled)
		{
			//printf("Raising MMC3 IRQ %d\n", ppuScanline);
			cpuIRQ = true;
		}
		//else
		//	cpuIRQ = false;
	}
	//else
	//	cpuIRQ = false;
#endif
	if (mmc3IRQCounter == 0)
		mmc3IRQCounter = mmc3IRQReloadValue;
	else
	{
		--mmc3IRQCounter;
		if (mmc3IRQCounter == 0 && mmc3IRQEnabled)
			cpuIRQ = true;
	}
}

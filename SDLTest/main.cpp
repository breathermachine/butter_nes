#include "SDL.h"
#include "SDL_main.h"
#include "SDL_events.h"
#include <Windows.h>
#include <tchar.h>
#include <assert.h>
#include <vector>

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


void PutPixel(void *surface, int x, int y, int argb, int pitch)
{
	for (int a = 0; a < 4; ++a)
	{
		unsigned int *ptr = reinterpret_cast<unsigned int*>((unsigned char*)surface + ((y << 2) + a) * pitch) + (x << 2);
		*ptr = argb;
		*(ptr + 1) = argb;
		*(ptr + 2) = argb;
		*(ptr + 3) = argb;
	}
}

OPENFILENAME ofn;
TCHAR szFile[512];

TCHAR *GetFilenameWin32()
{
	ZeroMemory(&ofn, sizeof(ofn));
	ofn.lStructSize = sizeof(ofn);
	ofn.hwndOwner = NULL;
	ofn.lpstrFile = szFile;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof(szFile);
	ofn.lpstrFilter = _T("NES Roms\0*.nes\0");
	ofn.nFilterIndex = 0;
	ofn.lpstrFileTitle = NULL;
	ofn.nMaxFileTitle = 0;
	ofn.lpstrInitialDir = NULL;
	ofn.Flags = OFN_PATHMUSTEXIST | OFN_FILEMUSTEXIST;

	if (!GetOpenFileName(&ofn))
		return 0;

	return ofn.lpstrFile;
}

int main(int argc, char *argv[])
{
	std::vector<unsigned char> chrRom;

	FILE *f = _tfopen(GetFilenameWin32(), _T("rb"));
	if (!f)
		return 1;

	INESHeader header;
	memset(&header, 0, sizeof(header));
	if (fread(&header, sizeof(header), 1, f) != 1)
		return 1;

	chrRom.resize(header.chrRomSize * 8192);
	fseek(f, header.prgRomSize * 16384 + sizeof(header), SEEK_SET);
	if (fread(&chrRom[0], 1, chrRom.size(), f) != chrRom.size())
		return 1;

	int ret = SDL_Init(SDL_INIT_VIDEO);

	SDL_Surface *surface = SDL_SetVideoMode(256*4, 128*4, 0, SDL_DOUBLEBUF);

	if (!surface)
		return 1;
	
	SDL_Event event;

	int colorMap[] = { 0, 0xFF00FF00, 0xFFFF0000, 0xFF00FFFF };
	for (;;)
	{
		if (SDL_PollEvent(&event) == 1)
		{
			if (event.type == SDL_QUIT)
				break;
		}

		// display tiles in 32x16 format
		void *s = surface->pixels;
		int x = 0;
		int y = 0;

		for (int row = 0; row < 16; ++row)
		{
			for (int col = 0; col < 32; ++col)
			{
				int tileNum = row * 32 + col;

				y = row * 8;

				for (int sl = 0; sl < 8; ++sl)				
				{
					x = col * 8;

					unsigned char pattern0 = chrRom[tileNum * 16 + sl];
					unsigned char pattern1 = chrRom[tileNum * 16 + 8 + sl];

					for (int bp = 0; bp < 8; ++bp)
					{
						PutPixel(s, x, y, colorMap[((pattern1 & 128) >> 6) | ((pattern0 & 128) >> 7)], surface->pitch);
						pattern0 <<= 1;
						pattern1 <<= 1;
						++x;
					}

					if (sl == 0)
					{
						PutPixel(s, col * 8, y, 0xFFFFFF00, surface->pitch);
					}

					++y;
				}
			}
		}

		
		SDL_Flip(surface);
	}

	SDL_Quit();

	return 0;
}

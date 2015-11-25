/**
 *  NESh v0.001a
 */

#define _CRT_SECURE_NO_WARNINGS

#include "cpu.h"
#include "graphics_system.h"
#include "SDL_graphics_system.h"
#include "SDL.h"
#include "SDL_thread.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <crtdbg.h>
#include <math.h>
#include <windows.h>

#define TARGET_FPS 60.0
#define SCALE 2
#define SCREEN_WIDTH (256*SCALE)
#define SCREEN_HEIGHT (240*SCALE)
#define SCREEN_DEPTH 8

CPU cpu;

bool joyStickStatus[8];
char *GetFilenameWin32();

char fpsText[128];
   
int frameCount = 0;

struct SteroSample
{
	short left;
	short right;
};

const int SAMPLES_SIZE = 6144;
SteroSample samples[SAMPLES_SIZE];
int write_pos = 0;
int read_pos = 0;
int samples_size = 0;

//puts a sound sample to the buffer (ring buffer?)
void put_sound_sample(int val)
{
	if (samples_size == SAMPLES_SIZE)
		return;

	samples[write_pos].left = val;
	samples[write_pos].right = val;
	write_pos = (write_pos + 1) % SAMPLES_SIZE;
	++samples_size;
}

static bool turboB = false;
static bool turboA = false;

void KeyHandler(GS_KEYEVENT keyEventType, GS_VIRTKEY virtKey)
{
	bool pressed = (keyEventType == GS_KEYEVENT::KEYDOWN);

	switch(virtKey)
	{
	case GS_VIRTKEY::VIRTKEY_X:
		joyStickStatus[0] = pressed;
		break;
	case GS_VIRTKEY::VIRTKEY_Z:
		joyStickStatus[1] = pressed;
		break;
	case GS_VIRTKEY::VIRTKEY_LEFT:
		joyStickStatus[6] = pressed;
		break;
	case GS_VIRTKEY::VIRTKEY_RIGHT:
		joyStickStatus[7] = pressed;
		break;
	case GS_VIRTKEY::VIRTKEY_UP:
		joyStickStatus[4] = pressed;
		break;
	case GS_VIRTKEY::VIRTKEY_DOWN:
		joyStickStatus[5] = pressed;
		break;
	case GS_VIRTKEY::VIRTKEY_RETURN:
		joyStickStatus[3] = pressed;
		break;
	case GS_VIRTKEY::VIRTKEY_LSHIFT:
		joyStickStatus[2] = pressed;
		break;
	case GS_VIRTKEY::VIRTKEY_Q:
		if (!pressed)
			cpu.resetCPU = true;
		break;
	case GS_VIRTKEY::VIRTKEY_W:
		if (!pressed)
		{
			cpu.romFilename = GetFilenameWin32();
			cpu.resetCPU = true;
			cpu.loadROM = true;
		}
		break;
	case GS_VIRTKEY::VIRTKEY_C:
		extern bool drawNT;
		drawNT = !drawNT;
		//turboB = pressed;
		//joyStickStatus[1] = pressed;
		break;
	case GS_VIRTKEY::VIRTKEY_V:
		turboA = pressed;
		joyStickStatus[0] = pressed;
		break;
	}
}

void FrameUpdate()
{
	if (turboB)
		joyStickStatus[1] = !joyStickStatus[1];
	if (turboA)
		joyStickStatus[0] = !joyStickStatus[0];

	IGraphicsSystem::GetSystem()->Flip();
}

OPENFILENAME ofn ;
char szFile[512];

char *GetFilenameWin32()
{
	ZeroMemory( &ofn , sizeof( ofn));
	ofn.lStructSize = sizeof ( ofn );
	ofn.hwndOwner = NULL  ;
	ofn.lpstrFile = szFile ;
	ofn.lpstrFile[0] = '\0';
	ofn.nMaxFile = sizeof( szFile );
	ofn.lpstrFilter = "NES Roms\0*.nes\0";
	ofn.nFilterIndex = 0;
	ofn.lpstrFileTitle = NULL ;
	ofn.nMaxFileTitle = 0 ;
	ofn.lpstrInitialDir=NULL ;
	ofn.Flags = OFN_PATHMUSTEXIST|OFN_FILEMUSTEXIST ;
	
    if (!GetOpenFileName( &ofn ))
		return 0;

    return ofn.lpstrFile;
}

bool closing = false;

int EmulatorThread(void *data)
{
	for (;;)
	{
		if (closing)
			break;

		cpu.update();
	}

	return 0;
}

#if _DEBUG
auto CRT_CHECK_MEMORY = _CrtCheckMemory;
#endif

int dsp_last_sample = 0;

void DSPCallback(void *data, unsigned int len)
{
#if USE_SOUND
	if (samples_size * sizeof(SteroSample) < len)
	{
		// fill data with dsp_last_byte
#if 0
		int *a = (int*) data;
		for (int i = 0; i < len / 4; ++i)
		{
			*a = dsp_last_sample;
			++a;
		}
#endif
		return;
	}
	
	int toReadSamples = len / 4;
	int firstRead = min(toReadSamples, SAMPLES_SIZE - read_pos);

	memcpy(data, samples + read_pos, firstRead * sizeof(SteroSample));
	samples_size -= firstRead;
	read_pos = (read_pos + firstRead) % SAMPLES_SIZE;
	toReadSamples -= firstRead;

	if (toReadSamples > 0)
	{
		memcpy((char*)data + firstRead * sizeof(SteroSample), samples, toReadSamples * sizeof(SteroSample));
		samples_size -= toReadSamples;
		read_pos = (read_pos + toReadSamples) % SAMPLES_SIZE;
	}

	memcpy(&dsp_last_sample, (unsigned char*)data + len - 4, 4);
#endif
}

LARGE_INTEGER NFREQ;

int main(int argc, char *argv[])
{
	QueryPerformanceFrequency(&NFREQ);

#if CPU_TEST
    cpu.load_rom("nestest.nes");
	cpu.reset();
    int clocks = 15000;
	/*if (argc >= 2)
		lines = atoi(argv[1]);*/

	for (int i = 1; i <= clocks; ++i)
		cpu.clock();
#else
	SDLGraphicsSystem sdlGraphicsSystem;
	IGraphicsSystem::SetSystem(&sdlGraphicsSystem);

	char *romFileName = GetFilenameWin32();
	if (!romFileName)
		return 1;

	cpu.load_rom(romFileName);
	cpu.reset();

	IGraphicsSystem *pSystem = IGraphicsSystem::GetSystem();
	
	pSystem->SetDSPHandler(&DSPCallback);
	pSystem->SetKeyHandler(&KeyHandler);
	pSystem->Init(256 * 4, 240 * 4);

	SDL_CreateThread(&EmulatorThread, NULL);

	pSystem->RunSystem(&FrameUpdate);
#endif

    return 0;
}

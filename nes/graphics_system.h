#pragma once
#ifndef GRAPHICS_SYSTEM_H
#define GRAPHICS_SYSTEM_H

enum GS_KEYEVENT { KEYUP, KEYDOWN };

// Made same with SDL virtkeys for ease of implementation ONLY
enum GS_VIRTKEY {
	VIRTKEY_DOWN = 274,
	VIRTKEY_UP = 273,
	VIRTKEY_LEFT = 276,
	VIRTKEY_RIGHT = 275,
	VIRTKEY_C = 99,
	VIRTKEY_Q = 113,
	VIRTKEY_V = 118,
	VIRTKEY_W = 119,
	VIRTKEY_X = 120,
	VIRTKEY_Z = 122,
	VIRTKEY_RETURN = 13,
	VIRTKEY_LSHIFT = 304,
	VIRTKEY_JOY1 = 1000,
	VIRTKEY_JOY2 = 1001,
	VIRTKEY_JOY3 = 1002,
	VIRTKEY_JOY4 = 1003,
	VIRTKEY_JOY5 = 1004,
	VIRTKEY_JOY6 = 1005,
	VIRTKEY_JOY7 = 1006,
	VIRTKEY_JOY8 = 1007,
	VIRTKEY_JOY_SELECT = 1008,
	VIRTKEY_JOY_START = 1009,
};

typedef void FrameAction();
typedef void KeyEventHandler(GS_KEYEVENT keyEventType, GS_VIRTKEY virtKey);
typedef void DSPHandler(void *data, unsigned int len);

class IGraphicsSystem
{
public:
	virtual void Init(int width, int height) = 0;
	virtual void RunSystem(FrameAction *loop) = 0;
	virtual void SetKeyHandler(KeyEventHandler *handler) = 0;
	virtual void SetDSPHandler(DSPHandler *handler) = 0;

	virtual void Flip() = 0;
	virtual void SetWindowCaption(const char *text) = 0;

	static IGraphicsSystem *GetSystem() 
	{
		return pSystem;
	}

	static void SetSystem(IGraphicsSystem *pSystem) 
	{
		IGraphicsSystem::pSystem = pSystem;
	}

	virtual ~IGraphicsSystem() {};

	void PutPixel4x4(unsigned int x, unsigned int y, int argb)
	{
		if (x << 2 >= width || y << 2 >= height) return;

		for (int a = 0; a < 4; ++a)
		{
			unsigned int *ptr = reinterpret_cast<unsigned int*>(pixels + ((y << 2) + a) * stride) + (x << 2);
			*ptr = argb;
			*(ptr + 1) = argb;
			*(ptr + 2) = argb;
			*(ptr + 3) = argb;
		}
	}

	void PutPixel2x2(unsigned int x, unsigned int y, int argb)
	{
		if (x << 1 >= width || y << 1 >= height) return;

		for (int a = 0; a < 2; ++a)
		{
			unsigned int *ptr = reinterpret_cast<unsigned int*>(pixels + ((y << 1) + a) * stride) + (x << 1);
			*ptr = argb;
			*(ptr + 1) = argb;
		}
	}

private:
	static IGraphicsSystem *pSystem;

protected:
	unsigned int width;
	unsigned int height;
	unsigned char *pixels;
	unsigned int stride;
};

#endif


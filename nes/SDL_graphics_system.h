#pragma once
#ifndef SDL_GRAPHICS_SYSTEM_H
#define SDL_GRAPHICS_SYSTEM_H

#include <SDL2/SDL.h>
struct SDLGraphicsSystemImpl;

typedef void FrameAction();
typedef void KeyEventHandler(Uint32 keyEventType, SDL_Keycode virtKey);
typedef void DSPHandler(void *data, unsigned int len);

class SDLGraphicsSystem
{
public:
	static SDLGraphicsSystem* GetSystem();
	SDLGraphicsSystem();
	void Init(int width, int height);
	void RunSystem(FrameAction *loop);
	void Flip();
	void SetWindowCaption(const char *text);
	void SetKeyHandler(KeyEventHandler *handler);
	void SetDSPHandler(DSPHandler *handler);
	void PutPixel(unsigned int x, unsigned int y, int argb);
	~SDLGraphicsSystem();

private:
	SDLGraphicsSystemImpl *m_pImpl;
};

#endif


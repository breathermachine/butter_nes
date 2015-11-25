#pragma once
#ifndef SDL_GRAPHICS_SYSTEM_H
#define SDL_GRAPHICS_SYSTEM_H

#include "graphics_system.h"

struct SDLGraphicsSystemImpl;

class SDLGraphicsSystem : public IGraphicsSystem
{
public:
	SDLGraphicsSystem();
	virtual void Init(int width, int height);
	virtual void RunSystem(FrameAction *loop);
	virtual void Flip();
	virtual void SetWindowCaption(const char *text);
	virtual void SetKeyHandler(KeyEventHandler *handler);
	virtual void SetDSPHandler(DSPHandler *handler);

	virtual ~SDLGraphicsSystem();

private:
	SDLGraphicsSystemImpl *m_pImpl;
};

#endif


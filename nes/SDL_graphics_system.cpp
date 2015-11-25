#include "SDL_graphics_system.h"
#include "SDL.h"
#include <Windows.h>
#include <assert.h>

struct SDLGraphicsSystemImpl
{
	SDL_Surface *m_pSurface;
	KeyEventHandler *m_keyHandler;
	DSPHandler *m_dspHandler;

	SDLGraphicsSystemImpl() :
		m_pSurface(0),
		m_keyHandler(0),
		m_dspHandler(0)
	{

	}
};

SDLGraphicsSystem::SDLGraphicsSystem() :
	m_pImpl(new SDLGraphicsSystemImpl)
{ 
}

static void SDLCALL SDLDSPCallback(void *userdata, Uint8 *stream, int len)
{
	SDLGraphicsSystemImpl *impl = static_cast<SDLGraphicsSystemImpl *>(userdata);
	impl->m_dspHandler(stream, len);
}

void SDLGraphicsSystem::Init(int width, int height)
{
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO);
	this->width = width;
	this->height = height;
	m_pImpl->m_pSurface = SDL_SetVideoMode(width, height, 0, SDL_DOUBLEBUF);
	pixels = static_cast<unsigned char*>(m_pImpl->m_pSurface->pixels);
	stride = m_pImpl->m_pSurface->pitch;

	SDL_AudioSpec desired, obtained;
	ZeroMemory(&desired, sizeof(desired));
	desired.freq = 44100;
	desired.format = AUDIO_S16LSB;
	desired.channels = 2;
	desired.samples = 4096;
	desired.callback = &SDLDSPCallback;
	desired.userdata = m_pImpl;

	ZeroMemory(&obtained, sizeof(obtained));

	int ret = 0;
	ret = SDL_Init(SDL_INIT_AUDIO);
	ret = SDL_OpenAudio(&desired, &obtained);
	SDL_PauseAudio(0);
}

void SDLGraphicsSystem::SetKeyHandler(KeyEventHandler *handler)
{
	m_pImpl->m_keyHandler = handler;
}

void SDLGraphicsSystem::SetDSPHandler(DSPHandler *handler)
{
	m_pImpl->m_dspHandler = handler;
}

void SDLGraphicsSystem::RunSystem(FrameAction *loop)
{
	SDL_Event event;
	bool quit = false;

	for(;;)
	{
		for (;;)
		{
			if (!SDL_PollEvent(&event))
				break;

			// Process the event
			if (event.type == SDL_QUIT)
			{
				extern bool closing;
				closing = true;
				quit = true;
				break;
			}
			else if (event.type == SDL_KEYDOWN)
			{
				m_pImpl->m_keyHandler(GS_KEYEVENT::KEYDOWN, static_cast<GS_VIRTKEY>(event.key.keysym.sym));
			}
			else if (event.type == SDL_KEYUP)
			{
				m_pImpl->m_keyHandler(GS_KEYEVENT::KEYUP, static_cast<GS_VIRTKEY>(event.key.keysym.sym));
			}
		}

		if (quit)
			break;

		// Process FrameAction
		loop();
		SDL_Delay(17);
	}
}

void SDLGraphicsSystem::Flip()
{
	SDL_Flip(m_pImpl->m_pSurface);
}

void SDLGraphicsSystem::SetWindowCaption(const char *text)
{
	SDL_WM_SetCaption(text, text);
}

SDLGraphicsSystem::~SDLGraphicsSystem()
{
	delete m_pImpl;
}

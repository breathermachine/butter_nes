#include "SDL_graphics_system.h"
#include "SDL2/SDL.h"
#include <assert.h>

#ifdef _WIN32
#include <Windows.h>
#endif

struct SDLGraphicsSystemImpl
{
	SDL_Window *m_pWindow;
	SDL_Renderer *m_pRenderer;
	SDL_Texture *m_pTexture;

	//SDL_Surface *m_pSurface;
	KeyEventHandler *m_keyHandler;
	DSPHandler *m_dspHandler;
	SDL_Joystick *m_pJoystick;
	int m_width;
	int m_height;
	unsigned char *m_pixels;
	int m_stride;

	SDLGraphicsSystemImpl() :
		m_pWindow(0),
		m_pRenderer(0),
		m_pTexture(0),
		m_keyHandler(0),
		m_dspHandler(0),
		m_pJoystick(0)
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
	SDL_Init(SDL_INIT_VIDEO | SDL_INIT_AUDIO | SDL_INIT_JOYSTICK);

	if (SDL_NumJoysticks() >= 1)
		m_pImpl->m_pJoystick = SDL_JoystickOpen(0);

	m_pImpl->m_width = width;
	m_pImpl->m_height = height;

	SDL_CreateWindowAndRenderer(width, height, SDL_WINDOW_OPENGL, &m_pImpl->m_pWindow, &m_pImpl->m_pRenderer);

	m_pImpl->m_pTexture = SDL_CreateTexture(m_pImpl->m_pRenderer,
                               SDL_PIXELFORMAT_ARGB8888,
                               SDL_TEXTUREACCESS_STREAMING,
                               width, height);

	m_pImpl->m_pixels = (unsigned char*) malloc(width * height * 4);
	m_pImpl->m_stride = width * 4;

	SDL_AudioSpec desired, obtained;
	memset(&desired, 0, sizeof(desired));
	desired.freq = 44100;
	desired.format = AUDIO_S16LSB;
	desired.channels = 2;
	desired.samples = 4096;
	desired.callback = &SDLDSPCallback;
	desired.userdata = m_pImpl;

	memset(&obtained, 0, sizeof(desired));

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
				m_pImpl->m_keyHandler(SDL_KEYDOWN, event.key.keysym.sym);
			}
			else if (event.type == SDL_KEYUP)
			{
				m_pImpl->m_keyHandler(SDL_KEYUP, event.key.keysym.sym);
			}
			else if (event.type == SDL_JOYBUTTONDOWN)
			{
				m_pImpl->m_keyHandler(SDL_KEYDOWN, event.jbutton.button);
			}
			else if (event.type == SDL_JOYBUTTONUP)
			{
				m_pImpl->m_keyHandler(SDL_KEYUP, event.jbutton.button);
			}
			else if (event.type == SDL_JOYAXISMOTION)
			{
				int axis = event.jaxis.axis;
				int axisvalue = event.jaxis.value;

				if (axis == 0) // x-axis
				{
					if (axisvalue == 32767) // right pressed
						m_pImpl->m_keyHandler(SDL_KEYDOWN, SDLK_RIGHT);
					else if (axisvalue == -32768) // left pressed
						m_pImpl->m_keyHandler(SDL_KEYDOWN, SDLK_LEFT);
					else if (axisvalue == -257) // left/right released
					{
						m_pImpl->m_keyHandler(SDL_KEYUP, SDLK_RIGHT);
						m_pImpl->m_keyHandler(SDL_KEYUP, SDLK_LEFT);
					}
				}
				else if (axis == 1) // y-axis
				{
					if (axisvalue == 32767) // down pressed
						m_pImpl->m_keyHandler(SDL_KEYDOWN, SDLK_DOWN);
					else if (axisvalue == -32768) // up pressed
						m_pImpl->m_keyHandler(SDL_KEYDOWN, SDLK_UP);
					else if (axisvalue == -257) // down/up released
					{
						m_pImpl->m_keyHandler(SDL_KEYUP, SDLK_DOWN);
						m_pImpl->m_keyHandler(SDL_KEYUP, SDLK_UP);
					}
				}
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
	SDL_UpdateTexture(m_pImpl->m_pTexture, NULL, m_pImpl->m_pixels, m_pImpl->m_width * sizeof (Uint32));
	SDL_RenderClear(m_pImpl->m_pRenderer);
	SDL_RenderCopy(m_pImpl->m_pRenderer, m_pImpl->m_pTexture, NULL, NULL);
	SDL_RenderPresent(m_pImpl->m_pRenderer);
}

void SDLGraphicsSystem::SetWindowCaption(const char *text)
{
	SDL_SetWindowTitle(m_pImpl->m_pWindow, text);
}

SDLGraphicsSystem::~SDLGraphicsSystem()
{
	delete m_pImpl;
}

void SDLGraphicsSystem::PutPixel(unsigned int x, unsigned int y, int argb)
{
    if (x >= m_pImpl->m_width || y >= m_pImpl->m_height) return;

    unsigned int *ptr = reinterpret_cast<unsigned int*>(m_pImpl->m_pixels + y * m_pImpl->m_stride) + x;
    *ptr = argb;
}

static SDLGraphicsSystem thesystem;

SDLGraphicsSystem* SDLGraphicsSystem::GetSystem() {
	return &thesystem;
}

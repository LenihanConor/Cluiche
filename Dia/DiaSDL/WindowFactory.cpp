////////////////////////////////////////////////////////////////////////////////
// Filename: WindowFactory.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaSDL/WindowFactory.h"
#include "DiaSDL/Window.h"

#include <DiaCore/Memory/Memory.h>
#include <DiaCore/Core/Assert.h>
#include <DiaObservation/Log/DiaLog.h>

#include <SDL3/SDL.h>

namespace Dia
{
	namespace SDL
	{
		//-------------------------------------------------------------------------------------
		Dia::Window::IWindow* WindowFactory::Create(const Dia::Window::IWindow::Settings& windowSetting)
		{
			SDL_Init(SDL_INIT_VIDEO);
			DIA_LOG_INFO("Application", "DiaSDL WindowFactory: SDL_Init + window created");
			Window* window = DIA_NEW(Window(windowSetting));
			window->Initialize(windowSetting);
			return window;
		}

		//-------------------------------------------------------------------------------------
		void WindowFactory::Destroy(Dia::Window::IWindow* window)
		{
			DIA_ASSERT(window, "Window is NULL");
			if (window)
			{
				DIA_DELETE(window);
				SDL_Quit();
				DIA_LOG_INFO("Application", "DiaSDL WindowFactory: window destroyed + SDL_Quit");
			}
		}
	}
}

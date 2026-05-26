////////////////////////////////////////////////////////////////////////////////
// Filename: WindowFactory.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaSFML/WindowFactory.h"
#include "DiaSFML/Window.h"
#include <DiaCore/Memory/Memory.h>

namespace Dia
{
	namespace SFML
	{
		//-------------------------------------------------------------------------------------
		Dia::Window::IWindow* WindowFactory::Create(const Dia::Window::IWindow::Settings& windowSetting)
		{
			Window* window = DIA_NEW(Window(windowSetting));
			window->Initialize(windowSetting);
			return window;
		}

		//-------------------------------------------------------------------------------------
		void WindowFactory::Destroy(Dia::Window::IWindow* window)
		{
			DIA_ASSERT(window, "Window is NULL");
			if (window)
				DIA_DELETE(window);
		}
	}
}

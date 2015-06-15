////////////////////////////////////////////////////////////////////////////////
// Filename: IWindow.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaGraphics/Interface/ICanvas.h>
#include <DiaWindow/Interface/IWindow.h>

#include <DiaWindow/Interface/IWindowFactory.h>

namespace Dia
{

	namespace Window
	{
		////////////////////////////////////////////////////////////////////////////////
		// Class name: IWindowFactory
		////////////////////////////////////////////////////////////////////////////////
		class IWindowFactory
		{
		public:
			virtual ~IWindowFactory(){};

			virtual Window::IWindow* Create(const Window::IWindow::Settings& windowSetting, const Graphics::ICanvas::Settings& canvasSettings) = 0;
			virtual void Destroy(Window::IWindow* window) = 0;
		};
	}
}
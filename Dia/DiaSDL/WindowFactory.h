////////////////////////////////////////////////////////////////////////////////
// Filename: WindowFactory.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaWindow/Interface/IWindowFactory.h>

namespace Dia
{
	namespace SDL
	{
		class WindowFactory : public Dia::Window::IWindowFactory
		{
		public:
			virtual ~WindowFactory() {}

			virtual Dia::Window::IWindow* Create(const Dia::Window::IWindow::Settings& windowSetting) override;
			virtual void Destroy(Dia::Window::IWindow* window) override;
		};
	}
}

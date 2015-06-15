////////////////////////////////////////////////////////////////////////////////
// Filename: RenderWindowFactory.h
////////////////////////////////////////////////////////////////////////////////
#pragma once



#include <DiaWindow/Interface/IWindowFactory.h>

namespace Dia
{
	namespace SFML
	{
		////////////////////////////////////////////////////////////////////////////////
		// Enum name: RenderWindowFactory
		////////////////////////////////////////////////////////////////////////////////
		class RenderWindowFactory: public Window::IWindowFactory
		{
		public:
			
			virtual ~RenderWindowFactory(){}

			virtual Window::IWindow* Create(const Window::IWindow::Settings& windowSetting, const Graphics::ICanvas::Settings& canvasSettings)override;
			virtual void Destroy(Window::IWindow* window)override;
		};
	}
}
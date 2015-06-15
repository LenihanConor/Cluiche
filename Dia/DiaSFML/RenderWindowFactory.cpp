////////////////////////////////////////////////////////////////////////////////
// Filename: Canvas
////////////////////////////////////////////////////////////////////////////////
#include "DiaSFML/RenderWindowFactory.h"

#include "DiaSFML/RenderWindow.h"

namespace Dia
{
	namespace SFML
	{
		//-------------------------------------------------------------------------------------
		Window::IWindow* RenderWindowFactory::Create(const Window::IWindow::Settings& windowSetting,
														const Graphics::ICanvas::Settings& canvasSettings)
		{		
			RenderWindow* renderWindow = DIA_NEW(RenderWindow(windowSetting, canvasSettings));

			renderWindow->Initialize(windowSetting);
			renderWindow->Initialize(canvasSettings);

			return renderWindow;
		}
		
		//-------------------------------------------------------------------------------------
		void RenderWindowFactory::Destroy(Window::IWindow* window)
		{
			DIA_ASSERT(window, "Window is NULL");

			if (window)
			{
				DIA_DELETE(window);
			}
		}
	}
}
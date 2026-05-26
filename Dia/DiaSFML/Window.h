////////////////////////////////////////////////////////////////////////////////
// Filename: Window.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaWindow/Interface/IWindow.h>
#include "DiaSFML/InputSource.h"

namespace sf
{
	class Window;
}

namespace Dia
{
	namespace SFML
	{
		////////////////////////////////////////////////////////////////////////////////
		// Class name: Window — IWindow + IInputSource; no ICanvas
		////////////////////////////////////////////////////////////////////////////////
		class Window : public Dia::Window::IWindow, public InputSource
		{
		public:
			friend class WindowFactory;

			virtual ~Window();

			// Inherited from IWindow
			virtual void Initialize(const Dia::Window::IWindow::Settings& settings) override;
			virtual void Close() override;
			virtual bool IsOpen() const override;
			virtual Maths::Vector2D GetPosition() const override;
			virtual void SetPosition(const Maths::Vector2D& position) override;
			virtual Maths::Vector2D GetSize() const override;
			virtual void SetSize(const Maths::Vector2D& size) override;
			virtual void SetTitle(const Core::Containers::String64& title) override;
			virtual void SetIcon(unsigned int width, unsigned int height, const unsigned char* pixels) override;
			virtual void SetVisible(bool visible) override;
			virtual bool SetActive(bool active = true) const override;
			virtual void SetMouseCursorVisible(bool visible) override;
			virtual Dia::Window::SystemHandle GetSystemHandle() const override;

		protected:
			virtual void OnRawSFMLEvent(const sf::Event& event) override;

		private:
			Window();
			Window(const Dia::Window::IWindow::Settings& windowSetting);

			sf::Window* mWindowContext;
		};
	}
}

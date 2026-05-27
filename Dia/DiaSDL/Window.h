////////////////////////////////////////////////////////////////////////////////
// Filename: Window.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaWindow/Interface/IWindow.h>
#include "DiaSDL/InputSource.h"

struct SDL_Window;
union SDL_Event;

namespace Dia
{
	namespace SDL
	{
		class Window : public Dia::Window::IWindow, public InputSource
		{
		public:
			friend class WindowFactory;

			virtual ~Window();

			void Initialize(const Dia::Window::IWindow::Settings& settings) override;
			void Close() override;
			bool IsOpen() const override;
			Maths::Vector2D GetPosition() const override;
			void SetPosition(const Maths::Vector2D& position) override;
			Maths::Vector2D GetSize() const override;
			void SetSize(const Maths::Vector2D& size) override;
			void SetTitle(const Core::Containers::String64& title) override;
			void SetIcon(unsigned int width, unsigned int height, const unsigned char* pixels) override;
			void SetVisible(bool visible) override;
			void SetMouseCursorVisible(bool visible) override;
			Dia::Window::SystemHandle GetSystemHandle() const override;

		protected:
			virtual void OnRawSDLEvent(const SDL_Event& /*event*/) override {}

		private:
			Window();
			Window(const Dia::Window::IWindow::Settings& windowSetting);

			SDL_Window* mWindowContext;
			bool        mIsOpen;
		};
	}
}

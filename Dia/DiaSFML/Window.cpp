////////////////////////////////////////////////////////////////////////////////
// Filename: Window.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaSFML/Window.h"

#include <DiaCore/Memory/Memory.h>
#include <DiaCore/Strings/stringutils.h>

#include <SFML/Window/Window.hpp>
#include <SFML/Window/VideoMode.hpp>
#include <SFML/Window/Event.hpp>

namespace Dia
{
	namespace SFML
	{
		//-------------------------------------------------------------------------------------
		Window::Window()
			: mWindowContext(nullptr)
		{
		}

		//-------------------------------------------------------------------------------------
		Window::Window(const Dia::Window::IWindow::Settings& windowSetting)
			: mWindowContext(nullptr)
		{
			wchar_t titleBuffer[1024];
			Dia::Core::StringToWString(windowSetting.GetTitle().AsCStr(), &titleBuffer[0]);
			std::wstring titleWStr(&titleBuffer[0]);

			unsigned int style = windowSetting.GetStyle().GetAllBits();

			sf::VideoMode videoMode({ windowSetting.GetDimensions().GetWidth(),
			                          windowSetting.GetDimensions().GetHeight() },
			                        windowSetting.GetDimensions().GetBitsPerPixel());

			sf::ContextSettings ctx;
			ctx.depthBits = 0;
			ctx.stencilBits = 0;
			ctx.antiAliasingLevel = 0;
			ctx.majorVersion = 3;
			ctx.minorVersion = 0;
			ctx.sRgbCapable = false;

			mWindowContext = DIA_NEW(sf::Window(videoMode, sf::String(titleWStr),
			                                    style, sf::State::Windowed, ctx));

			InputSource::SetWindowContext(mWindowContext);
		}

		//-------------------------------------------------------------------------------------
		Window::~Window()
		{
			DIA_ASSERT(mWindowContext, "Window is NULL");
			DIA_DELETE(mWindowContext);
		}

		//-------------------------------------------------------------------------------------
		void Window::OnRawSFMLEvent(const sf::Event& /*event*/)
		{
			// Win32WndProcChain is installed/uninstalled by BgfxImGuiBackend in KernelModule.
			// Nothing to forward here.
		}

		//-------------------------------------------------------------------------------------
		void Window::Initialize(const Dia::Window::IWindow::Settings& /*settings*/)
		{
		}

		//-------------------------------------------------------------------------------------
		void Window::Close()
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
				mWindowContext->close();
		}

		//-------------------------------------------------------------------------------------
		bool Window::IsOpen() const
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			return mWindowContext ? mWindowContext->isOpen() : false;
		}

		//-------------------------------------------------------------------------------------
		Maths::Vector2D Window::GetPosition() const
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
			{
				auto pos = mWindowContext->getPosition();
				return Maths::Vector2D(static_cast<float>(pos.x), static_cast<float>(pos.y));
			}
			return Maths::Vector2D::Zero();
		}

		//-------------------------------------------------------------------------------------
		void Window::SetPosition(const Maths::Vector2D& position)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
				mWindowContext->setPosition({ static_cast<int>(position.x), static_cast<int>(position.y) });
		}

		//-------------------------------------------------------------------------------------
		Maths::Vector2D Window::GetSize() const
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
			{
				auto sz = mWindowContext->getSize();
				return Maths::Vector2D(static_cast<float>(sz.x), static_cast<float>(sz.y));
			}
			return Maths::Vector2D::Zero();
		}

		//-------------------------------------------------------------------------------------
		void Window::SetSize(const Maths::Vector2D& size)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
				mWindowContext->setSize({ static_cast<unsigned int>(size.x), static_cast<unsigned int>(size.y) });
		}

		//-------------------------------------------------------------------------------------
		void Window::SetTitle(const Core::Containers::String64& title)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
				mWindowContext->setTitle(sf::String(title.AsCStr()));
		}

		//-------------------------------------------------------------------------------------
		void Window::SetIcon(unsigned int width, unsigned int height, const unsigned char* pixels)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
				mWindowContext->setIcon({ width, height }, pixels);
		}

		//-------------------------------------------------------------------------------------
		void Window::SetVisible(bool visible)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
				mWindowContext->setVisible(visible);
		}

		//-------------------------------------------------------------------------------------
		void Window::SetMouseCursorVisible(bool visible)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
				mWindowContext->setMouseCursorVisible(visible);
		}

		//-------------------------------------------------------------------------------------
		Dia::Window::SystemHandle Window::GetSystemHandle() const
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			return mWindowContext->getNativeHandle();
		}
	}
}

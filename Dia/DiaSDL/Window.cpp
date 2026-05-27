////////////////////////////////////////////////////////////////////////////////
// Filename: Window.cpp
////////////////////////////////////////////////////////////////////////////////
#include "DiaSDL/Window.h"

#include <SDL3/SDL.h>
#include <DiaCore/Memory/Memory.h>
#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace SDL
	{
		//-------------------------------------------------------------------------------------
		Window::Window()
			: mWindowContext(nullptr)
			, mIsOpen(false)
		{
		}

		//-------------------------------------------------------------------------------------
		Window::Window(const Dia::Window::IWindow::Settings& windowSetting)
			: mWindowContext(nullptr)
			, mIsOpen(false)
		{
			mWindowContext = SDL_CreateWindow(
				windowSetting.GetTitle().AsCStr(),
				static_cast<int>(windowSetting.GetDimensions().GetWidth()),
				static_cast<int>(windowSetting.GetDimensions().GetHeight()),
				SDL_WINDOW_RESIZABLE);

			if (mWindowContext)
			{
				mIsOpen = true;
				InputSource::SetWindowContext(mWindowContext);
			}
		}

		//-------------------------------------------------------------------------------------
		Window::~Window()
		{
			if (mWindowContext)
			{
				SDL_DestroyWindow(mWindowContext);
				mWindowContext = nullptr;
			}
		}

		//-------------------------------------------------------------------------------------
		// Initialize is a no-op — window is created in the constructor,
		// matching the DiaSFML pattern.
		void Window::Initialize(const Dia::Window::IWindow::Settings& /*settings*/)
		{
		}

		//-------------------------------------------------------------------------------------
		void Window::Close()
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
			{
				SDL_HideWindow(mWindowContext);
				mIsOpen = false;
			}
		}

		//-------------------------------------------------------------------------------------
		bool Window::IsOpen() const
		{
			return mWindowContext != nullptr && mIsOpen;
		}

		//-------------------------------------------------------------------------------------
		Maths::Vector2D Window::GetPosition() const
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
			{
				int x = 0;
				int y = 0;
				SDL_GetWindowPosition(mWindowContext, &x, &y);
				return Maths::Vector2D(static_cast<float>(x), static_cast<float>(y));
			}
			return Maths::Vector2D::Zero();
		}

		//-------------------------------------------------------------------------------------
		void Window::SetPosition(const Maths::Vector2D& position)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
				SDL_SetWindowPosition(mWindowContext,
					static_cast<int>(position.x),
					static_cast<int>(position.y));
		}

		//-------------------------------------------------------------------------------------
		Maths::Vector2D Window::GetSize() const
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
			{
				int w = 0;
				int h = 0;
				SDL_GetWindowSize(mWindowContext, &w, &h);
				return Maths::Vector2D(static_cast<float>(w), static_cast<float>(h));
			}
			return Maths::Vector2D::Zero();
		}

		//-------------------------------------------------------------------------------------
		void Window::SetSize(const Maths::Vector2D& size)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
				SDL_SetWindowSize(mWindowContext,
					static_cast<int>(size.x),
					static_cast<int>(size.y));
		}

		//-------------------------------------------------------------------------------------
		void Window::SetTitle(const Core::Containers::String64& title)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
				SDL_SetWindowTitle(mWindowContext, title.AsCStr());
		}

		//-------------------------------------------------------------------------------------
		// SetIcon is a stub — SDL_SetWindowIcon requires an SDL_Surface.
		// Deferred as a no-op for now (matching spec decision).
		void Window::SetIcon(unsigned int /*width*/, unsigned int /*height*/, const unsigned char* /*pixels*/)
		{
			// No-op: SDL_SetWindowIcon requires constructing an SDL_Surface from raw pixels.
			// Deferred until DiaSDL adds an SDL_Surface utility helper.
		}

		//-------------------------------------------------------------------------------------
		void Window::SetVisible(bool visible)
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (mWindowContext)
			{
				if (visible)
					SDL_ShowWindow(mWindowContext);
				else
					SDL_HideWindow(mWindowContext);
			}
		}

		//-------------------------------------------------------------------------------------
		void Window::SetMouseCursorVisible(bool visible)
		{
			if (visible)
				SDL_ShowCursor();
			else
				SDL_HideCursor();
		}

		//-------------------------------------------------------------------------------------
		Dia::Window::SystemHandle Window::GetSystemHandle() const
		{
			DIA_ASSERT(mWindowContext, "mWindowContext is NULL");
			if (!mWindowContext)
				return static_cast<Dia::Window::SystemHandle>(nullptr);

			SDL_PropertiesID props = SDL_GetWindowProperties(mWindowContext);

#if defined(WIN32)
			return static_cast<Dia::Window::SystemHandle>(
				SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WIN32_HWND_POINTER, nullptr));
#elif defined(__linux__)
			// Try X11 first, fall back to Wayland surface pointer
			void* handle = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_X11_WINDOW_POINTER, nullptr);
			if (!handle)
				handle = SDL_GetPointerProperty(props, SDL_PROP_WINDOW_WAYLAND_SURFACE_POINTER, nullptr);
			return static_cast<Dia::Window::SystemHandle>(handle);
#elif defined(ANDROID_OS)
			return static_cast<Dia::Window::SystemHandle>(
				SDL_GetPointerProperty(props, SDL_PROP_WINDOW_ANDROID_WINDOW_POINTER, nullptr));
#elif defined(__APPLE__) && defined(TARGET_OS_IOS) && TARGET_OS_IOS
			return static_cast<Dia::Window::SystemHandle>(
				SDL_GetPointerProperty(props, SDL_PROP_WINDOW_UIKIT_WINDOW_POINTER, nullptr));
#else
			return static_cast<Dia::Window::SystemHandle>(nullptr);
#endif
		}
	}
}

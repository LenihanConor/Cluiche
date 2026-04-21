#pragma once

#include <DiaWindow/Interface/IWindow.h>
#include <DiaWindow/Interface/IWindowFactory.h>
#include <DiaWindow/SystemHandle.h>

#include <functional>

namespace Dia
{
	namespace Window
	{
		using CloseCallback = std::function<void()>;
		using ResizeCallback = std::function<void(int, int)>;

		class Win32Window : public IWindow
		{
		public:
			Win32Window();
			~Win32Window();

			void SetCloseCallback(CloseCallback cb);
			void SetResizeCallback(ResizeCallback cb);

			void Initialize(const Settings& settings) override;
			void Close() override;
			bool IsOpen() const override;

			Maths::Vector2D GetPosition() const override;
			void SetPosition(const Maths::Vector2D& position) override;
			Maths::Vector2D GetSize() const override;
			void SetSize(const Maths::Vector2D& size) override;
			void SetTitle(const Core::Containers::String64& title) override;
			void SetIcon(unsigned int width, unsigned int height, const unsigned char* pixels) override;
			void SetVisible(bool visible) override;
			bool SetActive(bool active) const override;
			void SetMouseCursorVisible(bool visible) override;
			SystemHandle GetSystemHandle() const override;

			void PumpMessages();

		public:
			static long __stdcall WndProc(SystemHandle hwnd, unsigned int msg, unsigned long long wParam, long long lParam);

		private:
			SystemHandle mHwnd;
			bool mIsOpen;
			CloseCallback mCloseCallback;
			ResizeCallback mResizeCallback;

			static Win32Window* sLastCreated;
		};

		class Win32WindowFactory : public IWindowFactory
		{
		public:
			IWindow* Create(const IWindow::Settings& windowSettings, const Graphics::ICanvas::Settings& canvasSettings) override;
			void Destroy(IWindow* window) override;
		};
	}
}

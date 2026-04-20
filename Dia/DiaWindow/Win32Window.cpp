#define WIN32_LEAN_AND_MEAN
#define NOMINMAX
#include <windows.h>

#include "Win32Window.h"

#include <DiaCore/Core/Assert.h>

namespace Dia
{
	namespace Window
	{
		Win32Window* Win32Window::sLastCreated = nullptr;

		static const wchar_t* kWin32WindowClass = L"DiaWin32Window";
		static bool sClassRegistered = false;

		static void EnsureClassRegistered(HINSTANCE hInstance)
		{
			if (sClassRegistered)
				return;

			WNDCLASSEXW wc = {};
			wc.cbSize = sizeof(WNDCLASSEXW);
			wc.lpfnWndProc = reinterpret_cast<WNDPROC>(Win32Window::WndProc);
			wc.hInstance = hInstance;
			wc.hCursor = LoadCursor(nullptr, IDC_ARROW);
			wc.hbrBackground = (HBRUSH)(COLOR_WINDOW + 1);
			wc.lpszClassName = kWin32WindowClass;
			RegisterClassExW(&wc);
			sClassRegistered = true;
		}

		Win32Window::Win32Window()
			: mHwnd(nullptr)
			, mIsOpen(false)
		{
		}

		Win32Window::~Win32Window()
		{
			if (mIsOpen)
				Close();
		}

		void Win32Window::SetCloseCallback(CloseCallback cb)
		{
			mCloseCallback = cb;
		}

		void Win32Window::Initialize(const Settings& settings)
		{
			HINSTANCE hInstance = GetModuleHandleW(nullptr);
			EnsureClassRegistered(hInstance);

			sLastCreated = this;

			int sw = static_cast<int>(settings.GetDimensions().GetWidth())  + 16;
			int sh = static_cast<int>(settings.GetDimensions().GetHeight()) + 39;

			// Convert ASCII title to wide
			const char* titleA = settings.GetTitle().AsCStr();
			wchar_t titleW[64] = {};
			MultiByteToWideChar(CP_ACP, 0, titleA, -1, titleW, 64);

			mHwnd = CreateWindowExW(
				0, kWin32WindowClass, titleW,
				WS_OVERLAPPEDWINDOW,
				CW_USEDEFAULT, CW_USEDEFAULT, sw, sh,
				nullptr, nullptr, hInstance, nullptr);

			DIA_ASSERT(mHwnd != nullptr, "Win32Window: CreateWindowEx failed");
			if (!mHwnd)
				return;

			mIsOpen = true;
			ShowWindow(static_cast<HWND>(mHwnd), SW_SHOW);
			UpdateWindow(static_cast<HWND>(mHwnd));
		}

		void Win32Window::Close()
		{
			if (mHwnd)
			{
				DestroyWindow(static_cast<HWND>(mHwnd));
				mHwnd = nullptr;
			}
			mIsOpen = false;
		}

		bool Win32Window::IsOpen() const { return mIsOpen; }

		Maths::Vector2D Win32Window::GetPosition() const
		{
			RECT r; GetWindowRect(static_cast<HWND>(mHwnd), &r);
			return Maths::Vector2D(static_cast<float>(r.left), static_cast<float>(r.top));
		}

		void Win32Window::SetPosition(const Maths::Vector2D& pos)
		{
			SetWindowPos(static_cast<HWND>(mHwnd), nullptr,
				static_cast<int>(pos.X()), static_cast<int>(pos.Y()), 0, 0, SWP_NOSIZE | SWP_NOZORDER);
		}

		Maths::Vector2D Win32Window::GetSize() const
		{
			RECT r; GetClientRect(static_cast<HWND>(mHwnd), &r);
			return Maths::Vector2D(static_cast<float>(r.right - r.left), static_cast<float>(r.bottom - r.top));
		}

		void Win32Window::SetSize(const Maths::Vector2D& size)
		{
			SetWindowPos(static_cast<HWND>(mHwnd), nullptr, 0, 0,
				static_cast<int>(size.X()), static_cast<int>(size.Y()), SWP_NOMOVE | SWP_NOZORDER);
		}

		void Win32Window::SetTitle(const Core::Containers::String64& title)
		{
			SetWindowTextA(static_cast<HWND>(mHwnd), title.AsCStr());
		}

		void Win32Window::SetIcon(unsigned int, unsigned int, const unsigned char*) {}

		void Win32Window::SetVisible(bool visible)
		{
			ShowWindow(static_cast<HWND>(mHwnd), visible ? SW_SHOW : SW_HIDE);
		}

		bool Win32Window::SetActive(bool) const { return true; }

		void Win32Window::SetMouseCursorVisible(bool visible)
		{
			ShowCursor(visible ? TRUE : FALSE);
		}

		SystemHandle Win32Window::GetSystemHandle() const { return mHwnd; }

		void Win32Window::PumpMessages()
		{
			MSG msg;
			while (PeekMessageW(&msg, nullptr, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}

		long __stdcall Win32Window::WndProc(SystemHandle hwnd, unsigned int msg, unsigned long long wParam, long long lParam)
		{
			switch (msg)
			{
			case WM_CLOSE:
				if (sLastCreated && sLastCreated->mCloseCallback)
					sLastCreated->mCloseCallback();
				return 0;
			case WM_DESTROY:
				if (sLastCreated)
					sLastCreated->mIsOpen = false;
				PostQuitMessage(0);
				return 0;
			}
			return static_cast<long>(DefWindowProcW(
				static_cast<HWND>(hwnd),
				msg,
				static_cast<WPARAM>(wParam),
				static_cast<LPARAM>(lParam)));
		}

		IWindow* Win32WindowFactory::Create(const IWindow::Settings& settings, const Graphics::ICanvas::Settings&)
		{
			Win32Window* window = new Win32Window();
			window->Initialize(settings);
			return window;
		}

		void Win32WindowFactory::Destroy(IWindow* window)
		{
			delete window;
		}
	}
}

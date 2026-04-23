#include "SplashScreenModule.h"

namespace Cluiche
{
	namespace Editor
	{
		static const wchar_t* kSplashWindowClass = L"CluicheSplashWindow";

		static LRESULT CALLBACK SplashWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
		{
			if (msg == WM_PAINT)
			{
				PAINTSTRUCT ps;
				HDC hdc = BeginPaint(hwnd, &ps);

				HBITMAP hbmp = (HBITMAP)LoadImageW(nullptr,
					L"Assets\\CluicheEditor\\splash-logo.bmp",
					IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE);

				if (hbmp)
				{
					RECT rc;
					GetClientRect(hwnd, &rc);

					BITMAP bm;
					GetObject(hbmp, sizeof(bm), &bm);

					HDC memDC = CreateCompatibleDC(hdc);
					HGDIOBJ old = SelectObject(memDC, hbmp);
					StretchBlt(hdc, 0, 0, rc.right, rc.bottom, memDC, 0, 0, bm.bmWidth, bm.bmHeight, SRCCOPY);
					SelectObject(memDC, old);
					DeleteDC(memDC);
					DeleteObject(hbmp);
				}
				else
				{
					// BMP not found — fill with editor background colour so there is no white flash
					HBRUSH bg = CreateSolidBrush(RGB(30, 30, 30));
					RECT rc; GetClientRect(hwnd, &rc);
					FillRect(hdc, &rc, bg);
					DeleteObject(bg);
				}

				EndPaint(hwnd, &ps);
				return 0;
			}
			return DefWindowProcW(hwnd, msg, wParam, lParam);
		}

		const Dia::Core::StringCRC SplashScreenModule::kTypeId("SplashScreenModule");

		SplashScreenModule::SplashScreenModule(Dia::Application::ProcessingUnit* pu)
			: Dia::Application::Module(pu, kTypeId, RunningEnum::kNone)
			, mHwnd(nullptr)
		{
		}

		Dia::Application::StateObject::OpertionResponse SplashScreenModule::DoStart(const Dia::Application::StateObject::IStartData*)
		{
			HINSTANCE hInstance = GetModuleHandleW(nullptr);

			WNDCLASSEXW wc = {};
			wc.cbSize        = sizeof(WNDCLASSEXW);
			wc.lpfnWndProc   = SplashWndProc;
			wc.hInstance     = hInstance;
			wc.hbrBackground = (HBRUSH)GetStockObject(BLACK_BRUSH);
			wc.lpszClassName = kSplashWindowClass;
			RegisterClassExW(&wc);

			int screenW = GetSystemMetrics(SM_CXSCREEN);
			int screenH = GetSystemMetrics(SM_CYSCREEN);
			int x = (screenW - kWidth)  / 2;
			int y = (screenH - kHeight) / 2;

			mHwnd = CreateWindowExW(
				WS_EX_TOPMOST,
				kSplashWindowClass, L"",
				WS_POPUP | WS_VISIBLE,
				x, y, kWidth, kHeight,
				nullptr, nullptr, hInstance, nullptr);

			if (mHwnd)
				UpdateWindow(mHwnd);

			return Dia::Application::StateObject::OpertionResponse::kImmediate;
		}

		void SplashScreenModule::Dismiss()
		{
			if (mHwnd)
			{
				DestroyWindow(mHwnd);
				mHwnd = nullptr;
			}
		}

		void SplashScreenModule::DoStop()
		{
			Dismiss();
		}
	}
}

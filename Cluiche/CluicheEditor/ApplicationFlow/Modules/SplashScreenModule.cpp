#include "SplashScreenModule.h"

#include <DiaApplicationFlow/RegistrationMacrosV2.h>

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
					L"assets\\icons\\splash-logo.bmp",
					IMAGE_BITMAP, 0, 0, LR_LOADFROMFILE | LR_CREATEDIBSECTION);

				if (hbmp)
				{
					RECT rc;
					GetClientRect(hwnd, &rc);

					BITMAP bm;
					GetObject(hbmp, sizeof(bm), &bm);

					HDC memDC = CreateCompatibleDC(hdc);
					HGDIOBJ old = SelectObject(memDC, hbmp);
					SetStretchBltMode(hdc, HALFTONE);
					SetBrushOrgEx(hdc, 0, 0, nullptr);
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

		SplashScreenModule::SplashScreenModule(const Dia::Core::StringCRC& instanceId)
			: Dia::ApplicationFlow::Module(instanceId)
			, mHwnd(nullptr)
		{
		}

		Dia::ApplicationFlow::StartResult SplashScreenModule::DoStart()
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

			return Dia::ApplicationFlow::StartResult::kReady;
		}

		void SplashScreenModule::DoUpdate(float /*deltaTime*/)
		{
			MSG msg;
			while (PeekMessageW(&msg, mHwnd, 0, 0, PM_REMOVE))
			{
				TranslateMessage(&msg);
				DispatchMessageW(&msg);
			}
		}

		void SplashScreenModule::Dismiss()
		{
			if (mHwnd)
			{
				DestroyWindow(mHwnd);
				mHwnd = nullptr;
			}
		}

		Dia::ApplicationFlow::StopResult SplashScreenModule::DoStop()
		{
			Dismiss();
			return Dia::ApplicationFlow::StopResult::kDone;
		}
	}
}

namespace { using SplashScreenModule_ = Cluiche::Editor::SplashScreenModule; }
DIA_MODULE(SplashScreenModule_);

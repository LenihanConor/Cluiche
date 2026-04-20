////////////////////////////////////////////////////////////////////////////////
// Filename: CEFRenderHandler.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <include/cef_render_handler.h>

#include <mutex>

namespace Dia
{
	namespace UICEF
	{
		class CEFRenderHandler : public CefRenderHandler
		{
		public:
			CEFRenderHandler(int width, int height);
			virtual ~CEFRenderHandler();

			// CefRenderHandler
			void GetViewRect(CefRefPtr<CefBrowser> browser, CefRect& rect) override;
			void OnPaint(CefRefPtr<CefBrowser> browser, PaintElementType type,
				const RectList& dirtyRects, const void* buffer, int width, int height) override;
			void OnPopupShow(CefRefPtr<CefBrowser> browser, bool show) override;
			void OnPopupSize(CefRefPtr<CefBrowser> browser, const CefRect& rect) override;

			const unsigned char* GetPixelBuffer() const;
			int GetBufferSize() const;
			int GetWidth() const { return mWidth; }
			int GetHeight() const { return mHeight; }
			bool IsDirty() const { return mIsDirty; }
			void ClearDirty() { mIsDirty = false; }

			void Resize(int width, int height);

		private:
			int mWidth;
			int mHeight;
			bool mIsDirty;
			bool mPopupVisible;
			CefRect mPopupRect;
			mutable std::mutex mBufferMutex;

			static const int sBufferSize = (16 * 1024 * 1024);
			unsigned char mPixelBuffer[sBufferSize];

			IMPLEMENT_REFCOUNTING(CEFRenderHandler);
		};
	}
}

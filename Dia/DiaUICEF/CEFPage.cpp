////////////////////////////////////////////////////////////////////////////////
// Filename: CEFPage.cpp
////////////////////////////////////////////////////////////////////////////////
#include "CEFPage.h"

#include "CEFRenderHandler.h"
#include "CEFClientHandler.h"

#include <DiaCore/Core/Assert.h>
#include <DiaUI/UIDataBuffer.h>
#include <DiaUI/BoundMethod.h>

#include <include/cef_browser.h>

namespace Dia
{
	namespace UICEF
	{
		CEFPage::CEFPage(int pageId, const char* url, int width, int height)
			: mPageId(pageId)
			, mWidth(width)
			, mHeight(height)
			, mURL(url ? url : "")
		{
		}

		CEFPage::~CEFPage()
		{
		}

		bool CEFPage::Create()
		{
			mRenderHandler = new CEFRenderHandler(mWidth, mHeight);
			mClientHandler = new CEFClientHandler(this, mRenderHandler);

			CefWindowInfo windowInfo;
			windowInfo.SetAsWindowless(nullptr);

			CefBrowserSettings browserSettings;
			browserSettings.windowless_frame_rate = 60;
			browserSettings.background_color = CefColorSetARGB(0, 0, 0, 0);

			CefString cefUrl(mURL);
			return CefBrowserHost::CreateBrowser(windowInfo, mClientHandler, cefUrl,
				browserSettings, nullptr, nullptr);
		}

		void CEFPage::Close()
		{
			if (mBrowser)
				mBrowser->GetHost()->CloseBrowser(true);
		}

		void CEFPage::LoadURL(const char* url)
		{
			mURL = url ? url : "";
			if (mBrowser && mBrowser->GetMainFrame())
				mBrowser->GetMainFrame()->LoadURL(CefString(mURL));
		}

		void CEFPage::LoadHTML(const char* html, const char* /*baseURL*/)
		{
			if (mBrowser && mBrowser->GetMainFrame())
			{
				std::string dataUri = "data:text/html,";
				dataUri += html;
				mBrowser->GetMainFrame()->LoadURL(CefString(dataUri));
			}
		}

		void CEFPage::ExecuteJavaScript(const char* script)
		{
			if (mBrowser && mBrowser->GetMainFrame())
				mBrowser->GetMainFrame()->ExecuteJavaScript(CefString(script), "", 0);
		}

		void CEFPage::SetCallback(const char* /*callbackName*/, UI::BoundMethod* /*method*/)
		{
			// JS bridge callbacks are handled via IPC in CEFProcessHandler::OnContextCreated
			// and CEFJavaScriptBridge. Per-page callback registration routes through the bridge.
		}

		void CEFPage::RemoveCallback(const char* /*callbackName*/)
		{
		}

		bool CEFPage::IsLoading() const
		{
			return mBrowser ? mBrowser->IsLoading() : true;
		}

		const char* CEFPage::GetURL() const
		{
			return mURL.c_str();
		}

		void CEFPage::Resize(int width, int height)
		{
			mWidth = width;
			mHeight = height;
			if (mRenderHandler)
				mRenderHandler->Resize(width, height);
			if (mBrowser)
				mBrowser->GetHost()->WasResized();
		}

		void CEFPage::GetTextureData(UI::UIDataBuffer& outBuffer) const
		{
			if (!mRenderHandler)
				return;

			const unsigned char* pixels = mRenderHandler->GetPixelBuffer();
			int bufferSize = mRenderHandler->GetBufferSize();

			if (pixels && bufferSize > 0)
			{
				outBuffer.CreateFromPreallocatedBuffer(
					mRenderHandler->GetWidth(),
					mRenderHandler->GetHeight(),
					const_cast<unsigned char*>(pixels),
					bufferSize,
					false);
			}
		}

		//-------------------------------------------------------------------
		// Input
		//-------------------------------------------------------------------

		static CefBrowserHost::MouseButtonType ToCefMouseButton(int button)
		{
			switch (button)
			{
			case 0: return MBT_LEFT;
			case 1: return MBT_RIGHT;
			case 2: return MBT_MIDDLE;
			default: return MBT_LEFT;
			}
		}

		void CEFPage::InjectMouseMove(int x, int y)
		{
			if (!mBrowser) return;
			CefMouseEvent evt;
			evt.x = x;
			evt.y = y;
			evt.modifiers = 0;
			mBrowser->GetHost()->SendMouseMoveEvent(evt, false);
		}

		void CEFPage::InjectMouseDown(int button, int x, int y)
		{
			if (!mBrowser) return;
			CefMouseEvent evt;
			evt.x = x;
			evt.y = y;
			evt.modifiers = 0;
			mBrowser->GetHost()->SendMouseClickEvent(evt, ToCefMouseButton(button), false, 1);
		}

		void CEFPage::InjectMouseUp(int button, int x, int y)
		{
			if (!mBrowser) return;
			CefMouseEvent evt;
			evt.x = x;
			evt.y = y;
			evt.modifiers = 0;
			mBrowser->GetHost()->SendMouseClickEvent(evt, ToCefMouseButton(button), true, 1);
		}

		void CEFPage::InjectMouseClick(int button, int x, int y)
		{
			InjectMouseMove(x, y);
			InjectMouseDown(button, x, y);
			InjectMouseUp(button, x, y);
		}

		void CEFPage::InjectMouseWheel(int scroll_vert, int scroll_horz)
		{
			if (!mBrowser) return;
			CefMouseEvent evt;
			evt.x = 0;
			evt.y = 0;
			evt.modifiers = 0;
			mBrowser->GetHost()->SendMouseWheelEvent(evt, scroll_horz, scroll_vert);
		}

		void CEFPage::SetFocus(bool focused)
		{
			if (mBrowser)
				mBrowser->GetHost()->SetFocus(focused);
		}
	}
}

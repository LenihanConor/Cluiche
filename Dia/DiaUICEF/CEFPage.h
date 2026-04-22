////////////////////////////////////////////////////////////////////////////////
// Filename: CEFPage.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <DiaUI/IPage.h>

#include <include/cef_browser.h>

#include <mutex>
#include <string>

namespace Dia
{
	namespace UICEF
	{
		class CEFRenderHandler;
		class CEFClientHandler;
		class CEFJavaScriptBridge;

		class CEFPage : public UI::IPage
		{
		public:
			CEFPage(int pageId, const char* url, int width, int height);
			virtual ~CEFPage();

			// Must be called before Create() / CreateWindowed() so the client handler
			// can route IPC messages from the render process to the bridge.
			void SetJSBridge(CEFJavaScriptBridge* bridge) { mJSBridge = bridge; }

			bool Create();                         // offscreen / windowless
			bool CreateWindowed(void* parentHwnd); // windowed, CEF owns child window
			void Close();

			// IPage
			virtual void LoadURL(const char* url) override;
			virtual void LoadHTML(const char* html, const char* baseURL) override;
			virtual void ExecuteJavaScript(const char* script) override;
			virtual void SetCallback(const char* callbackName, UI::BoundMethod* method) override;
			virtual void RemoveCallback(const char* callbackName) override;
			virtual bool IsLoading() const override;
			virtual const char* GetURL() const override;
			virtual void Resize(int width, int height) override;
			virtual void GetTextureData(UI::UIDataBuffer& outBuffer) const override;
			virtual int GetPageId() const override { return mPageId; }

			// Input (button: 0=left, 1=right, 2=middle)
			void InjectMouseMove(int x, int y);
			void InjectMouseDown(int button, int x, int y);
			void InjectMouseUp(int button, int x, int y);
			void InjectMouseClick(int button, int x, int y);
			void InjectMouseWheel(int scroll_vert, int scroll_horz);
			void SetFocus(bool focused);

			CefRefPtr<CefBrowser> GetBrowser() { return mBrowser; }
			CefRefPtr<CefBrowser> GetBrowser() const { return mBrowser; }
			void SetBrowser(CefRefPtr<CefBrowser> browser) { mBrowser = browser; }

			CefRefPtr<CEFClientHandler> GetClientHandler() { return mClientHandler; }

		private:
			int mPageId;
			int mWidth;
			int mHeight;
			std::string mURL;
			CefRefPtr<CefBrowser> mBrowser;
			CefRefPtr<CEFRenderHandler> mRenderHandler;
			CefRefPtr<CEFClientHandler> mClientHandler;
			CEFJavaScriptBridge* mJSBridge = nullptr;
		};
	}
}

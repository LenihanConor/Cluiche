////////////////////////////////////////////////////////////////////////////////
// Filename: CEFClientHandler.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <include/cef_client.h>
#include <include/cef_life_span_handler.h>
#include <include/cef_load_handler.h>
#include <include/cef_display_handler.h>
#include <include/cef_keyboard_handler.h>

namespace Dia
{
	namespace UICEF
	{
		class CEFPage;
		class CEFRenderHandler;
		class CEFJavaScriptBridge;

		class CEFClientHandler
			: public CefClient
			, public CefLifeSpanHandler
			, public CefLoadHandler
			, public CefDisplayHandler
			, public CefKeyboardHandler
		{
		public:
			CEFClientHandler(CEFPage* page, CefRefPtr<CEFRenderHandler> renderHandler);

			void DetachPage() { mPage = nullptr; }
			void SetJSBridge(CEFJavaScriptBridge* bridge) { mJSBridge = bridge; }

			// CefClient IPC receiver (from render process)
			bool OnProcessMessageReceived(CefRefPtr<CefBrowser> browser,
				CefRefPtr<CefFrame> frame,
				CefProcessId source_process,
				CefRefPtr<CefProcessMessage> message) override;

			// CefClient
			CefRefPtr<CefRenderHandler> GetRenderHandler() override;
			CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
			CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
			CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }
			CefRefPtr<CefKeyboardHandler> GetKeyboardHandler() override { return this; }

			// CefLifeSpanHandler
			void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
			void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

			// CefLoadHandler
			void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
				ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl) override;

			// CefDisplayHandler
			bool OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level,
				const CefString& message, const CefString& source, int line) override;

			// CefKeyboardHandler - suppress Chromium's built-in shortcuts (F1 help, Ctrl+Shift+P print, etc.)
			bool OnPreKeyEvent(CefRefPtr<CefBrowser> browser, const CefKeyEvent& event,
				CefEventHandle os_event, bool* is_keyboard_shortcut) override;

		private:
			CEFPage* mPage;
			CefRefPtr<CEFRenderHandler> mRenderHandler;
			CEFJavaScriptBridge* mJSBridge = nullptr;

			IMPLEMENT_REFCOUNTING(CEFClientHandler);
		};
	}
}

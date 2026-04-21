////////////////////////////////////////////////////////////////////////////////
// Filename: CEFClientHandler.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <include/cef_client.h>
#include <include/cef_life_span_handler.h>
#include <include/cef_load_handler.h>
#include <include/cef_display_handler.h>

namespace Dia
{
	namespace UICEF
	{
		class CEFPage;
		class CEFRenderHandler;

		class CEFClientHandler
			: public CefClient
			, public CefLifeSpanHandler
			, public CefLoadHandler
			, public CefDisplayHandler
		{
		public:
			CEFClientHandler(CEFPage* page, CefRefPtr<CEFRenderHandler> renderHandler);

			void DetachPage() { mPage = nullptr; }

			// CefClient
			CefRefPtr<CefRenderHandler> GetRenderHandler() override;
			CefRefPtr<CefLifeSpanHandler> GetLifeSpanHandler() override { return this; }
			CefRefPtr<CefLoadHandler> GetLoadHandler() override { return this; }
			CefRefPtr<CefDisplayHandler> GetDisplayHandler() override { return this; }

			// CefLifeSpanHandler
			void OnAfterCreated(CefRefPtr<CefBrowser> browser) override;
			void OnBeforeClose(CefRefPtr<CefBrowser> browser) override;

			// CefLoadHandler
			void OnLoadError(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame,
				ErrorCode errorCode, const CefString& errorText, const CefString& failedUrl) override;

			// CefDisplayHandler
			bool OnConsoleMessage(CefRefPtr<CefBrowser> browser, cef_log_severity_t level,
				const CefString& message, const CefString& source, int line) override;

		private:
			CEFPage* mPage;
			CefRefPtr<CEFRenderHandler> mRenderHandler;

			IMPLEMENT_REFCOUNTING(CEFClientHandler);
		};
	}
}

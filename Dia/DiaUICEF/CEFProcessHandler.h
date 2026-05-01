////////////////////////////////////////////////////////////////////////////////
// Filename: CEFProcessHandler.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <include/cef_app.h>
#include <include/cef_browser_process_handler.h>
#include <include/cef_render_process_handler.h>

#include <string>

namespace Dia
{
	namespace UICEF
	{
		class CEFProcessHandler
			: public CefApp
			, public CefBrowserProcessHandler
			, public CefRenderProcessHandler
		{
		public:
			CEFProcessHandler(const std::string& assetBasePath);

			// CefApp
			CefRefPtr<CefBrowserProcessHandler> GetBrowserProcessHandler() override { return this; }
			CefRefPtr<CefRenderProcessHandler> GetRenderProcessHandler() override { return this; }
			void OnBeforeCommandLineProcessing(const CefString& process_type, CefRefPtr<CefCommandLine> command_line) override;
			void OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar) override;

			// CefBrowserProcessHandler
			void OnContextInitialized() override;

			// CefRenderProcessHandler
			void OnContextCreated(CefRefPtr<CefBrowser> browser, CefRefPtr<CefFrame> frame, CefRefPtr<CefV8Context> context) override;

		private:
			std::string mAssetBasePath;

			IMPLEMENT_REFCOUNTING(CEFProcessHandler);
		};
	}
}

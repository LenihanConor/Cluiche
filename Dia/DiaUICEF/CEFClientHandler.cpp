////////////////////////////////////////////////////////////////////////////////
// Filename: CEFClientHandler.cpp
////////////////////////////////////////////////////////////////////////////////
#include "CEFClientHandler.h"

#include "CEFPage.h"
#include "CEFRenderHandler.h"

#include <DiaCore/Core/Assert.h>
#include <DiaCore/Core/Log.h>

namespace Dia
{
	namespace UICEF
	{
		CEFClientHandler::CEFClientHandler(CEFPage* page, CefRefPtr<CEFRenderHandler> renderHandler)
			: mPage(page)
			, mRenderHandler(renderHandler)
		{
		}

		CefRefPtr<CefRenderHandler> CEFClientHandler::GetRenderHandler()
		{
			return mRenderHandler;
		}

		void CEFClientHandler::OnAfterCreated(CefRefPtr<CefBrowser> browser)
		{
			if (mPage)
				mPage->SetBrowser(browser);
		}

		void CEFClientHandler::OnBeforeClose(CefRefPtr<CefBrowser> /*browser*/)
		{
			if (mPage)
				mPage->SetBrowser(nullptr);
		}

		void CEFClientHandler::OnLoadError(CefRefPtr<CefBrowser> /*browser*/,
			CefRefPtr<CefFrame> frame, ErrorCode errorCode,
			const CefString& errorText, const CefString& failedUrl)
		{
			std::string url = failedUrl.ToString();
			std::string error = errorText.ToString();
			Dia::Core::Log::OutputVaradicLine("DiaUICEF: Failed to load %s. Error %d: %s (main=%d)",
				url.c_str(), static_cast<int>(errorCode), error.c_str(), frame->IsMain() ? 1 : 0);

			if (frame->IsMain())
				DIA_ASSERT(false, "DiaUICEF page load failure");
		}

		bool CEFClientHandler::OnConsoleMessage(CefRefPtr<CefBrowser> /*browser*/,
			cef_log_severity_t /*level*/, const CefString& message,
			const CefString& source, int line)
		{
			std::string msg = message.ToString();
			std::string src = source.ToString();
			Dia::Core::Log::OutputVaradicLine("DiaUICEF Log - Source: %s, Line: %d, Message: %s",
				src.c_str(), line, msg.c_str());
			return false;
		}
	}
}

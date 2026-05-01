////////////////////////////////////////////////////////////////////////////////
// Filename: CEFClientHandler.cpp
////////////////////////////////////////////////////////////////////////////////
#include "CEFClientHandler.h"

#include "CEFPage.h"
#include "CEFRenderHandler.h"
#include "CEFJavaScriptBridge.h"

#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>

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
			DIA_LOG_ERROR("UI", "DiaUICEF: Failed to load %s. Error %d: %s (main=%d)",
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
			DIA_LOG_DEBUG("UI", "DiaUICEF Log - Source: %s, Line: %d, Message: %s",
				src.c_str(), line, msg.c_str());
			return false;
		}

		bool CEFClientHandler::OnProcessMessageReceived(CefRefPtr<CefBrowser> /*browser*/,
			CefRefPtr<CefFrame> /*frame*/, CefProcessId /*source_process*/,
			CefRefPtr<CefProcessMessage> message)
		{
			if (!mJSBridge)
				return false;

			const std::string name = message->GetName().ToString();
			if (name != "dia_callCpp")
				return false;

			CefRefPtr<CefListValue> args = message->GetArgumentList();
			if (!args || args->GetSize() < 2)
				return false;

			std::string functionName = args->GetString(0).ToString();
			std::string argsJson = args->GetString(1).ToString();

			mJSBridge->HandleCall(functionName, argsJson);
			return true;
		}

		bool CEFClientHandler::OnPreKeyEvent(CefRefPtr<CefBrowser> /*browser*/,
			const CefKeyEvent& event, CefEventHandle /*os_event*/,
			bool* is_keyboard_shortcut)
		{
			if (event.type != KEYEVENT_RAWKEYDOWN)
				return false;

			const bool ctrl = (event.modifiers & EVENTFLAG_CONTROL_DOWN) != 0;
			const bool shift = (event.modifiers & EVENTFLAG_SHIFT_DOWN) != 0;
			const bool alt = (event.modifiers & EVENTFLAG_ALT_DOWN) != 0;

			// Tell Chromium these aren't browser shortcuts so its built-in
			// handlers (Ctrl+P print) don't fire. The event still propagates
			// to JS where the app decides what to do.
			const bool ctrlP    = ( ctrl && !shift && !alt && event.windows_key_code == 'P');
			const bool ctrlShP  = ( ctrl &&  shift && !alt && event.windows_key_code == 'P');

			if (ctrlP || ctrlShP)
			{
				if (is_keyboard_shortcut != nullptr)
					*is_keyboard_shortcut = false;
			}

			return false;
		}
	}
}

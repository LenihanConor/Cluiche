////////////////////////////////////////////////////////////////////////////////
// Filename: CEFJavaScriptBridge.cpp
////////////////////////////////////////////////////////////////////////////////
#include "CEFJavaScriptBridge.h"

#include <include/cef_process_message.h>

namespace Dia
{
	namespace UICEF
	{
		//-------------------------------------------------------------------
		// CEFJavaScriptBridge
		//-------------------------------------------------------------------

		void CEFJavaScriptBridge::RegisterFunction(const char* name, CppCallback callback)
		{
			std::lock_guard<std::mutex> lock(mFunctionsMutex);
			mRegisteredFunctions[name] = callback;
		}

		void CEFJavaScriptBridge::UnregisterFunction(const char* name)
		{
			std::lock_guard<std::mutex> lock(mFunctionsMutex);
			mRegisteredFunctions.erase(name);
		}

		std::string CEFJavaScriptBridge::HandleCall(const std::string& functionName,
			const std::string& argsJson)
		{
			std::lock_guard<std::mutex> lock(mFunctionsMutex);
			auto it = mRegisteredFunctions.find(functionName);
			if (it != mRegisteredFunctions.end())
				return it->second(argsJson);

			return "{\"error\":\"Function not found\"}";
		}

		void CEFJavaScriptBridge::ExecuteJavaScript(CefRefPtr<CefBrowser> browser,
			const char* code)
		{
			if (browser && browser->GetMainFrame())
				browser->GetMainFrame()->ExecuteJavaScript(CefString(code), "", 0);
		}

		void CEFJavaScriptBridge::CallJavaScript(CefRefPtr<CefBrowser> browser,
			const char* functionName, const char* argsJson)
		{
			std::string code = std::string(functionName) + "(" + (argsJson ? argsJson : "") + ");";
			ExecuteJavaScript(browser, code.c_str());
		}

		//-------------------------------------------------------------------
		// CEFJavaScriptHandler (V8 handler for window.dia.callCpp)
		//-------------------------------------------------------------------

		bool CEFJavaScriptHandler::Execute(const CefString& name,
			CefRefPtr<CefV8Value> /*object*/, const CefV8ValueList& arguments,
			CefRefPtr<CefV8Value>& /*retval*/, CefString& exception)
		{
			if (name != "callCpp")
			{
				exception = "Unknown function: " + name.ToString();
				return true;
			}

			if (arguments.size() < 1 || !arguments[0]->IsString())
			{
				exception = "callCpp requires a function name string as first argument";
				return true;
			}

			std::string functionName = arguments[0]->GetStringValue().ToString();
			std::string argsJson = "{}";
			if (arguments.size() >= 2 && arguments[1]->IsString())
				argsJson = arguments[1]->GetStringValue().ToString();

			// Send IPC message to browser process
			CefRefPtr<CefProcessMessage> msg = CefProcessMessage::Create("dia_callCpp");
			CefRefPtr<CefListValue> msgArgs = msg->GetArgumentList();
			msgArgs->SetString(0, functionName);
			msgArgs->SetString(1, argsJson);

			CefRefPtr<CefV8Context> context = CefV8Context::GetCurrentContext();
			if (context && context->GetBrowser())
				context->GetBrowser()->GetMainFrame()->SendProcessMessage(PID_BROWSER, msg);

			return true;
		}
	}
}

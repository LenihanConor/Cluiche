////////////////////////////////////////////////////////////////////////////////
// Filename: CEFJavaScriptBridge.h
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <include/cef_v8.h>
#include <include/cef_browser.h>
#include <include/cef_process_message.h>

#include <string>
#include <map>
#include <functional>
#include <mutex>

namespace Dia
{
	namespace UICEF
	{
		class CEFJavaScriptBridge
		{
		public:
			using CppCallback = std::function<std::string(const std::string& argsJson)>;

			void RegisterFunction(const char* name, CppCallback callback);
			void UnregisterFunction(const char* name);
			std::string HandleCall(const std::string& functionName, const std::string& argsJson);

			void ExecuteJavaScript(CefRefPtr<CefBrowser> browser, const char* code);
			void CallJavaScript(CefRefPtr<CefBrowser> browser, const char* functionName, const char* argsJson);

		private:
			std::map<std::string, CppCallback> mRegisteredFunctions;
			std::mutex mFunctionsMutex;
		};

		class CEFJavaScriptHandler : public CefV8Handler
		{
		public:
			bool Execute(const CefString& name, CefRefPtr<CefV8Value> object,
				const CefV8ValueList& arguments, CefRefPtr<CefV8Value>& retval,
				CefString& exception) override;

		private:
			IMPLEMENT_REFCOUNTING(CEFJavaScriptHandler);
		};
	}
}

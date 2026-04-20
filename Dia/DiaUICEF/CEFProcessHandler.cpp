////////////////////////////////////////////////////////////////////////////////
// Filename: CEFProcessHandler.cpp
////////////////////////////////////////////////////////////////////////////////
#include "CEFProcessHandler.h"

#include "CEFSchemeHandler.h"
#include "CEFJavaScriptBridge.h"

#include <include/cef_command_line.h>

namespace Dia
{
	namespace UICEF
	{
		CEFProcessHandler::CEFProcessHandler(const std::string& assetBasePath)
			: mAssetBasePath(assetBasePath)
		{
		}

		void CEFProcessHandler::OnBeforeCommandLineProcessing(const CefString& /*process_type*/,
			CefRefPtr<CefCommandLine> command_line)
		{
			command_line->AppendSwitch("disable-gpu-shader-disk-cache");
			command_line->AppendSwitchWithValue("autoplay-policy", "no-user-gesture-required");
		}

		void CEFProcessHandler::OnRegisterCustomSchemes(CefRawPtr<CefSchemeRegistrar> registrar)
		{
			RegisterDiaScheme(registrar);
		}

		void CEFProcessHandler::OnContextInitialized()
		{
			RegisterDiaSchemeHandlerFactory(mAssetBasePath);
		}

		void CEFProcessHandler::OnContextCreated(CefRefPtr<CefBrowser> /*browser*/,
			CefRefPtr<CefFrame> /*frame*/, CefRefPtr<CefV8Context> context)
		{
			CefRefPtr<CefV8Value> global = context->GetGlobal();

			CefRefPtr<CefV8Value> diaObj = CefV8Value::CreateObject(nullptr, nullptr);

			CefRefPtr<CEFJavaScriptHandler> handler = new CEFJavaScriptHandler();
			CefRefPtr<CefV8Value> callCppFunc = CefV8Value::CreateFunction("callCpp", handler);
			diaObj->SetValue("callCpp", callCppFunc, V8_PROPERTY_ATTRIBUTE_READONLY);

			CefRefPtr<CefV8Value> version = CefV8Value::CreateString("1.0.0");
			diaObj->SetValue("version", version, V8_PROPERTY_ATTRIBUTE_READONLY);

			global->SetValue("dia", diaObj, V8_PROPERTY_ATTRIBUTE_READONLY);
		}
	}
}

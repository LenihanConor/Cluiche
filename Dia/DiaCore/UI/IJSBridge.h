////////////////////////////////////////////////////////////////////////////////
// Filename: IJSBridge.h
// Description: Neutral JS<->C++ bridge contract. Lets foundation-tier consumers
//              (e.g. DiaEditor's WebUIBridge) register/invoke JS handlers without
//              depending on the full domain-tier IUISystem (Page, render overlay,
//              input injection). Implementor: Dia::UI::IUISystem.
////////////////////////////////////////////////////////////////////////////////
#pragma once

#include <functional>
#include <string>

namespace Dia
{
	namespace Core
	{
		class IJSBridge
		{
		public:
			using JSHandler = std::function<std::string(const std::string& argsJson)>;

			virtual ~IJSBridge() {};

			// RegisterJSHandler binds a name that JS can invoke as window.dia.callCpp(name, argsJson).
			// CallJSFunction pushes a notification to JS: dia.<functionName>(argsJson).
			virtual void RegisterJSHandler(const char* /*name*/, JSHandler /*handler*/) {}
			virtual void CallJSFunction(const char* /*functionName*/, const char* /*argsJson*/) {}
		};
	}
}

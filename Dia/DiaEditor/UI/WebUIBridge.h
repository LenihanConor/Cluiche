#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <functional>
#include <string>

namespace Dia
{
	namespace Core
	{
		class IJSBridge;
	}

	namespace Editor
	{
		class EditorViewController;

		class WebUIBridge
		{
		public:
			// Fire-and-forget: JS sent an event, no response expected.
			using EventHandler = std::function<void(const Json::Value& data)>;

			// Request-response: JS sent a request with a reqId, a JSON result is returned
			// to JS via CallJSFunction("DiaEditor_onResponse", { reqId, result }).
			using RequestHandler = std::function<Json::Value(const Json::Value& data)>;

			// Takes IJSBridge (not the concrete Dia::UI::IUISystem) so this foundation-tier
			// bridge doesn't depend on the domain-tier DiaUI module for its JS plumbing.
			explicit WebUIBridge(Dia::Core::IJSBridge* uiSystem);

			void Initialize(EditorViewController* controller);

			void RegisterEventHandler(const Dia::Core::StringCRC& eventType, EventHandler handler);
			void RegisterRequestHandler(const Dia::Core::StringCRC& eventType, RequestHandler handler);

			void UnregisterEventHandler(const Dia::Core::StringCRC& eventType);
			void UnregisterRequestHandler(const Dia::Core::StringCRC& eventType);

			// Push a data update to JS. JS receives via window.DiaEditor_onDataChanged({ topic, data }).
			void NotifyUIDataChanged(const char* topic, const Json::Value& data);

			// Synchronously invoke a registered request handler and return its result.
			// Returns an empty Value if no handler is registered for eventType.
			Json::Value InvokeRequestHandler(const Dia::Core::StringCRC& eventType, const Json::Value& data) const;

		private:
			std::string HandleEditorCall(const std::string& argsJson);
			void SendResponse(const std::string& reqId, const Json::Value& result);

			Dia::Core::IJSBridge* mUISystem;
			EditorViewController* mController;

			struct EventEntry
			{
				Dia::Core::StringCRC eventType;
				EventHandler handler;
			};

			struct RequestEntry
			{
				Dia::Core::StringCRC eventType;
				RequestHandler handler;
			};

			static const unsigned int kMaxHandlers = 256;
			Dia::Core::Containers::DynamicArrayC<EventEntry, kMaxHandlers> mEventHandlers;
			Dia::Core::Containers::DynamicArrayC<RequestEntry, kMaxHandlers> mRequestHandlers;
		};
	}
}

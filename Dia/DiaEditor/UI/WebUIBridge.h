#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

#include <DiaCore/Containers/Arrays/DynamicArrayC.h>

#include <functional>
#include <string>

namespace Dia
{
	namespace UI
	{
		class IUISystem;
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

			explicit WebUIBridge(Dia::UI::IUISystem* uiSystem);

			void Initialize(EditorViewController* controller);

			void RegisterEventHandler(const Dia::Core::StringCRC& eventType, EventHandler handler);
			void RegisterRequestHandler(const Dia::Core::StringCRC& eventType, RequestHandler handler);

			void UnregisterEventHandler(const Dia::Core::StringCRC& eventType);
			void UnregisterRequestHandler(const Dia::Core::StringCRC& eventType);

			// Push a data update to JS. JS receives via window.DiaEditor_onDataChanged({ topic, data }).
			void NotifyUIDataChanged(const char* topic, const Json::Value& data);

		private:
			std::string HandleEditorCall(const std::string& argsJson);
			void SendResponse(const std::string& reqId, const Json::Value& result);

			Dia::UI::IUISystem* mUISystem;
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

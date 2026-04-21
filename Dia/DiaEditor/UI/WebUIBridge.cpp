#include "DiaEditor/UI/WebUIBridge.h"

#include <DiaCore/Core/Assert.h>
#include <DiaUI/IUISystem.h>
#include "DiaEditor/MVC/EditorViewController.h"

#include <sstream>

namespace Dia
{
	namespace Editor
	{
		static const char* kEditorCallName = "DiaEditor_call";

		WebUIBridge::WebUIBridge(Dia::UI::IUISystem* uiSystem)
			: mUISystem(uiSystem)
			, mController(nullptr)
		{
		}

		void WebUIBridge::Initialize(EditorViewController* controller)
		{
			DIA_ASSERT(controller != nullptr, "WebUIBridge: controller must not be null");
			mController = controller;

			if (mUISystem)
			{
				mUISystem->RegisterJSHandler(kEditorCallName,
					[this](const std::string& argsJson) { return HandleEditorCall(argsJson); });
			}
		}

		void WebUIBridge::RegisterEventHandler(const Dia::Core::StringCRC& eventType, EventHandler handler)
		{
			DIA_ASSERT(!mEventHandlers.IsFull(), "WebUIBridge: max event handler capacity reached");
			EventEntry entry;
			entry.eventType = eventType;
			entry.handler = handler;
			mEventHandlers.Add(entry);
		}

		void WebUIBridge::RegisterRequestHandler(const Dia::Core::StringCRC& eventType, RequestHandler handler)
		{
			DIA_ASSERT(!mRequestHandlers.IsFull(), "WebUIBridge: max request handler capacity reached");
			RequestEntry entry;
			entry.eventType = eventType;
			entry.handler = handler;
			mRequestHandlers.Add(entry);
		}

		void WebUIBridge::NotifyUIDataChanged(const char* topic, const Json::Value& data)
		{
			if (!mUISystem || topic == nullptr)
				return;

			Json::Value envelope;
			envelope["topic"] = topic;
			envelope["data"] = data;

			Json::StreamWriterBuilder writer;
			writer["indentation"] = "";
			std::string json = Json::writeString(writer, envelope);
			mUISystem->CallJSFunction("DiaEditor_onDataChanged", json.c_str());
		}

		void WebUIBridge::SendResponse(const std::string& reqId, const Json::Value& result)
		{
			if (!mUISystem)
				return;

			Json::Value envelope;
			envelope["reqId"] = reqId;
			envelope["result"] = result;

			Json::StreamWriterBuilder writer;
			writer["indentation"] = "";
			std::string json = Json::writeString(writer, envelope);
			mUISystem->CallJSFunction("DiaEditor_onResponse", json.c_str());
		}

		std::string WebUIBridge::HandleEditorCall(const std::string& argsJson)
		{
			Json::Value args;
			Json::CharReaderBuilder builder;
			std::string errors;
			std::istringstream stream(argsJson);
			if (!Json::parseFromStream(builder, stream, &args, &errors))
				return "{\"error\":\"invalid json\"}";

			const std::string& eventTypeStr = args.get("type", "").asString();
			Dia::Core::StringCRC eventType(eventTypeStr.c_str());
			const Json::Value& data = args["data"];
			const std::string reqId = args.get("reqId", "").asString();

			if (!reqId.empty())
			{
				for (unsigned int i = 0; i < mRequestHandlers.Size(); ++i)
				{
					if (mRequestHandlers[i].eventType == eventType)
					{
						Json::Value result = mRequestHandlers[i].handler(data);
						SendResponse(reqId, result);
						return "{}";
					}
				}
				Json::Value empty;
				SendResponse(reqId, empty);
				return "{}";
			}

			if (mController != nullptr)
				mController->OnUIEvent(eventType, data);

			for (unsigned int i = 0; i < mEventHandlers.Size(); ++i)
			{
				if (mEventHandlers[i].eventType == eventType)
					mEventHandlers[i].handler(data);
			}

			return "{}";
		}
	}
}

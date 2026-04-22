#include "DiaEditor/UI/WebUIBridge.h"

#include <DiaCore/Core/Assert.h>
#include <DiaLogger/DiaLog.h>
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

			DIA_LOG_INFO("Editor", "WebUIBridge: Initialize uiSystem=%p controller=%p", mUISystem, controller);
			if (mUISystem)
			{
				mUISystem->RegisterJSHandler(kEditorCallName,
					[this](const std::string& argsJson) { return HandleEditorCall(argsJson); });
				DIA_LOG_INFO("Editor", "WebUIBridge: Registered JS handler '%s'", kEditorCallName);
			}
			else
			{
				DIA_LOG_WARNING("Editor", "WebUIBridge: uiSystem is null, JS handler not registered");
			}

		}

		void WebUIBridge::RegisterEventHandler(const Dia::Core::StringCRC& eventType, EventHandler handler)
		{
			DIA_ASSERT(!mEventHandlers.IsFull(), "WebUIBridge: max event handler capacity reached");
			DIA_LOG_INFO("Editor", "WebUIBridge: RegisterEventHandler '%s' (count=%u)", eventType.AsChar(), mEventHandlers.Size() + 1);
			EventEntry entry;
			entry.eventType = eventType;
			entry.handler = handler;
			mEventHandlers.Add(entry);
		}

		void WebUIBridge::RegisterRequestHandler(const Dia::Core::StringCRC& eventType, RequestHandler handler)
		{
			DIA_ASSERT(!mRequestHandlers.IsFull(), "WebUIBridge: max request handler capacity reached");
			DIA_LOG_INFO("Editor", "WebUIBridge: RegisterRequestHandler '%s' (count=%u)", eventType.AsChar(), mRequestHandlers.Size() + 1);
			RequestEntry entry;
			entry.eventType = eventType;
			entry.handler = handler;
			mRequestHandlers.Add(entry);
		}

		void WebUIBridge::NotifyUIDataChanged(const char* topic, const Json::Value& data)
		{
			if (!mUISystem || topic == nullptr)
			{
				DIA_LOG_WARNING("Editor", "WebUIBridge: NotifyUIDataChanged skipped (uiSystem=%p topic=%s)", mUISystem, topic ? topic : "(null)");
				return;
			}

			Json::Value envelope;
			envelope["topic"] = topic;
			envelope["data"] = data;

			Json::StreamWriterBuilder writer;
			writer["indentation"] = "";
			std::string json = Json::writeString(writer, envelope);
			DIA_LOG_TRACE("Editor", "WebUIBridge: NotifyUIDataChanged topic='%s' payload=%u bytes", topic, static_cast<unsigned>(json.size()));
			mUISystem->CallJSFunction("DiaEditor_onDataChanged", json.c_str());
		}

		void WebUIBridge::SendResponse(const std::string& reqId, const Json::Value& result)
		{
			if (!mUISystem)
			{
				DIA_LOG_WARNING("Editor", "WebUIBridge: SendResponse skipped (uiSystem is null) reqId='%s'", reqId.c_str());
				return;
			}

			Json::Value envelope;
			envelope["reqId"] = reqId;
			envelope["result"] = result;

			Json::StreamWriterBuilder writer;
			writer["indentation"] = "";
			std::string json = Json::writeString(writer, envelope);
			DIA_LOG_DEBUG("Editor", "WebUIBridge: SendResponse reqId='%s' payload=%u bytes", reqId.c_str(), static_cast<unsigned>(json.size()));
			mUISystem->CallJSFunction("DiaEditor_onResponse", json.c_str());
		}

		std::string WebUIBridge::HandleEditorCall(const std::string& argsJson)
		{
			Json::Value args;
			Json::CharReaderBuilder builder;
			std::string errors;
			std::istringstream stream(argsJson);
			if (!Json::parseFromStream(builder, stream, &args, &errors))
			{
				DIA_LOG_ERROR("Editor", "WebUIBridge: HandleEditorCall failed to parse JSON: %s", errors.c_str());
				return "{\"error\":\"invalid json\"}";
			}

			const std::string& eventTypeStr = args.get("type", "").asString();
			Dia::Core::StringCRC eventType(eventTypeStr.c_str());
			const Json::Value& data = args["data"];
			const std::string reqId = args.get("reqId", "").asString();

			DIA_LOG_DEBUG("Editor", "WebUIBridge: HandleEditorCall type='%s' reqId='%s'", eventTypeStr.c_str(), reqId.c_str());

			if (!reqId.empty())
			{
				for (unsigned int i = 0; i < mRequestHandlers.Size(); ++i)
				{
					if (mRequestHandlers[i].eventType == eventType)
					{
						DIA_LOG_DEBUG("Editor", "WebUIBridge: Matched request handler for '%s'", eventTypeStr.c_str());
						Json::Value result = mRequestHandlers[i].handler(data);
						SendResponse(reqId, result);
						return "{}";
					}
				}
				DIA_LOG_WARNING("Editor", "WebUIBridge: No request handler found for '%s', sending empty response", eventTypeStr.c_str());
				Json::Value empty;
				SendResponse(reqId, empty);
				return "{}";
			}

			if (mController != nullptr)
				mController->OnUIEvent(eventType, data);

			bool handled = false;
			for (unsigned int i = 0; i < mEventHandlers.Size(); ++i)
			{
				if (mEventHandlers[i].eventType == eventType)
				{
					mEventHandlers[i].handler(data);
					handled = true;
				}
			}
			if (!handled)
				DIA_LOG_WARNING("Editor", "WebUIBridge: No event handler found for '%s'", eventTypeStr.c_str());

			return "{}";
		}
	}
}

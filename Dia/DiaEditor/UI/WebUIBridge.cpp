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
			DIA_ASSERT(!mHandlers.IsFull(), "WebUIBridge: max handler capacity reached");
			HandlerEntry entry;
			entry.eventType = eventType;
			entry.handler = handler;
			mHandlers.Add(entry);
		}

		void WebUIBridge::NotifyUIDataChanged(const Dia::Core::StringCRC& /*dataPath*/, const Json::Value& data)
		{
			if (!mUISystem)
				return;

			Json::StreamWriterBuilder writer;
			writer["indentation"] = "";
			std::string json = Json::writeString(writer, data);
			mUISystem->CallJSFunction("DiaEditor_onDataChanged", json.c_str());
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

			if (mController != nullptr)
				mController->OnUIEvent(eventType, data);

			for (unsigned int i = 0; i < mHandlers.Size(); ++i)
			{
				if (mHandlers[i].eventType == eventType)
					mHandlers[i].handler(data);
			}

			return "{}";
		}
	}
}

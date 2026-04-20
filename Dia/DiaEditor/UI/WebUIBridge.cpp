#include "DiaEditor/UI/WebUIBridge.h"

#include <DiaCore/Core/Assert.h>
#include "DiaEditor/MVC/EditorViewController.h"

#include <sstream>

// CEFJavaScriptBridge is used via an opaque pointer here — DiaEditor does not link CEF
// directly. The full CEF header inclusion happens in CluicheEditor (which links DiaUICEF).
// The Initialize() call below is the handshake point where the bridge is wired up.

struct CefBridgeAccessor
{
	static void RegisterFunction(void* bridge, const char* name,
		std::function<std::string(const std::string&)> cb);
};

namespace Dia
{
	namespace Editor
	{
		WebUIBridge::WebUIBridge(Dia::UICEF::CEFJavaScriptBridge* cefBridge)
			: mCEFBridge(cefBridge)
			, mController(nullptr)
		{
		}

		void WebUIBridge::Initialize(EditorViewController* controller)
		{
			DIA_ASSERT(controller != nullptr, "WebUIBridge: controller must not be null");
			mController = controller;
			// CEF function registration deferred to Phase 7 when browser is available
		}

		void WebUIBridge::RegisterEventHandler(const Dia::Core::StringCRC& eventType, EventHandler handler)
		{
			DIA_ASSERT(!mHandlers.IsFull(), "WebUIBridge: max handler capacity reached");
			HandlerEntry entry;
			entry.eventType = eventType;
			entry.handler = handler;
			mHandlers.Add(entry);
		}

		void WebUIBridge::NotifyUIDataChanged(const Dia::Core::StringCRC& dataPath, const Json::Value& data)
		{
			// Deferred to Phase 7: requires live CefBrowser reference from EditorView
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

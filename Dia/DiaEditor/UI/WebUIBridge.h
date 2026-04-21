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
			using EventHandler = std::function<void(const Json::Value& data)>;

			explicit WebUIBridge(Dia::UI::IUISystem* uiSystem);

			void Initialize(EditorViewController* controller);

			void RegisterEventHandler(const Dia::Core::StringCRC& eventType, EventHandler handler);
			void NotifyUIDataChanged(const Dia::Core::StringCRC& dataPath, const Json::Value& data);

		private:
			std::string HandleEditorCall(const std::string& argsJson);

			Dia::UI::IUISystem* mUISystem;
			EditorViewController* mController;

			struct HandlerEntry
			{
				Dia::Core::StringCRC eventType;
				EventHandler handler;
			};

			static const unsigned int kMaxHandlers = 32;
			Dia::Core::Containers::DynamicArrayC<HandlerEntry, kMaxHandlers> mHandlers;
		};
	}
}

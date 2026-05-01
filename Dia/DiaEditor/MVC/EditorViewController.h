#pragma once

#include <DiaCore/CRC/StringCRC.h>
#include <DiaCore/Json/external/json/json.h>

namespace Dia
{
	namespace Editor
	{
		class CommandHistory;
		class EditorModel;

		class EditorViewController
		{
		public:
			static const Dia::Core::StringCRC kUniqueId;

			EditorViewController();

			void OnUIEvent(const Dia::Core::StringCRC& eventType, const Json::Value& data);

			void SetCommandHistory(CommandHistory* history);
			void SetModel(EditorModel* model);

		private:
			CommandHistory* mCommandHistory;
			EditorModel* mModel;
		};
	}
}

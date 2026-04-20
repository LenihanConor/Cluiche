#include "DiaEditor/MVC/EditorViewController.h"

#include "DiaEditor/Command/CommandHistory.h"
#include "DiaEditor/MVC/EditorModel.h"

namespace Dia
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorViewController::kUniqueId("EditorViewController");

		static const Dia::Core::StringCRC kEventUndo("undo");
		static const Dia::Core::StringCRC kEventRedo("redo");

		EditorViewController::EditorViewController()
			: mCommandHistory(nullptr)
			, mModel(nullptr)
		{
		}

		void EditorViewController::OnUIEvent(const Dia::Core::StringCRC& eventType, const Json::Value& data)
		{
			if (eventType == kEventUndo && mCommandHistory != nullptr)
				mCommandHistory->Undo();
			else if (eventType == kEventRedo && mCommandHistory != nullptr)
				mCommandHistory->Redo();
		}

		void EditorViewController::SetCommandHistory(CommandHistory* history)
		{
			mCommandHistory = history;
		}

		void EditorViewController::SetModel(EditorModel* model)
		{
			mModel = model;
		}
	}
}

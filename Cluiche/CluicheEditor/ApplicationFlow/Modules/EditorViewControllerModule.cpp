#include "EditorViewControllerModule.h"
#include "EditorModelModule.h"
#include "CommandHistoryModule.h"

#include <DiaApplicationFlow/ProcessingUnit.h>
#include <DiaApplicationFlow/RegistrationMacrosV2.h>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorViewControllerModule::kTypeId("EditorViewControllerModule");

		EditorViewControllerModule::EditorViewControllerModule(const Dia::Core::StringCRC& instanceId)
			: Dia::ApplicationFlow::Module(instanceId)
			, mModelRef(this, EditorModelModule::kTypeId)
			, mHistoryRef(this, CommandHistoryModule::kTypeId)
		{
		}

		Dia::ApplicationFlow::StartResult EditorViewControllerModule::DoStart()
		{
			EditorModelModule* modelModule = mModelRef.Get();
			CommandHistoryModule* historyModule = mHistoryRef.Get();

			if (modelModule != nullptr)
				mController.SetModel(&modelModule->GetModel());

			if (historyModule != nullptr)
				mController.SetCommandHistory(&historyModule->GetHistory());

			return Dia::ApplicationFlow::StartResult::kReady;
		}

		void EditorViewControllerModule::DoUpdate(float /*deltaTime*/)
		{
		}

		Dia::ApplicationFlow::StopResult EditorViewControllerModule::DoStop()
		{
			return Dia::ApplicationFlow::StopResult::kDone;
		}
	}
}

namespace { using EditorViewControllerModule_ = Cluiche::Editor::EditorViewControllerModule; }
DIA_MODULE(EditorViewControllerModule_);

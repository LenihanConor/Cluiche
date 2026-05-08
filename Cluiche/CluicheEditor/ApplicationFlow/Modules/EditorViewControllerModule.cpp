#include "EditorViewControllerModule.h"
#include "EditorModelModule.h"
#include "CommandHistoryModule.h"

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorViewControllerModule::kTypeId("EditorViewControllerModule");

		EditorViewControllerModule::EditorViewControllerModule(Dia::Application::ProcessingUnit* pu)
			: Dia::Application::Module(pu, kTypeId, RunningEnum::kIdle)
		{
		}

		void EditorViewControllerModule::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			AddDependancy(buildDependencies->GetModule(EditorModelModule::kTypeId));
			AddDependancy(buildDependencies->GetModule(CommandHistoryModule::kTypeId));
		}

		Dia::Application::StateObject::OpertionResponse EditorViewControllerModule::DoStart(const Dia::Application::StateObject::IStartData*)
		{
			EditorModelModule* modelModule = GetModule<EditorModelModule>();
			CommandHistoryModule* historyModule = GetModule<CommandHistoryModule>();

			mController.SetModel(&modelModule->GetModel());
			mController.SetCommandHistory(&historyModule->GetHistory());

			return Dia::Application::StateObject::OpertionResponse::kImmediate;
		}
	}
}

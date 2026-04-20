#include "CluicheEditorRunningPhase.h"
#include "CluicheEditorShutdownPhase.h"

#include "../Modules/EditorModelModule.h"
#include "../Modules/CommandHistoryModule.h"
#include "../Modules/EditorViewModule.h"
#include "../Modules/EditorViewControllerModule.h"
#include "../Modules/GameConnectionModule.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC CluicheEditorRunningPhase::kTypeId("CluicheEditorRunningPhase");

		CluicheEditorRunningPhase::CluicheEditorRunningPhase(Dia::Application::ProcessingUnit* pu)
			: Dia::Application::Phase(pu, kTypeId)
		{
		}

		bool CluicheEditorRunningPhase::FlaggedToStopUpdating() const
		{
			if (!ContainsModule(EditorModelModule::kTypeId))
				return false;

			const EditorModelModule* mod = static_cast<const EditorModelModule*>(GetModule(EditorModelModule::kTypeId));
			return mod != nullptr && mod->GetModel().IsCloseRequested();
		}

		void CluicheEditorRunningPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			AddModule(buildDependencies->GetModule(EditorModelModule::kTypeId));
			AddModule(buildDependencies->GetModule(CommandHistoryModule::kTypeId));
			AddModule(buildDependencies->GetModule(EditorViewModule::kTypeId));
			AddModule(buildDependencies->GetModule(EditorViewControllerModule::kTypeId));
			AddModule(buildDependencies->GetModule(GameConnectionModule::kTypeId));
		}

		void CluicheEditorRunningPhase::AfterModulesStart()
		{
		}
	}
}

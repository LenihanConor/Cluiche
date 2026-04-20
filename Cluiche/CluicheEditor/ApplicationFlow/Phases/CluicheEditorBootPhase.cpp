#include "CluicheEditorBootPhase.h"
#include "CluicheEditorRunningPhase.h"

#include "../Modules/EditorModelModule.h"
#include "../Modules/CommandHistoryModule.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC CluicheEditorBootPhase::kTypeId("CluicheEditorBootPhase");

		CluicheEditorBootPhase::CluicheEditorBootPhase(Dia::Application::ProcessingUnit* pu)
			: Dia::Application::Phase(pu, kTypeId)
		{
		}

		void CluicheEditorBootPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			AddModule(buildDependencies->GetModule(EditorModelModule::kTypeId));
			AddModule(buildDependencies->GetModule(CommandHistoryModule::kTypeId));
		}

		void CluicheEditorBootPhase::AfterModulesStart()
		{
			GetAssociatedProcessingUnit()->QueuePhaseTransition(CluicheEditorRunningPhase::kTypeId);
		}
	}
}

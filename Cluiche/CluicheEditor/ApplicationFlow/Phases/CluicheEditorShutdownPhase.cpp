#include "CluicheEditorShutdownPhase.h"

#include "../Modules/EditorModelModule.h"
#include "../Modules/EditorViewModule.h"

#include <DiaEditor/MVC/EditorView.h>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC CluicheEditorShutdownPhase::kTypeId("CluicheEditorShutdownPhase");

		CluicheEditorShutdownPhase::CluicheEditorShutdownPhase(Dia::Application::ProcessingUnit* pu)
			: Dia::Application::Phase(pu, kTypeId)
		{
		}

		void CluicheEditorShutdownPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			AddModule(buildDependencies->GetModule(EditorModelModule::kTypeId));
			AddModule(buildDependencies->GetModule(EditorViewModule::kTypeId));
		}

		void CluicheEditorShutdownPhase::BeforeModulesStop()
		{
			EditorViewModule* viewModule =
				static_cast<EditorViewModule*>(GetModule(EditorViewModule::kTypeId));
			if (viewModule == nullptr)
				return;

			Dia::Editor::EditorView& view = viewModule->GetView();
			view.SaveLayoutToDisk();
		}
	}
}

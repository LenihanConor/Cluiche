#include "CluicheEditorRunningPhase.h"
#include "CluicheEditorShutdownPhase.h"

#include "../Modules/EditorModelModule.h"
#include "../Modules/CommandHistoryModule.h"
#include "../Modules/EditorViewModule.h"
#include "../Modules/EditorViewControllerModule.h"
#include "../Modules/GameConnectionModule.h"
#include "../ProcessingUnits/CluicheEditorProcessingUnit.h"

#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/MVC/EditorView.h>
#include <DiaEditor/LiveConnection/GameConnectionController.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <string.h>

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
			CluicheEditorProcessingUnit* pu =
				static_cast<CluicheEditorProcessingUnit*>(GetAssociatedProcessingUnit());
			if (pu == nullptr)
				return;

			const char* projectPath = pu->GetProjectPath();
			if (projectPath == nullptr || projectPath[0] == '\0')
				return;

			EditorModelModule* modelModule =
				static_cast<EditorModelModule*>(GetModule(EditorModelModule::kTypeId));
			if (modelModule == nullptr)
				return;

			Dia::Editor::EditorModel& model = modelModule->GetModel();
			model.LoadProject(projectPath);

			for (unsigned int i = 0; i < model.GetManifestCount(); ++i)
				pu->LoadEditorManifest(model.GetManifestPath(i));

			EditorViewModule* viewModule =
				static_cast<EditorViewModule*>(GetModule(EditorViewModule::kTypeId));
			if (viewModule != nullptr)
			{
				Dia::Editor::EditorView& view = viewModule->GetView();
				view.SetLayoutPath("Data/editor-layout.json");
				view.LoadLayoutFromDisk();

				GameConnectionModule* gcModule =
					static_cast<GameConnectionModule*>(GetModule(GameConnectionModule::kTypeId));
				if (gcModule != nullptr && view.GetWebUIBridge() != nullptr)
				{
					Dia::Editor::GameConnectionController& controller = gcModule->GetController();
					controller.SetPersistencePath("Data/editor-connection.json");
					controller.LoadPersistedUrl();
					controller.Initialize(view.GetWebUIBridge(), &gcModule->GetManager());
				}
			}
		}
	}
}

#include "CluicheEditorRunningPhase.h"
#include "CluicheEditorShutdownPhase.h"

#include "../Modules/EditorModelModule.h"
#include "../Modules/CommandHistoryModule.h"
#include "../Modules/EditorViewModule.h"
#include "../Modules/EditorViewControllerModule.h"
#include "../Modules/GameConnectionModule.h"
#include "../Modules/PluginLoaderModule.h"
#include "../ProcessingUnits/CluicheEditorProcessingUnit.h"

#include <DiaApplication/ApplicationProcessingUnit.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/MVC/EditorView.h>
#include <DiaEditor/LiveConnection/GameConnectionController.h>
#include <DiaEditor/LiveConnection/GameConnectionManager.h>
#include <DiaCore/Core/Log.h>
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
			AddModule(buildDependencies->GetModule(PluginLoaderModule::kTypeId));
		}

		void CluicheEditorRunningPhase::AfterModulesStart()
		{
			Dia::Core::Log::OutputVaradicLine("CluicheEditorRunningPhase: AfterModulesStart");

			PluginLoaderModule* pluginLoader =
				static_cast<PluginLoaderModule*>(GetModule(PluginLoaderModule::kTypeId));
			if (pluginLoader == nullptr)
			{
				Dia::Core::Log::OutputVaradicLine("CluicheEditorRunningPhase: WARNING - PluginLoaderModule not found");
				return;
			}

			pluginLoader->LoadBuiltInPlugins();

			CluicheEditorProcessingUnit* pu =
				static_cast<CluicheEditorProcessingUnit*>(GetAssociatedProcessingUnit());
			const char* projectPath = (pu != nullptr) ? pu->GetProjectPath() : nullptr;

			if (projectPath != nullptr && projectPath[0] != '\0')
			{
				Dia::Core::Log::OutputVaradicLine("CluicheEditorRunningPhase: Loading project '%s'", projectPath);

				EditorModelModule* modelModule =
					static_cast<EditorModelModule*>(GetModule(EditorModelModule::kTypeId));
				if (modelModule != nullptr)
				{
					Dia::Editor::EditorModel& model = modelModule->GetModel();
					model.LoadProject(projectPath);

					for (unsigned int i = 0; i < model.GetManifestCount(); ++i)
						pluginLoader->LoadManifest(model.GetManifestPath(i));
				}
				else
				{
					Dia::Core::Log::OutputVaradicLine("CluicheEditorRunningPhase: WARNING - EditorModelModule not found");
				}
			}
			else
			{
				Dia::Core::Log::OutputVaradicLine("CluicheEditorRunningPhase: No project path set, skipping project load");
			}

			EditorViewModule* viewModule =
				static_cast<EditorViewModule*>(GetModule(EditorViewModule::kTypeId));
			if (viewModule != nullptr)
			{
				Dia::Editor::EditorView& view = viewModule->GetView();
				view.SetLayoutPath("Data/editor-layout.json");
				view.LoadLayoutFromDisk();

				pluginLoader->RegisterView(&view);

				GameConnectionModule* gcModule =
					static_cast<GameConnectionModule*>(GetModule(GameConnectionModule::kTypeId));
				Dia::Core::Log::OutputVaradicLine("CluicheEditorRunningPhase: GameConnectionModule=%p WebUIBridge=%p",
					gcModule, view.GetWebUIBridge());

				if (gcModule != nullptr && view.GetWebUIBridge() != nullptr)
				{
					Dia::Editor::GameConnectionController& controller = gcModule->GetController();
					controller.SetPersistencePath("Data/editor-connection.json");
					controller.LoadPersistedUrl();
					controller.Initialize(view.GetWebUIBridge(), &gcModule->GetManager());
					Dia::Core::Log::OutputVaradicLine("CluicheEditorRunningPhase: GameConnectionController initialized");
				}
				else
				{
					Dia::Core::Log::OutputVaradicLine("CluicheEditorRunningPhase: WARNING - GameConnection not wired (gcModule=%p bridge=%p)",
						gcModule, view.GetWebUIBridge());
				}
			}
			else
			{
				Dia::Core::Log::OutputVaradicLine("CluicheEditorRunningPhase: WARNING - EditorViewModule not found");
			}
		}
	}
}

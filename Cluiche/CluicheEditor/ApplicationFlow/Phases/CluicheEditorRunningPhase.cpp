#include "CluicheEditorRunningPhase.h"
#include "CluicheEditorShutdownPhase.h"

#include "../Modules/EditorModelModule.h"
#include "../Modules/CommandHistoryModule.h"
#include "../Modules/EditorViewModule.h"
#include "../Modules/EditorViewControllerModule.h"
#include "../Modules/SplashScreenModule.h"
#include "../Modules/PluginLoaderModule.h"
#include "../Modules/LoggerModule.h"
#include "../Modules/EditorConsoleSinkModule.h"
#include "../ProcessingUnits/CluicheEditorProcessingUnit.h"

#include <DiaApplicationFlow/ApplicationProcessingUnit.h>
#include <DiaEditor/MVC/EditorModel.h>
#include <DiaEditor/MVC/EditorView.h>
#include <DiaLogger/DiaLog.h>
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
			AddModule(buildDependencies->GetModule(LoggerModule::kTypeId));
			AddModule(buildDependencies->GetModule(EditorModelModule::kTypeId));
			AddModule(buildDependencies->GetModule(CommandHistoryModule::kTypeId));
			AddModule(buildDependencies->GetModule(SplashScreenModule::kTypeId));
			AddModule(buildDependencies->GetModule(EditorViewModule::kTypeId));
			AddModule(buildDependencies->GetModule(EditorViewControllerModule::kTypeId));
			AddModule(buildDependencies->GetModule(PluginLoaderModule::kTypeId));
			AddModule(buildDependencies->GetModule(EditorConsoleSinkModule::kTypeId));
		}

		void CluicheEditorRunningPhase::AfterModulesStart()
		{
			DIA_LOG_INFO("Application", "CluicheEditorRunningPhase: AfterModulesStart");

			EditorViewModule* viewModule =
				static_cast<EditorViewModule*>(GetModule(EditorViewModule::kTypeId));
			if (viewModule == nullptr)
			{
				DIA_LOG_WARNING("Application", "CluicheEditorRunningPhase: EditorViewModule not found");
				return;
			}

			Dia::Editor::EditorView& view = viewModule->GetView();
			view.SetLayoutPath("assets/configs/editor-layout.json");
			view.LoadLayoutFromDisk();

			PluginLoaderModule* pluginLoader =
				static_cast<PluginLoaderModule*>(GetModule(PluginLoaderModule::kTypeId));
			if (pluginLoader == nullptr)
			{
				DIA_LOG_WARNING("Application", "CluicheEditorRunningPhase: PluginLoaderModule not found");
				return;
			}

			pluginLoader->SetBridge(view.GetWebUIBridge());
			pluginLoader->RegisterView(&view);
			pluginLoader->LoadBuiltInPlugins();

			CluicheEditorProcessingUnit* pu =
				static_cast<CluicheEditorProcessingUnit*>(GetAssociatedProcessingUnit());
			const char* projectPath = (pu != nullptr) ? pu->GetProjectPath() : nullptr;

			if (projectPath != nullptr && projectPath[0] != '\0')
			{
				DIA_LOG_INFO("Application", "CluicheEditorRunningPhase: Loading project '%s'", projectPath);

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
					DIA_LOG_WARNING("Application", "CluicheEditorRunningPhase: EditorModelModule not found");
				}
			}
			else
			{
				DIA_LOG_INFO("Application", "CluicheEditorRunningPhase: No project path set, skipping project load");
			}
		}
	}
}

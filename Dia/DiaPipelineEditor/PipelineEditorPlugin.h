#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Project/ProjectContext.h>
#include <DiaCore/Architecture/Observer.h>

namespace Dia
{
	namespace PipelineEditor
	{
		class PipelineLogTailer;
		class PipelineBuildManager;
		class RunHistoryStore;

		class PipelineEditorPlugin : public Dia::Editor::IEditorPlugin, public Dia::Core::Observer
		{
		public:
			PipelineEditorPlugin();
			~PipelineEditorPlugin();

			const char* GetName() const override { return "Pipeline Editor"; }
			const char* GetVersion() const override { return "1.0.0"; }
			const char* GetDescription() const override { return "Live pipeline viewer and build trigger"; }
			const char* GetUIPath() const override { return "dia://plugins/diapipelineeditor/index.html"; }
			Dia::Editor::LayoutMode GetLayoutMode() const override { return Dia::Editor::LayoutMode::kDockable; }
			Dia::Editor::EditorToolbarItem GetToolbarItem() const override { Dia::Editor::EditorToolbarItem item = Dia::Editor::IEditorPlugin::GetToolbarItem(); item.pinned = true; return item; }

			void OnLoad(const Dia::Editor::EditorPluginContext& context) override;
			void OnUnload() override;
			void OnUpdate(float deltaTime) override;

			void ObserverNotification(const Dia::Core::ObserverSubject* subject, int message) override;

		private:
			static void OnProjectChangedStatic(const Dia::Editor::ProjectContext& ctx, void* ud);
			static void ExtractTarget(const char* diagamePath, char* targetOut, size_t targetSize);

			void PushEventsToUI();
			void RegisterCommands();
			void UnregisterCommands();

			PipelineLogTailer* mTailer;
			PipelineBuildManager* mBuildManager;
			RunHistoryStore* mHistoryStore;
			Dia::Editor::WebUIBridge* mBridge;
			int mLastPushedEventIndex;
			bool mLastBuildRunning;
			int mLastExitCode;
			char mRepoRoot[512];

			static const unsigned int kDiagamePathLength = 512;
			char mDiagamePath[kDiagamePathLength];
		};
	}
}

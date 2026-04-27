#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaCore/Architecture/Observer.h>

namespace Dia
{
	namespace PipelineEditor
	{
		class PipelineLogTailer;
		class PipelineBuildManager;

		class PipelineEditorPlugin : public Dia::Editor::IEditorPlugin, public Dia::Core::Observer
		{
		public:
			PipelineEditorPlugin();
			~PipelineEditorPlugin();

			const char* GetName() const override { return "DiaPipelineEditor"; }
			const char* GetVersion() const override { return "1.0.0"; }
			const char* GetDescription() const override { return "Live pipeline viewer and build trigger"; }
			const char* GetUIPath() const override { return "dia://diapipelineeditor/index.html"; }
			Dia::Editor::LayoutMode GetLayoutMode() const override { return Dia::Editor::LayoutMode::kDockable; }

			void OnLoad(const Dia::Editor::EditorPluginContext& context) override;
			void OnUnload() override;
			void OnUpdate(float deltaTime) override;

			void ObserverNotification(const Dia::Core::ObserverSubject* subject, int message) override;

		private:
			void PushEventsToUI();
			void RegisterCommands();
			void UnregisterCommands();

			PipelineLogTailer* mTailer;
			PipelineBuildManager* mBuildManager;
			Dia::Editor::WebUIBridge* mBridge;
			int mLastPushedEventIndex;
			char mRepoRoot[512];
		};
	}
}

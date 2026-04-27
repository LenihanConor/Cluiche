#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>

namespace Dia
{
	namespace PipelineEditor
	{
		class PipelineLogTailer;

		class PipelineEditorPlugin : public Dia::Editor::IEditorPlugin
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

		private:
			PipelineLogTailer* mTailer;
			Dia::Editor::WebUIBridge* mBridge;
		};
	}
}

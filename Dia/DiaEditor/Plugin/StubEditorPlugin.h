#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaEditor/Plugin/EditorPluginRegistrationMacros.h>

namespace Dia
{
	namespace Editor
	{
		class StubEditorPlugin : public IEditorPlugin
		{
		public:
			const char* GetName() const override { return "StubEditorPlugin"; }
			const char* GetVersion() const override { return "1.0"; }
			const char* GetDescription() const override { return "Stub plugin for testing"; }
			const char* GetUIPath() const override { return "dia://editor/stub/index.html"; }
			LayoutMode GetLayoutMode() const override { return LayoutMode::kDockable; }
			void OnLoad(EditorModel* /*model*/) override {}
			void OnUnload() override {}
			void OnUpdate(float /*deltaTime*/) override {}
		};
	}
}


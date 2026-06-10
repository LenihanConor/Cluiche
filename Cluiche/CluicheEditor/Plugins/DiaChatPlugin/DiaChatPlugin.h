#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;
	}
}

namespace CluicheEditor
{
	class ChatPanelBridge;

	class DiaChatPlugin : public Dia::Editor::IEditorPlugin
	{
	public:
		DiaChatPlugin();

		static const Dia::Core::StringCRC kPluginId;

		const char* GetName()        const override { return "DiaChatPlugin"; }
		const char* GetVersion()     const override { return "1.0"; }
		const char* GetDescription() const override { return "Dockable AI assistant for CluicheEditor"; }
		const char* GetUIPath()      const override { return "dia://chat/"; }
		Dia::Editor::LayoutMode GetLayoutMode() const override { return Dia::Editor::LayoutMode::kDockable; }

		void OnLoad(const Dia::Editor::EditorPluginContext& context) override;
		void OnUnload() override;
		void OnUpdate(float deltaTime) override;

	private:
		ChatPanelBridge*         mBridge    = nullptr;
		Dia::Editor::WebUIBridge* mWebBridge = nullptr;
	};
}

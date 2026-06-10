#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaCore/CRC/StringCRC.h>

#include <atomic>
#include <string>
#include <thread>

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;
		class EditorActionQueue;
	}
}

namespace CluicheEditor
{
	class ChatPanelBridge;

	class DiaChatPlugin : public Dia::Editor::IEditorPlugin
	{
	public:
		DiaChatPlugin();
		~DiaChatPlugin();

		static const Dia::Core::StringCRC kPluginId;

		const char* GetName()        const override { return "DiaChatPlugin"; }
		const char* GetVersion()     const override { return "1.0"; }
		const char* GetDescription() const override { return "Dockable AI assistant for CluicheEditor"; }
		const char* GetUIPath()      const override { return "dia://plugins/diachat/index.html"; }
		Dia::Editor::LayoutMode GetLayoutMode() const override { return Dia::Editor::LayoutMode::kDockable; }

		void OnLoad(const Dia::Editor::EditorPluginContext& context) override;
		void OnUnload() override;
		void OnUpdate(float deltaTime) override;

	private:
		void RegisterPythonModule();
		void RegisterStubHandlers();
		void DispatchSendMessage(const std::string& text, const std::string& contextMode);

		ChatPanelBridge*              mBridge        = nullptr;
		Dia::Editor::WebUIBridge*     mWebBridge     = nullptr;
		Dia::Editor::EditorActionQueue* mActionQueue = nullptr;

		// Background thread for the current LLM stream (at most one at a time).
		std::thread                   mSendThread;
		std::atomic<bool>             mSendInFlight  { false };
	};
}

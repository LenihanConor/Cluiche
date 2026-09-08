#pragma once

#include <DiaEditor/Plugin/IEditorPlugin.h>
#include <DiaCore/CRC/StringCRC.h>
#include <DiaObservation/Health/HealthReporterBase.h>

#include <atomic>
#include <string>
#include <thread>

namespace Dia
{
	namespace Editor
	{
		class WebUIBridge;
		class EditorActionQueue;
		class EditorActionRegistry;
	}
	namespace Observation { namespace Metric { class Counter; } }
}

namespace CluicheEditor
{
	class ChatPanelBridge;

	class DiaChatPluginHealth : public Dia::Observation::Health::HealthReporterBase
	{
	public:
		static const Dia::Core::StringCRC kName;
		Dia::Core::StringCRC GetReporterName() const override { return kName; }
		void SetPythonReady(bool ok);
	};

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

		ChatPanelBridge*                            mBridge        = nullptr;
		Dia::Editor::WebUIBridge*                   mWebBridge     = nullptr;
		Dia::Editor::EditorActionQueue*             mActionQueue   = nullptr;
		Dia::Editor::EditorActionRegistry*          mRegistry      = nullptr;

		std::thread                                 mSendThread;
		std::atomic<bool>                           mSendInFlight  { false };

		DiaChatPluginHealth                         mPluginHealth;
		Dia::Observation::Metric::Counter*          mMetricPythonInitErrors      = nullptr;
		Dia::Observation::Metric::Counter*          mMetricMessagesDispatched    = nullptr;
		Dia::Observation::Metric::Counter*          mMetricActionQueueNullErrors = nullptr;
	};
}

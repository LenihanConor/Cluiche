#include "EditorConsoleSinkModule.h"
#include "EditorViewModule.h"

#include <DiaEditor/UI/WebUIBridge.h>
#include <DiaLogger/Logger.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Cluiche
{
	namespace Editor
	{
		const Dia::Core::StringCRC EditorConsoleSinkModule::kTypeId("EditorConsoleSinkModule");

		EditorConsoleSinkModule::EditorConsoleSinkModule(Dia::Application::ProcessingUnit* pu)
			: Dia::Application::Module(pu, kTypeId, RunningEnum::kIdle)
		{
		}

		void EditorConsoleSinkModule::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
			AddDependancy(buildDependencies->GetModule(EditorViewModule::kTypeId));
		}

		Dia::Application::StateObject::OpertionResponse EditorConsoleSinkModule::DoStart(const Dia::Application::StateObject::IStartData*)
		{
			EditorViewModule* viewModule = GetModule<EditorViewModule>();

			Dia::Editor::WebUIBridge* bridge = viewModule->GetView().GetWebUIBridge();
			mConsoleSink.SetBridge(bridge);

			bridge->RegisterEventHandler(Dia::Core::StringCRC("console_ready"),
				[this](const Json::Value&) { mConsoleSink.NotifyConsoleReady(); });

			Dia::Logger::Logger::Instance().RegisterSink(&mConsoleSink);

			return Dia::Application::StateObject::OpertionResponse::kImmediate;
		}

		void EditorConsoleSinkModule::DoStop()
		{
			Dia::Logger::Logger::Instance().UnregisterSink(&mConsoleSink);
			mConsoleSink.SetBridge(nullptr);
		}
	}
}

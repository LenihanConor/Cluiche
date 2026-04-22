#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaCore/Architecture/Observer.h>
#include <DiaLogger/DebugOutputSink.h>
#include <DiaEditor/Sinks/EditorConsoleSink.h>

namespace Cluiche
{
	namespace Editor
	{
		class EditorViewModule;

		class LoggerModule : public Dia::Application::Module, public Dia::Core::Observer
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			LoggerModule(Dia::Application::ProcessingUnit* pu);
			~LoggerModule();

			void SetViewModule(EditorViewModule* viewModule) { mViewModule = viewModule; }
			void ApplyConfig(const char* configPath);

			void DisconnectConsoleSinkBridge();

			void ObserverNotification(const Dia::Core::ObserverSubject* subject, int message) override;

		protected:
			Dia::Application::StateObject::OpertionResponse DoStart(const Dia::Application::StateObject::IStartData*) override;
			void DoUpdate() override;
			void DoStop() override;

		private:
			Dia::Logger::DebugOutputSink mDebugOutputSink;
			Dia::Editor::EditorConsoleSink mConsoleSink;
			EditorViewModule* mViewModule;
			bool mConsoleSinkBridgeConnected;
		};
	}
}

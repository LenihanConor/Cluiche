#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaApplication/ModuleRef.h>
#include <DiaLogger/DebugOutputSink.h>
#include <DiaEditor/Sinks/EditorConsoleSink.h>

namespace Cluiche
{
	namespace Editor
	{
		class EditorViewModule;

		class LoggerModule : public Dia::Application::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			LoggerModule(Dia::Application::ProcessingUnit* pu);
			~LoggerModule();

			void ApplyConfig(const char* configPath);

		protected:
			Dia::Application::StateObject::OpertionResponse DoStart(const Dia::Application::StateObject::IStartData*) override;
			void DoUpdate() override;
			void DoStop() override;

		private:
			Dia::Logger::DebugOutputSink mDebugOutputSink;
			Dia::Editor::EditorConsoleSink mConsoleSink;
			Dia::Application::ModuleRef<EditorViewModule> mViewRef;
			bool mConsoleSinkBridgeConnected;
		};
	}
}

#pragma once

#include <DiaApplication/ApplicationModule.h>
#include <DiaLogger/DebugOutputSink.h>

namespace Cluiche
{
	namespace Editor
	{
		class LoggerModule : public Dia::Application::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			LoggerModule(Dia::Application::ProcessingUnit* pu);

			void ApplyConfig(const char* configPath);

		protected:
			Dia::Application::StateObject::OpertionResponse DoStart(const Dia::Application::StateObject::IStartData*) override;
			void DoUpdate() override;
			void DoStop() override;

		private:
			Dia::Logger::DebugOutputSink mDebugOutputSink;
		};
	}
}

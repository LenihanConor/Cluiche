#pragma once

#include <DiaApplicationFlow/ApplicationModule.h>
#include <DiaEditor/Sinks/EditorConsoleSink.h>

namespace Cluiche
{
	namespace Editor
	{
		class EditorConsoleSinkModule : public Dia::Application::Module
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit EditorConsoleSinkModule(Dia::Application::ProcessingUnit* pu);

		protected:
			void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies) override;
			Dia::Application::StateObject::OpertionResponse DoStart(const Dia::Application::StateObject::IStartData*) override;
			void DoStop() override;

		private:
			Dia::Editor::EditorConsoleSink mConsoleSink;
		};
	}
}

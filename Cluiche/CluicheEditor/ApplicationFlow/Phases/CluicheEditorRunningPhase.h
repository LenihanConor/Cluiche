#pragma once

#include <DiaApplication/ApplicationPhase.h>

namespace Cluiche
{
	namespace Editor
	{
		class CluicheEditorRunningPhase : public Dia::Application::Phase
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit CluicheEditorRunningPhase(Dia::Application::ProcessingUnit* pu);

			bool FlaggedToStopUpdating() const override;

		protected:
			void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies) override;
			void AfterModulesStart() override;
		};
	}
}

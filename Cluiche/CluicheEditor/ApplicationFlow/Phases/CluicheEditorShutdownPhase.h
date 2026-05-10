#pragma once

#include <DiaApplicationFlow/ApplicationPhase.h>

namespace Cluiche
{
	namespace Editor
	{
		class CluicheEditorShutdownPhase : public Dia::Application::Phase
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit CluicheEditorShutdownPhase(Dia::Application::ProcessingUnit* pu);

			bool FlaggedToStopUpdating() const override { return true; }

		protected:
			void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies) override;
			void BeforeModulesStop() override;
		};
	}
}

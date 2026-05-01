#pragma once

#include <DiaApplication/ApplicationPhase.h>

namespace Cluiche
{
	namespace Editor
	{
		class CluicheEditorBootPhase : public Dia::Application::Phase
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			explicit CluicheEditorBootPhase(Dia::Application::ProcessingUnit* pu);

			bool FlaggedToStopUpdating() const override { return false; }

		protected:
			void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies) override;
			void AfterModulesStart() override;
		};
	}
}

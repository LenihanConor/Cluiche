#pragma once

#include <DiaApplication/ApplicationPhase.h>
#include <DiaCore/CRC/StringCRC.h>

namespace Dia { namespace Application { class ProcessingUnit; } }

namespace Cluiche
{
	namespace DummyStage
	{
		class SimRunningPhase : public Dia::Application::Phase
		{
		public:
			static const Dia::Core::StringCRC kTypeId;

			SimRunningPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit);

			virtual bool FlaggedToStopUpdating(void)const override { return false; }
			virtual void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies) override;
		};
	}
}

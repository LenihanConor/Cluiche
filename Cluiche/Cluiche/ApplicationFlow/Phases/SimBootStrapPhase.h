#pragma once

#include <DiaApplication/ApplicationPhase.h>

namespace Dia { namespace Application { class ProcessingUnit; } }

namespace Cluiche
{
	////////////////////////////////////////////////////
	//
	// SimBootStrapPhase: Running phase for sim bootstrap
	//
	////////////////////////////////////////////////////
	class SimBootStrapPhase : public Dia::Application::Phase
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		SimBootStrapPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit);

		virtual bool FlaggedToStopUpdating(void)const override { return true; }
		virtual void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)override;
	};
}

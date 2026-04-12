#pragma once

#include <DiaApplication/ApplicationPhase.h>

namespace Dia { namespace Application { class ProcessingUnit; } }

namespace Cluiche
{
	////////////////////////////////////////////////////
	//
	// SimBootPhase: Start phase for the sim thread
	//
	////////////////////////////////////////////////////
	class SimBootPhase : public Dia::Application::Phase
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		SimBootPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit);

		virtual void AfterModulesStart()override;
		virtual bool FlaggedToStopUpdating(void)const override { return true; }
		virtual void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)override;
	};
}

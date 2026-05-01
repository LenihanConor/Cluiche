#pragma once

#include "CluicheKernel/ApplicationFlow/Phases/MainPhaseBase.h"

namespace Dia { namespace Application { class ProcessingUnit; } }

namespace Cluiche
{
	////////////////////////////////////////////////////
	//
	// MainBootPhase: Initial phase for the main thread
	//
	////////////////////////////////////////////////////
	class MainBootPhase : public Cluiche::Main::MainPhaseBase
	{
	public:
		static const Dia::Core::StringCRC kTypeId;

		MainBootPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& instanceId = kTypeId);

		virtual void AfterModulesStart()override;

		virtual void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)override;
	};
}

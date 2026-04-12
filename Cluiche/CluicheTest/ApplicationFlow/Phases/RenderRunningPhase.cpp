#include "ApplicationFlow/Phases/RenderRunningPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	const Dia::Core::StringCRC RenderRunningPhase::kUniqueId("RenderRunningPhase");

	RenderRunningPhase::RenderRunningPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
		: Dia::Application::Phase(associatedProcessingUnit, kUniqueId)
	{}

	void RenderRunningPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
	{
		//AddModule(buildDependencies->GetModule(KernelModule::kUniqueId));
	}
}
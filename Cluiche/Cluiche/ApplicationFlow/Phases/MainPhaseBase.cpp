#include "ApplicationFlow/Phases/MainBootPhase.h"

#include "ApplicationFlow/Modules/MainKernelModule.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	MainPhaseBase::MainPhaseBase(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& uniqueId, unsigned int maxModules)
		: Dia::Application::Phase(associatedProcessingUnit, uniqueId, maxModules)
	{}

	bool MainPhaseBase::ShouldQuitApplication()const
	{
		bool containsKernel = this->ContainsModule(MainKernelModule::kUniqueId);

		if (!containsKernel)
		{
			DIA_ASSERT(0, "%s Phase does not contain MainKernelModule, which is used to shut down th application");
			return false;
		}

		const Cluiche::MainKernelModule* kernel = static_cast<const Cluiche::MainKernelModule*>(this->GetModule(MainKernelModule::kUniqueId));

		return kernel->ShouldQuitApplication();
	}
}
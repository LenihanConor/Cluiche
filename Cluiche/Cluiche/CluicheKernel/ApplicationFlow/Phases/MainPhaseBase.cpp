#include "CluicheKernel/ApplicationFlow/Phases/MainPhaseBase.h"

#include "CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	namespace Main
	{
		MainPhaseBase::MainPhaseBase(Dia::Application::ProcessingUnit* associatedProcessingUnit, const Dia::Core::StringCRC& uniqueId, unsigned int maxModules)
			: Dia::Application::Phase(associatedProcessingUnit, uniqueId, maxModules)
		{}

		bool MainPhaseBase::FlaggedToStopUpdating()const
		{
			bool containsKernel = this->ContainsModule(Main::KernelModule::kUniqueId);

			if (!containsKernel)
			{
				DIA_ASSERT(0, "%s Phase does not contain KernelModule, which is used to shut down the application", this->GetUniqueId().AsChar());
				return false;
			}

			const Cluiche::Main::KernelModule* kernel = this->GetModule<Cluiche::Main::KernelModule>();

			return kernel->FlaggedToStopUpdating();
		}
	}
}
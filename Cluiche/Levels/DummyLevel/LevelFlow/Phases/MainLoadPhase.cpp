#include "LevelFlow/Phases/MainLoadPhase.h"

//#include "ApplicationFlow/Modules/MainKernelModule.h"
//#include "ApplicationFlow/Phases/MainBootStrapPhase.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

namespace Cluiche
{
	namespace DummyLevel
	{
		const Dia::Core::StringCRC MainLoadPhase::kUniqueId("DummyProject::MainLoadPhase");

		MainLoadPhase::MainLoadPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
			: Dia::Application::Phase(associatedProcessingUnit, kUniqueId, 2)
		{}

		void MainLoadPhase::AfterModulesStart()
		{
	//		GetAssociatedProcessingUnit()->QueuePhaseTransition(MainBootStrapPhase::kUniqueId);
		}

		void MainLoadPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
		{
	//		AddModule(buildDependencies->GetModule(MainKernelModule::kUniqueId));
		}

		bool MainLoadPhase::FlaggedToStopUpdating()const
		{
	/*		bool containsKernel = this->ContainsModule(MainKernelModule::kUniqueId);

			if (!containsKernel)
			{
				DIA_ASSERT(0, "%s Phase does not contain MainKernelModule, which is used to shut down th application");
				return false;
			}

			const Cluiche::MainKernelModule* kernel = this->GetModule<Cluiche::MainKernelModule>();

			return kernel->FlaggedToStopUpdating();
			*/

			return true;
		}
	}
}
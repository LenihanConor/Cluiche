#include "ApplicationFlow/Phases/MainBootStrapPhase.h"

#include "CluicheKernel/MainKernelModule.h"
#include "ApplicationFlow/Modules/LevelRegistryModule.h"
#include "Levels/DummyProject/DummyProject.h"

#include <Cluiche/Source/LevelRegistry.h>

#include <DiaApplication/ApplicationProcessingUnit.h>

#include <DiaCore/Strings/stringutils.h>

namespace Cluiche
{
	const Dia::Core::StringCRC MainBootStrapPhase::kUniqueId("MainBootStrapPhase");

	MainBootStrapPhase::MainBootStrapPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
		: MainPhaseBase(associatedProcessingUnit, kUniqueId)
		, mLaunchUIPage(this)
	{}

	void MainBootStrapPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
	{
		AddModule(buildDependencies->GetModule(Kernel::MainKernelModule::kUniqueId));
		AddModule(buildDependencies->GetModule(LevelRegistryModule::kUniqueId));
	}

	void MainBootStrapPhase::AfterModulesStart()
	{
		// Initialize all Levels
		Cluiche::LevelRegistry& levelRegister = GetModule<Cluiche::LevelRegistryModule>()->GetLevelRegistry();

		levelRegister.Register(Cluiche::DummyProject::kLevelUniqueId, Cluiche::LevelRegistry::Data(Cluiche::DummyProject::kLevelUniqueId));
		

		REGISTER ALL THE FLOWS HERE
		HOW WILL THIS WORK FOR SIM/RENDER PHASES, DO THEY TRANSITION? RENDER MAY NOT BUT SIM WILL. 


		mLaunchUIPage.InitializePage();

		Cluiche::Kernel::MainKernelModule* kernel = this->GetModule<Cluiche::Kernel::MainKernelModule>();
		kernel->GetUISystem()->LoadPage(mLaunchUIPage); //TODO replace this with a templated version
	}

	void MainBootStrapPhase::RequestLaunchLevel(const Dia::Core::Containers::String64& levelName)
	{
		//TODO how do we seperate from not selected to not found
		// If the launch_id = zero then we have no selected Id, through up a pop up
		if (levelName.Length() <= 0)
		{
			//TODO POP-UPS
			//webix.alert({ type: "alert-warning", text : "Select a game to launch." });
			DIA_ASSERT(0, "Did not select a game to launch");
			return;
		}

		// Get Cluiche level manager to translate the UI string into an actual registered level
		// From this level we can get its entry phase and transition to this.
		const Cluiche::LevelRegistry& levelRegister = GetModule<Cluiche::LevelRegistryModule>()->GetLevelRegistry();

		const Dia::Core::StringCRC& entryPhase = levelRegister.FetchEntryPhase(Dia::Core::StringCRC(levelName.AsCStr()));

		if (entryPhase == Dia::Core::StringCRC::kZero)
		{
			DIA_ASSERT(0, "Could not find entryPhase for %s", levelName.AsCStr());
			return;
		}

		GetAssociatedProcessingUnit()->QueuePhaseTransition(entryPhase);
	}
}
#include "ApplicationFlow/Phases/MainBootStrapPhase.h"

#include "CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h"
#include "ApplicationFlow/Modules/LevelRegistryModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/MainUIModule.h"
#include "Levels/DummyLevel/DummyLevel.h"
#include "Levels/DummyLevel/LevelFlow/Phases/MainLoadPhase.h"

#include <CluicheKernel/LevelRegistry.h>

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
		AddModule(buildDependencies->GetModule(MainUIModule::kUniqueId));
	}

	void MainBootStrapPhase::AfterModulesStart()
	{
		mLaunchUIPage.InitializePage();

		Cluiche::MainUIModule* ui = this->GetModule<Cluiche::MainUIModule>();
		ui->GetUISystem()->LoadPage(mLaunchUIPage); 
	}


	void MainBootStrapPhase::BeforeModulesStop()
	{
		Cluiche::MainUIModule* ui = this->GetModule<Cluiche::MainUIModule>();
		ui->GetUISystem()->UnloadPage(); 
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

		// Allocate level
		Cluiche::Kernel::ILevel* level = nullptr;
		if (levelName == "dummy_level")
		{
			level = DIA_NEW(Cluiche::DummyLevel::Level(this, this->GetAssociatedProcessingUnit(), nullptr, nullptr)); //TODO MEMORY LEAK,  a reference should be kept in the level regsitry
		}

		if (level == nullptr)
		{
			DIA_ASSERT(0, "Counld not find level");
			return;
		}
		
		// Transition to the next phase
		const Dia::Core::StringCRC& entryPhaseUniqueId = level->GetEntryPhaseUniqueId();
			
		if (entryPhaseUniqueId == Dia::Core::StringCRC::kZero)
		{
			DIA_ASSERT(0, "Could not find entryPhase for %s", levelName.AsCStr());
			return;
		}

		GetAssociatedProcessingUnit()->QueuePhaseTransition(entryPhaseUniqueId);
	}
}
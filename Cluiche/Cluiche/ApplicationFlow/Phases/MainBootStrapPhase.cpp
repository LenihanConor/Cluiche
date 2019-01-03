#include "ApplicationFlow/Phases/MainBootStrapPhase.h"

#include "CluicheKernel/ApplicationFlow/Modules/MainKernelModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/LevelFactoryModule.h"
#include "CluicheKernel/ApplicationFlow/Modules/MainUIModule.h"
#include "Levels/DummyLevel/DummyLevel.h"
#include "Levels/UnitTestLevel/UnitTestLevel.h"

#include <CluicheKernel/LevelFactory.h>

#include <DiaApplication/ApplicationProcessingUnit.h>

#include <DiaCore/Strings/stringutils.h>

namespace Cluiche
{
	const Dia::Core::StringCRC MainBootStrapPhase::kUniqueId("MainBootStrapPhase");

	MainBootStrapPhase::MainBootStrapPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
		: MainPhaseBase(associatedProcessingUnit, kUniqueId)
		, mLaunchUIPage(this)
		, mDummyLevel(nullptr)
		, mUnitTestLevel(nullptr)
	{}

	void MainBootStrapPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
	{
		AddModule(buildDependencies->GetModule(Main::KernelModule::kUniqueId));
		AddModule(buildDependencies->GetModule(Main::LevelFactoryModule::kUniqueId));
		AddModule(buildDependencies->GetModule(Main::UIModule::kUniqueId));
	}

	void MainBootStrapPhase::AfterModulesStart()
	{
		mDummyLevel = DIA_NEW(Cluiche::DummyLevel::Level(this, this->GetAssociatedProcessingUnit(), nullptr, nullptr)); 
		mUnitTestLevel = DIA_NEW(Cluiche::UnitTestLevel::Level(this, this->GetAssociatedProcessingUnit(), nullptr, nullptr));
		
		mLaunchUIPage.InitializePage();

		Cluiche::Main::UIModule* ui = this->GetModule<Cluiche::Main::UIModule>();
		ui->GetUISystem()->LoadPage(mLaunchUIPage); 

		Cluiche::Main::LevelFactoryModule* levelRegistry = this->GetModule<Cluiche::Main::LevelFactoryModule>();

		// This is for the re-entrance case
		if (levelRegistry->GetLevelRegistry().GetCurrentLevel() != nullptr)
		{
			levelRegistry->GetLevelRegistry().DeleteLevel();
		}
	}

	void MainBootStrapPhase::BeforeModulesStop()
	{
		DIA_DELETE(mDummyLevel);

		Cluiche::Main::UIModule* ui = this->GetModule<Cluiche::Main::UIModule>();
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
			level = mDummyLevel;
		}
		else if (levelName == "unit_test")
		{
			level = mUnitTestLevel;
		}
		if (level == nullptr)
		{
			//webix.alert({ type: "alert-warning", text : "Could not find level." });

			DIA_ASSERT(0, "Could not find level");
			return;
		}

		this->GetModule<Cluiche::Main::LevelFactoryModule>()->GetLevelRegistry().SetCurrentLevel(level);
		
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
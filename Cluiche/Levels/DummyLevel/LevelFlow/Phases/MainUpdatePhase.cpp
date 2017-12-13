#include "ApplicationFlow/Phases/MainBootStrapPhase.h"

#include "CluicheBase/ApplicationFlow/Modules/MainKernelModule.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

#include <DiaUI/IUISystem.h>
#include <DiaUI/Page.h>

#include <DiaCore/Strings/stringutils.h>

namespace Cluiche
{
	class LaunchUIPage : public Dia::UI::Page
	{
	public:
		void LaunchLevel(const Dia::UI::BoundMethodArgs& arg)
		{
			DIA_ASSERT(arg.Size() == 2, "Unexpected inputs from UI");
			
			int levelID = Dia::Core::StringConvertToInt( arg.At(0).GetString().AsCStr() );
			const Dia::Core::Containers::String64& levelName = arg.At(1).GetString();
			
			// If the launch_id = zero then we have no selected Id, through up a pop up
			if (levelID == 0)
			{
				//TODO POP-UPS
				//webix.alert({ type: "alert-warning", text : "Select a game to launch." });
				return;
			}

			else if (levelID == -1)
			{
				//TODO POP-UPS
				//webix.alert({ type: "alert-error", text : "There has been a problem we could not find the selected item. Talk to an engineer" });
				return;
			}


			// Launch Level
		}

		LaunchUIPage()
			: Dia::UI::Page(Dia::Core::FilePath("root", "BootStrap/", "bootscreen.html"))
		{
			BindMethod(Dia::UI::BoundMethod::CreateBoundMethod("Application_LaunchLevel", 
				Dia::UI::BoundMethod::MethodPtr(this, &LaunchUIPage::LaunchLevel)));
		}
	};

	const Dia::Core::StringCRC MainBootStrapPhase::kUniqueId("MainBootStrapPhase");

	MainBootStrapPhase::MainBootStrapPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit)
		: MainPhaseBase(associatedProcessingUnit, kUniqueId)
	{}

	void MainBootStrapPhase::DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)
	{
		AddModule(buildDependencies->GetModule(MainKernelModule::kUniqueId));
	}

	void MainBootStrapPhase::AfterModulesStart()
	{
		LaunchUIPage launchUIPage;
		Cluiche::MainKernelModule* kernel = this->GetModule<Cluiche::MainKernelModule>();
		kernel->GetUISystem()->LoadPage(launchUIPage); //TODO replace this with a templated version
	}
}
#include "ApplicationFlow/Phases/MainBootStrapPhase.h"

#include "ApplicationFlow/Modules/MainKernelModule.h"

#include <DiaApplication/ApplicationProcessingUnit.h>

#include <DiaUI/IUISystem.h>
#include <DiaUI/Page.h>

namespace Cluiche
{
	class LaunchUIPage : public Dia::UI::Page
	{
	public:
		void DoSomething(const Dia::UI::BoundMethodArgs&)
		{
			int x = 0;
			x++;
		}

		LaunchUIPage()
			: Dia::UI::Page(Dia::Core::FilePath("root", "DiaGraphicWithUITest/", "bootscreen.html"))
		{
			BindMethod(Dia::UI::BoundMethod("backgroundGrey", Dia::UI::BoundMethod::MethodPtr(this, &LaunchUIPage::DoSomething)));
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
		static_cast<MainKernelModule*>(GetModule(MainKernelModule::kUniqueId))->mAwesomiumUISystem->LoadPage(launchUIPage);
	}
}
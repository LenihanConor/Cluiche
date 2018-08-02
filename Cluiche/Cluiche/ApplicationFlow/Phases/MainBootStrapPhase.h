#pragma once

#include "CluicheKernel/ApplicationFlow/Phases/MainPhaseBase.h"

#include "Cluiche/Source/LaunchUIPage.h"

#include <DiaCore/Strings/String64.h>

namespace Dia { namespace Application { class ProcessingUnit; } }
namespace Cluiche { namespace DummyLevel { class Level; } }
namespace Cluiche { namespace UnitTestLevel { class Level; } }


namespace Cluiche
{
	////////////////////////////////////////////////////
	//
	// This application phase sends us to the bootstrap UI. 
	// This kept as a different phase as we could bypass it 
	// and laynch straight into a game.
	//
	////////////////////////////////////////////////////
	class MainBootStrapPhase : public Main::MainPhaseBase, LaunchUIPageExternalInterface
	{
	public:
		static const Dia::Core::StringCRC kUniqueId;

		MainBootStrapPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit);

		void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)override;

		virtual void AfterModulesStart() override;
		virtual void BeforeModulesStop() override;

		virtual void RequestLaunchLevel(const Dia::Core::Containers::String64& levelName)override;

	private:
		LaunchUIPage mLaunchUIPage;

		Cluiche::DummyLevel::Level* mDummyLevel;
		Cluiche::UnitTestLevel::Level* mUnitTestLevel;
	};
}

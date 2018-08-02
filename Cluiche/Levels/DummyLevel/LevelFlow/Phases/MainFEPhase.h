#pragma once

#include "UI\DummyUIPage.h"

#include <DiaApplication/ApplicationPhase.h>
#include <DiaCore/CRC/StringCRC.h>

#include <CluicheKernel/ApplicationFlow/Phases/MainPhaseBase.h>

namespace Dia { namespace Application { class ProcessingUnit; } }

namespace Cluiche
{
	namespace DummyLevel
	{
		////////////////////////////////////////////////////
		//
		// MainLoadPhase: Initial phase for the main thread to load dummy project
		//
		////////////////////////////////////////////////////
		class MainFEPhase : public Cluiche::Main::MainPhaseBase, public DummyUIPageExternalInterface
		{
		public:
			static const Dia::Core::StringCRC kUniqueId;

			MainFEPhase(Dia::Application::ProcessingUnit* associatedProcessingUnit);
		
			virtual void RequestExitLevel();

		private:
			virtual void AfterModulesStart()override;
			virtual void BeforeModulesStop()override;

			virtual void DoBuildDependancies(Dia::Application::IBuildDependencyData* buildDependencies)override;

			DummyUIPage mUi;
		};
	}
}
